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

#include "kcm_clock.h"

#include <QThread>
#include <QHeaderView>
#include <kaboutdata.h>
#include <kpluginfactory.h>
#include <kauthorization.h>
#include <ksystemtimezone.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kdebug.h>

static const int s_updatetime = 1000;
static const int s_waittime = 100;
static const QString s_dateformat = QString::fromLatin1("yyyy-MM-dd HH:mm:ss");

static void kWatiForTimeZone(const QString &zone)
{
    while (KSystemTimeZones::local().name() != zone) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, s_waittime);
        QThread::msleep(s_waittime);
    }
}

static bool kCanChangeClock()
{
    return (!KStandardDirs::findRootExe("hwclock").isEmpty() || !KStandardDirs::findRootExe("timedatectl").isEmpty());
}

K_PLUGIN_FACTORY(KCMClockFactory, registerPlugin<KCMClock>();)
K_EXPORT_PLUGIN(KCMClockFactory("kcmclock", "kcmclock"))

KCMClock::KCMClock(QWidget *parent, const QVariantList &args)
    : KCModule(KCMClockFactory::componentData(), parent),
    m_layout(nullptr),
    m_datetimebox(nullptr),
    m_datetimelayout(nullptr),
    m_timeedit(nullptr),
    m_dateedit(nullptr),
    m_timezonebox(nullptr),
    m_timezonelayout(nullptr),
    m_timezonesearch(nullptr),
    m_timezonewidget(nullptr),
    m_timer(nullptr),
    m_timechanged(false),
    m_datechanged(false),
    m_zonechanged(false)
{
    Q_UNUSED(args);

    setButtons(KCModule::Apply);
    setQuickHelp(i18n("<h1>Date and Time</h1> This module allows you to change system date and time options."));

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmclock"), 0,
        ki18n("KDE Date and Time Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2024, Ivailo Monev <email>xakepa10@gmail.com</email>")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    m_layout = new QVBoxLayout(this);
    setLayout(m_layout);

    m_datetimebox = new QGroupBox(this);
    m_datetimebox->setTitle(i18n("Date and time"));
    m_datetimelayout = new QHBoxLayout(m_datetimebox);
    m_datetimebox->setLayout(m_datetimelayout);
    m_timeedit = new QTimeEdit(m_datetimebox);
    // to show seconds the time format has to be in C locale
    m_timeedit->setLocale(QLocale::c());
    connect(m_timeedit, SIGNAL(timeChanged(QTime)), this, SLOT(slotTimeChanged()));
    m_datetimelayout->addWidget(m_timeedit);
    m_dateedit = new QDateEdit(m_datetimebox);
    m_dateedit->setLocale(KGlobal::locale()->toLocale());
    connect(m_dateedit, SIGNAL(dateChanged(QDate)), this, SLOT(slotDateChanged()));
    m_datetimelayout->addWidget(m_dateedit);
    m_layout->addWidget(m_datetimebox);

    m_timezonebox = new QGroupBox(this);
    m_timezonebox->setTitle(i18n("Time zone"));
    m_timezonelayout = new QVBoxLayout(m_timezonebox);
    m_timezonebox->setLayout(m_timezonelayout);
    m_timezonesearch = new KTreeWidgetSearchLine(m_timezonebox);
    m_timezonesearch->setClickMessage(i18n("Search"));
    m_timezonelayout->addWidget(m_timezonesearch);
    m_timezonewidget = new QTreeWidget(m_timezonebox);
    m_timezonewidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_timezonewidget->setColumnCount(2);
    QStringList treeheaders = QStringList()
        << i18n("Time zone")
        << i18n("Comment");
    m_timezonewidget->setHeaderLabels(treeheaders);
    m_timezonewidget->setRootIsDecorated(false);
    m_timezonewidget->header()->setMovable(false);
    m_timezonewidget->header()->setStretchLastSection(false);
    m_timezonewidget->header()->setResizeMode(0, QHeaderView::Stretch);
    m_timezonewidget->header()->setResizeMode(1, QHeaderView::Stretch);
    QMap<QString, QString> sortedzones;
    foreach (const KTimeZone &ktimezone, KSystemTimeZones::zones()) {
        const QString zonename = ktimezone.name();
        sortedzones.insert(KSystemTimeZones::zoneName(zonename), zonename);
    }
    QMapIterator<QString, QString> sortedzonesiter(sortedzones);
    while (sortedzonesiter.hasNext()) {
        sortedzonesiter.next();
        const QString zonename = sortedzonesiter.value();
        const KTimeZone ktimezone = KSystemTimeZones::zone(zonename);
        const QString zoneflag = KStandardDirs::locate(
            "locale",
            QString::fromLatin1("l10n/%1/flag.png").arg(ktimezone.countryCode().toLower())
        );;
        QTreeWidgetItem* zoneitem = new QTreeWidgetItem();
        zoneitem->setData(0, Qt::UserRole, zonename);
        zoneitem->setIcon(0, KIcon(zoneflag));
        zoneitem->setText(0, sortedzonesiter.key());
        zoneitem->setData(1, Qt::UserRole, ktimezone.comment());
        zoneitem->setText(1, KSystemTimeZones::zoneComment(zonename));
        m_timezonewidget->addTopLevelItem(zoneitem);
    }
    connect(m_timezonewidget, SIGNAL(itemSelectionChanged()), this, SLOT(slotZoneChanged()));
    m_timezonesearch->setTreeWidget(m_timezonewidget);
    m_timezonelayout->addWidget(m_timezonewidget);
    m_layout->addWidget(m_timezonebox);

    m_timer = new QTimer(this);
    m_timer->setInterval(s_updatetime);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotUpdate()));

    if (!KAuthorization::isAuthorized("org.kde.kcontrol.kcmclock")) {
        setUseRootOnlyMessage(true);
        setRootOnlyMessage(i18n("You are not allowed to save the configuration"));
        setDisabled(true);
    } else if (!kCanChangeClock()) {
        setUseRootOnlyMessage(true);
        setRootOnlyMessage(i18n("Neither 'hwclock' nor 'timedatectl' utility not found, setting the date is not possible"));
        m_timeedit->setEnabled(false);
        m_dateedit->setEnabled(false);
    }
}

