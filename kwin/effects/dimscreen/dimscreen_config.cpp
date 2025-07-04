/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#include "dimscreen_config.h"
#include "dimscreenconfig.h"

#include <kwineffects.h>

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

DimScreenEffectConfigForm::DimScreenEffectConfigForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

DimScreenEffectConfig::DimScreenEffectConfig(QWidget *parent, const QVariantList &args)
    : KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new DimScreenEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    addConfig(DimScreenConfig::self(), m_ui);

    load();
}

void DimScreenEffectConfig::save()
{
    KCModule::save();
    EffectsHandler::sendReloadMessage("dimscreen");
}

} // namespace

#include "moc_dimscreen_config.cpp"
