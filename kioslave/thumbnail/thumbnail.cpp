/*  This file is part of the KDE libraries
    Copyright (C) 2000 Malte Starostik <malte@kde.org>
                  2000 Carsten Pfeiffer <pfeiffer@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "thumbnail.h"
#include "config-unix.h" // For HAVE_NICE

#include <QBuffer>
#include <QFile>
#include <QBitmap>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QLibrary>
#include <QDirIterator>
#include <QImageWriter>

#include <kurl.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <kmimetype.h>
#include <kdebug.h>
#include <kservice.h>
#include <kservicetype.h>
#include <kservicetypetrader.h>
#include <kmimetypetrader.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>
#include <klocale.h>
#include <kde_file.h>
#include <kdemacros.h>
#include <kiconeffect.h>
#include <krandom.h>
#include <kio/thumbcreator.h>
#include <kconfiggroup.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>

// Recognized metadata entries:
// mimeType     - the mime type of the file, used for the overlay icon if any
// width        - maximum width for the thumbnail
// height       - maximum height for the thumbnail
// iconSize     - the size of the overlay icon to use if any
// iconAlpha    - the transparency value used for icon overlays
// plugin       - the name of the plugin library to be used for thumbnail creation.
//                Provided by the application to save an addition KTrader
//                query here.
//                The data returned is the image in PNG format.

using namespace KIO;

static const QByteArray thumbFormat = QImageWriter::defaultImageFormat();
static const QString thumbExt = QLatin1String(".") + thumbFormat;

// Minimum thumbnail icon ratio
static const qreal s_iconratio = 4.0;
static const int s_maxdirectoryfiles = 500;
static const int s_maxsubdirectories = 50;

int main(int argc, char **argv)
{
    if (argc != 2) {
        kError(7115) << "Usage: kio_thumbnail app-socket";
        exit(-1);
    }

#ifdef HAVE_NICE
    nice( 5 );
#endif

    kDebug(7115) << "Starting" << ::getpid();

    QApplication app(argc, argv);
    KComponentData("kio_thumbnail", "kdelibs4");
    KGlobal::locale();

    ThumbnailProtocol slave(argv[1]);
    slave.dispatchLoop();

    kDebug(7115) << "Done";
    return 0;
}

ThumbnailProtocol::ThumbnailProtocol(const QByteArray &app)
    : SlaveBase("thumbnail", app),
      m_iconSize(0),
      m_maxFileSize(0)
{

}

ThumbnailProtocol::~ThumbnailProtocol()
{
    qDeleteAll( m_creators );
    m_creators.clear();
}

void ThumbnailProtocol::get(const KUrl &url)
{
    m_mimeType = metaData("mimeType");
    kDebug(7115) << "Wanting MIME Type:" << m_mimeType;

    if (m_mimeType.isEmpty()) {
        error(KIO::ERR_INTERNAL, i18n("No MIME Type specified."));
        return;
    }

    m_width = metaData("width").toInt();
    m_height = metaData("height").toInt();
    int iconSize = metaData("iconSize").toInt();

    if (m_width < 0 || m_height < 0) {
        error(KIO::ERR_INTERNAL, i18n("No or invalid size specified."));
        return;
    }

    if (!iconSize) {
        iconSize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    }
    if (iconSize != m_iconSize) {
        m_iconDict.clear();
    }
    m_iconSize = iconSize;

    m_iconAlpha = metaData("iconAlpha").toInt();

    QImage img;
    ThumbCreator::Flags flags = ThumbCreator::None;

    QString plugin = metaData("plugin");
    if ((plugin.isEmpty() || plugin == "directorythumbnail") && m_mimeType == "inode/directory") {
        img = thumbForDirectory(url);
        if(img.isNull()) {
            error(KIO::ERR_INTERNAL, i18n("Cannot create thumbnail for directory"));
            return;
        }
    } else {
        if (plugin.isEmpty()) {
            error(KIO::ERR_INTERNAL, i18n("No plugin specified."));
            return;
        }

        ThumbCreator* creator = getThumbCreator(plugin);
        if(!creator) {
            error(KIO::ERR_INTERNAL, i18n("Cannot load ThumbCreator %1", plugin));
            return;
        }

        if (!creator->create(url.path(), m_width, m_height, img)) {
            error(KIO::ERR_INTERNAL, i18n("Cannot create thumbnail for %1", url.path()));
            return;
        }
        flags = creator->flags();
    }

    scaleDownImage(img, m_width, m_height);

    if (flags & ThumbCreator::DrawFrame) {
        int x2 = img.width() - 1;
        int y2 = img.height() - 1;
        // paint a black rectangle around the "page"
        QPainter p;
        p.begin( &img );
        p.setPen( QColor( 48, 48, 48 ));
        p.drawLine( x2, 0, x2, y2 );
        p.drawLine( 0, y2, x2, y2 );
        p.setPen( QColor( 215, 215, 215 ));
        p.drawLine( 0, 0, x2, 0 );
        p.drawLine( 0, 0, 0, y2 );
        p.end();
    }

    if ((flags & ThumbCreator::BlendIcon) && KIconLoader::global()->alphaBlending(KIconLoader::Desktop)) {
        // blending the mimetype icon in
        QImage icon = getIcon();
        const qreal widthratio = (qreal(img.width()) / icon.width());
        const qreal heightratio = (qreal(img.height()) / icon.height());
        // but only if it will not cover too much of the thumbnail
        if (widthratio >= s_iconratio && heightratio >= s_iconratio) {

            int x = img.width() - icon.width() - 4;
            x = qMax( x, 0 );
            int y = img.height() - icon.height() - 6;
            y = qMax( y, 0 );
            QPainter p(&img);
            p.setOpacity(m_iconAlpha/255.0);
            p.drawImage(x, y, icon);
        }
    }

    if (img.isNull()) {
        error(KIO::ERR_INTERNAL, i18n("Failed to create a thumbnail."));
        return;
    }

    const QString imgpath = KTemporaryFile::filePath(QString::fromLatin1("XXXXXXXXXX%1").arg(thumbExt));
    img.save(imgpath, thumbFormat);
    data(QFile::encodeName(imgpath));
    finished();
}

QString ThumbnailProtocol::pluginForMimeType(const QString& mimeType) {
    KService::List offers = KMimeTypeTrader::self()->query( mimeType, QLatin1String("ThumbCreator"));
    if (!offers.isEmpty()) {
        const KService::Ptr serv = offers.first();
        return serv->library();
    }

    //Match group mimetypes
    ///@todo Move this into some central location together with the related matching code in previewjob.cpp. This doesn't handle inheritance and such
    const KService::List plugins = KServiceTypeTrader::self()->query("ThumbCreator");
    foreach(KService::Ptr plugin, plugins) {
        const QStringList mimeTypes = plugin->serviceTypes();
        foreach(QString mime, mimeTypes) {
            if(mime.endsWith('*')) {
                mime = mime.left(mime.length()-1);
                if(mimeType.startsWith(mime))
                    return plugin->library();
            }
        }
    }

    return QString();
}

bool ThumbnailProtocol::isOpaque(const QImage &image) const
{
    // Test the corner pixels
    return qAlpha(image.pixel(QPoint(0, 0))) == 255 &&
           qAlpha(image.pixel(QPoint(image.width()-1, 0))) == 255 &&
           qAlpha(image.pixel(QPoint(0, image.height()-1))) == 255 &&
           qAlpha(image.pixel(QPoint(image.width()-1, image.height()-1))) == 255;
}

void ThumbnailProtocol::drawPictureFrame(QPainter *painter, const QPoint &centerPos,
                                         const QImage &image, int frameWidth, QSize imageTargetSize) const
{
    // Scale the image down so it matches the aspect ratio
    float scaling = 1.0;

    if ((image.size().width() > imageTargetSize.width()) && (imageTargetSize.width() != 0)) {
        scaling = float(imageTargetSize.width()) / float(image.size().width());
    }

    QImage frame(imageTargetSize + QSize(frameWidth * 2, frameWidth * 2),
                 QImage::Format_ARGB32);
    frame.fill(0);

    float scaledFrameWidth = frameWidth / scaling;

    QTransform m;
    m.rotate(KRandom::randomMax(17) - 8); // Random rotation ±8°
    m.scale(scaling, scaling);

    QRectF frameRect(QPointF(0, 0), QPointF(image.width() + scaledFrameWidth*2, image.height() + scaledFrameWidth*2));

    QRect r = m.mapRect(QRectF(frameRect)).toAlignedRect();

    QImage transformed(r.size(), QImage::Format_ARGB32);
    transformed.fill(0);
    QPainter p(&transformed);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setCompositionMode(QPainter::CompositionMode_Source);

    p.translate(-r.topLeft());
    p.setWorldTransform(m, true);

    if (isOpaque(image)) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(frameRect, scaledFrameWidth / 2, scaledFrameWidth / 2);
    }
    p.drawImage(scaledFrameWidth, scaledFrameWidth, image);
    p.end();

    int radius = qMax(frameWidth, 1);

    QImage shadow(r.size() + QSize(radius * 2, radius * 2), QImage::Format_ARGB32);
    shadow.fill(0);

    p.begin(&shadow);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(radius, radius, transformed);
    p.end();

    KIconEffect::shadowBlur(shadow, radius, QColor(0, 0, 0, 128));

    r.moveCenter(centerPos);

    painter->drawImage(r.topLeft() - QPoint(radius / 2, radius / 2), shadow);
    painter->drawImage(r.topLeft(), transformed);
}

QImage ThumbnailProtocol::thumbForDirectory(const KUrl& directory)
{
    QImage img;
    if (m_propagationDirectories.isEmpty()) {
        // Directories that the directory preview will be propagated into if there is no direct sub-directories
        const KConfigGroup globalConfig(KGlobal::config(), "PreviewSettings");
        m_propagationDirectories = globalConfig.readEntry("PropagationDirectories", QStringList() << "VIDEO_TS").toSet();
        m_maxFileSize = globalConfig.readEntry("MaximumSize", PreviewDefaults::MaxLocalSize * 1024 * 1024);
    }

    const int tiles = 2; //Count of items shown on each dimension
    const int spacing = 1;
    const int visibleCount = tiles * tiles;

    // TODO: the margins are optimized for the Oxygen iconset
    // Provide a fallback solution for other iconsets (e. g. draw folder
    // only as small overlay, use no margins)

    //Use the current (custom) folder icon
    KUrl tempDirectory = directory;
    tempDirectory.setScheme("file"); //iconNameForUrl will not work with the "thumbnail:/" scheme
    QString iconName = KMimeType::iconNameForUrl(tempDirectory, S_IFDIR);

    const QPixmap folder = KIconLoader::global()->loadMimeTypeIcon(iconName,
                                                                   KIconLoader::Desktop,
                                                                   qMin(m_width, m_height));
    const int folderWidth  = folder.width();
    const int folderHeight = folder.height();

    const int topMargin = folderHeight * 30 / 100;
    const int bottomMargin = folderHeight / 6;
    const int leftMargin = folderWidth / 13;
    const int rightMargin = leftMargin;

    const int segmentWidth  = (folderWidth  - leftMargin - rightMargin  + spacing) / tiles - spacing;
    const int segmentHeight = (folderHeight - topMargin  - bottomMargin + spacing) / tiles - spacing;
    if ((segmentWidth < 5) || (segmentHeight <  5)) {
        // the segment size is too small for a useful preview
        return img;
    }

    QString localFile = directory.path();

    img = QImage(QSize(folderWidth, folderHeight), QImage::Format_ARGB32);
    img.fill(0);

    QPainter p;
    p.begin(&img);

    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawPixmap(0, 0, folder);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    int xPos = leftMargin;
    int yPos = topMargin;

    int frameWidth = qRound(folderWidth / 85.);

    int iterations = 0;
    QString hadFirstThumbnail;
    int skipped = 0;
    int skipValidItems = 0;

    const int maxYPos = folderHeight - bottomMargin - segmentHeight;

    // Setup image object for preview with only one tile
    QImage oneTileImg(folder.size(), QImage::Format_ARGB32);
    oneTileImg.fill(0);

    QPainter oneTilePainter(&oneTileImg);
    oneTilePainter.setCompositionMode(QPainter::CompositionMode_Source);
    oneTilePainter.drawPixmap(0, 0, folder);
    oneTilePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const int oneTileWidth = folderWidth - leftMargin - rightMargin;
    const int oneTileHeight = folderHeight - topMargin - bottomMargin;

    int validThumbnails = 0;

    QList<QPair<QString,QString>> subDirectoriesToPropagate;
    while ((skipped <= skipValidItems) && (yPos <= maxYPos) && validThumbnails == 0) {
        QDirIterator dir(localFile, QDir::Dirs | QDir::Files | QDir::Readable);
        if (!dir.hasNext()) {
            break;
        }

        while (dir.hasNext() && (yPos <= maxYPos)) {
            ++iterations;
            if (iterations > s_maxdirectoryfiles) {
                skipValidItems = skipped = 0;
                break;
            }

            dir.next();

            const QFileInfo dirInfo = dir.fileInfo();
            if (dirInfo.isDir()) {
                // To be propagated later if necessary
                subDirectoriesToPropagate.append(qMakePair(dir.filePath(), dir.fileName()));
                continue;
            }

            if (validThumbnails > 0 && hadFirstThumbnail == dir.filePath()) {
                // Never show the same thumbnail twice
                break;
            }

            if (dirInfo.size() > m_maxFileSize) {
                // don't create thumbnails for files that exceed
                // the maximum set file size
                continue;
            }

            if (!drawSubThumbnail(p, dir.filePath(), segmentWidth, segmentHeight, xPos, yPos, frameWidth)) {
                continue;
            }

            if (validThumbnails == 0) {
                drawSubThumbnail(oneTilePainter, dir.filePath(), oneTileWidth, oneTileHeight, xPos, yPos, frameWidth);
            }

            if (skipped < skipValidItems) {
                ++skipped;
                continue;
            }

            if (hadFirstThumbnail.isEmpty()) {
                hadFirstThumbnail = dir.filePath();
            }

            ++validThumbnails;

            xPos += segmentWidth + spacing;
            if (xPos > folderWidth - rightMargin - segmentWidth) {
                xPos = leftMargin;
                yPos += segmentHeight + spacing;
            }
        }

        if (skipped != 0) { // Round up to full pages
            const int roundedDown = (skipped / visibleCount) * visibleCount;
            if (roundedDown < skipped) {
                skipped = roundedDown + visibleCount;
            } else {
                skipped = roundedDown;
            }
        }

        if (skipped == 0) {
            break; // No valid items were found
        }

        // We don't need to iterate again and again: Subtract any multiple of "skipped" from the count we still need to skip
        skipValidItems -= (skipValidItems / skipped) * skipped;
        skipped = 0;
    }

    p.end();

    if (validThumbnails == 0) {
        // Eventually propagate the contained items from a sub-directory
        for (int i = 0; i < s_maxsubdirectories && i < subDirectoriesToPropagate.size(); i++) {
            const QPair<QString,QString> subDirectoryPair = subDirectoriesToPropagate.at(i);
            if (m_propagationDirectories.contains(subDirectoryPair.second)) {
                return thumbForDirectory(KUrl(subDirectoryPair.first));
            }
        }

        // If no thumbnail could be found, return an empty image which indicates
        // that no preview for the directory is available.
        img = QImage();
    }

    // If only for one file a thumbnail could be generated then use image with only one tile
    if (validThumbnails == 1) {
        return oneTileImg;
    }

    return img;
}

ThumbCreator* ThumbnailProtocol::getThumbCreator(const QString& plugin)
{
    ThumbCreator *creator = m_creators[plugin];
    if (!creator) {
        // Don't use KPluginFactory here, this is not a QObject and
        // neither is ThumbCreator
        QLibrary library(plugin);
        if (library.load()) {
            newCreator create = (newCreator)library.resolve("new_creator");
            if (create) {
                creator = create();
            }
        }
        if (!creator) {
            return 0;
        }

        m_creators.insert(plugin, creator);
    }

    return creator;
}


const QImage ThumbnailProtocol::getIcon()
{
    ///@todo Can we really do this? It doesn't seem to respect the size
    if (!m_iconDict.contains(m_mimeType)) { // generate it
        QImage icon( KIconLoader::global()->loadMimeTypeIcon( KMimeType::mimeType(m_mimeType)->iconName(), KIconLoader::Desktop, m_iconSize ).toImage() );
        icon = icon.convertToFormat(QImage::Format_ARGB32);
        m_iconDict.insert(m_mimeType, icon);

        return icon;
    }

    return m_iconDict.value(m_mimeType);
}

bool ThumbnailProtocol::createSubThumbnail(QImage& thumbnail, const QString& filePath,
                                           int segmentWidth, int segmentHeight)
{
    if (m_enabledPlugins.isEmpty()) {
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
    }

    const KUrl fileName = filePath;
    const QString subPlugin = pluginForMimeType(KMimeType::findByUrl(fileName)->name());
    if (subPlugin.isEmpty() || !m_enabledPlugins.contains(subPlugin)) {
        return false;
    }

    ThumbCreator* subCreator = getThumbCreator(subPlugin);
    if (!subCreator) {
        // kDebug(7115) << "found no creator for" << dir.filePath();
        return false;
    }

    if ((segmentWidth <= 256) && (segmentHeight <= 256)) {
        // check whether a cached version of the file is available for
        // 128 x 128 or 256 x 256 pixels
        int cacheSize = 0;
        // NOTE: make sure the algorithm and name match those used in kdelibs/kio/kio/previewjob.cpp
        const QByteArray hash = QFile::encodeName(filePath).toHex();
        const QString modTime = QString::number(QFileInfo(filePath).lastModified().toTime_t());
        const QString thumbName = hash + modTime + thumbExt;
        if (m_thumbBasePath.isEmpty()) {
            m_thumbBasePath = QDir::homePath() + "/.thumbnails/";
            KStandardDirs::makeDir(m_thumbBasePath + "normal/", 0700);
            KStandardDirs::makeDir(m_thumbBasePath + "large/", 0700);
        }

        QString thumbPath = m_thumbBasePath;
        if ((segmentWidth <= 128) && (segmentHeight <= 128)) {
            cacheSize = 128;
            thumbPath += "normal/";
        } else {
            cacheSize = 256;
            thumbPath += "large/";
        }
        if (!thumbnail.load(thumbPath + thumbName)) {
            // no cached version is available, a new thumbnail must be created

            QString tempFileName;
            bool savedCorrectly = false;
            if (subCreator->create(filePath, cacheSize, cacheSize, thumbnail)) {
                scaleDownImage(thumbnail, cacheSize, cacheSize);

                // The thumbnail has been created successfully. Store the thumbnail
                // to the cache for future access.
                tempFileName = KTemporaryFile::filePath(QString::fromLatin1("XXXXXXXXXX%1").arg(thumbExt));
                savedCorrectly = thumbnail.save(tempFileName, thumbFormat);
            } else {
                return false;
            }
            if(savedCorrectly)
            {
                Q_ASSERT(!tempFileName.isEmpty());
                KDE::rename(tempFileName, thumbPath + thumbName);
            }
        }
    } else if (!subCreator->create(filePath, segmentWidth, segmentHeight, thumbnail)) {
        return false;
    }
    return true;
}

void ThumbnailProtocol::scaleDownImage(QImage& img, int maxWidth, int maxHeight)
{
    if (img.width() > maxWidth || img.height() > maxHeight) {
        img = img.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

bool ThumbnailProtocol::drawSubThumbnail(QPainter& p, const QString& filePath, int width, int height, int xPos, int yPos, int frameWidth)
{
    QImage subThumbnail;
    if (!createSubThumbnail(subThumbnail, filePath, width, height)) {
        return false;
    }

    // Apply fake smooth scaling, as seen on several blogs
    if (subThumbnail.width() > width * 4 || subThumbnail.height() > height * 4) {
        subThumbnail = subThumbnail.scaled(width*4, height*4, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    QSize targetSize(subThumbnail.size());
    targetSize.scale(width, height, Qt::KeepAspectRatio);

    // center the image inside the segment boundaries
    const QPoint centerPos(xPos + (width/ 2), yPos + (height / 2));
    drawPictureFrame(&p, centerPos, subThumbnail, frameWidth, targetSize);

    return true;
}
