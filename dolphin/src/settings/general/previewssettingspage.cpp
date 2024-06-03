/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz <peter.penz19@gmail.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "previewssettingspage.h"

#include "dolphin_generalsettings.h"
#include "configurepreviewplugindialog.h"

#include <KConfigGroup>
#include <KDialog>
#include <KGlobal>
#include <KLocale>
#include <KNumInput>
#include <KServiceTypeTrader>
#include <KService>

#include <settings/serviceitemdelegate.h>
#include <settings/servicemodel.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QScrollBar>
#include <QtGui/qevent.h>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

// default settings
// NOTE: keep in sync with:
// kdelibs/kio/kio/previewjob.cpp
// kde-workspace/kioslave/thumbnail/thumbnail.h
enum PreviewDefaults {
    MaxLocalSize = 20, // 20 MB
    MaxRemoteSize = 5  // 5 MB
};

PreviewsSettingsPage::PreviewsSettingsPage(QWidget* parent) :
    SettingsPageBase(parent),
    m_initialized(false),
    m_listView(0),
    m_enabledPreviewPlugins(),
    m_localFileSizeBox(0),
    m_remoteFileSizeBox(0)
{
    QVBoxLayout* topLayout = new QVBoxLayout(this);

    QLabel* showPreviewsLabel = new QLabel(i18nc("@title:group", "Show previews for:"), this);

    m_listView = new QListView(this);

    ServiceItemDelegate* delegate = new ServiceItemDelegate(m_listView, m_listView);
    connect(delegate, SIGNAL(requestServiceConfiguration(QModelIndex)),
            this, SLOT(configureService(QModelIndex)));

    ServiceModel* serviceModel = new ServiceModel(this);
    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(serviceModel);
    proxyModel->setSortRole(Qt::DisplayRole);

    m_listView->setModel(proxyModel);
    m_listView->setItemDelegate(delegate);
    m_listView->setVerticalScrollMode(QListView::ScrollPerPixel);

    QLabel* localFileSizeLabel = new QLabel(i18nc("@label", "Skip previews for local files above:"), this);
    QLabel* remoteFileSizeLabel = new QLabel(i18nc("@label", "Skip previews for remote files above:"), this);

    m_localFileSizeBox = new KIntNumInput(this);
    m_localFileSizeBox->setSingleStep(1);
    m_localFileSizeBox->setSuffix(QLatin1String(" MB"));
    m_localFileSizeBox->setRange(0, 9999999); /* MB */

    m_remoteFileSizeBox = new KIntNumInput(this);
    m_remoteFileSizeBox->setSingleStep(1);
    m_remoteFileSizeBox->setSuffix(QLatin1String(" MB"));
    m_remoteFileSizeBox->setRange(0, 9999999); /* MB */

    QGridLayout* fileSizeLayout = new QGridLayout(this);
    fileSizeLayout->addWidget(localFileSizeLabel, 0, 0);
    fileSizeLayout->addWidget(m_localFileSizeBox, 0, 1, Qt::AlignRight);
    fileSizeLayout->addWidget(remoteFileSizeLabel, 1, 0);
    fileSizeLayout->addWidget(m_remoteFileSizeBox, 1, 1, Qt::AlignRight);

    topLayout->addSpacing(KDialog::spacingHint());
    topLayout->addWidget(showPreviewsLabel);
    topLayout->addWidget(m_listView);
    topLayout->addLayout(fileSizeLayout);

    loadSettings();

    connect(m_listView, SIGNAL(clicked(QModelIndex)), this, SIGNAL(changed()));
    connect(m_localFileSizeBox, SIGNAL(valueChanged(int)), this, SIGNAL(changed()));
    connect(m_remoteFileSizeBox, SIGNAL(valueChanged(int)), this, SIGNAL(changed()));
}

PreviewsSettingsPage::~PreviewsSettingsPage()
{
}

