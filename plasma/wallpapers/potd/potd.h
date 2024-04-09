/*
    This file is part of the KDE project
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#ifndef POTD_H
#define POTD_H

#include <QImage>
#include <QColor>
#include <QTimer>
#include <QPointer>
#include <QComboBox>
#include <QSpacerItem>
#include <Plasma/Wallpaper>
#include <KJob>
#include <KIO/StoredTransferJob>
#include <KColorButton>

class PoTD : public Plasma::Wallpaper
{
    Q_OBJECT
public:
    PoTD(QObject *parent, const QVariantList &args);

    void paint(QPainter* painter, const QRectF &exposedRect) final;
    void init(const KConfigGroup &config) final;
    void save(KConfigGroup &config) final;
    QWidget* createConfigurationInterface(QWidget *parent) final;

Q_SIGNALS:
    void settingsChanged(bool changed);

private Q_SLOTS:
    void slotRenderCompleted(const QImage &image);
    void slotSettingsChanged();
    void slotProviderChanged(int index);
    void slotResizeMethodChanged(int index);
    void slotColorChanged(const QColor &color);
    void slotTimeout();

    void pexelsFinished(KJob *kjob);
    void flickrFinished(KJob *kjob);
    void imageFinished(KJob *kjob);

private:
    void flickrDownload();
    void pexelsDownload();
    bool checkWallpaper();
    void repaintWallpaper();

    QString m_provider;
    QString m_imagepath;
    QSize m_size;
    QImage m_image;
    int m_resizemethod;
    QColor m_color;
    QTimer* m_timer;
    QPointer<KIO::StoredTransferJob> m_storejob;
    QDate m_flickrdate;
    QComboBox* m_providerbox;
    QComboBox* m_resizemethodbox;
    KColorButton* m_colorbutton;
    QSpacerItem* m_spacer;
};

K_EXPORT_PLASMA_WALLPAPER(potd, PoTD)

#endif // POTD_H
