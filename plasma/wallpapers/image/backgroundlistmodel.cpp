#ifndef BACKGROUNDLISTMODEL_CPP
#define BACKGROUNDLISTMODEL_CPP
/*
  Copyright (c) 2007 Paolo Capriotti <p.capriotti@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
*/

#include "backgroundlistmodel.h"

#include <QFile>
#include <QDir>
#include <QImageReader>

#include <KDebug>
#include <KGlobal>
#include <KIO/PreviewJob>
#include <KProgressDialog>
#include <KStandardDirs>
#include <KImageIO>

#include <Plasma/Package>
#include <Plasma/PackageStructure>

#include "backgrounddelegate.h"
#include "image.h"

BackgroundListModel::BackgroundListModel(Image *listener, QObject *parent)
    : QAbstractListModel(parent),
      m_structureParent(listener),
      m_size(0,0),
      m_resizeMethod(Plasma::Wallpaper::ScaledResize)
{
    connect(&m_dirwatch, SIGNAL(dirty(QString)), this, SLOT(reload()));
    m_previewUnavailablePix.fill(Qt::transparent);
    //m_previewUnavailablePix = KIcon("unknown").pixmap(m_previewUnavailablePix.size());
}

BackgroundListModel::~BackgroundListModel()
{
    qDeleteAll(m_packages);
}

void BackgroundListModel::reload()
{
    reload(QStringList());
}

void BackgroundListModel::reload(const QStringList &selected)
{
    if (!m_packages.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_packages.count() - 1);
        qDeleteAll(m_packages);
        m_packages.clear();
        m_sizeCache.clear();
        m_previews.clear();
        endRemoveRows();
    }

    if (!m_structureParent) {
        return;
    }

    if (!selected.isEmpty()) {
        processPaths(selected);
    }

    const QStringList dirs = KGlobal::dirs()->findDirs("wallpaper", "");
    kDebug() << "going looking in" << dirs;
    
    // add wallpaper dirs to dirwatch (recursively)
    foreach (const QString &dir, dirs) {
        m_dirwatch.addDir(dir, true);
    }

    BackgroundFinder *finder = new BackgroundFinder(m_structureParent.data(), dirs);
    connect(finder, SIGNAL(backgroundsFound(QStringList)), this, SLOT(processPaths(QStringList)));
    finder->start();
}

void BackgroundListModel::processPaths(const QStringList &paths)
{
    if (!m_structureParent) {
        return;
    }

    QList<Plasma::Package *> newPackages;
    foreach (const QString &path, paths) {
        if (!contains(path) && QFileInfo(path).exists()) {
            Plasma::PackageStructure::Ptr structure = Plasma::Wallpaper::packageStructure(m_structureParent.data());
            Plasma::Package *package  = new Plasma::Package(path, structure);
            if (package->isValid()) {
                newPackages << package;
            } else {
                delete package;
            }
        }
    }

    if (!newPackages.isEmpty()) {
        const int start = rowCount();
        beginInsertRows(QModelIndex(), start, start + newPackages.size());
        m_packages.append(newPackages);
        endInsertRows();
    }
    //kDebug() << t.elapsed();
}

void BackgroundListModel::addBackground(const QString& path)
{
    if (!m_structureParent || !contains(path)) {
        if (!m_dirwatch.contains(path)) {
            m_dirwatch.addFile(path);
        }
        beginInsertRows(QModelIndex(), 0, 0);
        Plasma::PackageStructure::Ptr structure = Plasma::Wallpaper::packageStructure(m_structureParent.data());
        Plasma::Package *pkg = new Plasma::Package(path, structure);
        m_packages.prepend(pkg);
        endInsertRows();
    }
}

QModelIndex BackgroundListModel::indexOf(const QString &path) const
{
    for (int i = 0; i < m_packages.size(); i++) {
        // packages will end with a '/', but the path passed in may not
        QString package = m_packages[i]->path();
        if (package.at(package.length() - 1) == '/') {
            package.truncate(package.length() - 1);
        }

        if (path.startsWith(package)) {
            // FIXME: ugly hack to make a difference between local files in the same dir
            // package->path does not contain the actual file name
            if ((!m_packages[i]->structure()->contentsPrefixPaths().isEmpty()) ||
                (path == m_packages[i]->filePath("preferred"))) {
                return index(i, 0);
            }
        }
    }
    return QModelIndex();
}

bool BackgroundListModel::contains(const QString &path) const
{
    return indexOf(path).isValid();
}

int BackgroundListModel::rowCount(const QModelIndex &) const
{
    return m_packages.size();
}

QSize BackgroundListModel::bestSize(Plasma::Package *package) const
{
    if (m_sizeCache.contains(package)) {
        return m_sizeCache.value(package);
    }

    const QString image = package->filePath("preferred");
    if (image.isEmpty()) {
        return QSize();
    }

    QImageReader imagereader(image);
    QSize size = imagereader.size();
    // backup solution if image handler does not provide size option
    if (size.width() == 0 || size.height() == 0) {
        size = imagereader.read().size();
    }
    m_sizeCache.insert(package, size);
    if (m_structureParent) {
        QModelIndex index = indexOf(image);
        if (index.isValid()) {
            m_structureParent.data()->updateScreenshot(index);
        }
    }

    return size;
}

