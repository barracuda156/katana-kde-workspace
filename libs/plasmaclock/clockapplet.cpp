/***************************************************************************
 *   Copyright (C) 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>      *
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
 *   Copyright (C) 2008-2009 by Davide Bettio <davide.bettio@kdemail.net>  *
 *   Copyright (C) 2009 by John Layt <john@layt.net>                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "clockapplet.h"

#include <math.h>

#include <QtGui/QPainter>
#include <QtGui/qstyleoption.h>
#include <QtGui/QSpinBox>
#include <QtGui/QClipboard>
#include <QtCore/QTimeLine>
#include <QtGui/QGraphicsProxyWidget>
#include <QtGui/qgraphicssceneevent.h>
#include <QtGui/QGraphicsView>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>

#include <KColorScheme>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KMenu>
#include <KDebug>
#include <KDialog>
#include <KGlobalSettings>
#include <KRun>
#include <KLocale>
#include <KService>
#include <KServiceTypeTrader>
#include <KTimeZone>
#include <KToolInvocation>
#include <KMessageBox>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/DataEngine>
#include <Plasma/Dialog>
#include <Plasma/Theme>
#include <Plasma/Label>

#include "calendar.h"

#include "ui_timezonesConfig.h"

class ClockApplet::Private
{
public:
    Private(ClockApplet *clockapplet)
        : q(clockapplet),
          timezone(ClockApplet::localTimezoneUntranslated()),
          clipboardMenu(nullptr),
          adjustSystemTimeAction(nullptr),
          label(nullptr),
          calendarWidget(nullptr),
          forceTzDisplay(false)
    {}

    ClockApplet *q;
    Ui::timezonesConfig timezonesUi;
    QString timezone;
    QString defaultTimezone;
    QPoint clicked;
    QStringList selectedTimezones;
    KMenu *clipboardMenu;
    QAction *adjustSystemTimeAction;
    QString prettyTimezone;
    Plasma::Label *label;
    Plasma::Calendar *calendarWidget;
    QTime lastTimeSeen;
    bool forceTzDisplay;

    QDate addTzToTipText(QString &subText, const Plasma::DataEngine::Data &data, const QDate &prevDate, bool highlight)
    {
        const QDate date = data["Date"].toDate();
        // show the date if it is different from the previous date in its own row
        if (date != prevDate) {
            subText.append("<tr><td>&nbsp;</td></tr>")
                   .append("<tr><th colspan=2 align=left>")
                   .append(KGlobal::locale()->formatDate(data["Date"].toDate()).replace(' ', "&nbsp;"))
                   .append("</th></tr>");
        }

        // start the row
        subText.append("<tr>");

        // put the name of the city in the first cell
        // highlight the name of the place if requested
        subText.append(highlight ? "<th align=\"right\">" : "<td align=\"right\">");
        subText.append(data["Timezone City"].toString().replace('_', "&nbsp;"));
        subText.append(':').append(highlight ? "</th>" : "</td>");

        // now the time
        subText.append("<td>")
               .append(KGlobal::locale()->formatTime(data["Time"].toTime()).replace(' ', "&nbsp;"))
               .append("</td>");

        // and close out the row.
        subText.append("</tr>");
        return date;
    }

    void createCalendar()
    {
        if (q->config().readEntry("ShowCalendarPopup", true)) {
            if (!calendarWidget) {
                calendarWidget = new Plasma::Calendar();
                calendarWidget->setFlag(QGraphicsItem::ItemIsFocusable);
            }
        } else {
            delete calendarWidget;
            calendarWidget = 0;
        }

        q->setGraphicsWidget(calendarWidget);
    }

    void setPrettyTimezone()
    {
        QString timezonetranslated = i18n(timezone.toUtf8().data());
        if (timezone == "UTC")  {
            prettyTimezone = timezonetranslated;
        } else if (!q->isLocalTimezone()) {
            QStringList tzParts = timezonetranslated.split('/', QString::SkipEmptyParts);
            if (tzParts.count() == 1) {
                prettyTimezone = timezonetranslated;
            } else if (tzParts.count() > 0) {
                prettyTimezone = tzParts.last();
            } else {
                prettyTimezone = timezonetranslated;
            }
        } else {
            prettyTimezone = localTimezone();
        }

        prettyTimezone = prettyTimezone.replace('_', ' ');
    }
};

ClockApplet::ClockApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
      d(new Private(this))
{
    setPopupIcon(QIcon());
}

ClockApplet::~ClockApplet()
{
    delete d->clipboardMenu;
    delete d;
}

void ClockApplet::toolTipAboutToShow()
{
    updateTipContent();
}

void ClockApplet::toolTipHidden()
{
    Plasma::ToolTipManager::self()->clearContent(this);
}

//bool sortTzByData(const Plasma::DataEngine::Data &left, const Plasma::DataEngine::Data &right)
bool sortTzByData(const QHash<QString, QVariant> &left, const QHash<QString, QVariant> &right)
{
    const int leftOffset = left.value("Offset").toInt();
    const int rightOffset = right.value("Offset").toInt();
    if (leftOffset == rightOffset) {
        return left.value("Timezone City").toString() < right.value("Timezone City").toString();
    }

    return leftOffset < rightOffset;
}

void ClockApplet::updateTipContent()
{
    Plasma::ToolTipContent tipData;

    QString subText("<table>");
    QList<Plasma::DataEngine::Data> tzs;
    Plasma::DataEngine *engine = dataEngine("time");
    Plasma::DataEngine::Data data = engine->query("Local");
    const QString localTz = data["Timezone"].toString();
    tzs.append(data);
    bool highlightLocal = false;

    tipData.setMainText(i18n("Current Time"));

    foreach (const QString &tz, d->selectedTimezones) {
        if (tz != "UTC") {
            highlightLocal = true;
        }

        if (tz != localTz) {
            data = engine->query(tz);
            tzs.append(data);
        }
    }

    qSort(tzs.begin(), tzs.end(), sortTzByData);
    QDate currentDate;
    foreach (const Plasma::DataEngine::Data &data, tzs) {
        bool shouldHighlight = highlightLocal && (data["Timezone"].toString() == localTz);
        currentDate = d->addTzToTipText(subText, data, currentDate, shouldHighlight);
    }

    subText.append("</table>");

    tipData.setSubText(subText);

    // query for custom content
    Plasma::ToolTipContent customContent = toolTipContent();
    if (!customContent.image().isNull()) {
        tipData.setImage(customContent.image());
    }

    if (!customContent.mainText().isEmpty()) {
        // add their main text
        tipData.setMainText(customContent.mainText());
    }

    if (!customContent.subText().isEmpty()) {
        // add their sub text
        tipData.setSubText(customContent.subText() + "<br>" + tipData.subText());
    }

    tipData.setAutohide(false);
    Plasma::ToolTipManager::self()->setContent(this, tipData);
}

void ClockApplet::updateClockApplet()
{
    Plasma::DataEngine::Data data = dataEngine("time")->query(currentTimezone());
    updateClockApplet(data);
}

void ClockApplet::updateClockApplet(const Plasma::DataEngine::Data &data)
{
    if (d->calendarWidget) {
        d->calendarWidget->setDate(data["Date"].toDate());
    }

    d->lastTimeSeen = data["Time"].toTime();
}

QTime ClockApplet::lastTimeSeen() const
{
    return d->lastTimeSeen;
}

void ClockApplet::resetLastTimeSeen()
{
    d->lastTimeSeen = QTime();
}

Plasma::ToolTipContent ClockApplet::toolTipContent()
{
    return Plasma::ToolTipContent();
}

void ClockApplet::createConfigurationInterface(KConfigDialog *parent)
{
    createClockConfigurationInterface(parent);

    QWidget *widget = new QWidget();
    d->timezonesUi.setupUi(widget);
    d->timezonesUi.searchLine->addTreeWidget(d->timezonesUi.timeZones);

    parent->addPage(widget, i18n("Time Zones"), "preferences-desktop-locale");

    foreach (const QString &tz, d->selectedTimezones) {
        d->timezonesUi.timeZones->setSelected(tz, true);
    }

    updateClockDefaultsTo();
    int defaultSelection = d->timezonesUi.clockDefaultsTo->findData(d->defaultTimezone);
    if (defaultSelection < 0) {
        defaultSelection = 0; //if it's something unexpected default to local
        //kDebug() << d->defaultTimezone << "not in list!?";
    }
    d->timezonesUi.clockDefaultsTo->setCurrentIndex(defaultSelection);

    connect(d->timezonesUi.timeZones, SIGNAL(itemChanged(QTreeWidgetItem*,int)), parent, SLOT(settingsModified()));
    connect(d->timezonesUi.timeZones, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(updateClockDefaultsTo()));
    connect(d->timezonesUi.clockDefaultsTo,SIGNAL(activated(QString)), parent, SLOT(settingsModified()));
}

void ClockApplet::createClockConfigurationInterface(KConfigDialog *parent)
{
    Q_UNUSED(parent)
}

void ClockApplet::clockConfigChanged()
{

}

void ClockApplet::configChanged()
{
    d->createCalendar();

    if (isUserConfiguring()) {
        configAccepted();
    }

    KConfigGroup cg = config();
    d->selectedTimezones = cg.readEntry("timeZones", QStringList() << "UTC");
    d->timezone = cg.readEntry("timezone", d->timezone);
    d->defaultTimezone = cg.readEntry("defaultTimezone", d->timezone);
    d->forceTzDisplay = d->timezone != d->defaultTimezone;
    d->setPrettyTimezone();

    clockConfigChanged();

    if (isUserConfiguring()) {
        constraintsEvent(Plasma::SizeConstraint);
        update();
    } else if (d->calendarWidget) {
        // setCurrentTimezone() is called in configAccepted(),
        // so we only need to do this in the case where the user hasn't been
        // configuring things
        Plasma::DataEngine::Data data = dataEngine("time")->query(d->timezone);
        d->calendarWidget->setDate(data["Date"].toDate());
    }
}

void ClockApplet::clockConfigAccepted()
{

}

void ClockApplet::configAccepted()
{
    KConfigGroup cg = config();

    cg.writeEntry("timeZones", d->timezonesUi.timeZones->selection());

    if (d->timezonesUi.clockDefaultsTo->currentIndex() == 0) {
        //The first position in timezonesUi.clockDefaultsTo is "Local"
        d->defaultTimezone = localTimezoneUntranslated();
    } else {
        d->defaultTimezone = d->timezonesUi.clockDefaultsTo->itemData(d->timezonesUi.clockDefaultsTo->currentIndex()).toString();
    }

    cg.writeEntry("defaultTimezone", d->defaultTimezone);
    QString cur = currentTimezone();
    setCurrentTimezone(d->defaultTimezone);
    changeEngineTimezone(cur, d->defaultTimezone);

    clockConfigAccepted();

    emit configNeedsSaving();
}

void ClockApplet::updateClockDefaultsTo()
{
    QString oldSelection = d->timezonesUi.clockDefaultsTo->currentText();
    d->timezonesUi.clockDefaultsTo->clear();
    d->timezonesUi.clockDefaultsTo->addItem(localTimezone(), localTimezone());
    foreach(const QString &tz, d->timezonesUi.timeZones->selection()) {
        d->timezonesUi.clockDefaultsTo->addItem(KTimeZoneWidget::displayName(KTimeZone(tz)), tz);
    }
    int newPosition = d->timezonesUi.clockDefaultsTo->findText(oldSelection);
    if (newPosition >= 0) {
        d->timezonesUi.clockDefaultsTo->setCurrentIndex(newPosition);
    }
    if (d->timezonesUi.clockDefaultsTo->count() > 1) {
        d->timezonesUi.clockDefaultsTo->setEnabled(true);
    } else {
        // Only "Local" in timezonesUi.clockDefaultsTo
        d->timezonesUi.clockDefaultsTo->setEnabled(false);
    }
}

void ClockApplet::changeEngineTimezone(const QString &oldTimezone, const QString &newTimezone)
{
    // reimplemented by subclasses to get the new data
    Q_UNUSED(oldTimezone);
    Q_UNUSED(newTimezone);
}

bool ClockApplet::shouldDisplayTimezone() const
{
    return d->forceTzDisplay;
}

QList<QAction *> ClockApplet::contextualActions()
{
    if (!d->clipboardMenu) {
        d->clipboardMenu = new KMenu(i18n("C&opy to Clipboard"));
        d->clipboardMenu->setIcon(KIcon("edit-copy"));
        connect(d->clipboardMenu, SIGNAL(aboutToShow()), this, SLOT(updateClipboardMenu()));
        connect(d->clipboardMenu, SIGNAL(triggered(QAction*)), this, SLOT(copyToClipboard(QAction*)));

        KService::List offers = KServiceTypeTrader::self()->query("KCModule", "Library == 'kcm_clock'");
        if (!offers.isEmpty()) {
            d->adjustSystemTimeAction = new QAction(this);
            d->adjustSystemTimeAction->setText(i18n("Adjust Date and Time..."));
            d->adjustSystemTimeAction->setIcon(KIcon(icon()));
            connect(d->adjustSystemTimeAction, SIGNAL(triggered()), this, SLOT(launchTimeControlPanel()));
        }
    }

    QList<QAction*> contextualActions;
    contextualActions << d->clipboardMenu->menuAction();

    if (d->adjustSystemTimeAction) {
        contextualActions << d->adjustSystemTimeAction;
    }
    return contextualActions;
}

void ClockApplet::launchTimeControlPanel()
{
    KService::List offers = KServiceTypeTrader::self()->query("KCModule", "Library == 'kcm_clock'");
    if (offers.isEmpty()) {
        kDebug() << "fail";
        return;
    }

    KUrl::List urls;
    KService::Ptr service = offers.first();
    KRun::run(*service, urls, 0);
}

void ClockApplet::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (d->selectedTimezones.isEmpty()) {
        return;
    }

    QString newTimezone;

    if (isLocalTimezone()) {
        if (event->delta() > 0) {
            newTimezone = d->selectedTimezones.last();
        } else {
            newTimezone = d->selectedTimezones.first();
        }
    } else {
        int current = d->selectedTimezones.indexOf(currentTimezone());

        if (event->delta() > 0) {
            int previous = current - 1;
            if (previous < 0) {
                newTimezone = localTimezoneUntranslated();
            } else {
                newTimezone = d->selectedTimezones.at(previous);
            }
        } else {
            int next = current + 1;
            if (next > d->selectedTimezones.count() - 1) {
                newTimezone = localTimezoneUntranslated();
            } else {
                newTimezone = d->selectedTimezones.at(next);
            }
        }
    }

    QString cur = currentTimezone();
    setCurrentTimezone(newTimezone);
    changeEngineTimezone(cur, newTimezone);
    update();
}

void ClockApplet::init()
{
    configChanged();
    Plasma::ToolTipManager::self()->registerWidget(this);
}

void ClockApplet::focusInEvent(QFocusEvent* event)
{
    Q_UNUSED(event);
    if (d->calendarWidget) {
        d->calendarWidget->setFocus();
    }
}

void ClockApplet::popupEvent(bool show)
{
    if (show && d->calendarWidget) {
        Plasma::DataEngine::Data data = dataEngine("time")->query(currentTimezone());
        const QDate date = data["Date"].toDate();
        d->calendarWidget->setCurrentDate(date.addDays(-1));
        d->calendarWidget->setDate(date);
        d->calendarWidget->setCurrentDate(date);
    }
}

void ClockApplet::constraintsEvent(Plasma::Constraints constraints)
{
    Q_UNUSED(constraints)
}

void ClockApplet::setCurrentTimezone(const QString &tz)
{
    if (d->timezone == tz) {
        return;
    }

    if (tz == localTimezone()) {
        // catch people accidentally passing in the translation of "Local"
        d->timezone = localTimezoneUntranslated();
    } else {
        d->timezone = tz;
    }

    d->forceTzDisplay = d->timezone != d->defaultTimezone;
    d->setPrettyTimezone();

    if (d->calendarWidget) {
        Plasma::DataEngine::Data data = dataEngine("time")->query(d->timezone);
        d->calendarWidget->setDate(data["Date"].toDate());
    }

    KConfigGroup cg = config();
    cg.writeEntry("timezone", d->timezone);
    emit configNeedsSaving();
}

QString ClockApplet::currentTimezone() const
{
    return d->timezone;
}

QString ClockApplet::prettyTimezone() const
{
    return d->prettyTimezone;
}

bool ClockApplet::isLocalTimezone() const
{
    return d->timezone == localTimezoneUntranslated();
}

QString ClockApplet::localTimezone()
{
    return i18nc("Local time zone", "Local");
}

QString ClockApplet::localTimezoneUntranslated()
{
    return "Local";
}

void ClockApplet::updateClipboardMenu()
{
    d->clipboardMenu->clear();
    QList<QAction*> actions;
    Plasma::DataEngine::Data data = dataEngine("time")->query(currentTimezone());
    QDateTime dateTime = QDateTime(data["Date"].toDate(), data["Time"].toTime());

    d->clipboardMenu->addAction(KGlobal::locale()->formatDate(dateTime.date(), QLocale::LongFormat));
    d->clipboardMenu->addAction(KGlobal::locale()->formatDate(dateTime.date(), QLocale::ShortFormat));

    QAction *sep0 = new QAction(this);
    sep0->setSeparator(true);
    d->clipboardMenu->addAction(sep0);

    d->clipboardMenu->addAction(KGlobal::locale()->formatTime(dateTime.time(), QLocale::LongFormat));
    d->clipboardMenu->addAction(KGlobal::locale()->formatTime(dateTime.time(), QLocale::ShortFormat));

    QAction *sep1 = new QAction(this);
    sep1->setSeparator(true);
    d->clipboardMenu->addAction(sep1);

    d->clipboardMenu->addAction(KGlobal::locale()->formatDateTime(dateTime, QLocale::LongFormat));
    d->clipboardMenu->addAction(KGlobal::locale()->formatDateTime(dateTime, QLocale::ShortFormat));
    d->clipboardMenu->addAction(KGlobal::locale()->formatDateTime(dateTime, QLocale::NarrowFormat));

    QAction *sep2 = new QAction(this);
    sep2->setSeparator(true);
    d->clipboardMenu->addAction(sep2);
}

void ClockApplet::copyToClipboard(QAction* action)
{
    QString text = action->text();
    text.remove(QChar('&'));

    QApplication::clipboard()->setText(text);
}

#include "moc_clockapplet.cpp"
