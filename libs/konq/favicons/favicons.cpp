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

#include "favicons.h"
#include "favicons_adaptor.h"

#include <kstandarddirs.h>
#include <kio/job.h>
#include <kdebug.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QImage>
#include <QImageReader>

K_PLUGIN_FACTORY(FavIconsFactory, registerPlugin<FavIconsModule>();)
K_EXPORT_PLUGIN(FavIconsFactory("favicons"))

static bool isIconOld(const QString &iconFile)
{
    const QFileInfo iconInfo(iconFile);
    const QDateTime iconLastModified = iconInfo.lastModified();
    if (!iconInfo.isFile() || !iconLastModified.isValid()) {
        kDebug() << "Icon file does not exist or its modification time is invalid" << iconFile;
        return true;
    }

    const QDateTime currentTime = QDateTime::currentDateTime();
     // arbitrary value (one week)
    const bool isOld = ((currentTime.toTime_t() - iconLastModified.toTime_t()) > 604800);
    kDebug() << "isIconOld" << iconFile << isOld;
    return isOld;
}

static QString iconNameFromURL(const QString &url)
{
    return QString::fromLatin1("favicons/%1").arg(KUrl(url).host());
}

static QString iconFilePath(const QString &iconName)
{
    const QString iconDir = KGlobal::dirs()->saveLocation("cache", "favicons/");
    return QString::fromLatin1("%1/%2.png").arg(iconDir.mid(0, iconDir.size() - 9), iconName);
}

static QString faviconFromUrl(const QString &url, const QLatin1String &extension)
{
    KUrl faviconUrl(url);
    faviconUrl.setPath(QString::fromLatin1("/favicon.%1").arg(extension));
    faviconUrl.setQuery(QString());
    return faviconUrl.url();
}

class FavIconsModulePrivate
{
public:
    KUrl::List queuedDownloads;
    KUrl::List failedDownloads;
    KIO::MetaData metaData;
};

FavIconsModule::FavIconsModule(QObject* parent, const QList<QVariant> &args)
    : KDEDModule(parent),
    d(new FavIconsModulePrivate())
{
    Q_UNUSED(args);

    d->metaData.insert(QLatin1String("no-www-auth"), QLatin1String("true"));

    new FavIconsAdaptor(this);
}

FavIconsModule::~FavIconsModule()
{
    delete d;
}

QString FavIconsModule::iconForUrl(const QString &url)
{
    if (url.isEmpty()) {
        return QString();
    }

    const QString iconName = iconNameFromURL(url);
    const QString iconFile = iconFilePath(iconName);
    if (QFile::exists(iconFile)) {
        kDebug() << "URL" << url << "icon" << iconName;
        return iconName;
    }

    return QString();
}

void FavIconsModule::downloadUrlIcon(const QString &url)
{
    const QString iconName = iconNameFromURL(url);
    const QString iconFile = iconFilePath(iconName);
    if (!isIconOld(iconFile)) {
        kDebug() << "Icon for URL already downloaded" << url;
        emit iconChanged(url, iconName);
        return;
    }
    if (d->queuedDownloads.contains(iconName)) {
        kDebug() << "Icon download queued for" << url;
        return;
    }
    if (d->failedDownloads.contains(iconName)) {
        kDebug() << "Icon download already failed for" << url;
        emit iconChanged(url, QString());
        return;
    }

    d->queuedDownloads.append(iconName);
    const QString faviconUrl = faviconFromUrl(url, QLatin1String("ico"));
    startJob(url, faviconUrl, iconFile);
}

void FavIconsModule::forceDownloadUrlIcon(const QString &url)
{
    const QString iconName = iconNameFromURL(url);
    d->failedDownloads.removeAll(iconName); // force a download to happen
    const QString iconFile = iconFilePath(iconName);
    QFile::remove(iconFile);
    downloadUrlIcon(url);
}

void FavIconsModule::startJob(const QString &url, const QString &faviconUrl, const QString &iconFile)
{
    kDebug() << "Downloading" << faviconUrl << "as" << iconFile;
    KIO::StoredTransferJob *tjob = KIO::storedGet(faviconUrl, KIO::HideProgressInfo);
    tjob->setAutoDelete(false);
    tjob->addMetaData(d->metaData);
    tjob->setProperty("faviconsUrl", url);
    tjob->setProperty("faviconsFile", iconFile);
    connect(tjob, SIGNAL(finished(KJob*)), SLOT(slotFinished(KJob*)));
    tjob->start();
}

void FavIconsModule::slotFinished(KJob *kjob)
{
    KIO::StoredTransferJob* tjob = qobject_cast<KIO::StoredTransferJob*>(kjob);
    const QString faviconUrl = tjob->url().url();
    const QString faviconsUrl = tjob->property("faviconsUrl").toString();
    const QString faviconsFile = tjob->property("faviconsFile").toString();
    if (tjob->error()) {
        kWarning() << "Job error" << tjob->errorString();
        tjob->deleteLater();
        downloadError(faviconsUrl, faviconUrl, faviconsFile);
        return;
    }
    QBuffer buffer;
    buffer.setData(tjob->data());
    buffer.open(QIODevice::ReadOnly);
    tjob->deleteLater();
    QImageReader ir(&buffer);
    if (!ir.canRead()) {
        kWarning() << "Image reader cannot read the data" << ir.errorString();
        downloadError(faviconsUrl, faviconUrl, faviconsFile);
        return;
    }
    const QImage img = ir.read();
    if (img.isNull()) {
        kWarning() << "Image reader returned null image" << ir.errorString();
        downloadError(faviconsUrl, faviconUrl, faviconsFile);
        return;
    }
    if (!img.save(faviconsFile, "PNG")) {
        kWarning() << "Error saving image to" << faviconsFile;
        downloadError(faviconsUrl, faviconUrl, faviconsFile);
        return;
    }
    downloadSuccess(faviconsUrl);
}

void FavIconsModule::downloadSuccess(const QString &url)
{
    kDebug() << "Downloaded icon for" << url;
    const QString iconName = iconNameFromURL(url);
    d->queuedDownloads.removeAll(iconName);
    emit iconChanged(url, iconName);
}

void FavIconsModule::downloadError(const QString &url, const QString &faviconUrl, const QString &iconFile)
{
    if (faviconUrl.endsWith(QLatin1String(".ico"))) {
        const QString alternativeFaviconUrl = faviconFromUrl(url, QLatin1String("png"));
        kDebug() << "Attempting alternative icon" << alternativeFaviconUrl;
        startJob(url, alternativeFaviconUrl, iconFile);
        return;
    }

    const QString iconName = iconNameFromURL(url);
    if (!d->failedDownloads.contains(iconName)) {
        kDebug() << "Adding" << url << "to failed downloads";
        d->failedDownloads.append(iconName);
    }
    d->queuedDownloads.removeAll(iconName);
    emit iconChanged(url, QString());
}

#include "moc_favicons.cpp"
