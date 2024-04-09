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

#include "potd.h"

#include <QImageWriter>
#include <QFileInfo>
#include <QJsonDocument>
#include <QGridLayout>
#include <QLabel>
#include <KStandardDirs>
#include <KIO/Job>
#include <KRandom>
#include <KDebug>

static const QString s_defaultprovider = QString::fromLatin1("pexels");
// same defaults as that of Plasma::Wallpaper
static const int s_defaultresizemethod = Plasma::Wallpaper::ScaledResize;
static const QColor s_defaultcolor = QColor(0, 0, 0);
static const int s_potdtimeout = 10000;
static const QByteArray s_podformat = QImageWriter::defaultImageFormat();

// for reference:
// https://www.pexels.com/api/documentation/#photos-curated
static const QString s_pexelsapikey = QString::fromLatin1("WeDUAEYr4oV9211fAie7Jcwat6tNlkrW0BTx7kaoiyITGWp7WMcxpSoM");
static const QString s_pexelsapiurl = QString::fromLatin1("https://api.pexels.com/v1/curated");
// for reference:
// https://www.flickr.com/services/api/misc.urls.html
// https://www.flickr.com/services/api/explore/flickr.interestingness.getList
static const QString s_flickrapiurl = QString::fromLatin1(
    "https://api.flickr.com/services/rest/?api_key=31f4917c363e2f76b9fc944790dcc338&format=json&method=flickr.interestingness.getList&date="
);
static const QString s_flickrimageurl = QString::fromLatin1("https://live.staticflickr.com/%1/%2_%3_b.jpg");

static QString kPoTDPath(const QString &provider)
{
    return KGlobal::dirs()->locateLocal("cache", "plasma-potd/" + provider + "." + s_podformat);
}

PoTD::PoTD(QObject *parent, const QVariantList &args)
    : Plasma::Wallpaper(parent, args),
    m_provider(s_defaultprovider),
    m_resizemethod(s_defaultresizemethod),
    m_color(s_defaultcolor),
    m_timer(nullptr),
    m_storejob(nullptr),
    m_providerbox(nullptr),
    m_resizemethodbox(nullptr),
    m_colorbutton(nullptr),
    m_spacer(nullptr)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(s_potdtimeout);
    connect(
        m_timer, SIGNAL(timeout()),
        this, SLOT(slotTimeout())
    );
    connect(
        this, SIGNAL(renderCompleted(QImage)),
        this, SLOT(slotRenderCompleted(QImage))
    );
}

void PoTD::init(const KConfigGroup &config)
{
    m_provider = config.readEntry("provider", s_defaultprovider);
    if (m_provider != QLatin1String("pexels") && m_provider != QLatin1String("flickr")) {
        kWarning() << "invalid provider" << m_provider;
        m_provider = s_defaultprovider;
    }
    m_resizemethod = config.readEntry("resizemethod", s_defaultresizemethod);
    m_color = config.readEntry("color", s_defaultcolor);
    if (!m_color.isValid()) {
        kWarning() << "invalid color" << m_color;
        m_color = s_defaultcolor;
    }
    if (!checkWallpaper()) {
        // force update if checkWallpaper() did not for settings to apply
        repaintWallpaper();
    }
    m_timer->start();
}

void PoTD::save(KConfigGroup &config)
{
    config.writeEntry("provider", m_provider);
    config.writeEntry("color", m_color);
    config.writeEntry("resizemethod", m_resizemethod);
}

void PoTD::paint(QPainter *painter, const QRectF &exposedRect)
{
    const QSize brectsize = boundingRect().size().toSize();
    if (m_image.isNull() || m_size != brectsize) {
        painter->fillRect(exposedRect, m_color);
        m_size = brectsize;
        if (!m_imagepath.isEmpty()) {
            kDebug() << "rendering potd" << m_imagepath << m_size << m_resizemethod << m_color;
            render(m_imagepath, m_size, static_cast<Plasma::Wallpaper::ResizeMethod>(m_resizemethod), m_color);
        }
        return;
    }
    painter->drawImage(exposedRect, m_image, exposedRect);
}

