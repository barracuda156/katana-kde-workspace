/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "config-kwin.h"

#include "main.h"
#include "dbus.h"

#include "kwin_interface.h"
#include "kwinglobals.h"

#include <kaboutdata.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kpluginselector.h>
#include <kservicetypetrader.h>
#include <kplugininfo.h>
#include <kservice.h>
#include <ktitlewidget.h>
#include <knotification.h>
#include <kdialog.h>

#include <QtDBus/QtDBus>
#include <QPainter>
#include <QPaintEngine>
#include <QTimer>
#include <QLabel>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KIcon>

static int s_xrenderfilter = 1; // KWin::Scene::ImageFilterGood

K_PLUGIN_FACTORY(KWinCompositingConfigFactory,
                 registerPlugin<KWin::KWinCompositingConfig>();
                )
K_EXPORT_PLUGIN(KWinCompositingConfigFactory("kcmkwincompositing"))

namespace KWin
{

KWinCompositingConfig::KWinCompositingConfig(QWidget *parent, const QVariantList &args)
    : KCModule(KWinCompositingConfigFactory::componentData(), parent)
    , mKWinConfig(KSharedConfig::openConfig("kwinrc"))
    , m_showDetailedErrors(new QAction(i18nc("Action to open a dialog showing detailed information why an effect could not be loaded",
                                             "Details"), this))
    , m_dontShowAgain(new QAction(i18nc("Prevent warning from bein displayed again", "Don't show again!"), this))
{
    QDBusConnection::sessionBus().registerService("org.kde.kwinCompositingDialog");
    QDBusConnection::sessionBus().registerObject("/CompositorSettings", this);
    new MainAdaptor(this);
    KGlobal::locale()->insertCatalog("kwin_effects");
    ui.setupUi(this);
    layout()->setMargin(0);
    layout()->activate();
    ui.tabWidget->setCurrentIndex(0);
    ui.statusTitleWidget->hide();
    ui.messageBox->setMessageType(KMessageWidget::Warning);
    ui.messageBox->addAction(m_dontShowAgain);
    foreach (QWidget *w, m_dontShowAgain->associatedWidgets())
        w->setVisible(false);
    ui.messageBox->addAction(m_showDetailedErrors);

    bool showMessage = false;
    QString message, details, dontAgainKey;
    if (args.count() > 1) {
        for (int i = 0; i < args.count() - 1; ++i) {
            if (args.at(i).toString() == "warn") {
                showMessage = true;
                message = QString::fromLocal8Bit(QByteArray::fromBase64(args.at(i+1).toByteArray()));
            } else if (args.at(i).toString() == "details") {
                showMessage = true;
                details = QString::fromLocal8Bit(QByteArray::fromBase64(args.at(i+1).toByteArray()));
            } else if (args.at(i).toString() == "dontagain") {
                showMessage = true;
                dontAgainKey = args.at(i+1).toString();
            }
        }
    }

    if (showMessage) {
        ui.messageBox->setVisible(showMessage); // first show, animation is broken on init
        warn(message, details, dontAgainKey);
    } else
        ui.messageBox->setVisible(false);

#ifndef KWIN_BUILD_COMPOSITE
    ui.compositingType->removeItem(0);
#define XRENDER_INDEX -1
#define NONE_INDEX 0
#else
#define XRENDER_INDEX 0
#define NONE_INDEX 1
#endif

    connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

    connect(ui.useCompositing, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectWinManagement, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.effectAnimations, SIGNAL(toggled(bool)), this, SLOT(changed()));

    connect(ui.effectSelector, SIGNAL(changed(bool)), this, SLOT(changed()));
    connect(ui.effectSelector, SIGNAL(configCommitted(QByteArray)),
            this, SLOT(reparseConfiguration(QByteArray)));

    connect(ui.desktopSwitchingCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.animationSpeedCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(alignGuiToCompositingType(int)));
    connect(ui.windowThumbnails, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui.unredirectFullscreen , SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.xrScaleFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(m_showDetailedErrors, SIGNAL(triggered(bool)), SLOT(showDetailedEffectLoadingInformation()));
    connect(m_dontShowAgain, SIGNAL(triggered(bool)), SLOT(blockFutureWarnings()));

    // Open the temporary config file
    // Temporary conf file is used to synchronize effect checkboxes with effect
    // selector by loading/saving effects from/to temp config when active tab
    // changes.
    mTmpConfigFile.open();
    mTmpConfig = KSharedConfig::openConfig(mTmpConfigFile.fileName());

    // toggle effects shortcut button stuff - /HAS/ to happen before load!
    m_actionCollection = new KActionCollection( this, KComponentData("kwin") );

    KAction* a = static_cast<KAction*>(m_actionCollection->addAction( "Suspend Compositing" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( QKeySequence( Qt::ALT + Qt::SHIFT + Qt::Key_F12 ));
    connect(ui.toggleEffectsShortcut, SIGNAL(keySequenceChanged(QKeySequence)), this, SLOT(toggleEffectShortcutChanged(QKeySequence)));

    // Initialize the user interface with the config loaded from kwinrc.
    load();

    KAboutData *about = new KAboutData(I18N_NOOP("kcmkwincompositing"), 0,
                                       ki18n("KWin Desktop Effects Configuration Module"),
                                       0, KLocalizedString(), KAboutData::License_GPL, ki18n("(c) 2007 Rivo Laks"));
    about->addAuthor(ki18n("Rivo Laks"), KLocalizedString(), "rivolaks@hot.ee");
    setAboutData(about);

    // search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    QString slide, cube, fadedesktop;
    // desktop switcher
    services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_slide'");
    if (!services.isEmpty())
        slide = services.first()->name();

    ui.desktopSwitchingCombo->addItem(i18n("No effect"));
    ui.desktopSwitchingCombo->addItem(slide);
}

void KWinCompositingConfig::reparseConfiguration(const QByteArray& conf)
{
    KComponentData component(conf);
    KSharedConfig::Ptr config = component.config();
    config->reparseConfiguration();
}

void KWinCompositingConfig::initEffectSelector()
{
    // Find all .desktop files of the effects
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QList<KPluginInfo> effectinfos = KPluginInfo::fromServices(offers);

    // Add them to the plugin selector
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Appearance"), "Appearance", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Accessibility"), "Accessibility", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Focus"), "Focus", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Window Management"), "Window Management", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Candy"), "Candy", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Demos"), "Demos", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tests"), "Tests", mTmpConfig);
    ui.effectSelector->addPlugins(effectinfos, KPluginSelector::ReadConfigFile, i18n("Tools"), "Tools", mTmpConfig);
}

void KWinCompositingConfig::currentTabChanged(int tab)
{
    // block signals to don't emit the changed()-signal by just switching the current tab
    blockSignals(true);

    // write possible changes done to synchronize effect checkboxes and selector
    // TODO: This segment is prone to fail when the UI is changed;
    // you'll most likely not think of the hard coded numbers here when just changing the order of the tabs.
    if (tab == 0) {
        // General tab was activated
        saveEffectsTab();
        loadGeneralTab();
    } else if (tab == 1) {
        // Effects tab was activated
        saveGeneralTab();
        loadEffectsTab();
    }

    blockSignals(false);
}

void KWinCompositingConfig::loadGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    bool enabled = config.readEntry("Enabled", true);
    ui.useCompositing->setChecked(enabled);
    