QVariant BackgroundListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= m_packages.size()) {
        return QVariant();
    }

    Plasma::Package *b = package(index.row());
    if (!b) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole: {
        QString title = b->metadata().name();

        if (title.isEmpty()) {
            return QFileInfo(b->filePath("preferred")).completeBaseName();
        }

        return title;
    }

    case BackgroundDelegate::ScreenshotRole: {
        if (m_previews.contains(b)) {
            return m_previews.value(b);
        }

        KUrl file(b->filePath("preferred"));
        if (!m_previewJobs.contains(file) && file.isValid()) {
            KFileItemList list;
            list.append(KFileItem(file));
            KIO::PreviewJob* job = KIO::filePreview(list,
                                                    QSize(BackgroundDelegate::SCREENSHOT_SIZE,
                                                    BackgroundDelegate::SCREENSHOT_SIZE/1.6));
            job->setIgnoreMaximumSize(true);
            connect(job, SIGNAL(gotPreview(KFileItem,QPixmap)),
                    this, SLOT(showPreview(KFileItem,QPixmap)));
            connect(job, SIGNAL(failed(KFileItem)),
                    this, SLOT(previewFailed(KFileItem)));
            const_cast<BackgroundListModel *>(this)->m_previewJobs.insert(file, QPersistentModelIndex(index));
        }

        const_cast<BackgroundListModel *>(this)->m_previews.insert(b, m_previewUnavailablePix);
        return m_previewUnavailablePix;
    }

    case BackgroundDelegate::AuthorRole:
        return b->metadata().author();

    case BackgroundDelegate::ResolutionRole:{
        QSize size = bestSize(b);

        if (size.isValid()) {
            return QString("%1x%2").arg(size.width()).arg(size.height());
        }

        return QString();
    }

    default:
        return QVariant();
    }
}

void BackgroundListModel::showPreview(const KFileItem &item, const QPixmap &preview)
{
    if (!m_structureParent) {
        return;
    }

    QPersistentModelIndex index = m_previewJobs.value(item.url());
    m_previewJobs.remove(item.url());

    if (!index.isValid()) {
        return;
    }

    Plasma::Package *b = package(index.row());
    if (!b) {
        return;
    }

    m_previews.insert(b, preview);
    //kDebug() << "preview size:" << preview.size();
    m_structureParent.data()->updateScreenshot(index);
}

void BackgroundListModel::previewFailed(const KFileItem &item)
{
    m_previewJobs.remove(item.url());
}

Plasma::Package* BackgroundListModel::package(int index) const
{
    return m_packages.at(index);
}

void BackgroundListModel::setWallpaperSize(const QSize& size)
{
    m_size = size;
}

void BackgroundListModel::setResizeMethod(Plasma::Wallpaper::ResizeMethod resizeMethod)
{
    m_resizeMethod = resizeMethod;
}

BackgroundFinder::BackgroundFinder(Plasma::Wallpaper *structureParent, const QStringList &paths)
    : QThread(structureParent),
      m_structure(Plasma::Wallpaper::packageStructure(structureParent)),
      m_paths(paths)
{
}

BackgroundFinder::~BackgroundFinder()
{
    wait();
}

QStringList BackgroundFinder::suffixes()
{
    return KImageIO::types(KImageIO::Reading);
}

void BackgroundFinder::run()
{
    //QTime t;
    //t.start();
    const QStringList fileSuffixes = suffixes();

    QStringList papersFound;
    //kDebug() << "starting with" << m_paths;

    QDir dir;
    dir.setFilter(QDir::AllDirs | QDir::Files | QDir::Hidden | QDir::Readable);
    Plasma::Package pkg(QString(), m_structure);

    int i;
    for (i = 0; i < m_paths.count(); ++i) {
        const QString path = m_paths.at(i);
        //kDebug() << "doing" << path;
        dir.setPath(path);
        const QFileInfoList files = dir.entryInfoList();
        foreach (const QFileInfo &wp, files) {
            if (wp.isDir()) {
                //kDebug() << "directory" << wp.fileName() << validPackages.contains(wp.fileName());
                const QString name = wp.fileName();
                if (name == "." || name == "..") {
                    // do nothing
                    continue;
                }

                const QString filePath = wp.filePath();
                if (QFile::exists(filePath + "/metadata.desktop")) {
                    pkg.setPath(filePath);
                    if (pkg.isValid()) {
                        papersFound << pkg.path();
                        continue;
                        //kDebug() << "gots a" << wp.filePath();
                    }
                }

                // add this to the directories we should be looking at
                m_paths.append(filePath);
            } else if (fileSuffixes.contains(wp.suffix().toLower())) {
                //kDebug() << "     adding image file" << wp.filePath();
                papersFound << wp.filePath();
            }
        }
    }

    //kDebug() << "background found!" << papersFound.size() << "in" << i << "dirs, taking" << t.elapsed() << "ms";
    emit backgroundsFound(papersFound);
    deleteLater();
}

#include "moc_backgroundlistmodel.cpp"


#endif // BACKGROUNDLISTMODEL_CPP
