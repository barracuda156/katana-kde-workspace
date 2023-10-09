/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "kdebug.h"

#include "main.h"

#include <kservicetypetrader.h>

#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginLoader>
#include <QtDBus/QtDBus>

K_PLUGIN_FACTORY(KWinScreenEdgesConfigFactory, registerPlugin<KWin::KWinScreenEdgesConfig>();)
K_EXPORT_PLUGIN(KWinScreenEdgesConfigFactory("kcmkwinscreenedges"))

namespace KWin
{

KWinScreenEdgesConfigForm::KWinScreenEdgesConfigForm(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

KWinScreenEdgesConfig::KWinScreenEdgesConfig(QWidget* parent, const QVariantList& args)
    : KCModule(KWinScreenEdgesConfigFactory::componentData(), parent, args)
    , m_config(KSharedConfig::openConfig("kwinrc"))
{
    m_ui = new KWinScreenEdgesConfigForm(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_ui);

    monitorInit();

    connect(m_ui->monitor, SIGNAL(changed()), this, SLOT(changed()));

    connect(m_ui->desktopSwitchCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(m_ui->activationDelaySpin, SIGNAL(valueChanged(int)), this, SLOT(sanitizeCooldown()));
    connect(m_ui->activationDelaySpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->triggerCooldownSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->quickMaximizeBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->quickTileBox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
    connect(m_ui->electricBorderCornerRatio, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    // Visual feedback of action group conflicts
    connect(m_ui->desktopSwitchCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(groupChanged()));
    connect(m_ui->quickMaximizeBox, SIGNAL(stateChanged(int)), this, SLOT(groupChanged()));
    connect(m_ui->quickTileBox, SIGNAL(stateChanged(int)), this, SLOT(groupChanged()));

    load();

    sanitizeCooldown();
}

KWinScreenEdgesConfig::~KWinScreenEdgesConfig()
{
}

void KWinScreenEdgesConfig::groupChanged()
{
    // Monitor conflicts
    bool hide = false;
    if (m_ui->desktopSwitchCombo->currentIndex() == 2)
        hide = true;
    monitorHideEdge(ElectricTop, hide);
    monitorHideEdge(ElectricRight, hide);
    monitorHideEdge(ElectricBottom, hide);
    monitorHideEdge(ElectricLeft, hide);
}

void KWinScreenEdgesConfig::load()
{
    KCModule::load();

    monitorLoad();

    KConfigGroup config(m_config, "Windows");

    m_ui->desktopSwitchCombo->setCurrentIndex(config.readEntry("ElectricBorders", 0));
    m_ui->activationDelaySpin->setValue(config.readEntry("ElectricBorderDelay", 150));
    m_ui->triggerCooldownSpin->setValue(config.readEntry("ElectricBorderCooldown", 350));
    m_ui->quickMaximizeBox->setChecked(config.readEntry("ElectricBorderMaximize", true));
    m_ui->quickTileBox->setChecked(config.readEntry("ElectricBorderTiling", true));
    m_ui->electricBorderCornerRatio->setValue(qRound(config.readEntry("ElectricBorderCornerRatio", 0.25)*100));

    emit changed(false);
}

void KWinScreenEdgesConfig::save()
{
    KCModule::save();

    monitorSave();

    KConfigGroup config(m_config, "Windows");

    config.writeEntry("ElectricBorders", m_ui->desktopSwitchCombo->currentIndex());
    config.writeEntry("ElectricBorderDelay", m_ui->activationDelaySpin->value());
    config.writeEntry("ElectricBorderCooldown", m_ui->triggerCooldownSpin->value());
    config.writeEntry("ElectricBorderMaximize", m_ui->quickMaximizeBox->isChecked());
    config.writeEntry("ElectricBorderTiling", m_ui->quickTileBox->isChecked());
    config.writeEntry("ElectricBorderCornerRatio", m_ui->electricBorderCornerRatio->value()/100.0);

    config.sync();

    // Reload KWin.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    emit changed(false);
}

void KWinScreenEdgesConfig::defaults()
{
    monitorDefaults();

    m_ui->desktopSwitchCombo->setCurrentIndex(0);
    m_ui->activationDelaySpin->setValue(150);
    m_ui->triggerCooldownSpin->setValue(350);
    m_ui->quickMaximizeBox->setChecked(true);
    m_ui->quickTileBox->setChecked(true);
    m_ui->electricBorderCornerRatio->setValue(25);

    emit changed(true);
}

void KWinScreenEdgesConfig::showEvent(QShowEvent* e)
{
    KCModule::showEvent(e);

    monitorShowEvent();
}

void KWinScreenEdgesConfig::sanitizeCooldown()
{
    m_ui->triggerCooldownSpin->setMinimum(m_ui->activationDelaySpin->value() + 50);
}

// Copied from kcmkwin/kwincompositing/main.cpp
bool KWinScreenEdgesConfig::effectEnabled(const QString& effect, const KConfigGroup& cfg) const
{
    KService::List services = KServiceTypeTrader::self()->query(
                                  "KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_" + effect + '\'');
    if (services.isEmpty())
        return false;
    QVariant v = services.first()->property("X-KDE-PluginInfo-EnabledByDefault");
    return cfg.readEntry("kwin4_effect_" + effect + "Enabled", v.toBool());
}

//-----------------------------------------------------------------------------
// Monitor

void KWinScreenEdgesConfig::monitorAddItem(const QString& item)
{
    for (int i = 0; i < 8; i++)
        m_ui->monitor->addEdgeItem(i, item);
}

void KWinScreenEdgesConfig::monitorItemSetEnabled(int index, bool enabled)
{
    for (int i = 0; i < 8; i++)
        m_ui->monitor->setEdgeItemEnabled(i, index, enabled);
}

void KWinScreenEdgesConfig::monitorInit()
{
    monitorAddItem(i18n("No Action"));
    monitorAddItem(i18n("Show Desktop"));
    monitorAddItem(i18n("Lock Screen"));

    // Search the effect names
    KServiceTypeTrader* trader = KServiceTypeTrader::self();
    KService::List services = trader->query("KWin/Effect", "[X-KDE-PluginInfo-Name] == 'kwin4_effect_presentwindows'");
    if (!services.isEmpty()) {
        monitorAddItem(services.first()->name() + " - " + i18n("All Desktops"));
        monitorAddItem(services.first()->name() + " - " + i18n("Current Desktop"));
        monitorAddItem(services.first()->name() + " - " + i18n("Current Application"));
    }

    monitorAddItem(i18n("Toggle window switching"));
    monitorAddItem(i18n("Toggle alternative window switching"));

    monitorShowEvent();
}

void KWinScreenEdgesConfig::monitorLoadAction(ElectricBorder edge, const QString& configName)
{
    KConfigGroup config(m_config, "ElectricBorders");
    QString lowerName = config.readEntry(configName, "None").toLower();
    if (lowerName == "showdesktop") monitorChangeEdge(edge, ElectricActionShowDesktop);
    else if (lowerName == "lockscreen") monitorChangeEdge(edge, ElectricActionLockScreen);
}

void KWinScreenEdgesConfig::monitorLoad()
{
    // Load ElectricBorderActions
    monitorLoadAction(ElectricTop,         "Top");
    monitorLoadAction(ElectricTopRight,    "TopRight");
    monitorLoadAction(ElectricRight,       "Right");
    monitorLoadAction(ElectricBottomRight, "BottomRight");
    monitorLoadAction(ElectricBottom,      "Bottom");
    monitorLoadAction(ElectricBottomLeft,  "BottomLeft");
    monitorLoadAction(ElectricLeft,        "Left");
    monitorLoadAction(ElectricTopLeft,     "TopLeft");

    // Load effect-specific actions:

    // Present Windows
    KConfigGroup presentWindowsConfig(m_config, "Effect-PresentWindows");
    QList<int> list = QList<int>();
    // PresentWindows BorderActivateAll
    list.append(ElectricTopLeft);
    list = presentWindowsConfig.readEntry("BorderActivateAll", list);
    foreach (const int i, list) {
        monitorChangeEdge(ElectricBorder(i), PresentWindowsAll);
    }
    // PresentWindows BorderActivate
    list.clear();
    list.append(ElectricNone);
    list = presentWindowsConfig.readEntry("BorderActivate", list);
    foreach (const int i, list) {
        monitorChangeEdge(ElectricBorder(i), PresentWindowsCurrent);
    }
    // PresentWindows BorderActivateClass
    list.clear();
    list.append(ElectricNone);
    list = presentWindowsConfig.readEntry("BorderActivateClass", list);
    foreach (const int i, list) {
        monitorChangeEdge(ElectricBorder(i), PresentWindowsClass);
    }
}

void KWinScreenEdgesConfig::monitorSaveAction(int edge, const QString& configName)
{
    KConfigGroup config(m_config, "ElectricBorders");
    int item = m_ui->monitor->selectedEdgeItem(edge);
    if (item == 1)
        config.writeEntry(configName, "ShowDesktop");
    else if (item == 2)
        config.writeEntry(configName, "LockScreen");
    else // Anything else
        config.writeEntry(configName, "None");
}

void KWinScreenEdgesConfig::monitorSave()
{
    // Save ElectricBorderActions
    monitorSaveAction(Monitor::Top,         "Top");
    monitorSaveAction(Monitor::TopRight,    "TopRight");
    monitorSaveAction(Monitor::Right,       "Right");
    monitorSaveAction(Monitor::BottomRight, "BottomRight");
    monitorSaveAction(Monitor::Bottom,      "Bottom");
    monitorSaveAction(Monitor::BottomLeft,  "BottomLeft");
    monitorSaveAction(Monitor::Left,        "Left");
    monitorSaveAction(Monitor::TopLeft,     "TopLeft");

    // Save effect-specific actions:

    // Present Windows
    KConfigGroup presentWindowsConfig(m_config, "Effect-PresentWindows");
    presentWindowsConfig.writeEntry("BorderActivateAll",
                                    monitorCheckEffectHasEdge(PresentWindowsAll));
    presentWindowsConfig.writeEntry("BorderActivate",
                                    monitorCheckEffectHasEdge(PresentWindowsCurrent));
    presentWindowsConfig.writeEntry("BorderActivateClass",
                                    monitorCheckEffectHasEdge(PresentWindowsClass));
}

void KWinScreenEdgesConfig::monitorDefaults()
{
    // Clear all edges
    for (int i = 0; i < 8; i++)
        m_ui->monitor->selectEdgeItem(i, 1); // the index will be deduced to 0

    // Present windows = Top-left
    m_ui->monitor->selectEdgeItem(Monitor::TopLeft, PresentWindowsAll);
}

void KWinScreenEdgesConfig::monitorShowEvent()
{
    // Check if they are enabled
    KConfigGroup config(m_config, "Compositing");
    if (config.readEntry("Enabled", true)) {
        // Compositing enabled
        config = KConfigGroup(m_config, "Plugins");

        // Present Windows
        bool enabled = effectEnabled("presentwindows", config);
        monitorItemSetEnabled(PresentWindowsCurrent, enabled);
        monitorItemSetEnabled(PresentWindowsAll, enabled);
    } else { // Compositing disabled
        monitorItemSetEnabled(PresentWindowsCurrent, false);
        monitorItemSetEnabled(PresentWindowsAll, false);
    }
}

void KWinScreenEdgesConfig::monitorChangeEdge(ElectricBorder border, int index)
{
    switch(border) {
    case ElectricTop:
        m_ui->monitor->selectEdgeItem(Monitor::Top, index);
        break;
    case ElectricTopRight:
        m_ui->monitor->selectEdgeItem(Monitor::TopRight, index);
        break;
    case ElectricRight:
        m_ui->monitor->selectEdgeItem(Monitor::Right, index);
        break;
    case ElectricBottomRight:
        m_ui->monitor->selectEdgeItem(Monitor::BottomRight, index);
        break;
    case ElectricBottom:
        m_ui->monitor->selectEdgeItem(Monitor::Bottom, index);
        break;
    case ElectricBottomLeft:
        m_ui->monitor->selectEdgeItem(Monitor::BottomLeft, index);
        break;
    case ElectricLeft:
        m_ui->monitor->selectEdgeItem(Monitor::Left, index);
        break;
    case ElectricTopLeft:
        m_ui->monitor->selectEdgeItem(Monitor::TopLeft, index);
        break;
    default: // Nothing
        break;
    }
}

void KWinScreenEdgesConfig::monitorHideEdge(ElectricBorder border, bool hidden)
{
    switch(border) {
    case ElectricTop:
        m_ui->monitor->setEdgeHidden(Monitor::Top, hidden);
        break;
    case ElectricTopRight:
        m_ui->monitor->setEdgeHidden(Monitor::TopRight, hidden);
        break;
    case ElectricRight:
        m_ui->monitor->setEdgeHidden(Monitor::Right, hidden);
        break;
    case ElectricBottomRight:
        m_ui->monitor->setEdgeHidden(Monitor::BottomRight, hidden);
        break;
    case ElectricBottom:
        m_ui->monitor->setEdgeHidden(Monitor::Bottom, hidden);
        break;
    case ElectricBottomLeft:
        m_ui->monitor->setEdgeHidden(Monitor::BottomLeft, hidden);
        break;
    case ElectricLeft:
        m_ui->monitor->setEdgeHidden(Monitor::Left, hidden);
        break;
    case ElectricTopLeft:
        m_ui->monitor->setEdgeHidden(Monitor::TopLeft, hidden);
        break;
    default: // Nothing
        break;
    }
}

QList<int> KWinScreenEdgesConfig::monitorCheckEffectHasEdge(int index) const
{
    QList<int> list = QList<int>();
    if (m_ui->monitor->selectedEdgeItem(Monitor::Top) == index)
        list.append(int(ElectricTop));
    if (m_ui->monitor->selectedEdgeItem(Monitor::TopRight) == index)
        list.append(int(ElectricTopRight));
    if (m_ui->monitor->selectedEdgeItem(Monitor::Right) == index)
        list.append(int(ElectricRight));
    if (m_ui->monitor->selectedEdgeItem(Monitor::BottomRight) == index)
        list.append(int(ElectricBottomRight));
    if (m_ui->monitor->selectedEdgeItem(Monitor::Bottom) == index)
        list.append(int(ElectricBottom));
    if (m_ui->monitor->selectedEdgeItem(Monitor::BottomLeft) == index)
        list.append(int(ElectricBottomLeft));
    if (m_ui->monitor->selectedEdgeItem(Monitor::Left) == index)
        list.append(int(ElectricLeft));
    if (m_ui->monitor->selectedEdgeItem(Monitor::TopLeft) == index)
        list.append(int(ElectricTopLeft));

    if (list.isEmpty())
        list.append(int(ElectricNone));
    return list;
}

} // namespace

#include "moc_main.cpp"
