/*
    This file is part of the KDE project
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "calendar.h"

#include <QGraphicsLinearLayout>
#include <QPainter>
#include <KCalendarWidget>
#include <KSystemTimeZones>
#include <KIcon>
#include <KCModuleInfo>
#include <Plasma/Theme>
#include <Plasma/CalendarWidget>
#include <Plasma/ToolTipManager>
#include <KDebug>

static const int s_svgiconsize = 256;
static const QString s_defaultpopupicon = QString::fromLatin1("view-pim-calendar");

static int kGetDay()
{
    return QDate::currentDate().day();
}

class CalendarWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    CalendarWidget(QGraphicsWidget *parent);

    void showToday();

private:
    QGraphicsLinearLayout* m_layout;
    Plasma::CalendarWidget* m_plasmacalendar;
    KCalendarWidget* m_nativewidget;
};

CalendarWidget::CalendarWidget(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    m_layout(nullptr),
    m_plasmacalendar(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_plasmacalendar = new Plasma::CalendarWidget(this);
    m_plasmacalendar->setMinimumSize(QSize(300, 250));
    m_plasmacalendar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_nativewidget = m_plasmacalendar->nativeWidget();
    // changing the date on the calendar does not make sense
    m_nativewidget->setSelectionMode(QCalendarWidget::NoSelection);
    m_nativewidget->setDateEditEnabled(false);
    m_layout->addItem(m_plasmacalendar);
    setLayout(m_layout);
}

void CalendarWidget::showToday()
{
    m_nativewidget->showToday();
}


CalendarApplet::CalendarApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_calendarwidget(nullptr),
    m_svg(nullptr),
    m_timer(nullptr),
    m_day(-1),
    m_kcmclockproxy(nullptr)
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setPopupIcon(s_defaultpopupicon);

    m_calendarwidget = new CalendarWidget(this);

    m_svg = new Plasma::Svg(this);
    m_svg->setImagePath("calendar/mini-calendar");
    m_svg->setContainsMultipleImages(true);

    m_timer = new QTimer(this);
    // 3sec to account for localtime changes for example
    m_timer->setInterval(3000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
}

void CalendarApplet::init()
{
    Plasma::ToolTipManager::self()->registerWidget(this);
    slotTimeout();
    m_timer->start();
}

QGraphicsWidget *CalendarApplet::graphicsWidget()
{
    return m_calendarwidget;
}

void CalendarApplet::createConfigurationInterface(KConfigDialog *parent)
{
    m_kcmclockproxy = new KCModuleProxy("clock");
    parent->addPage(
        m_kcmclockproxy, m_kcmclockproxy->moduleInfo().moduleName(),
        m_kcmclockproxy->moduleInfo().icon()
    );

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_kcmclockproxy, SIGNAL(changed(bool)), parent, SLOT(settingsModified()));
}

void CalendarApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint || constraints & Plasma::SizeConstraint) {
        paintIcon();
    }
}

void CalendarApplet::popupEvent(bool show)
{
    if (show) {
        m_calendarwidget->showToday();
    }
}

void CalendarApplet::slotTimeout()
{
    const int today = kGetDay();
    if (today != m_day) {
        m_day = today;
        kDebug() << "updating calendar icon" << m_day << today;
        paintIcon();
    }
    // affected by locale changes so always updated
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(i18n("Current Date"));
    const QString calendarstring = KGlobal::locale()->formatDate(QDate::currentDate());
    QString calendartooltip;
    if (KSystemTimeZones::local() != KTimeZone::utc()) {
        calendartooltip.append(i18n("UTC: %1<br/>", KGlobal::locale()->formatDate(QDateTime::currentDateTimeUtc().date())));
        calendartooltip.append(i18n("Local: %1", calendarstring));
    } else {
        calendartooltip.append(QString::fromLatin1("<center>%1</center>").arg(calendarstring));
    }
    plasmatooltip.setSubText(calendartooltip);
    plasmatooltip.setImage(KIcon("office-calendar"));
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltip);
}

void CalendarApplet::slotConfigAccepted()
{
    m_kcmclockproxy->save();
}

void CalendarApplet::paintIcon()
{
    if (m_svg->isValid()) {
        QFont font = KGlobalSettings::smallestReadableFont();
        font.setBold(true);
        font.setPointSize(qMax(font.pointSize(), qRound(s_svgiconsize / 3)));

        QPixmap iconpixmap(s_svgiconsize, s_svgiconsize);
        iconpixmap.fill(Qt::transparent);
        QPainter iconpainter(&iconpixmap);
        m_svg->paint(&iconpainter, iconpixmap.rect(), "mini-calendar");
        iconpainter.setFont(font);
        iconpainter.drawText(iconpixmap.rect(), Qt::AlignCenter, QString::number(kGetDay()));

        setPopupIcon(QIcon(iconpixmap));
    } else {
        setPopupIcon(KIcon(s_defaultpopupicon));
    }
}

#include "moc_calendar.cpp"
#include "calendar.moc"
