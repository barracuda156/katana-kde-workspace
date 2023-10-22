/*
    This is the new kwindecoration kcontrol module

    Copyright (c) 2001
        Karol Szwed <gallium@kde.org>
        http://gallium.n3.net/
    Copyright 2009, 2010 Martin Gräßlin <mgraesslin@kde.org>

    Supports new kwin configuration plugins, and titlebar button position
    modification via dnd interface.

    Based on original "kwintheme" (Window Borders)
    Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "kwindecoration.h"

#include <QLibrary>
#include <QDBusMessage>
#include <QDBusConnection>
#include <KPluginFactory>
#include <KAboutData>
#include <KConfigGroup>
#include <KVBox>
#include <KLocale>

static QString styleToConfigLib(KConfigGroup &conf)
{
    QString styleLib = conf.readEntry("PluginLib", "kwin3_oxygen");
    if (styleLib.isEmpty()) {
        // Selected decoration doesn't exist, use the default
        styleLib = "kwin3_oxygen";
    }
    if (styleLib.startsWith(QLatin1String("kwin3_"))) {
        return "kwin_" + styleLib.mid(6) + "_config";
    }
    return styleLib + "_config";
}

// KCModule plugin interface
// =========================
K_PLUGIN_FACTORY(KWinDecoFactory, registerPlugin<KWin::KWinDecorationModule>();)
K_EXPORT_PLUGIN(KWinDecoFactory("kcmkwindecoration"))

namespace KWin
{

KWinDecorationModule::KWinDecorationModule(QWidget *parent, const QVariantList &args)
    : KCModule(KWinDecoFactory::componentData(), parent)
    , m_kwinConfig(KSharedConfig::openConfig("kwinrc"))
    , m_layout(nullptr)
    , m_pluginObject(nullptr)
    , m_pluginConfigWidget(nullptr)
{
    Q_UNUSED(args);

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmkwindecoration"), 0,
        ki18n("Window Decoration Control Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("(c) 2001 Karol Szwed")
    );
    about->addAuthor(ki18n("Karol Szwed"), KLocalizedString(), "gallium@kde.org");
    setAboutData(about);

    
    m_layout = new QVBoxLayout(this);
}

KWinDecorationModule::~KWinDecorationModule()
{
    delete m_pluginObject;
}

void KWinDecorationModule::load()
{
    KConfigGroup config(m_kwinConfig, "Style");
    QLibrary library(styleToConfigLib(config));
    if (library.load()) {
        void *alloc_ptr = library.resolve("allocate_config");
        if (alloc_ptr != NULL) {
            allocatePlugin = (QObject * (*)(KConfigGroup & conf, QWidget * parent))alloc_ptr;
            m_pluginConfigWidget = new KVBox(this);
            m_pluginObject = (QObject*)(allocatePlugin(config, m_pluginConfigWidget));

            // connect required signals and slots together...
            connect(m_pluginObject, SIGNAL(changed()), this, SLOT(slotSelectionChanged()));
            connect(this, SIGNAL(pluginSave(KConfigGroup&)), m_pluginObject, SLOT(save(KConfigGroup&)));
        }
    }

    if (m_pluginConfigWidget) {
        m_layout->addWidget(m_pluginConfigWidget);
    }
}

void KWinDecorationModule::save()
{
    KConfigGroup config(m_kwinConfig, "Style");
    emit pluginSave(config);
    config.sync();

    // We saved, so tell kcmodule that there have been  no new user changes made.
    emit KCModule::changed(false);

    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}

void KWinDecorationModule::defaults()
{
    QMetaObject::invokeMethod(m_pluginObject, "defaults");
    emit changed(true);
}

// This is the selection handler setting
void KWinDecorationModule::slotSelectionChanged()
{
    emit KCModule::changed(true);
}

} // namespace KWin
