/***************************************************************************
 *   Copyright (C) 2011 by Peter Penz <peter.penz19@gmail.com>             *
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

#include "kfileitemmodelrolesupdater.h"

#include "kfileitemmodel.h"

#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KFileItem>
#include <KGlobal>
#include <KServiceTypeTrader>
#include <KIO/JobUiDelegate>
#include <KIO/PreviewJob>

#include "private/kdirectorycontentscounter.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QElapsedTimer>
#include <QTimer>

#include <algorithm>


// #define KFILEITEMMODELROLESUPDATER_DEBUG

namespace {
    // Maximum time in ms that the KFileItemModelRolesUpdater
    // may perform a blocking operation
    const int MaxBlockTimeout = 200;

    // If the number of items is smaller than ResolveAllItemsLimit,
    // the roles of all items will be resolved.
    const int ResolveAllItemsLimit = 500;

    // Not only the visible area, but up to ReadAheadPages before and after
    // this area will be resolved.
    const int ReadAheadPages = 5;
}

KFileItemModelRolesUpdater::KFileItemModelRolesUpdater(KFileItemModel* model, QObject* parent) :
    QObject(parent),
    m_state(Idle),
    m_previewChangedDuringPausing(false),
    m_iconSizeChangedDuringPausing(false),
    m_rolesChangedDuringPausing(false),
    m_previewShown(false),
    m_clearPreviews(false),
    m_finishedItems(),
    m_model(model),
    m_iconSize(),
    m_firstVisibleIndex(0),
    m_lastVisibleIndex(-1),
    m_maximumVisibleItems(50),
    m_roles(),
    m_resolvableRoles(),
    m_enabledPlugins(),
    m_pendingSortRoleItems(),
    m_pendingIndexes(),
    m_pendingPreviewItems(),
    m_previewJob(),
    m_recentlyChangedItemsTimer(0),
    m_recentlyChangedItems(),
    m_changedItems(),
    m_directoryContentsCounter(0)
{
    Q_ASSERT(model);

    QStringList enabledByDefault;
    const KService::List plugins = KServiceTypeTrader::self()->query(QLatin1String("ThumbCreator"));
    foreach (const KSharedPtr<KService>& service, plugins) {
        const bool enabled = service->property("X-KDE-PluginInfo-EnabledByDefault", QVariant::Bool).toBool();
        if (enabled) {
            enabledByDefault << service->desktopEntryName();
        }
    }

    const KConfigGroup globalConfig(KGlobal::config(), "PreviewSettings");
    m_enabledPlugins = globalConfig.readEntry("Plugins", enabledByDefault);

    connect(m_model, SIGNAL(itemsInserted(KItemRangeList)),
            this,    SLOT(slotItemsInserted(KItemRangeList)));
    connect(m_model, SIGNAL(itemsRemoved(KItemRangeList)),
            this,    SLOT(slotItemsRemoved(KItemRangeList)));
    connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
            this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
    connect(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)),
            this,    SLOT(slotItemsMoved(KItemRange,QList<int>)));
    connect(m_model, SIGNAL(sortRoleChanged(QByteArray,QByteArray)),
            this,    SLOT(slotSortRoleChanged(QByteArray,QByteArray)));

    // Use a timer to prevent that each call of slotItemsChanged() results in a synchronous
    // resolving of the roles. Postpone the resolving until no update has been done for 1 second.
    m_recentlyChangedItemsTimer = new QTimer(this);
    m_recentlyChangedItemsTimer->setInterval(1000);
    m_recentlyChangedItemsTimer->setSingleShot(true);
    connect(m_recentlyChangedItemsTimer, SIGNAL(timeout()), this, SLOT(resolveRecentlyChangedItems()));

    m_resolvableRoles.insert("size");
    m_resolvableRoles.insert("type");

    m_directoryContentsCounter = new KDirectoryContentsCounter(m_model, this);
    connect(m_directoryContentsCounter, SIGNAL(result(QString,int)),
            this,                       SLOT(slotDirectoryContentsCountReceived(QString,int)));
}

KFileItemModelRolesUpdater::~KFileItemModelRolesUpdater()
{
    killPreviewJob();
}

void KFileItemModelRolesUpdater::setIconSize(const QSize& size)
{
    if (size != m_iconSize) {
        m_iconSize = size;
        if (m_state == Paused) {
            m_iconSizeChangedDuringPausing = true;
        } else if (m_previewShown) {
            // An icon size change requires the regenerating of
            // all previews
            m_finishedItems.clear();
            startUpdating();
        }
    }
}

QSize KFileItemModelRolesUpdater::iconSize() const
{
    return m_iconSize;
}

void KFileItemModelRolesUpdater::setVisibleIndexRange(int index, int count)
{
    if (index < 0) {
        index = 0;
    }
    if (count < 0) {
        count = 0;
    }

    if (index == m_firstVisibleIndex && count == m_lastVisibleIndex - m_firstVisibleIndex + 1) {
        // The range has not been changed
        return;
    }

    m_firstVisibleIndex = index;
    m_lastVisibleIndex = qMin(index + count - 1, m_model->count() - 1);

    startUpdating();
}

void KFileItemModelRolesUpdater::setMaximumVisibleItems(int count)
{
    m_maximumVisibleItems = count;
}

void KFileItemModelRolesUpdater::setPreviewsShown(bool show)
{
    if (show == m_previewShown) {
        return;
    }

    m_previewShown = show;
    if (!show) {
        m_clearPreviews = true;
    }

    updateAllPreviews();
}

bool KFileItemModelRolesUpdater::previewsShown() const
{
    return m_previewShown;
}

void KFileItemModelRolesUpdater::setEnabledPlugins(const QStringList& list)
{
    if (m_enabledPlugins != list) {
        m_enabledPlugins = list;
        if (m_previewShown) {
            updateAllPreviews();
        }
    }
}

void KFileItemModelRolesUpdater::setPaused(bool paused)
{
    if (paused == (m_state == Paused)) {
        return;
    }

    if (paused) {
        m_state = Paused;
        killPreviewJob();
    } else {
        const bool updatePreviews = (m_iconSizeChangedDuringPausing && m_previewShown) ||
                                    m_previewChangedDuringPausing;
        const bool resolveAll = updatePreviews || m_rolesChangedDuringPausing;
        if (resolveAll) {
            m_finishedItems.clear();
        }

        m_iconSizeChangedDuringPausing = false;
        m_previewChangedDuringPausing = false;
        m_rolesChangedDuringPausing = false;

        if (!m_pendingSortRoleItems.isEmpty()) {
            m_state = ResolvingSortRole;
            resolveNextSortRole();
        } else {
            m_state = Idle;
        }

        startUpdating();
    }
}

void KFileItemModelRolesUpdater::setRoles(const QSet<QByteArray>& roles)
{
    if (m_roles != roles) {
        m_roles = roles;


        if (m_state == Paused) {
            m_rolesChangedDuringPausing = true;
        } else {
            startUpdating();
        }
    }
}

QSet<QByteArray> KFileItemModelRolesUpdater::roles() const
{
    return m_roles;
}

bool KFileItemModelRolesUpdater::isPaused() const
{
    return m_state == Paused;
}

QStringList KFileItemModelRolesUpdater::enabledPlugins() const
{
    return m_enabledPlugins;
}

void KFileItemModelRolesUpdater::slotItemsInserted(const KItemRangeList& itemRanges)
{
    QElapsedTimer timer;
    timer.start();

    // Determine the sort role synchronously for as many items as possible.
    if (m_resolvableRoles.contains(m_model->sortRole())) {
        int insertedCount = 0;
        foreach (const KItemRange& range, itemRanges) {
            const int lastIndex = insertedCount + range.index + range.count - 1;
            for (int i = insertedCount + range.index; i <= lastIndex; ++i) {
                if (timer.elapsed() < MaxBlockTimeout) {
                    applySortRole(i);
                } else {
                    m_pendingSortRoleItems.insert(m_model->fileItem(i));
                }
            }
            insertedCount += range.count;
        }

        applySortProgressToModel();

        // If there are still items whose sort role is unknown, check if the
        // asynchronous determination of the sort role is already in progress,
        // and start it if that is not the case.
        if (!m_pendingSortRoleItems.isEmpty() && m_state != ResolvingSortRole) {
            killPreviewJob();
            m_state = ResolvingSortRole;
            resolveNextSortRole();
        }
    }

    startUpdating();
}

void KFileItemModelRolesUpdater::slotItemsRemoved(const KItemRangeList& itemRanges)
{
    Q_UNUSED(itemRanges);

    const bool allItemsRemoved = (m_model->count() == 0);


    if (allItemsRemoved) {
        m_state = Idle;

        m_finishedItems.clear();
        m_pendingSortRoleItems.clear();
        m_pendingIndexes.clear();
        m_pendingPreviewItems.clear();
        m_recentlyChangedItems.clear();
        m_recentlyChangedItemsTimer->stop();
        m_changedItems.clear();

        killPreviewJob();
    } else {
        // Only remove the items from m_finishedItems. They will be removed
        // from the other sets later on.
        QSet<KFileItem>::iterator it = m_finishedItems.begin();
        while (it != m_finishedItems.end()) {
            if (m_model->index(*it) < 0) {
                it = m_finishedItems.erase(it);
            } else {
                ++it;
            }
        }

        // The visible items might have changed.
        startUpdating();
    }
}

void KFileItemModelRolesUpdater::slotItemsMoved(const KItemRange& itemRange, QList<int> movedToIndexes)
{
    Q_UNUSED(itemRange);
    Q_UNUSED(movedToIndexes);

    // The visible items might have changed.
    startUpdating();
}

void KFileItemModelRolesUpdater::slotItemsChanged(const KItemRangeList& itemRanges,
                                                  const QSet<QByteArray>& roles)
{
    Q_UNUSED(roles);

    // Find out if slotItemsChanged() has been done recently. If that is the
    // case, resolving the roles is postponed until a timer has exceeded
    // to prevent expensive repeated updates if files are updated frequently.
    const bool itemsChangedRecently = m_recentlyChangedItemsTimer->isActive();

    QSet<KFileItem>& targetSet = itemsChangedRecently ? m_recentlyChangedItems : m_changedItems;

    foreach (const KItemRange& itemRange, itemRanges) {
        int index = itemRange.index;
        for (int count = itemRange.count; count > 0; --count) {
            const KFileItem item = m_model->fileItem(index);
            targetSet.insert(item);
            ++index;
        }
    }

    m_recentlyChangedItemsTimer->start();

    if (!itemsChangedRecently) {
        updateChangedItems();
    }
}

void KFileItemModelRolesUpdater::slotSortRoleChanged(const QByteArray& current,
                                                     const QByteArray& previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);

    if (m_resolvableRoles.contains(current)) {
        m_pendingSortRoleItems.clear();
        m_finishedItems.clear();

        const int count = m_model->count();
        QElapsedTimer timer;
        timer.start();

        // Determine the sort role synchronously for as many items as possible.
        for (int index = 0; index < count; ++index) {
            if (timer.elapsed() < MaxBlockTimeout) {
                applySortRole(index);
            } else {
                m_pendingSortRoleItems.insert(m_model->fileItem(index));
            }
        }

        applySortProgressToModel();

        if (!m_pendingSortRoleItems.isEmpty()) {
            // Trigger the asynchronous determination of the sort role.
            killPreviewJob();
            m_state = ResolvingSortRole;
            resolveNextSortRole();
        }
    } else {
        m_state = Idle;
        m_pendingSortRoleItems.clear();
        applySortProgressToModel();
    }
}

void KFileItemModelRolesUpdater::slotGotPreview(const KFileItem& item, const QPixmap& pixmap)
{
    if (m_state != PreviewJobRunning) {
        return;
    }

    m_changedItems.remove(item);

    const int index = m_model->index(item);
    if (index < 0) {
        return;
    }

    QPixmap scaledPixmap = pixmap.scaled(m_iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);;

    QHash<QByteArray, QVariant> data = rolesData(item);

    // TODO: version plugin (KVersionControlPlugin) overlays

    data.insert("iconPixmap", scaledPixmap);

    disconnect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
               this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
    m_model->setData(index, data);
    connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
            this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));

    m_finishedItems.insert(item);
}

void KFileItemModelRolesUpdater::slotPreviewFailed(const KFileItem& item)
{
    if (m_state != PreviewJobRunning) {
        return;
    }

    m_changedItems.remove(item);

    const int index = m_model->index(item);
    if (index >= 0) {
        QHash<QByteArray, QVariant> data;
        data.insert("iconPixmap", QPixmap());

        disconnect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                   this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
        m_model->setData(index, data);
        connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));

        applyResolvedRoles(index, ResolveAll);
        m_finishedItems.insert(item);
    }
}

void KFileItemModelRolesUpdater::slotPreviewJobFinished()
{
    m_previewJob = 0;

    if (m_state != PreviewJobRunning) {
        return;
    }

    m_state = Idle;

    if (!m_pendingPreviewItems.isEmpty()) {
        startPreviewJob();
    } else {
        if (!m_changedItems.isEmpty()) {
            updateChangedItems();
        }
    }
}

void KFileItemModelRolesUpdater::resolveNextSortRole()
{
    if (m_state != ResolvingSortRole) {
        return;
    }

    QSet<KFileItem>::iterator it = m_pendingSortRoleItems.begin();
    while (it != m_pendingSortRoleItems.end()) {
        const KFileItem item = *it;
        const int index = m_model->index(item);

        // Continue if the sort role has already been determined for the
        // item, and the item has not been changed recently.
        if (!m_changedItems.contains(item) && m_model->data(index).contains(m_model->sortRole())) {
            it = m_pendingSortRoleItems.erase(it);
            continue;
        }

        applySortRole(index);
        m_pendingSortRoleItems.erase(it);
        break;
    }

    if (!m_pendingSortRoleItems.isEmpty()) {
        applySortProgressToModel();
        QTimer::singleShot(0, this, SLOT(resolveNextSortRole()));
    } else {
        m_state = Idle;

        // Prevent that we try to update the items twice.
        disconnect(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)),
                   this,    SLOT(slotItemsMoved(KItemRange,QList<int>)));
        applySortProgressToModel();
        connect(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)),
                this,    SLOT(slotItemsMoved(KItemRange,QList<int>)));
        startUpdating();
    }
}

void KFileItemModelRolesUpdater::resolveNextPendingRoles()
{
    if (m_state != ResolvingAllRoles) {
        return;
    }

    while (!m_pendingIndexes.isEmpty()) {
        const int index = m_pendingIndexes.takeFirst();
        const KFileItem item = m_model->fileItem(index);

        if (m_finishedItems.contains(item)) {
            continue;
        }

        applyResolvedRoles(index, ResolveAll);
        m_finishedItems.insert(item);
        m_changedItems.remove(item);
        break;
    }

    if (!m_pendingIndexes.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(resolveNextPendingRoles()));
    } else {
        m_state = Idle;

        if (m_clearPreviews) {
            // Only go through the list if there are items which might still have previews.
            if (m_finishedItems.count() != m_model->count()) {
                QHash<QByteArray, QVariant> data;
                data.insert("iconPixmap", QPixmap());

                disconnect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                           this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
                for (int index = 0; index <= m_model->count(); ++index) {
                    if (m_model->data(index).contains("iconPixmap")) {
                        m_model->setData(index, data);
                    }
                }
                connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                        this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));

            }
            m_clearPreviews = false;
        }

        if (!m_changedItems.isEmpty()) {
            updateChangedItems();
        }
    }
}

void KFileItemModelRolesUpdater::resolveRecentlyChangedItems()
{
    m_changedItems += m_recentlyChangedItems;
    m_recentlyChangedItems.clear();
    updateChangedItems();
}

void KFileItemModelRolesUpdater::slotDirectoryContentsCountReceived(const QString& path, int count)
{
    const bool getSizeRole = m_roles.contains("size");

    if (getSizeRole) {
        const int index = m_model->index(KUrl(path));
        if (index >= 0) {
            QHash<QByteArray, QVariant> data;

            if (getSizeRole) {
                data.insert("size", count);
            }

            disconnect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                       this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
            m_model->setData(index, data);
            connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                    this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
        }
    }
}

void KFileItemModelRolesUpdater::startUpdating()
{
    if (m_state == Paused) {
        return;
    }

    if (m_finishedItems.count() == m_model->count()) {
        // All roles have been resolved already.
        m_state = Idle;
        return;
    }

    // Terminate all updates that are currently active.
    killPreviewJob();
    m_pendingIndexes.clear();

    QElapsedTimer timer;
    timer.start();

    // Determine the icons for the visible items synchronously.
    updateVisibleIcons();

    // A detailed update of the items in and near the visible area
    // only makes sense if sorting is finished.
    if (m_state == ResolvingSortRole) {
        return;
    }

    // Start the preview job or the asynchronous resolving of all roles.
    QList<int> indexes = indexesToResolve();

    if (m_previewShown) {
        m_pendingPreviewItems.clear();
        m_pendingPreviewItems.reserve(indexes.count());

        foreach (int index, indexes) {
            const KFileItem item = m_model->fileItem(index);
            if (!m_finishedItems.contains(item)) {
                m_pendingPreviewItems.append(item);
            }
        }

        startPreviewJob();
    } else {
        m_pendingIndexes = indexes;
        // Trigger the asynchronous resolving of all roles.
        m_state = ResolvingAllRoles;
        QTimer::singleShot(0, this, SLOT(resolveNextPendingRoles()));
    }
}

void KFileItemModelRolesUpdater::updateVisibleIcons()
{
    int lastVisibleIndex = m_lastVisibleIndex;
    if (lastVisibleIndex <= 0) {
        // Guess a reasonable value for the last visible index if the view
        // has not told us about the real value yet.
        lastVisibleIndex = qMin(m_firstVisibleIndex + m_maximumVisibleItems, m_model->count() - 1);
        if (lastVisibleIndex <= 0) {
            lastVisibleIndex = qMin(200, m_model->count() - 1);
        }
    }

    QElapsedTimer timer;
    timer.start();

    // Try to determine the final icons for all visible items.
    int index;
    for (index = m_firstVisibleIndex; index <= lastVisibleIndex && timer.elapsed() < MaxBlockTimeout; ++index) {
        applyResolvedRoles(index, ResolveFast);
    }

    // KFileItemListView::initializeItemListWidget(KItemListWidget*) will load
    // preliminary icons (i.e., without mime type determination) for the
    // remaining items.
}

void KFileItemModelRolesUpdater::startPreviewJob()
{
    m_state = PreviewJobRunning;

    if (m_pendingPreviewItems.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(slotPreviewJobFinished()));
        return;
    }

    // PreviewJob internally caches items always with the size of
    // 128 x 128 pixels or 256 x 256 pixels. A (slow) downscaling is done
    // by PreviewJob if a smaller size is requested. For images KFileItemModelRolesUpdater must
    // do a downscaling anyhow because of the frame, so in this case only the provided
    // cache sizes are requested.
    const QSize cacheSize = QSize(128, 128);

    // KIO::filePreview() will request the MIME-type of all passed items, which (in the
    // worst case) might block the application for several seconds. To prevent such
    // a blocking, we only pass items with known mime type to the preview job.
    const int count = m_pendingPreviewItems.count();
    KFileItemList itemSubSet;
    itemSubSet.reserve(count);

    if (!m_pendingPreviewItems.first().mimeTypePtr().isNull()) {
        // Some mime types are known already, probably because they were
        // determined when loading the icons for the visible items. Start
        // a preview job for all items at the beginning of the list which
        // have a known mime type.
        do {
            itemSubSet.append(m_pendingPreviewItems.takeFirst());
        } while (!m_pendingPreviewItems.isEmpty() && !m_pendingPreviewItems.first().mimeTypePtr().isNull());
    } else {
        // Determine mime types for MaxBlockTimeout ms, and start a preview
        // job for the corresponding items.
        QElapsedTimer timer;
        timer.start();

        do {
            const KFileItem item = m_pendingPreviewItems.takeFirst();
            itemSubSet.append(item);
        } while (!m_pendingPreviewItems.isEmpty() && timer.elapsed() < MaxBlockTimeout);
    }

    KIO::PreviewJob* job = new KIO::PreviewJob(itemSubSet, cacheSize, &m_enabledPlugins);

    job->setIgnoreMaximumSize(itemSubSet.first().isLocalFile());
    if (job->ui()) {
        job->ui()->setWindow(qApp->activeWindow());
    }

    connect(job,  SIGNAL(gotPreview(KFileItem,QPixmap)),
            this, SLOT(slotGotPreview(KFileItem,QPixmap)));
    connect(job,  SIGNAL(failed(KFileItem)),
            this, SLOT(slotPreviewFailed(KFileItem)));
    connect(job,  SIGNAL(finished(KJob*)),
            this, SLOT(slotPreviewJobFinished()));

    m_previewJob = job;
}

void KFileItemModelRolesUpdater::updateChangedItems()
{
    if (m_state == Paused) {
        return;
    }

    if (m_changedItems.isEmpty()) {
        return;
    }

    m_finishedItems -= m_changedItems;

    if (m_resolvableRoles.contains(m_model->sortRole())) {
        m_pendingSortRoleItems += m_changedItems;

        if (m_state != ResolvingSortRole) {
            // Stop the preview job if necessary, and trigger the
            // asynchronous determination of the sort role.
            killPreviewJob();
            m_state = ResolvingSortRole;
            QTimer::singleShot(0, this, SLOT(resolveNextSortRole()));
        }

        return;
    }

    QList<int> visibleChangedIndexes;
    QList<int> invisibleChangedIndexes;

    QMutableSetIterator<KFileItem> changedItemsIter(m_changedItems);
    while (changedItemsIter.hasNext()) {
        const KFileItem& item = changedItemsIter.next();
        const int index = m_model->index(item);

        if (index < 0) {
            changedItemsIter.remove();
            continue;
        }

        if (index >= m_firstVisibleIndex && index <= m_lastVisibleIndex) {
            visibleChangedIndexes.append(index);
        } else {
            invisibleChangedIndexes.append(index);
        }
    }

    std::sort(visibleChangedIndexes.begin(), visibleChangedIndexes.end());

    if (m_previewShown) {
        foreach (int index, visibleChangedIndexes) {
            m_pendingPreviewItems.append(m_model->fileItem(index));
        }

        foreach (int index, invisibleChangedIndexes) {
            m_pendingPreviewItems.append(m_model->fileItem(index));
        }

        if (!m_previewJob) {
            startPreviewJob();
        }
    } else {
        const bool resolvingInProgress = !m_pendingIndexes.isEmpty();
        m_pendingIndexes = visibleChangedIndexes + m_pendingIndexes + invisibleChangedIndexes;
        if (!resolvingInProgress) {
            // Trigger the asynchronous resolving of the changed roles.
            m_state = ResolvingAllRoles;
            QTimer::singleShot(0, this, SLOT(resolveNextPendingRoles()));
        }
    }
}

void KFileItemModelRolesUpdater::applySortRole(int index)
{
    QHash<QByteArray, QVariant> data;
    const KFileItem item = m_model->fileItem(index);

    if (m_model->sortRole() == "type") {
        data.insert("type", item.mimeComment());
    } else if (m_model->sortRole() == "size" && item.isLocalFile() && item.isDir()) {
        const QString path = item.localPath();
        data.insert("size", m_directoryContentsCounter->countDirectoryContentsSynchronously(path));
    } else {
        // Probably the sort role is a baloo role - just determine all roles.
        data = rolesData(item);
    }

    disconnect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
               this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
    m_model->setData(index, data);
    connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
            this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
}

void KFileItemModelRolesUpdater::applySortProgressToModel()
{
    // Inform the model about the progress of the resolved items,
    // so that it can give an indication when the sorting has been finished.
    const int resolvedCount = m_model->count() - m_pendingSortRoleItems.count();
    m_model->emitSortProgress(resolvedCount);
}

bool KFileItemModelRolesUpdater::applyResolvedRoles(int index, ResolveHint hint)
{
    const KFileItem item = m_model->fileItem(index);
    const bool resolveAll = (hint == ResolveAll);

    bool iconChanged = false;
    if (item.mimeTypePtr().isNull()) {
        iconChanged = true;
    } else if (!m_model->data(index).contains("iconName")) {
        iconChanged = true;
    }

    if (iconChanged || resolveAll || m_clearPreviews) {
        if (index < 0) {
            return false;
        }

        QHash<QByteArray, QVariant> data;
        if (resolveAll) {
            data = rolesData(item);
        }

        data.insert("iconName", item.iconName());

        if (m_clearPreviews) {
            data.insert("iconPixmap", QPixmap());
        }

        disconnect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                   this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
        m_model->setData(index, data);
        connect(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)),
                this,    SLOT(slotItemsChanged(KItemRangeList,QSet<QByteArray>)));
        return true;
    }

    return false;
}

QHash<QByteArray, QVariant> KFileItemModelRolesUpdater::rolesData(const KFileItem& item)
{
    QHash<QByteArray, QVariant> data;

    const bool getSizeRole = m_roles.contains("size");

    if (getSizeRole && item.isDir()) {
        if (item.isLocalFile()) {
            // Tell m_directoryContentsCounter that we want to count the items
            // inside the directory. The result will be received in slotDirectoryContentsCountReceived.
            const QString path = item.localPath();
            m_directoryContentsCounter->addDirectory(path);
        } else if (getSizeRole) {
            data.insert("size", -1); // -1 indicates an unknown number of items
        }
    }

    if (m_roles.contains("type")) {
        data.insert("type", item.mimeComment());
    }

    data.insert("iconOverlays", item.overlays());

    return data;
}

void KFileItemModelRolesUpdater::updateAllPreviews()
{
    if (m_state == Paused) {
        m_previewChangedDuringPausing = true;
    } else {
        m_finishedItems.clear();
        startUpdating();
    }
}

void KFileItemModelRolesUpdater::killPreviewJob()
{
    if (m_previewJob) {
        disconnect(m_previewJob,  SIGNAL(gotPreview(KFileItem,QPixmap)),
                   this, SLOT(slotGotPreview(KFileItem,QPixmap)));
        disconnect(m_previewJob,  SIGNAL(failed(KFileItem)),
                   this, SLOT(slotPreviewFailed(KFileItem)));
        disconnect(m_previewJob,  SIGNAL(finished(KJob*)),
                   this, SLOT(slotPreviewJobFinished()));
        m_previewJob->kill();
        m_previewJob = 0;
        m_pendingPreviewItems.clear();
    }
}

QList<int> KFileItemModelRolesUpdater::indexesToResolve() const
{
    const int count = m_model->count();

    QList<int> result;
    result.reserve(ResolveAllItemsLimit);

    // Add visible items.
    for (int i = m_firstVisibleIndex; i <= m_lastVisibleIndex; ++i) {
        result.append(i);
    }

    // We need a reasonable upper limit for number of items to resolve after
    // and before the visible range. m_maximumVisibleItems can be quite large
    // when using Compace View.
    const int readAheadItems = qMin(ReadAheadPages * m_maximumVisibleItems, ResolveAllItemsLimit / 2);

    // Add items after the visible range.
    const int endExtendedVisibleRange = qMin(m_lastVisibleIndex + readAheadItems, count - 1);
    for (int i = m_lastVisibleIndex + 1; i <= endExtendedVisibleRange; ++i) {
        result.append(i);
    }

    // Add items before the visible range in reverse order.
    const int beginExtendedVisibleRange = qMax(0, m_firstVisibleIndex - readAheadItems);
    for (int i = m_firstVisibleIndex - 1; i >= beginExtendedVisibleRange; --i) {
        result.append(i);
    }

    // Add items on the last page.
    const int beginLastPage = qMax(qMin(endExtendedVisibleRange + 1, count - 1), count - m_maximumVisibleItems);
    for (int i = beginLastPage; i < count; ++i) {
        result.append(i);
    }

    // Add items on the first page.
    const int endFirstPage = qMin(qMax(beginExtendedVisibleRange - 1, 0), m_maximumVisibleItems);
    for (int i = 0; i <= endFirstPage; ++i) {
        result.append(i);
    }

    // Continue adding items until ResolveAllItemsLimit is reached.
    int remainingItems = ResolveAllItemsLimit - result.count();

    for (int i = endExtendedVisibleRange + 1; i < beginLastPage && remainingItems > 0; ++i) {
        result.append(i);
        --remainingItems;
    }

    for (int i = beginExtendedVisibleRange - 1; i > endFirstPage && remainingItems > 0; --i) {
        result.append(i);
        --remainingItems;
    }

    return result;
}

#include "moc_kfileitemmodelrolesupdater.cpp"
