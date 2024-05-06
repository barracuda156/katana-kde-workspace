/***************************************************************************
                          componentchooserwm.cpp  -  description
                             -------------------
    copyright            : (C) 2002 by Joseph Wenninger
    email                : jowenn@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License verstion 2 as    *
 *   published by the Free Software Foundation                             *
 *                                                                         *
 ***************************************************************************/

#include "componentchooserwm.h"
#include "moc_componentchooserwm.cpp"

#include <kdebug.h>
#include <kdesktopfile.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <ktimerdialog.h>
#include <kselectionowner.h>
#include <kprocess.h>
#include <kshell.h>
#include <qthread.h>
#include <qfileinfo.h>
#include <qdbusinterface.h>
#include <qdbusconnectioninterface.h>
#include <netwm.h>
#include <qx11info_x11.h>

static const int s_eventstime = 500;
static const int s_sleeptime = 500;

// TODO: kill and start WM on each screen?
static int getWMScreen()
{
    return QX11Info::appScreen();
}

static QByteArray getWMAtom()
{
    char snprintfbuff[30];
    ::memset(snprintfbuff, '\0', sizeof(snprintfbuff));
    ::sprintf(snprintfbuff, "WM_S%d", getWMScreen());
    return QByteArray(snprintfbuff);
}

void killWM()
{
    const QByteArray wmatom = getWMAtom();
    KSelectionOwner kselectionowner(wmatom.constData(), getWMScreen());
    kselectionowner.claim(true);
    kselectionowner.release();
}

bool startWM(const QString &wmexec)
{
    // HACK: openbox crashes shortly after it is started if started from this process so start it
    // via klauncher
    if (wmexec.contains(QLatin1String("openbox"))) {
        QDBusInterface klauncher(
            "org.kde.klauncher", "/KLauncher","org.kde.KLauncher",
            QDBusConnection::sessionBus()
        );
        QStringList wmcommand = KShell::splitArgs(wmexec);
        if (wmcommand.isEmpty()) {
            return false;
        }
        const QString wmprog = wmcommand.takeFirst();
        QDBusReply<void> reply = klauncher.call("exec_blind", wmprog, wmcommand);
        return reply.isValid();
    }
    KProcess kproc;
    kproc.setShellCommand(wmexec);
    return (kproc.startDetached() > 0);
}

CfgWm::CfgWm(QWidget *parent)
    : QWidget(parent)
    , Ui::WmConfig_UI()
    , CfgPlugin()
{
    setupUi(this);
    connect(wmCombo,SIGNAL(activated(int)), this, SLOT(configChanged()));
    connect(kwinRB,SIGNAL(toggled(bool)),this,SLOT(configChanged()));
    connect(differentRB,SIGNAL(toggled(bool)),this,SLOT(configChanged()));
    connect(differentRB,SIGNAL(toggled(bool)),this,SLOT(checkConfigureWm()));
    connect(wmCombo,SIGNAL(activated(int)),this,SLOT(checkConfigureWm()));
    connect(configureButton,SIGNAL(clicked()),this,SLOT(configureWm()));

    KGlobal::dirs()->addResourceType("windowmanagers", "data", "plasma/windowmanagers");
}

CfgWm::~CfgWm()
{
}

void CfgWm::configChanged()
{
    emit changed(true);
}

void CfgWm::defaults()
{
    wmCombo->setCurrentIndex( 0 );
}


void CfgWm::load(KConfig *)
{
    KConfig cfg("plasmarc", KConfig::NoGlobals);
    KConfigGroup c( &cfg, "General");
    loadWMs(c.readEntry("windowManager", "kwin"));
    emit changed(false);
}

void CfgWm::save(KConfig *)
{
    saveAndConfirm();
}

bool CfgWm::saveAndConfirm()
{
    KConfig cfg("plasmarc", KConfig::NoGlobals);
    KConfigGroup c( &cfg, "General");
    c.writeEntry("windowManager", currentWm());
    emit changed(false);
    if (oldwm == currentWm()) {
        return true;
    }
    if (tryWmLaunch()) {
        oldwm = currentWm();
        cfg.sync();
        QDBusInterface plasma("org.kde.plasma-desktop", "/App", "local.PlasmaApp");
        plasma.call(QDBus::NoBlock, "wmChanged");
        KMessageBox::information(
            window(),
            i18n("A new window manager is running.\n"
                 "It is still recommended to restart this KDE session to make sure "
                 "all running applications adjust for this change."),
            i18n("Window Manager Replaced"),
            "restartafterwmchange"
        );
        return true;
    } else {
        // revert config
        emit changed(true);
        c.writeEntry("windowManager", oldwm);
        if (oldwm == "kwin") {
            kwinRB->setChecked(true);
            wmCombo->setEnabled(false);
        } else {
            differentRB->setChecked(true);
            wmCombo->setEnabled(true);
            for (QHash< QString, WmData >::ConstIterator it = wms.constBegin(); it != wms.constEnd(); ++it) {
                if ((*it).internalName == oldwm) {
                    // make it selected
                    wmCombo->setCurrentIndex(wmCombo->findText(it.key()));
                }
            }
        }
        return false;
    }
}

