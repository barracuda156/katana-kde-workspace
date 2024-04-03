/*
 *  main.cpp
 *
 *  Copyright (C) 1998 Luca Montecchiani <m.luca@usa.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include "main.h"

#include <unistd.h>

#include <QVBoxLayout>
#include <QtDBus/QtDBus>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kdialog.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kauthorization.h>
#include <ksystemtimezone.h>

#include "moc_main.cpp"

#include "dtime.h"
#include "helper.h"

static const int s_checktime = 1000; // 1sec

K_PLUGIN_FACTORY(KlockModuleFactory, registerPlugin<KclockModule>();)
K_EXPORT_PLUGIN(KlockModuleFactory("kcmkclock"))

KclockModule::KclockModule(QWidget *parent, const QVariantList &)
    : KCModule(KlockModuleFactory::componentData(), parent)
{
    KAboutData *about =
    new KAboutData(
        I18N_NOOP("kcmclock"), 0, ki18n("KDE Clock Control Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("(c) 1996 - 2001 Luca Montecchiani\n(c) 2014 Ivailo Monev")
    );

    about->addAuthor(ki18n("Luca Montecchiani"), ki18n("Original author"), "m.luca@usa.net");
    about->addAuthor(ki18n("Paul Campbell"), ki18n("Past Maintainer"), "paul@taniwha.com");
    about->addAuthor(ki18n("Benjamin Meyer"), ki18n("Added NTP support"), "ben+kcmclock@meyerhome.net");
    about->addAuthor(ki18n("Ivailo Monev"), ki18n("Current Maintainer"), "xakepa10@gmail.com");
    setAboutData(about);
    setQuickHelp(
        i18n(
            "<h1>Date & Time</h1> This control module can be used to set the system date and"
            " time. As these settings do not only affect you as a user, but rather the whole system, you"
            " can only change these settings when you start the System Settings as root. If you do not have"
            " the root password, but feel the system time should be corrected, please contact your system"
            " administrator."
        )
    );

    KGlobal::locale()->insertCatalog("timezones4"); // For time zone translations

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(KDialog::spacingHint());

    dtime = new Dtime(this);
    layout->addWidget(dtime);
    connect(dtime, SIGNAL(timeChanged(bool)), this, SIGNAL(changed(bool)));

    setButtons(Help|Apply);

    if (!KAuthorization::isAuthorized("org.kde.kcontrol.kcmclock")) {
        setUseRootOnlyMessage(true);
        setRootOnlyMessage(i18n("You are not allowed to save the configuration"));
        setDisabled(true);
    }

    m_tz = KSystemTimeZones::local().name();
    QTimer::singleShot(s_checktime, this, SLOT(checkTZ()));
}

void KclockModule::save()
{
    setDisabled(true);

    QVariantMap helperargs;
    dtime->save(helperargs);

    const int reply = KAuthorization::execute(
        "org.kde.kcontrol.kcmclock", "save", helperargs
    );
    if (reply != KAuthorization::NoError) {
        if (reply < KAuthorization::NoError) {
            KMessageBox::error(this, i18n("Unable to authenticate/execute the action: %1", KAuthorization::errorString(reply)));
        } else {
            dtime->processHelperErrors(reply);
        }
        setDisabled(false);
    } else {
        // explicit load() in case its not the timezone that is changed
        load();
        // not watched for
#if 0
        QDBusMessage msg = QDBusMessage::createSignal("/org/kde/kcmshell_clock", "org.kde.kcmshell_clock", "clockUpdated");
        QDBusConnection::sessionBus().send(msg);
#endif
    }
}

void KclockModule::load()
{
    dtime->load();
    setDisabled(useRootOnlyMessage());
    emit changed(false);
}

void KclockModule::checkTZ()
{
    const QString localtz = KSystemTimeZones::local().name();
    if (localtz != m_tz) {
        m_tz = localtz;

        load();
    }
    QTimer::singleShot(s_checktime, this, SLOT(checkTZ()));
}