void KCMClock::save()
{
    setEnabled(false);
    m_timer->stop();
    m_timechanged = false;
    m_datechanged = false;
    m_zonechanged = false;
    QVariantMap savearguments;
    if (kCanChangeClock()) {
        const QDateTime datetime = QDateTime(m_dateedit->date(), m_timeedit->time());
        savearguments.insert("datetime", datetime.toString(s_dateformat));
        // qDebug() << Q_FUNC_INFO << datetime;
    }
    QString zone;
    const QList<QTreeWidgetItem*> selectedzones = m_timezonewidget->selectedItems();
    if (selectedzones.isEmpty()) {
        zone = QLatin1String("UTC"); // what else?
    } else {
        zone = selectedzones.at(0)->data(0, Qt::UserRole).toString();
    }
    // qDebug() << Q_FUNC_INFO << zone;
    savearguments.insert("zone", zone);
    const int clockreply = KAuthorization::execute(
        "org.kde.kcontrol.kcmclock", "save", savearguments
    );
    if (clockreply > 0) {
        if (clockreply == 1) {
            KMessageBox::error(this, i18n("Unable to set date and time"));
            kWatiForTimeZone(zone);
        } else if (clockreply == 2) {
            KMessageBox::error(this, i18n("Unable to set timezone"));
        }
    } else if (clockreply != KAuthorization::NoError) {
        KMessageBox::error(this, i18n("Unable to authenticate/execute the action: %1", KAuthorization::errorString(clockreply)));
    } else {
        kWatiForTimeZone(zone);
    }
    slotUpdate();
    emit changed(false);
    m_timer->start();
    setEnabled(true);
}

void KCMClock::load()
{
    setEnabled(false);
    m_timechanged = false;
    m_datechanged = false;
    m_zonechanged = false;
    slotUpdate();
    emit changed(false);
    m_timer->start();
    setEnabled(true);
}

void KCMClock::slotUpdate()
{
    const QDateTime now = QDateTime::currentDateTime();
    if (!m_timechanged) {
        m_timeedit->blockSignals(true);
        m_timeedit->setTime(now.time());
        m_timeedit->blockSignals(false);
    }
    if (!m_datechanged) {
        m_dateedit->blockSignals(true);
        m_dateedit->setDate(now.date());
        m_dateedit->blockSignals(false);
    }
    if (!m_zonechanged) {
        const QString localzonename = KSystemTimeZones::local().name();
        for (int i = 0; i < m_timezonewidget->topLevelItemCount(); i++) {
            QTreeWidgetItem* zoneitem = m_timezonewidget->topLevelItem(i);
            if (zoneitem->data(0, Qt::UserRole).toString() == localzonename) {
                m_timezonewidget->setCurrentItem(zoneitem, 0);
            }
        }
    }
}

void KCMClock::slotTimeChanged()
{
    m_timechanged = true;
    emit changed(true);
}

void KCMClock::slotDateChanged()
{
    m_datechanged = true;
    emit changed(true);
}

void KCMClock::slotZoneChanged()
{
    m_zonechanged = true;
    emit changed(true);
}

#include "moc_kcm_clock.cpp"
