/*
 *  Copyright (C) 2009 Marco Martin <notmart@gmail.com>
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
#include "workspaceoptions.h"

#include "ui_mainpage.h"

#include <QDBusInterface>

#include <KDebug>
#include <KAboutData>
#include <KMessageBox>
#include <KPluginFactory>
#include <KRun>
#include <KStandardDirs>
#include <KUrl>
#include <KConfigGroup>

K_PLUGIN_FACTORY(WorkspaceOptionsModuleFactory, registerPlugin<WorkspaceOptionsModule>();)
K_EXPORT_PLUGIN(WorkspaceOptionsModuleFactory("kcmworkspaceoptions"))


WorkspaceOptionsModule::WorkspaceOptionsModule(QWidget *parent, const QVariantList &)
  : KCModule(WorkspaceOptionsModuleFactory::componentData(), parent),
    m_kwinConfig( KSharedConfig::openConfig("kwinrc")),
    m_ownConfig( KSharedConfig::openConfig("workspaceoptionsrc")),
    m_currentlyIsDesktop(false),
    m_plasmaFound(false),
    m_ui(new Ui_MainPage())
{
    KAboutData *about =
    new KAboutData("kcmworkspaceoptions", 0, ki18n("Global options for the Plasma Workspace"),
                   0, KLocalizedString(), KAboutData::License_GPL,
                   ki18n("(c) 2009 Marco Martin"));

    about->addAuthor(ki18n("Marco Martin"), ki18n("Maintainer"), "notmart@gmail.com");

    setAboutData(about);

    setButtons(Help|Apply);

    m_plasmaFound = !KStandardDirs::findExe("plasma-desktop").isNull();

    m_ui->setupUi(this);
    // NOTE: the i18n() bellow is using translation from:
    // kdelibs/kdecore/localization/klocale.cpp
    m_ui->themeCacheSize->setSuffix(i18n("%1 MB", QLatin1String("")));

    connect(m_ui->formFactor, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->showToolTips, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->cacheTheme, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->cacheTheme, SIGNAL(toggled(bool)), this, SLOT(cacheThemeChanged(bool)));
    connect(m_ui->themeCacheSize, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->sessionManagement, SIGNAL(toggled(bool)), this, SLOT(changed()));

    if (!m_plasmaFound) {
        m_ui->formFactor->setEnabled(false);
        m_ui->cacheTheme->setEnabled(false);
        m_ui->themeCacheSize->setEnabled(false);
        m_ui->sessionManagement->setEnabled(false);
    }
}

WorkspaceOptionsModule::~WorkspaceOptionsModule()
{
    delete m_ui;
}

void WorkspaceOptionsModule::save()
{
    {
        KConfig config("plasmarc");
        KConfigGroup cg(&config, "PlasmaToolTips");
        cg.writeEntry("Delay", m_ui->showToolTips->isChecked() ? 0.7 : -1);
        KConfigGroup cg2(&config, "CachePolicies");
        cg2.writeEntry("CacheTheme", m_ui->cacheTheme->isChecked());
        cg2.writeEntry("ThemeCacheKb", m_ui->themeCacheSize->value() * 1024);
        KConfigGroup cg3(&config, "General");
        cg3.writeEntry("SessionManagement", m_ui->sessionManagement->isChecked());
    }

    const bool isDesktop = (m_ui->formFactor->currentIndex() == 0);

    KConfigGroup winCg(m_kwinConfig, "Windows");

    winCg.writeEntry("BorderlessMaximizedWindows", !isDesktop);
    if (!isDesktop) {
        winCg.writeEntry("Placement", "Maximizing");
    } else {
        winCg.writeEntry("Placement", "Smart");
    }
    winCg.sync();

    KConfigGroup kwinStyleCg(m_kwinConfig, "Style");
    KConfigGroup kwinPresentWindowsCg(m_kwinConfig, "Effect-PresentWindows");
    KConfigGroup kwinCompositingCg(m_kwinConfig, "Compositing");

    static const QString desktopTitleBarButtonsLeft = "MS";
    static const QString desktopTitleBarButtonsRight = "HIAX";
    static const QString netbookTitleBarButtonsLeft = "MS";
    static const QString netbookTitleBarButtonsRight = "HAX";
    static const int desktopPresentWindowsLayoutMode = 1;
    static const int netbookPresentWindowsLayoutMode = 1;
    static const bool desktopUnredirectFullscreen = true;
    static const bool netbookUnredirectFullscreen = false;

    kwinStyleCg.writeEntry("CustomButtonPositions", true);
    if (isDesktop) {
        kwinStyleCg.writeEntry("ButtonsOnLeft", desktopTitleBarButtonsLeft);
        kwinStyleCg.writeEntry("ButtonsOnRight", desktopTitleBarButtonsRight);

        kwinCompositingCg.writeEntry("UnredirectFullscreen", desktopUnredirectFullscreen);

        kwinPresentWindowsCg.writeEntry("LayoutMode", desktopPresentWindowsLayoutMode);
    } else {
        kwinStyleCg.writeEntry("ButtonsOnLeft", netbookTitleBarButtonsLeft);
        kwinStyleCg.writeEntry("ButtonsOnRight", netbookTitleBarButtonsRight);

        kwinCompositingCg.writeEntry("UnredirectFullscreen", netbookUnredirectFullscreen);

        kwinPresentWindowsCg.writeEntry("LayoutMode", netbookPresentWindowsLayoutMode);
    }

    kwinStyleCg.sync();
    kwinPresentWindowsCg.sync();
    kwinCompositingCg.sync();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal( "/KWin", "org.kde.KWin", "reloadConfig" );
    QDBusConnection::sessionBus().send(message);

    m_currentlyIsDesktop = isDesktop;
}

void WorkspaceOptionsModule::load()
{
    KConfigGroup winCg(m_kwinConfig, "Windows");
    m_currentlyIsDesktop = !winCg.readEntry("BorderlessMaximizedWindows", false);
    if (m_currentlyIsDesktop) {
        m_ui->formFactor->setCurrentIndex(0);
    } else {
        m_ui->formFactor->setCurrentIndex(1);
    }

    KConfig config("plasmarc");
    KConfigGroup cg(&config, "PlasmaToolTips");
    m_ui->showToolTips->setChecked(cg.readEntry("Delay", 0.7) > 0);
    KConfigGroup cg2(&config, "CachePolicies");
    m_ui->cacheTheme->setChecked(cg2.readEntry("CacheTheme", true));
    const int themeCacheKb = cg2.readEntry("ThemeCacheKb", 81920);
    m_ui->themeCacheSize->setValue(themeCacheKb / 1024);
    KConfigGroup cg3(&config, "General");
    m_ui->sessionManagement->setChecked(cg3.readEntry("SessionManagement", true));
}

void WorkspaceOptionsModule::defaults()
{
    m_ui->formFactor->setCurrentIndex(0);
}

void WorkspaceOptionsModule::cacheThemeChanged(bool cacheTheme)
{
    Q_ASSERT(m_plasmaFound);
    m_ui->themeCacheSize->setEnabled(cacheTheme);
}

#include "moc_workspaceoptions.cpp"