void PreviewsSettingsPage::applySettings()
{
    const QAbstractItemModel* model = m_listView->model();
    const int rowCount = model->rowCount();
    if (rowCount > 0) {
        m_enabledPreviewPlugins.clear();
        for (int i = 0; i < rowCount; ++i) {
            const QModelIndex index = model->index(i, 0);
            const bool checked = model->data(index, Qt::CheckStateRole).toBool();
            if (checked) {
                const QString enabledPlugin = model->data(index, Qt::UserRole).toString();
                m_enabledPreviewPlugins.append(enabledPlugin);
            }
        }
    }

    KConfigGroup globalConfig(KGlobal::config(), QLatin1String("PreviewSettings"));
    globalConfig.writeEntry("Plugins", m_enabledPreviewPlugins,
                            KConfigBase::Normal | KConfigBase::Global);

    const qulonglong maximumLocalSize = static_cast<qulonglong>(m_localFileSizeBox->value()) * 1024 * 1024;
    globalConfig.writeEntry("MaximumSize",
                            maximumLocalSize,
                            KConfigBase::Normal | KConfigBase::Global);

    const qulonglong maximumRemoteSize = static_cast<qulonglong>(m_remoteFileSizeBox->value()) * 1024 * 1024;
    globalConfig.writeEntry("MaximumRemoteSize",
                            maximumRemoteSize,
                            KConfigBase::Normal | KConfigBase::Global);
    globalConfig.sync();
}

void PreviewsSettingsPage::restoreDefaults()
{
    m_localFileSizeBox->setValue(PreviewDefaults::MaxLocalSize);
    m_remoteFileSizeBox->setValue(PreviewDefaults::MaxRemoteSize);
}

void PreviewsSettingsPage::showEvent(QShowEvent* event)
{
    if (!event->spontaneous() && !m_initialized) {
        loadPreviewPlugins();
        m_initialized = true;
    }
    SettingsPageBase::showEvent(event);
}

void PreviewsSettingsPage::configureService(const QModelIndex& index)
{
    const QAbstractItemModel* model = index.model();
    const QString pluginName = model->data(index).toString();
    const QString desktopEntryName = model->data(index, ServiceModel::DesktopEntryNameRole).toString();

    ConfigurePreviewPluginDialog* dialog = new ConfigurePreviewPluginDialog(pluginName, desktopEntryName, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void PreviewsSettingsPage::loadPreviewPlugins()
{
    QAbstractItemModel* model = m_listView->model();

    const KService::List plugins = KServiceTypeTrader::self()->query(QLatin1String("ThumbCreator"));
    foreach (const KSharedPtr<KService>& service, plugins) {
        const bool configurable = service->property("Configurable", QVariant::Bool).toBool();
        const bool show = m_enabledPreviewPlugins.contains(service->desktopEntryName());

        model->insertRow(0);
        const QModelIndex index = model->index(0, 0);
        model->setData(index, show, Qt::CheckStateRole);
        model->setData(index, configurable, ServiceModel::ConfigurableRole);
        model->setData(index, service->name(), Qt::DisplayRole);
        model->setData(index, service->desktopEntryName(), ServiceModel::DesktopEntryNameRole);
    }

    model->sort(Qt::DisplayRole);
}

void PreviewsSettingsPage::loadSettings()
{
    KConfigGroup globalConfig(KGlobal::config(), "PreviewSettings");

    QStringList enabledByDefault;
    const KService::List plugins = KServiceTypeTrader::self()->query(QLatin1String("ThumbCreator"));
    foreach (const KSharedPtr<KService>& service, plugins) {
        const bool enabled = service->property("X-KDE-PluginInfo-EnabledByDefault", QVariant::Bool).toBool();
        if (enabled) {
            enabledByDefault << service->desktopEntryName();
        }
    }

    m_enabledPreviewPlugins = globalConfig.readEntry("Plugins", enabledByDefault);

    const qulonglong defaultLocalPreview = static_cast<qulonglong>(PreviewDefaults::MaxLocalSize) * 1024 * 1024;
    const qulonglong defaultRemotePreview = static_cast<qulonglong>(PreviewDefaults::MaxRemoteSize) * 1024 * 1024;
    const qulonglong maxLocalByteSize = globalConfig.readEntry("MaximumSize", defaultLocalPreview);
    const qulonglong maxRemoteByteSize = globalConfig.readEntry("MaximumRemoteSize", defaultRemotePreview);
    const int maxLocalMByteSize = maxLocalByteSize / (1024 * 1024);
    const int maxRemoteMByteSize = maxRemoteByteSize / (1024 * 1024);
    m_localFileSizeBox->setValue(maxLocalMByteSize);
    m_remoteFileSizeBox->setValue(maxRemoteMByteSize);
}

#include "moc_previewssettingspage.cpp"
