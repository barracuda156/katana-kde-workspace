/*  This file is part of the KDE project
    Copyright (C) 2023 Ivailo Monev <xakepa10@gmail.com>

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

#include "spellchecking.h"

#include <kpluginfactory.h>

K_PLUGIN_FACTORY(SpellFactory, registerPlugin<SpellCheckingModule>();)
K_EXPORT_PLUGIN(SpellFactory("kcmspellchecking"))

SpellCheckingModule::SpellCheckingModule(QWidget *parent, const QVariantList &args)
    : KCModule(SpellFactory::componentData(), parent, args),
    m_layout(nullptr),
    m_configWidget(nullptr),
    m_config(nullptr)
{
    Q_UNUSED(args);

    m_layout = new QVBoxLayout(this);
    m_layout->setMargin(0);
    m_config = new KConfig("kdeglobals");
}

SpellCheckingModule::~SpellCheckingModule()
{
    delete m_config;
}

void SpellCheckingModule::load()
{
    if (m_configWidget) {
        delete m_configWidget;
    }
    m_configWidget = new KSpellConfigWidget(m_config, this);
    m_layout->addWidget(m_configWidget);
    connect(m_configWidget, SIGNAL(configChanged()), this, SLOT(slotConfigChanged()));
    emit changed(false);
}

void SpellCheckingModule::save()
{
    m_configWidget->save();
    emit changed(false);
}

void SpellCheckingModule::defaults()
{
    m_configWidget->slotDefault();
}

void SpellCheckingModule::slotConfigChanged()
{
    emit changed(true);
}

#include "moc_spellchecking.cpp"
