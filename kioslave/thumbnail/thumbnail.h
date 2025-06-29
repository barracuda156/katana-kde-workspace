/*  This file is part of the KDE libraries
    Copyright (C) 2000 Malte Starostik <malte@kde.org>

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

#ifndef _THUMBNAIL_H_
#define _THUMBNAIL_H_

#include <QtCore/QHash>
#include <QImage>
#include <QSet>

#include <kio/slavebase.h>

// NOTE: keep in sync with:
// kde-workspace/dolphin/src/settings/general/previewssettingspage.cpp
// kdelibs/kio/kio/previewjob.cpp
enum PreviewDefaults {
    MaxLocalSize = 20, // 20 MB
    MaxRemoteSize = 5  // 5 MB
};

class ThumbCreator;

class ThumbnailProtocol : public KIO::SlaveBase
{
public:
    ThumbnailProtocol(const QByteArray &app);
    virtual ~ThumbnailProtocol();

    void get(const KUrl &url) final;

protected:
    ThumbCreator* getThumbCreator(const QString& plugin);
    bool isOpaque(const QImage &image) const;
    void drawPictureFrame(QPainter *painter, const QPoint &pos, const QImage &image,
                          int frameWidth, QSize imageTargetSize) const;
    QImage thumbForDirectory(const KUrl& directory);
    QString pluginForMimeType(const QString& mimeType);

private:
    /**
     * Creates a sub thumbnail for the directory thumbnail. If a cached
     * version of the sub thumbnail is available, the cached version will be used.
     * If no cached version is available, the created sub thumbnail will be
     * added to the cache for later use.
     */
    bool createSubThumbnail(QImage& thumbnail, const QString& filePath,
                            int segmentWidth, int segmentHeight);

    /**
     * Scales down the image \p img in a way that it fits into the
     * given maximum width and height.
     */
    void scaleDownImage(QImage& img, int maxWidth, int maxHeight);

    /**
     * Create and draw the SubThumbnail
     **/
    bool drawSubThumbnail(QPainter& p, const QString& filePath, int width, int height,
                          int xPos, int yPos, int frameWidth);
private:
    int m_width;
    int m_height;
    // Thumbnail creators
    QHash<QString, ThumbCreator*> m_creators;
    QStringList m_enabledPlugins;
    QSet<QString> m_propagationDirectories;
    QString m_thumbBasePath;
    qint64 m_maxFileSize;
};

#endif