    m_actionCollection->readSettings();

    // this works by global shortcut magics - it will pick the current sc
    // but the constructor line that adds the default alt+shift+f12 gsc is IMPORTANT!
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action("Suspend Compositing")))
        ui.toggleEffectsShortcut->setKeySequence(a->globalShortcut());

    ui.animationSpeedCombo->setCurrentIndex(config.readEntry("AnimationSpeed", 3));

    // Load effect settings
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define LOAD_EFFECT_CONFIG(effectname)  effectconfig.readEntry("kwin4_effect_" effectname "Enabled", true)
    int winManagementEnabled = LOAD_EFFECT_CONFIG("presentwindows")
                               + LOAD_EFFECT_CONFIG("desktopgrid")
                               + LOAD_EFFECT_CONFIG("dialogparent");
    if (winManagementEnabled > 0 && winManagementEnabled < 3) {
        ui.effectWinManagement->setTristate(true);
        ui.effectWinManagement->setCheckState(Qt::PartiallyChecked);
    } else
        ui.effectWinManagement->setChecked(winManagementEnabled);
    ui.effectAnimations->setChecked(LOAD_EFFECT_CONFIG("minimizeanimation"));
#undef LOAD_EFFECT_CONFIG

    // desktop switching
    // Set current option to "none" if no plugin is activated.
    ui.desktopSwitchingCombo->setCurrentIndex(0);
    if (effectEnabled("slide", effectconfig))
        ui.desktopSwitchingCombo->setCurrentIndex(1);
}