QWidget* PoTD::createConfigurationInterface(QWidget *parent)
{
    QWidget* potdconfigwidget = new QWidget(parent);
    QGridLayout* potdconfiglayout = new QGridLayout(potdconfigwidget);
    potdconfigwidget->setLayout(potdconfiglayout);

    QLabel* providerlabel = new QLabel(potdconfigwidget);
    providerlabel->setText(i18n("Provider:"));
    potdconfiglayout->addWidget(providerlabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_providerbox = new QComboBox(potdconfigwidget);
    m_providerbox->addItem(i18n("Pexels"), "pexels");
    m_providerbox->addItem(i18n("Flickr"), "flickr");
    for (int i = 0; i < m_providerbox->count(); i++) {
        if (m_providerbox->itemData(i).toString() == m_provider) {
            m_providerbox->setCurrentIndex(i);
            break;
        }
    }
    connect(
        m_providerbox, SIGNAL(currentIndexChanged(int)),
        this, SLOT(slotProviderChanged(int))
    );
    potdconfiglayout->addWidget(m_providerbox, 0, 1);

    QLabel* resizemodelabel = new QLabel(potdconfigwidget);
    resizemodelabel->setText(i18n("Positioning:"));
    potdconfiglayout->addWidget(resizemodelabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_resizemethodbox = new QComboBox(potdconfigwidget);
    m_resizemethodbox->addItem(i18n("Scaled & Cropped"), Plasma::Wallpaper::ResizeMethod::ScaledAndCroppedResize);
    m_resizemethodbox->addItem(i18n("Scaled"), Plasma::Wallpaper::ResizeMethod::ScaledResize);
    m_resizemethodbox->addItem(i18n("Scaled, keep proportions"), Plasma::Wallpaper::ResizeMethod::MaxpectResize);
    m_resizemethodbox->addItem(i18n("Centered"), Plasma::Wallpaper::ResizeMethod::CenteredResize);
    m_resizemethodbox->addItem(i18n("Tiled"), Plasma::Wallpaper::ResizeMethod::TiledResize);
    m_resizemethodbox->addItem(i18n("Center Tiled"), Plasma::Wallpaper::ResizeMethod::CenterTiledResize);
    for (int i = 0; i < m_resizemethodbox->count(); i++) {
        if (m_resizemethodbox->itemData(i).toInt() == m_resizemethod) {
            m_resizemethodbox->setCurrentIndex(i);
            break;
        }
    }
    connect(
        m_resizemethodbox, SIGNAL(currentIndexChanged(int)),
        this, SLOT(slotResizeMethodChanged(int))
    );
    potdconfiglayout->addWidget(m_resizemethodbox, 1, 1);

    QLabel* colorlabel = new QLabel(potdconfigwidget);
    colorlabel->setText(i18n("Color:"));
    potdconfiglayout->addWidget(colorlabel, 2, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_colorbutton = new KColorButton(potdconfigwidget);
    m_colorbutton->setColor(m_color);
    m_colorbutton->setToolTip(i18n("Change wallpaper frame color"));
    m_colorbutton->setWhatsThis(i18n("Change the color of the frame that it may be visible when the wallpaper is centered or scaled with the same proportions."));
    connect(m_colorbutton, SIGNAL(changed(QColor)), this, SLOT(slotColorChanged(QColor)));
    potdconfiglayout->addWidget(m_colorbutton, 2, 1);

    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    potdconfiglayout->addItem(m_spacer, 3, 0, 1, 2);

    connect(this, SIGNAL(settingsChanged(bool)), parent, SLOT(settingsChanged(bool)));
    return potdconfigwidget;
}

void PoTD::slotRenderCompleted(const QImage &image)
{
    m_image = image;
    emit update(boundingRect());
}

void PoTD::slotSettingsChanged()
{
    emit settingsChanged(true);
}

void PoTD::slotProviderChanged(int index)
{
    m_provider = m_providerbox->itemData(index).toString();
    slotSettingsChanged();
    if (m_storejob) {
        m_storejob->kill();
    }
    checkWallpaper();
}

void PoTD::slotResizeMethodChanged(int index)
{
    m_resizemethod = m_resizemethodbox->itemData(index).toInt();
    slotSettingsChanged();
}

void PoTD::slotColorChanged(const QColor &color)
{
    m_color = color;
    slotSettingsChanged();
}

void PoTD::slotTimeout()
{
    if (m_storejob) {
        kDebug() << "download in progress";
        return;
    }
    checkWallpaper();
}

void PoTD::pexelsDownload()
{
    const KUrl potdurl(s_pexelsapiurl);
    kDebug() << "starting job for" << potdurl.prettyUrl();
    m_storejob = KIO::storedGet(potdurl, KIO::HideProgressInfo);
    m_storejob->setAutoDelete(false);
    m_storejob->addMetaData("Authorization", s_pexelsapikey);
    connect(m_storejob, SIGNAL(finished(KJob*)), SLOT(pexelsFinished(KJob*)));
}

void PoTD::pexelsFinished(KJob *kjob)
{
    Q_ASSERT(kjob == m_storejob);
    if (m_storejob->error() != KJob::NoError) {
        kWarning() << "request error" << m_storejob->url();
        m_storejob->deleteLater();
        return;
    }

    const QByteArray jsondata = m_storejob->data();

    const QJsonDocument jsondoc = QJsonDocument::fromJson(jsondata);
    if (jsondoc.isNull()) {
        kWarning() << "JSON error" << jsondoc.errorString();
        m_storejob->deleteLater();
        return;
    }

    QStringList pexelsphotolist;
    const QVariantMap jsonmap = jsondoc.toVariant().toMap();
    const QVariantList jsonphotoslist = jsonmap["photos"].toList();
    foreach (const QVariant &photo, jsonphotoslist) {
        const QVariantMap photomap = photo.toMap()["src"].toMap();
        const QString photourl = photomap["landscape"].toString();
        if (photourl.isEmpty()) {
            kDebug() << "skipping photo without landscape url";
            continue;
        }
        kDebug() << "photo" << photourl;
        pexelsphotolist.append(photourl);
    }

    if (pexelsphotolist.isEmpty()) {
        kWarning() << "empty photo list";
        m_storejob->deleteLater();
        return;
    }

    const KUrl photourl(pexelsphotolist.at(KRandom::randomMax(pexelsphotolist.size())));
    kDebug() << "chosen photo" << photourl.prettyUrl();
    m_storejob->deleteLater();
    m_storejob = KIO::storedGet(photourl, KIO::HideProgressInfo);
    m_storejob->setAutoDelete(false);
    m_storejob->addMetaData("Authorization", s_pexelsapikey);
    connect(m_storejob, SIGNAL(finished(KJob*)), SLOT(imageFinished(KJob*)));
}

void PoTD::flickrDownload()
{
    Q_ASSERT(m_storejob == nullptr);
    m_flickrdate = QDate::currentDate();
    const KUrl potdurl(s_flickrapiurl + m_flickrdate.toString(Qt::ISODate));
    kDebug() << "starting job for" << potdurl.prettyUrl();
    m_storejob = KIO::storedGet(potdurl, KIO::HideProgressInfo);
    m_storejob->setAutoDelete(false);
    connect(m_storejob, SIGNAL(finished(KJob*)), SLOT(flickrFinished(KJob*)));
}

void PoTD::flickrFinished(KJob *kjob)
{
    Q_ASSERT(kjob == m_storejob);
    if (m_storejob->error() != KJob::NoError) {
        kWarning() << "request error" << m_storejob->url();
        m_storejob->deleteLater();
        return;
    }

    // HACK: fix the data to be valid JSON
    QByteArray jsondata = m_storejob->data();
    if (jsondata.startsWith("jsonFlickrApi(") && jsondata.endsWith(')')) {
        jsondata = jsondata.mid(14, jsondata.size() - 15);
    }

    const QJsonDocument jsondoc = QJsonDocument::fromJson(jsondata);
    if (jsondoc.isNull()) {
        kWarning() << "JSON error" << jsondoc.errorString();
        m_storejob->deleteLater();
        return;
    }

    const QVariantMap jsonmap = jsondoc.toVariant().toMap();
    const QString apistat = jsonmap["stat"].toString();
    if (apistat == QLatin1String("fail")) {
        // No pictures available for the specified parameters, decrement the date to two days earlier...
        m_flickrdate = m_flickrdate.addDays(-2);

        const KUrl queryurl(s_flickrapiurl + m_flickrdate.toString(Qt::ISODate));
        kDebug() << "stat fail, retrying with" << queryurl.prettyUrl();
        m_storejob->deleteLater();
        m_storejob = KIO::storedGet(queryurl, KIO::HideProgressInfo);
        m_storejob->setAutoDelete(false);
        connect(m_storejob, SIGNAL(finished(KJob*)), SLOT(flickrFinished(KJob*)));
        return;
    }

    QStringList flickrphotolist;
    foreach (const QVariant &photo, jsonmap["photos"].toMap()["photo"].toList()) {
        const QVariantMap photomap = photo.toMap();
        const QString photoispublic = photomap["ispublic"].toString();
        if (photoispublic !=  QLatin1String("1")) {
            kDebug() << "skipping non-public photo";
            continue;
        }
        const QString photoserver = photomap["server"].toString();
        const QString photoid = photomap["id"].toString();
        const QString photosecret = photomap["secret"].toString();
        const QString photourl = s_flickrimageurl.arg(photoserver, photoid, photosecret);
        kDebug() << "photo" << photourl;
        flickrphotolist.append(photourl);
    }

    if (flickrphotolist.isEmpty()) {
        kWarning() << "empty photo list";
        m_storejob->deleteLater();
        return;
    }

    const KUrl photourl(flickrphotolist.at(KRandom::randomMax(flickrphotolist.size())));
    kDebug() << "chosen photo" << photourl.prettyUrl();
    m_storejob->deleteLater();
    m_storejob = KIO::storedGet(photourl, KIO::HideProgressInfo);
    m_storejob->setAutoDelete(false);
    connect(m_storejob, SIGNAL(finished(KJob*)), SLOT(imageFinished(KJob*)));
}

void PoTD::imageFinished(KJob *kjob)
{
    Q_ASSERT(kjob == m_storejob);
    if (m_storejob->error() != KJob::NoError) {
        kWarning() << "image job error" << m_storejob->url();
        m_storejob->deleteLater();
        return;
    }

    const QImage potdimage = QImage::fromData(m_storejob->data());
    if (potdimage.isNull()) {
        kWarning() << "null image for" << m_storejob->url();
    } else {
        const QString potdimagepath = kPoTDPath(m_provider);
        if (!potdimage.save(potdimagepath, s_podformat)) {
            kWarning() << "could not save image for" << m_storejob->url();
        } else {
            kDebug() << "saved fresh potd" << potdimagepath;
            m_imagepath = potdimagepath;
            repaintWallpaper();
        }
    }
    m_storejob->deleteLater();
}

bool PoTD::checkWallpaper()
{
    const QString potdimagepath = kPoTDPath(m_provider);
    kDebug() << "checking potd" << potdimagepath;
    QFileInfo potdimageinfo(potdimagepath);
    if (!potdimageinfo.isFile()
        || potdimageinfo.lastModified().date().day() != QDate::currentDate().day()) {
        m_imagepath.clear();
        repaintWallpaper();
        if (m_provider == "pexels") {
            pexelsDownload();
        } else if (m_provider == "flickr") {
            flickrDownload();
        }
        return true;
    } else if (m_imagepath != potdimagepath) {
        kDebug() << "using up-to-date potd" << potdimagepath;
        m_imagepath = potdimagepath;
        repaintWallpaper();
        return true;
    }
    return false;
}

void PoTD::repaintWallpaper()
{
    kDebug() << "repainting potd" << m_imagepath << m_provider;
    m_image = QImage();
    emit update(boundingRect());
}

#include "moc_potd.cpp"
