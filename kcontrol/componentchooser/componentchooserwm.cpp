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

#include <kdesktopfile.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <qfileinfo.h>
#include <qprocess.h>
#include <netwm.h>

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
    oldwm = currentWm();
    cfg.sync();
    KMessageBox::information(
        window(),
        i18n(
            "The change will take effect on the next KDE session."
        ),
        i18n("Window Manager Changed"),
        "restartafterwmchange"
    );
    return true;
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