void KWinCompositingConfig::alignGuiToCompositingType(int compositingType)
{
    ui.scaleMethodLabel->setVisible(compositingType == XRENDER_INDEX);
    ui.xrScaleFilter->setVisible(compositingType == XRENDER_INDEX);
    if (compositingType == XRENDER_INDEX) {
        ui.scaleMethodLabel->setBuddy(ui.xrScaleFilter);
    }
}

void KWinCompositingConfig::toggleEffectShortcutChanged(const QKeySequence &seq)
{
    if (KAction *a = qobject_cast<KAction*>(m_actionCollection->action("Suspend Compositing")))
        a->setGlobalShortcut(seq, KAction::ActiveShortcut);
    emit changed(true);
}

bool KWinCompositingConfig::effectEnabled(const QString& effect, const KConfigGroup& cfg) const
{
    KService::List services = KServiceTypeTrader::self()->query(
                                  "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

void KWinCompositingConfig::loadEffectsTab()
{
    ui.effectSelector->load();
}

void KWinCompositingConfig::loadAdvancedTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    QString backend = config.readEntry("Backend", "XRender");
    if (backend == "XRender") {
        ui.compositingType->setCurrentIndex(XRENDER_INDEX);
    } else {
        ui.compositingType->setCurrentIndex(NONE_INDEX);
    }

    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if (hps == 6)   // always
        ui.windowThumbnails->setCurrentIndex(0);
    else if (hps == 4)   // never
        ui.windowThumbnails->setCurrentIndex(2);
    else // shown, or default
        ui.windowThumbnails->setCurrentIndex(1);
    ui.unredirectFullscreen->setChecked(config.readEntry("UnredirectFullscreen", false));

    ui.xrScaleFilter->setCurrentIndex(config.readEntry("XRenderFilter", s_xrenderfilter));

    alignGuiToCompositingType(ui.compositingType->currentIndex());
}

void KWinCompositingConfig::updateStatusUI(bool compositingIsPossible)
{
    if (compositingIsPossible) {
        ui.compositingOptionsContainer->show();
        ui.statusTitleWidget->hide();
    } else {
        OrgKdeKWinInterface kwin("org.kde.KWin", "/KWin", QDBusConnection::sessionBus());
        ui.compositingOptionsContainer->hide();
        QString text = i18n("Desktop effects are not available on this system due to the following technical issues:");
        text += "<hr>";
        text += kwin.isValid() ? kwin.compositingNotPossibleReason() : i18nc("Reason shown when trying to activate desktop effects and KWin (most likely) crashes",
                                                                             "Window Manager seems not to be running");
        ui.statusTitleWidget->setText(text);
        ui.statusTitleWidget->setPixmap(KTitleWidget::InfoMessage, KTitleWidget::ImageLeft);
        ui.statusTitleWidget->show();
    }
}

void KWinCompositingConfig::load()
{
    initEffectSelector();
    mKWinConfig->reparseConfiguration();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.KWin", "/KWin", "org.kde.KWin", "compositingPossible");
    QDBusConnection::sessionBus().callWithCallback(msg, this, SLOT(updateStatusUI(bool)));

    // Copy Plugins group to temp config file
    QMap<QString, QString> entries = mKWinConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup tmpconfig(mTmpConfig, "Plugins");
    tmpconfig.deleteGroup();
    for (; it != entries.constEnd(); ++it)
        tmpconfig.writeEntry(it.key(), it.value());

    loadGeneralTab();
    loadEffectsTab();
    loadAdvancedTab();

    emit changed(false);
}

void KWinCompositingConfig::saveGeneralTab()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    // Check if any critical settings that need confirmation have changed
    config.writeEntry("Enabled", ui.useCompositing->isChecked());
    config.writeEntry("AnimationSpeed", ui.animationSpeedCombo->currentIndex());

    // Save effects
    KConfigGroup effectconfig(mTmpConfig, "Plugins");
#define WRITE_EFFECT_CONFIG(effectname, widget)  effectconfig.writeEntry("kwin4_effect_" effectname "Enabled", widget->isChecked())
    if (ui.effectWinManagement->checkState() != Qt::PartiallyChecked) {
        WRITE_EFFECT_CONFIG("presentwindows", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("desktopgrid", ui.effectWinManagement);
        WRITE_EFFECT_CONFIG("dialogparent", ui.effectWinManagement);
    }
    // TODO: maybe also do some effect-specific configuration here, e.g.
    //  enable/disable desktopgrid's animation according to this setting
    WRITE_EFFECT_CONFIG("minimizeanimation", ui.effectAnimations);
#undef WRITE_EFFECT_CONFIG

    int desktopSwitcher = ui.desktopSwitchingCombo->currentIndex();
    switch(desktopSwitcher) {
    case 0:
        // no effect
        effectconfig.writeEntry("kwin4_effect_slideEnabled", false);
        break;
    case 1:
        // slide
        effectconfig.writeEntry("kwin4_effect_slideEnabled", true);
        break;
    }
}

void KWinCompositingConfig::saveEffectsTab()
{
    ui.effectSelector->save();
}

bool KWinCompositingConfig::saveAdvancedTab()
{
    bool advancedChanged = false;
    static const int hps[] = { 6 /*always*/, 5 /*shown*/,  4 /*never*/ };

    KConfigGroup config(mKWinConfig, "Compositing");

    QString backend = "none";

    switch (ui.compositingType->currentIndex()) {
    case XRENDER_INDEX:
        backend  = "XRender";
        break;
    }

    if (config.readEntry("HiddenPreviews", 5) != hps[ ui.windowThumbnails->currentIndex()]
        || config.readEntry("XRenderFilter", s_xrenderfilter) != ui.xrScaleFilter->currentIndex()
        || config.readEntry("Backend") != ui.compositingType->currentText()) {
        advancedChanged = true;
    }

    config.writeEntry("Backend", backend);

    config.writeEntry("HiddenPreviews", hps[ ui.windowThumbnails->currentIndex()]);
    config.writeEntry("UnredirectFullscreen", ui.unredirectFullscreen->isChecked());

    config.writeEntry("XRenderFilter", ui.xrScaleFilter->currentIndex());

    return advancedChanged;
}

void KWinCompositingConfig::save()
{
    // Save current config. We'll use this for restoring in case something goes wrong.
    KConfigGroup config(mKWinConfig, "Compositing");
    mPreviousConfig = config.entryMap();

    // Save shortcut changes
    m_actionCollection->writeSettings(nullptr, true);

    // bah; tab content being dependent on the other is really bad; and
    // deprecated in the HIG for a reason.  It is confusing!
    // Make sure we only call save on each tab once; as they are stateful due to the revert concept
    if (ui.tabWidget->currentIndex() == 0) {  // "General" tab was active
        saveGeneralTab();
        loadEffectsTab();
        saveEffectsTab();
    } else {
        saveEffectsTab();
        loadGeneralTab();
        saveGeneralTab();
    }
    bool advancedChanged = saveAdvancedTab();

    // Copy Plugins group from temp config to real config
    QMap<QString, QString> entries = mTmpConfig->entryMap("Plugins");
    QMap<QString, QString>::const_iterator it = entries.constBegin();
    KConfigGroup realconfig(mKWinConfig, "Plugins");
    realconfig.deleteGroup();
    for (; it != entries.constEnd(); ++it)
        realconfig.writeEntry(it.key(), it.value());

    emit changed(false);

    configChanged(advancedChanged);

    // This assumes that this KCM is running with the same environment variables as KWin
    // TODO: Detect KWIN_COMPOSE=N as well
    if (!qgetenv("KDE_FAILSAFE").isNull() && ui.useCompositing->isChecked()) {
        KMessageBox::sorry(this, i18n(
                               "Your settings have been saved but as KDE is currently running in failsafe "
                               "mode desktop effects cannot be enabled at this time.\n\n"
                               "Please exit failsafe mode to enable desktop effects."));
    }
}

void KWinCompositingConfig::checkLoadedEffects()
{
    // check for effects not supported by Backend or hardware
    // such effects are enabled but not returned by DBus method loadedEffects
    OrgKdeKWinInterface kwin("org.kde.KWin", "/KWin", QDBusConnection::sessionBus());
    KConfigGroup effectConfig = KConfigGroup(mKWinConfig, "Compositing");
    bool enabledAfter = effectConfig.readEntry("Enabled", true);

    QDBusPendingReply< QStringList > reply = kwin.loadedEffects();

    if (!reply.isError() && enabledAfter && !getenv("KDE_FAILSAFE")) {
        effectConfig = KConfigGroup(mKWinConfig, "Plugins");
        QStringList loadedEffects = reply.value();
        QStringList effects = effectConfig.keyList();
        QStringList disabledEffects = QStringList();
        foreach (QString effect, effects) { // krazy:exclude=foreach
            QString temp = effect.mid(13, effect.length() - 13 - 7);
            effect.truncate(effect.length() - 7);
            if (effectEnabled(temp, effectConfig) && !loadedEffects.contains(effect)) {
                disabledEffects << effect;
            }
        }
        if (!disabledEffects.isEmpty()) {
            m_showDetailedErrors->setData(disabledEffects);
            foreach (QWidget *w, m_showDetailedErrors->associatedWidgets())
                w->setVisible(true);
            ui.messageBox->setText(i18ncp("Error Message shown when a desktop effect could not be loaded",
                                          "One desktop effect could not be loaded.",
                                          "%1 desktop effects could not be loaded.", disabledEffects.count()));
            ui.messageBox->show();
        } else {
            foreach (QWidget *w, m_showDetailedErrors->associatedWidgets())
                w->setVisible(false);
        }
    }
}

void KWinCompositingConfig::showDetailedEffectLoadingInformation()
{
    QStringList disabledEffects = m_showDetailedErrors->data().toStringList();
    OrgKdeKWinInterface kwin("org.kde.KWin", "/KWin", QDBusConnection::sessionBus());
    QDBusPendingReply< QString > pendingCompositingType = kwin.compositingType();
    QString compositingType = pendingCompositingType.isError() ? "none" : pendingCompositingType.value();
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services;
    const KLocalizedString unknownReason = ki18nc("Effect with given name could not be activated due to unknown reason",
                                                    "%1 effect failed to load due to unknown reason.");
    const KLocalizedString requiresShaders = ki18nc("Effect with given name could not be activated as it requires hardware shaders",
                                                    "%1 effect requires hardware support.");
    KDialog *dialog = new KDialog(this);
    dialog->setWindowTitle(i18nc("Window title", "List of effects which could not be loaded"));
    dialog->setButtons(KDialog::Ok);
    QWidget *mainWidget = new QWidget(dialog);
    dialog->setMainWidget(mainWidget);
    QVBoxLayout *vboxLayout = new QVBoxLayout(mainWidget);
    mainWidget->setLayout(vboxLayout);
    KTitleWidget *titleWidget = new KTitleWidget(mainWidget);
    titleWidget->setText(i18n("For technical reasons it is not possible to determine all possible error causes."),
                         KTitleWidget::InfoMessage);
    QLabel *label = new QLabel(mainWidget);
    label->setOpenExternalLinks(true);
    vboxLayout->addWidget(titleWidget);
    vboxLayout->addWidget(label);
    if (!m_externErrorDetails.isNull()) {
        label->setText(m_externErrorDetails);
    } else if (compositingType != "none") {
        QString text;
        if (disabledEffects.count() > 1) {
            text = "<ul>";
        }
        foreach (const QString & effect, disabledEffects) {
            QString message;
            services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == '" + effect + '\'');
            if (!services.isEmpty()) {
                KService::Ptr service = services.first();
                message = unknownReason.subs(service->name()).toString();
            } else {
                message = unknownReason.subs(effect).toString();
            }
            if (disabledEffects.count() > 1) {
                text.append("<li>");
                text.append(message);
                text.append("</li>");
            } else {
                text = message;
            }
        }
        if (disabledEffects.count() > 1) {
            text.append("</ul>");
        }
        label->setText(text);
    } else {
        // compositing is not active - no effect can be active
        label->setText(i18nc("Error Message shown when compositing is not active after tried activation",
                             "Desktop effect system is not running."));
    }
    dialog->show();
}

