/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "thumbnailaside_config.h"
// KConfigSkeleton
#include "thumbnailasideconfig.h"

#include <kwineffects.h>

#include <KLocalizedString>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>

#include <QWidget>
#include <QVBoxLayout>

namespace KWin
{

KWIN_EFFECT_CONFIG_FACTORY

ThumbnailAsideEffectConfigForm::ThumbnailAsideEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

ThumbnailAsideEffectConfig::ThumbnailAsideEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(EffectFactory::componentData(), parent, args)
{
    m_ui = new ThumbnailAsideEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));

    addConfig(ThumbnailAsideConfig::self(), this);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, KComponentData("kwin"));

    KAction* a = (KAction*)m_actionCollection->addAction("ToggleCurrentThumbnail");
    a->setText(i18n("Toggle Thumbnail for Current Window"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(QKeySequence(Qt::META + Qt::CTRL + Qt::Key_T));

    m_ui->editor->addCollection(m_actionCollection);

    load();
}

void ThumbnailAsideEffectConfig::load()
{
    KCModule::load();
    m_ui->editor->importConfiguration();
}

void ThumbnailAsideEffectConfig::save()
{
    KCModule::save();
    m_ui->editor->exportConfiguration();
    EffectsHandler::sendReloadMessage("thumbnailaside");
}

} // namespace

#include "moc_thumbnailaside_config.cpp"
