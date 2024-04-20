/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "kmetainfoconfig.h"

#include <kdebug.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kstandarddirs.h>
#include <kfilemetainfo.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kservicetypetrader.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

K_PLUGIN_FACTORY(KCMMetaInfoFactory, registerPlugin<KCMMetaInfo>();)
K_EXPORT_PLUGIN(KCMMetaInfoFactory("kcmmetainfoconfig", "kcm_metainfoconfig"))

KCMMetaInfo::KCMMetaInfo(QWidget* parent, const QVariantList& args)
    : KCModule(KCMMetaInfoFactory::componentData(), parent)
{
    Q_UNUSED(args);

    setQuickHelp(i18n("<h1>KFileMetaInfo</h1> This module allows you to change KDE meta information preferences."));

    setupUi(this);

    KAboutData *about = new KAboutData(
        I18N_NOOP("KCMMetaInfo"), 0,
        ki18n("KDE Meta Information Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2022, Ivailo Monev xakepa10@gmail.com")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    setButtons(KCModule::Help | KCModule::Apply);

    connect(
        pluginstree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
        this, SLOT(slotPluginItemChanged(QTreeWidgetItem*,int))
    );

    connect(
        metainfolist, SIGNAL(itemChanged(QListWidgetItem*)),
        this, SLOT(slotMetaItemChanged(QListWidgetItem*))
    );

    load();
}

KCMMetaInfo::~KCMMetaInfo()
{
}

void KCMMetaInfo::load()
{
    {
        KConfig config("kmetainformationrc", KConfig::NoGlobals);
        pluginstree->clear();
        KConfigGroup pluginsgroup = config.group("Plugins");
        const KService::List kfmdplugins = KServiceTypeTrader::self()->query("KFileMetaData/Plugin");
        foreach (const KService::Ptr &kfmdplugin, kfmdplugins) {
            const QString key = kfmdplugin->desktopEntryName();
            const bool enable = pluginsgroup.readEntry(key, true);

            QTreeWidgetItem* pluginitem = new QTreeWidgetItem();
            pluginitem->setData(0, Qt::UserRole, key);
            pluginitem->setCheckState(0, enable ? Qt::Checked : Qt::Unchecked);
            pluginitem->setText(1, kfmdplugin->genericName());
            pluginitem->setText(2, kfmdplugin->comment());
            pluginstree->addTopLevelItem(pluginitem);
        }
        pluginstree->resizeColumnToContents(0);
        pluginstree->resizeColumnToContents(1);
        pluginstree->resizeColumnToContents(2);
    }

    loadMetaInfo();

    emit changed(false);
}

void KCMMetaInfo::save()
{
    {
        KConfig config("kmetainformationrc", KConfig::NoGlobals);
        KConfigGroup pluginsgroup = config.group("Plugins");
        for (int i = 0; i < pluginstree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = pluginstree->topLevelItem(i);
            const bool enable = (item->checkState(0) == Qt::Checked);
            const QString key = item->data(0, Qt::UserRole).toString();
            pluginsgroup.writeEntry(key, enable);
        }

        KConfigGroup showgroup = config.group("Show");
        for (int i = 0; i < metainfolist->count(); ++i) {
            QListWidgetItem* item = metainfolist->item(i);
            const bool show = (item->checkState() == Qt::Checked);
            const QString key = item->data(Qt::UserRole).toString();
            showgroup.writeEntry(key, show);
        }
    }

    emit changed(false);

    loadMetaInfo();
}

void KCMMetaInfo::slotPluginItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    emit changed(true);
}

void KCMMetaInfo::slotMetaItemChanged(QListWidgetItem *item)
{
    Q_UNUSED(item);
    emit changed(true);
}

void KCMMetaInfo::loadMetaInfo()
{
    metainfolist->clear();
    KConfig config("kmetainformationrc", KConfig::NoGlobals);
    KConfigGroup showgroup = config.group("Show");
    foreach (const QString &key, KFileMetaInfo::supportedKeys()) {
        QListWidgetItem* item = new QListWidgetItem(KFileMetaInfo::name(key), metainfolist);
        item->setData(Qt::UserRole, key);
        const bool show = showgroup.readEntry(key, true);
        item->setCheckState(show ? Qt::Checked : Qt::Unchecked);
    }
}

#include "moc_kmetainfoconfig.cpp"