bool CfgWm::tryWmLaunch()
{
    if (currentWm() == "kwin"
        && qstrcmp(NETRootInfo(QX11Info::display(), NET::SupportingWMCheck).wmName(), "KWin") == 0) {
        // it is already running, don't necessarily restart e.g. after a failure with other WM
        return true;
    }
    KMessageBox::information(
        window(),
        i18n("Your running window manager will be now replaced with the configured one."),
        i18n("Window Manager Change"),
        "windowmanagerchange"
    );

    bool ret = false;
    setEnabled(false);
    killWM();
    if (startWM(currentWmData().exec)) {
        // it's forked into background
        ret = true;

        // NOTE: wait for the WM to be operational otherwise the timer dialog may not
        // show up and the configuration window becomes non-interactive until the timeout is
        // reached
        const QByteArray wmatom = getWMAtom();
        KSelectionOwner kselectionowner(wmatom.constData(), getWMScreen());
        ushort counter = 0;
        while (counter < 10 && kselectionowner.currentOwnerWindow() == XNone) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, s_eventstime);
            QThread::msleep(s_sleeptime);
            counter++;
        }

        KTimerDialog* wmDialog = new KTimerDialog(
            20000, KTimerDialog::CountDown, window(),
            i18n("Config Window Manager Change"),
            KTimerDialog::Ok | KTimerDialog::Cancel, KTimerDialog::Cancel
        );
        wmDialog->setButtonGuiItem(KDialog::Ok, KGuiItem(i18n("&Accept Change"), "dialog-ok"));
        wmDialog->setButtonGuiItem(KDialog::Cancel, KGuiItem(i18n("&Revert to Previous"), "dialog-cancel"));
        QLabel *label = new QLabel(
            i18n( "The configured window manager is being launched.\n"
                "Please check it has started properly and confirm the change.\n"
                "The launch will be automatically reverted in 20 seconds." ), wmDialog );
        label->setWordWrap(true);
        wmDialog->setMainWidget(label);

        if (wmDialog->exec() != QDialog::Accepted ) {
            // cancelled for some reason
            ret = false;

            KMessageBox::sorry(
                window(),
                i18n("The running window manager has been reverted to the previous window manager.")
            );
        }

        delete wmDialog;
        wmDialog = NULL;
    } else {
        ret = false;

        KMessageBox::sorry(
            window(),
            i18n("The new window manager has failed to start.\nThe running window manager has been reverted to the previous window manager.")
        );
    }

    if (!ret) {
        // case-insensitive search
        foreach (const QString &wmkey, wms.keys()) {
            if (wmkey.toLower() == oldwm) {
                WmData oldwmdata = wms.value(wmkey);
                killWM();
                startWM(oldwmdata.exec);
                break;
            }
        }
    }

    setEnabled(true);
    return ret;
}

void CfgWm::loadWMs(const QString &current)
{
    WmData kwin;
    kwin.internalName = "kwin";
    kwin.exec = "kwin";
    kwin.configureCommand = "";
    kwin.parentArgument = "";
    wms["KWin"] = kwin;
    oldwm = "kwin";
    kwinRB->setChecked(true);
    wmCombo->setEnabled(false);

    QStringList list = KGlobal::dirs()->findAllResources("windowmanagers", QString(), KStandardDirs::NoDuplicates);
    foreach (const QString& wmfile, list) {
        KDesktopFile file(wmfile);
        if (file.noDisplay())
            continue;
        if (!file.tryExec())
            continue;
        QString name = file.readName();
        if (name.isEmpty())
            continue;
        QString wm = QFileInfo(file.name()).baseName();
        if (wms.contains(name))
            continue;
        WmData data;
        data.internalName = wm;
        data.exec = file.desktopGroup().readEntry("Exec");
        if (data.exec.isEmpty())
            continue;
        data.configureCommand = file.desktopGroup().readEntry("X-KDE-WindowManagerConfigure");
        data.parentArgument = file.desktopGroup().readEntry("X-KDE-WindowManagerConfigureParentArgument");
        wms[name] = data;
        wmCombo->addItem(name);
        if (wms[name].internalName == current) {
             // make it selected
            wmCombo->setCurrentIndex(wmCombo->count() - 1);
            oldwm = wm;
            differentRB->setChecked(true);
            wmCombo->setEnabled(true);
        }
    }
    differentRB->setEnabled(wmCombo->count() > 0);
    checkConfigureWm();
}

CfgWm::WmData CfgWm::currentWmData() const
{
    return kwinRB->isChecked() ? wms["KWin"] : wms[wmCombo->currentText()];
}

QString CfgWm::currentWm() const
{
    return currentWmData().internalName;
}

void CfgWm::checkConfigureWm()
{
    configureButton->setEnabled(!currentWmData().configureCommand.isEmpty());
}

void CfgWm::configureWm()
{
    if (oldwm != currentWm() && !saveAndConfirm()) {
        // needs switching first
        return;
    }
    QStringList args;
    if (!currentWmData().parentArgument.isEmpty()) {
        args << currentWmData().parentArgument << QString::number(window()->winId());
    }
    if (!QProcess::startDetached(currentWmData().configureCommand, args)) {
        KMessageBox::sorry(window(), i18n("Running the configuration tool failed"));
    }
}