void KWinCompositingConfig::warn(QString message, QString details, QString dontAgainKey)
{
    ui.messageBox->setText(message);
    m_dontShowAgain->setData(dontAgainKey);
    foreach (QWidget *w, m_dontShowAgain->associatedWidgets())
        w->setVisible(!dontAgainKey.isEmpty());
    m_externErrorDetails = details.isNull() ? "" : details;
    foreach (QWidget *w, m_showDetailedErrors->associatedWidgets())
        w->setVisible(!m_externErrorDetails.isEmpty());
    ui.messageBox->show();
}

void KWinCompositingConfig::blockFutureWarnings() {
    QString key;
    if (QAction *act = qobject_cast<QAction*>(sender()))
        key = act->data().toString();
    if (key.isEmpty())
        return;
    QStringList l = key.split(':', QString::SkipEmptyParts);
    KConfig cfg(l.count() > 1 ? l.at(0) : "kwin_dialogsrc");
    KConfigGroup(&cfg, "Notification Messages").writeEntry(l.last(), false);
    cfg.sync();
    ui.messageBox->hide();
}

void KWinCompositingConfig::configChanged(bool reinitCompositing)
{
    // Send signal to kwin
    mKWinConfig->sync();

    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin",
                           reinitCompositing ? "reinitCompositing" : "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    // maybe it's ok now?
    if (reinitCompositing && !ui.compositingOptionsContainer->isVisible())
        load();

    // HACK: We can't just do this here, due to the asynchronous nature of signals.
    // We also can't change reinitCompositing into a message (which would allow
    // callWithCallbac() to do this neater) due to multiple kwin instances.
    QTimer::singleShot(1000, this, SLOT(checkLoadedEffects()));
}


void KWinCompositingConfig::defaults()
{
    ui.tabWidget->setCurrentIndex(0);

    ui.useCompositing->setChecked(true);
    ui.effectWinManagement->setChecked(true);
    ui.effectAnimations->setChecked(true);

    ui.desktopSwitchingCombo->setCurrentIndex(1);
    ui.animationSpeedCombo->setCurrentIndex(3);

    ui.effectSelector->defaults();

    ui.compositingType->setCurrentIndex(XRENDER_INDEX);
    ui.windowThumbnails->setCurrentIndex(1);
    ui.unredirectFullscreen->setChecked(false);
    ui.xrScaleFilter->setCurrentIndex(s_xrenderfilter);
}

QString KWinCompositingConfig::quickHelp() const
{
    return i18n("<h1>Desktop Effects</h1>");
}

} // namespace

#include "moc_dbus.cpp"
#include "moc_main.cpp"
