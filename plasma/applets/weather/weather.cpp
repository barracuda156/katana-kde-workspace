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

#include "weather.h"

#include <QTimer>
#include <QJsonDocument>
#include <QGridLayout>
#include <QLabel>
#include <QGraphicsGridLayout>
#include <Plasma/Frame>
#include <Plasma/IconWidget>
#include <Plasma/Label>
#include <Plasma/ToolTipManager>
#include <KSystemTimeZones>
#include <KIO/StoredTransferJob>
#include <KIO/Job>
#include <KIcon>
#include <KDebug>
#include <ksettings.h>

static const QSizeF s_minimumsize = QSizeF(420, 200);
static const QString s_defaultweathericon = QString::fromLatin1("weather-none-available");
static const KTemperature::KTempUnit s_defaulttempunit = KTemperature::Celsius;
static const QChar s_weatherdataseparator = QChar::fromLatin1('#');
// for reference:
// http://www.geoplugin.com/quickstart
// alternatively:
// https://location.services.mozilla.com/v1/geolocate?key=b9255e08-838f-4d4e-b270-bec2cc89719b
// https://location.services.mozilla.com/v1/country?key=b9255e08-838f-4d4e-b270-bec2cc89719b
static const QString s_geoname = QString::fromLatin1("geoplugin.net");
static const QString s_geourl = QString::fromLatin1("http://www.geoplugin.net");
static const QString s_geoapiurl = QString::fromLatin1("http://www.geoplugin.net/json.gp");
// the usual data length multiplied by 2
static const int s_geomaxsize = 2000;
// for reference:
// https://api.met.no/weatherapi/locationforecast/2.0/documentation
// https://api.met.no/doc/ForecastJSON
static const QString s_weathername = QString::fromLatin1("met.no");
static const QString s_weatherurl = QString::fromLatin1("https://www.met.no/");
static const QString s_weatherapiurl = QString::fromLatin1("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=%1&lon=%2");
// the usual data length multiplied by 4
static const int s_weathermaxsize = 160000;
// that would be me
static const QString s_developerurl = QString::fromLatin1("mailto:xakepa10@gmail.com");
static const QString s_developername = QString::fromLatin1("Ivailo Monev");

// NOTE: order is longest to shortest for a reason
static const struct conditionDescriptionData {
    const char* condition;
    const char* description;
} conditionDescriptionTbl[] = {
    { "lightssleetshowersandthunder", I18N_NOOP("Light sleet showers and thunder") },
    { "lightssnowshowersandthunder", I18N_NOOP("Light snow showers and thunder") },
    { "heavysleetshowersandthunder", I18N_NOOP("Heavy sleet showers and thunder") },
    { "heavysnowshowersandthunder", I18N_NOOP("Heavy snow showers and thunder") },
    { "lightrainshowersandthunder", I18N_NOOP("Light rain showers and thunder") },
    { "heavyrainshowersandthunder", I18N_NOOP("Heavy rain showers and thunder") },
    { "sleetshowersandthunder", I18N_NOOP("Sleet showers and thunder") },
    { "rainshowersandthunder", I18N_NOOP("Rain showers and thunder") },
    { "snowshowersandthunder", I18N_NOOP("Snow showers and thunder") },
    { "heavysleetandthunder", I18N_NOOP("Heavy sleet and thunder") },
    { "lightsleetandthunder", I18N_NOOP("Light sleet and thunder") },
    { "heavysnowandthunder", I18N_NOOP("Heavy snow and thunder") },
    { "lightrainandthunder", I18N_NOOP("Light rain and thunder") },
    { "lightsnowandthunder", I18N_NOOP("Light snow and thunder") },
    { "heavyrainandthunder", I18N_NOOP("Heavy rain and thunder") },
    { "heavysleetshowers", I18N_NOOP("Heavy sleet showers") },
    { "lightsleetshowers", I18N_NOOP("Light sleet showers") },
    { "heavyrainshowers", I18N_NOOP("Heavy rain showers") },
    { "heavysnowshowers", I18N_NOOP("Heavy snow showers") },
    { "lightrainshowers", I18N_NOOP("Light rain showers") },
    { "lightsnowshowers", I18N_NOOP("Light snow showers") },
    { "sleetandthunder", I18N_NOOP("Sleet and thunder") },
    { "rainandthunder", I18N_NOOP("Rain and thunder") },
    { "snowandthunder", I18N_NOOP("Snow and thunder") },
    { "sleetshowers", I18N_NOOP("Sleet showers") },
    { "partlycloudy", I18N_NOOP("Partly cloudy") },
    { "snowshowers", I18N_NOOP("Snow showers") },
    { "rainshowers", I18N_NOOP("Rain showers") },
    { "lightsleet", I18N_NOOP("Light sleet") },
    { "heavysleet", I18N_NOOP("Heavy sleet") },
    { "lightrain", I18N_NOOP("Light rain") },
    { "lightsnow", I18N_NOOP("Light snow") },
    { "heavysnow", I18N_NOOP("Heavy snow") },
    { "heavyrain", I18N_NOOP("Heavy rain") },
    { "clearsky", I18N_NOOP("Clear sky") },
    { "cloudy", I18N_NOOP("Cloudy") },
    { "sleet", I18N_NOOP("Sleet") },
    { "snow", I18N_NOOP("Snow") },
    { "rain", I18N_NOOP("Rain") },
    { "fair", I18N_NOOP("Fair") },
    { "fog", I18N_NOOP("Fog") }
};
static const qint16 conditionDescriptionTblSize = sizeof(conditionDescriptionTbl) / sizeof(conditionDescriptionData);

static bool kIsNightTime(const QDateTime &datetime)
{
    const int month = datetime.date().month();
    const int hour = datetime.time().hour();
    if (month <= 3 || month >= 9) {
        return (hour >= 19 || hour <= 6);
    }
    return (hour >= 20 || hour <= 5);
}

static QString kMakeID(const KUrl &url, const QDate &day0)
{
    const QByteArray urlhex = url.url().toLatin1().toHex();
    const QByteArray day0hex = day0.toString(Qt::ISODate).toLatin1().toHex();
    QString result = QString::fromLatin1(urlhex.constData(), urlhex.size());
    result.append(QString::fromLatin1(day0hex.constData(), day0hex.size()));
    return result;
}

static void kResetIconWidget(Plasma::IconWidget *iconwidget)
{
    iconwidget->setIcon(KIcon(s_defaultweathericon));
    iconwidget->setText(i18n("N/A"));
    Plasma::ToolTipManager::self()->clearContent(iconwidget);
}

static Plasma::IconWidget* kMakeIconWidget(QGraphicsWidget *parent)
{
    const int desktopiconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    const QSizeF desktopiconsizef = QSizeF(desktopiconsize, desktopiconsize);
    Plasma::IconWidget* result = new Plasma::IconWidget(parent);
    // has to accept hover events for the tooltip
    result->setAcceptHoverEvents(true);
    result->setAcceptedMouseButtons(Qt::NoButton);
    result->setOrientation(Qt::Vertical);
    result->setPreferredIconSize(desktopiconsizef);
    kResetIconWidget(result);
    return result;
}

static QString kTemperatureDisplayString(const QString &temperature,
                                         const KTemperature::KTempUnit fromunit,
                                         const KTemperature::KTempUnit tounit)
{
    if (temperature.isEmpty()) {
        return i18n("N/A");
    }
    const KTemperature ktemperature(temperature.toDouble(), fromunit);
    if (fromunit == tounit) {
        return ktemperature.toString();
    }
    return KTemperature(ktemperature.convertTo(tounit), tounit).toString();
}

static QIcon kDisplayIcon(const QString &icon, const bool isnighttime)
{
    if (icon.isEmpty()) {
        return KIcon(s_defaultweathericon);
    } else if (icon == QLatin1String("fog")) {
        return KIcon("weather-mist");
    } else if (icon == QLatin1String("cloudy")) {
        return KIcon("weather-many-clouds");
    } else if (icon.startsWith(QLatin1String("clearsky_"))) {
        return KIcon(isnighttime ? "weather-clear-night" : "weather-clear");
    } else if (icon.startsWith(QLatin1String("partlycloudy_")) || icon.startsWith(QLatin1String("fair_"))) {
        return KIcon(isnighttime ? "weather-few-clouds-night" : "weather-few-clouds");
    } else if (icon.contains(QLatin1String("lightrain"))) {
        return KIcon(isnighttime ? "weather-showers-scattered-night" : "weather-showers-scattered");
    } else if (icon.contains(QLatin1String("rain"))) {
        return KIcon(isnighttime ? "weather-showers-night" : "weather-showers");
    } else if (icon.contains(QLatin1String("sleet"))) {
        return KIcon("weather-hail");
    } else if (icon.contains(QLatin1String("lightsnow"))) {
        return KIcon("weather-snow-scattered");
    } else if (icon.contains(QLatin1String("snow"))) {
        return KIcon("weather-snow");
    }
    kWarning() << "unhandled weather icon" << icon;
    return KIcon(s_defaultweathericon);
}

static QString kDisplayCondition(const QString &icon)
{
    if (icon.isEmpty()) {
        return i18n("N/A");
    }
    const QByteArray iconbytes = icon.toLatin1();
    for (int i = 0; i < conditionDescriptionTblSize; i++) {
        if (iconbytes.contains(conditionDescriptionTbl[i].condition)) {
            return ki18nc("Weather condition", conditionDescriptionTbl[i].description).toString();
        }
    }
    kWarning() << "could not find condition description for" << iconbytes;
    return i18n("N/A");
}

static QString kDisplayZoneName(const QString &ktimezonename)
{
    const QByteArray ktimezonenamebytes = ktimezonename.toUtf8();
    QString result = i18n(ktimezonenamebytes.constData());
    // replace underscore with space like KTimeZoneWidget
    result = result.replace(QLatin1Char('_'), QLatin1Char(' '));
    return result;
}

class KWeatherData
{
public:
    KWeatherData();
    explicit KWeatherData(const KTemperature::KTempUnit tempunit);

    bool isValid() const;
    static KWeatherData fromString(const QString &data);
    QString toString() const;

    KTemperature::KTempUnit tempunit;
    QString daytemperature;
    QString dayicon;
    QString nighttemperature;
    QString nighticon;
};

static void kUpdateIconWidget(Plasma::IconWidget *iconwidget, const KWeatherData &weatherdata,
                              const bool isnighttime, const KTemperature::KTempUnit tempunit,
                              const QString &dayname)
{
    iconwidget->setText(
        kTemperatureDisplayString(
            isnighttime ? weatherdata.nighttemperature : weatherdata.daytemperature,
            weatherdata.tempunit, tempunit
        )
    );
    iconwidget->setIcon(
        kDisplayIcon(
            isnighttime ? weatherdata.nighticon : weatherdata.dayicon,
            isnighttime
        )
    );
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(QString::fromLatin1("<center>%1</center>").arg(dayname));
    plasmatooltip.setSubText(
        QString::fromLatin1("<center>%1</center>").arg(
            kDisplayCondition(
                isnighttime ? weatherdata.nighticon : weatherdata.dayicon
            )
        )
    );
    Plasma::ToolTipManager::self()->setContent(iconwidget, plasmatooltip);
}

KWeatherData::KWeatherData()
    : tempunit(s_defaulttempunit)
{
}

KWeatherData::KWeatherData(const KTemperature::KTempUnit _tempunit)
    : tempunit(_tempunit)
{
}

bool KWeatherData::isValid() const
{
    // qDebug() << Q_FUNC_INFO << tempunit << daytemperature << dayicon << nighttemperature << nighticon;
    return (
        tempunit != KTemperature::Invalid
        && !daytemperature.isEmpty()
        && !dayicon.isEmpty()
        && !nighttemperature.isEmpty()
        && !nighticon.isEmpty()
    );
}

KWeatherData KWeatherData::fromString(const QString &data)
{
    KWeatherData result;
    if (data.isEmpty()) {
        // no warning for when there is no saved data
        return result;
    }
    const QStringList splitdata = data.split(s_weatherdataseparator);
    if (splitdata.size() != 5) {
        kWarning() << "invalid KWeatherData" << data;
        return result;
    }
    result.tempunit = static_cast<KTemperature::KTempUnit>(splitdata.at(0).toInt());
    result.daytemperature = splitdata.at(1);
    result.dayicon = splitdata.at(2);
    result.nighttemperature = splitdata.at(3);
    result.nighticon = splitdata.at(4);
    return result;
}

QString KWeatherData::toString() const
{
    QString result;
    result.append(QString::number(static_cast<int>(tempunit)));
    result.append(s_weatherdataseparator);
    result.append(daytemperature);
    result.append(s_weatherdataseparator);
    result.append(dayicon);
    result.append(s_weatherdataseparator);
    result.append(nighttemperature);
    result.append(s_weatherdataseparator);
    result.append(nighticon);
    return result;
}


class WeatherWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    WeatherWidget(WeatherApplet *weather);

    void setup(const QString &source, const float latitude, const float longitude, const KTemperature::KTempUnit tempunit);

private Q_SLOTS:
    void slotGeoResult(KJob *kjob);
    void slotWeatherResult(KJob *kjob);
    void slotUpdateWidgets();

private:
    void startGeoJob();
    void startWeatherJob(const QString &source, const float latitude, const float longitude);

    WeatherApplet* m_weather;
    QGraphicsLinearLayout* m_layout;
    int m_lastupdate;
    QString m_source;
    KTemperature::KTempUnit m_tempunit;
    float m_latitude;
    float m_longitude;
    KIO::StoredTransferJob* m_geojob;
    KIO::StoredTransferJob* m_weatherjob;
    QVector<KWeatherData> m_weatherdata;
    QTimer* m_timer;
    Plasma::Frame* m_forecastframe;
    QGraphicsGridLayout* m_forecastlayout;
    Plasma::IconWidget* m_day0iconwidget;
    Plasma::IconWidget* m_night0iconwidget;
    Plasma::IconWidget* m_day1iconwidget;
    Plasma::IconWidget* m_night1iconwidget;
    Plasma::IconWidget* m_day2iconwidget;
    Plasma::IconWidget* m_night2iconwidget;
    Plasma::IconWidget* m_day3iconwidget;
    Plasma::IconWidget* m_night3iconwidget;
    Plasma::IconWidget* m_day4iconwidget;
    Plasma::IconWidget* m_night4iconwidget;
    Plasma::Label* m_infolabel;
};

WeatherWidget::WeatherWidget(WeatherApplet* weather)
    : QGraphicsWidget(weather),
    m_weather(weather),
    m_layout(nullptr),
    m_lastupdate(-1),
    m_tempunit(s_defaulttempunit),
    m_latitude(KTimeZone::UNKNOWN),
    m_longitude(KTimeZone::UNKNOWN),
    m_geojob(nullptr),
    m_weatherjob(nullptr),
    m_timer(nullptr),
    m_forecastframe(nullptr),
    m_forecastlayout(nullptr),
    m_day0iconwidget(nullptr),
    m_night0iconwidget(nullptr),
    m_day1iconwidget(nullptr),
    m_night1iconwidget(nullptr),
    m_day2iconwidget(nullptr),
    m_night2iconwidget(nullptr),
    m_day3iconwidget(nullptr),
    m_night3iconwidget(nullptr),
    m_day4iconwidget(nullptr),
    m_night4iconwidget(nullptr),
    m_infolabel(nullptr)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setMinimumSize(s_minimumsize);

    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    m_forecastframe = new Plasma::Frame(this);
    m_forecastframe->setFrameShadow(Plasma::Frame::Sunken);
    m_forecastframe->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_forecastlayout = new QGraphicsGridLayout(m_forecastframe);
    m_forecastframe->setLayout(m_forecastlayout);
    m_day0iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_day0iconwidget, 0, 0);
    m_night0iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_night0iconwidget, 1, 0);
    m_day1iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_day1iconwidget, 0, 1);
    m_night1iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_night1iconwidget, 1, 1);
    m_day2iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_day2iconwidget, 0, 2);
    m_night2iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_night2iconwidget, 1, 2);
    m_day3iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_day3iconwidget, 0, 3);
    m_night3iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_night3iconwidget, 1, 3);
    m_day4iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_day4iconwidget, 0, 4);
    m_night4iconwidget = kMakeIconWidget(m_forecastframe);
    m_forecastlayout->addItem(m_night4iconwidget, 1, 4);
    m_layout->addItem(m_forecastframe);

    m_infolabel = new Plasma::Label(this);
    m_infolabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_infolabel->setAlignment(Qt::AlignCenter);
    m_infolabel->setWordWrap(false);
    m_infolabel->nativeWidget()->setOpenExternalLinks(true);
    m_infolabel->setText(
        i18n(
            "Powered by <a href=\"%1\">%2</a> and <a href=\"%3\">%4</a>, developed by <a href=\"%5\">%6</a>",
            s_geourl, s_geoname, s_weatherurl, s_weathername, s_developerurl, s_developername
        )
    );
    m_layout->addItem(m_infolabel);

    m_timer = new QTimer(this);
    m_timer->setInterval(60000); // 1min
    connect(
        m_timer, SIGNAL(timeout()),
        this, SLOT(slotUpdateWidgets())
    );
}

void WeatherWidget::setup(const QString &source, const float latitude, const float longitude, const KTemperature::KTempUnit tempunit)
{
    m_timer->stop();
    m_lastupdate = -1;
    m_source = source;
    m_tempunit = tempunit;
    m_latitude = latitude;
    m_longitude = longitude;
    m_weatherdata.clear();
    m_weatherdata.reserve(5);
    m_weatherdata.fill(KWeatherData(s_defaulttempunit), 5);
    m_forecastframe->setText(i18n("Unknown"));
    kResetIconWidget(m_day0iconwidget);
    kResetIconWidget(m_night0iconwidget);
    kResetIconWidget(m_day1iconwidget);
    kResetIconWidget(m_night1iconwidget);
    kResetIconWidget(m_day2iconwidget);
    kResetIconWidget(m_night2iconwidget);
    kResetIconWidget(m_day3iconwidget);
    kResetIconWidget(m_night3iconwidget);
    kResetIconWidget(m_day4iconwidget);
    kResetIconWidget(m_night4iconwidget);

    m_weather->setBusy(true);
    if (latitude == KTimeZone::UNKNOWN || longitude == KTimeZone::UNKNOWN) {
        startGeoJob();
        return;
    }

    startWeatherJob(source, latitude, longitude);
}

void WeatherWidget::startGeoJob()
{
    if (m_geojob) {
        kDebug() << "killing geo job";
        m_geojob->kill(KJob::Quietly);
        m_geojob->deleteLater();
        m_geojob = nullptr;
    }
    const KUrl geojoburl = s_geoapiurl;
    kDebug() << "starting geo job for" << geojoburl;
    m_geojob = KIO::storedGet(geojoburl, KIO::NoReload, KIO::HideProgressInfo);
    m_geojob->setAutoDelete(false);
    connect(
        m_geojob, SIGNAL(result(KJob*)),
        this, SLOT(slotGeoResult(KJob*))
    );
}

void WeatherWidget::startWeatherJob(const QString &source, const float latitude, const float longitude)
{
    if (m_weatherjob) {
        kDebug() << "killing weather job";
        m_weatherjob->kill(KJob::Quietly);
        m_weatherjob->deleteLater();
        m_weatherjob = nullptr;
    }
    m_forecastframe->setText(source.isEmpty() ? i18n("Unknown") : source);
    const KUrl weatherjoburl = s_weatherapiurl.arg(
        QString::number(latitude),
        QString::number(longitude)
    );
    kDebug() << "starting weather job for" << weatherjoburl;
    m_weatherjob = KIO::storedGet(weatherjoburl, KIO::NoReload, KIO::HideProgressInfo);
    m_weatherjob->setAutoDelete(false);
    connect(
        m_weatherjob, SIGNAL(result(KJob*)),
        this, SLOT(slotWeatherResult(KJob*))
    );
}

void WeatherWidget::slotGeoResult(KJob *kjob)
{
    // the fallback is to local timezone coordinates
    const KTimeZone ktimezone = KSystemTimeZones::local();
    if (kjob->error() != KJob::NoError) {
        kWarning() << "geo job error" << kjob->errorString();
        m_geojob = nullptr;
        kjob->deleteLater();
        startWeatherJob(ktimezone.name(), ktimezone.latitude(), ktimezone.longitude());
        return;
    }
    kDebug() << "geo job completed" << m_geojob->url();
    const QByteArray geodata = m_geojob->data();
    m_geojob = nullptr;
    kjob->deleteLater();
    // never trust a data-feed, ever!
    const int geodatasize = geodata.size();
    // qDebug() << Q_FUNC_INFO << geodata << geodatasize;
    if (geodatasize > s_geomaxsize) {
        kWarning() << "geo data size too large, rejecting it" << geodatasize;
        startWeatherJob(ktimezone.name(), ktimezone.latitude(), ktimezone.longitude());
        return;
    }
    const QJsonDocument geojson = QJsonDocument::fromJson(geodata);
    if (geojson.isNull()) {
        kWarning() << "null geo json document" << geojson.errorString();
        startWeatherJob(ktimezone.name(), ktimezone.latitude(), ktimezone.longitude());
        return;
    }
    const QVariantMap geomap = geojson.toVariant().toMap();
    if (geomap.isEmpty()) {
        kWarning() << "null weather map";
        return;
    }
    const QString georegion = geomap.value("geoplugin_regionName").toString();
    const QString geocountry = geomap.value("geoplugin_countryName").toString();
    const float geolatitude = geomap.value("geoplugin_latitude").toFloat();
    const float geolongitude = geomap.value("geoplugin_longitude").toFloat();
    const QString weathersource = QString::fromLatin1("%1 - %2").arg(georegion, geocountry);
    startWeatherJob(weathersource, geolatitude, geolongitude);
}

void WeatherWidget::slotWeatherResult(KJob *kjob)
{
    if (kjob->error() != KJob::NoError) {
        const QString kjoberror = kjob->errorString();
        kWarning() << "weather job error" << kjoberror;
        m_weatherjob = nullptr;
        kjob->deleteLater();
        m_weather->setBusy(false);
        m_weather->showMessage(KIcon("dialog-error"), kjoberror, Plasma::MessageButton::ButtonOk);
        return;
    }
    const KUrl weatherjoburl = m_weatherjob->url();
    kDebug() << "weather job completed" << weatherjoburl;
    const QByteArray weatherdata = m_weatherjob->data();
    m_weatherjob = nullptr;
    kjob->deleteLater();
    const int weatherdatasize = weatherdata.size();
    // qDebug() << Q_FUNC_INFO << weatherdata << weatherdatasize;
    if (weatherdatasize > s_weathermaxsize) {
        kWarning() << "weather data size too large, rejecting it" << weatherdatasize;
        m_weather->setBusy(false);
        return;
    }
    const QJsonDocument weatherjson = QJsonDocument::fromJson(weatherdata);
    if (weatherjson.isNull()) {
        kWarning() << "null weather json document" << weatherjson.errorString();
        m_weather->setBusy(false);
        return;
    }
    const QVariantMap weathermap = weatherjson.toVariant().toMap();
    if (weathermap.isEmpty()) {
        kWarning() << "null weather map";
        m_weather->setBusy(false);
        return;
    }
    const QVariantMap weatherpropertiesmap = weathermap.value("properties").toMap();
    if (weatherpropertiesmap.isEmpty()) {
        kWarning() << "null weather properties map";
        m_weather->setBusy(false);
        return;
    }
    const QVariantList weathertimeserieslist = weatherpropertiesmap.value("timeseries").toList();
    if (weathertimeserieslist.isEmpty()) {
        kWarning() << "null weather timeseries list";
        m_weather->setBusy(false);
        return;
    }

    KTemperature::KTempUnit tempunit = s_defaulttempunit;
    const QString weathertemperatureunit = weatherpropertiesmap.value("meta").toMap().value("units").toMap().value("air_temperature").toString();
    // qDebug() << Q_FUNC_INFO << weathertemperatureunit << weatherpropertiesmap;
    if (!weathertemperatureunit.isEmpty()) {
        tempunit = KTemperature(0.0, weathertemperatureunit).unitEnum();
        if (tempunit == KTemperature::Invalid) {
            kWarning() << "invalid weather temperature unit" << weathertemperatureunit;
            tempunit = s_defaulttempunit;
        }
    }
    m_weatherdata.fill(KWeatherData(tempunit), 5);

    const QDateTime utcnow = QDateTime::currentDateTimeUtc();
    const QDate utc0 = utcnow.date();
    const QDate utc1 = utc0.addDays(1);
    const QDate utc2 = utc0.addDays(2);
    const QDate utc3 = utc0.addDays(3);
    const QDate utc4 = utc0.addDays(4);
    foreach (const QVariant &weathertimevariant, weathertimeserieslist) {
        const QVariantMap weathertimemap = weathertimevariant.toMap();
        const QString weathertimestring = weathertimemap.value("time").toString();
        const QDateTime weatherdatetime = QDateTime::fromString(weathertimestring, Qt::ISODate).toUTC();
        if (weatherdatetime.isNull()) {
            kWarning() << "invalid weather time" << weathertimestring;
            continue;
        }

        const QVariantMap weatherdatamap = weathertimemap.value("data").toMap();
        if (weatherdatamap.isEmpty()) {
            kWarning() << "invalid weather data" << weathertimestring;
            continue;
        }

        const QDate weatherdate = weatherdatetime.date();
        int weatherdataindex = -1;
        if (weatherdate == utc0) {
            weatherdataindex = 0;
        } else if (weatherdate == utc1) {
            weatherdataindex = 1;
        } else if (weatherdate == utc2) {
            weatherdataindex = 2;
        } else if (weatherdate == utc3) {
            weatherdataindex = 3;
        } else if (weatherdate == utc4) {
            weatherdataindex = 4;
        }
        // qDebug() << Q_FUNC_INFO << weatherdataindex;
        if (weatherdataindex != -1) {
            kDebug() << "found weather data for day number" << weatherdataindex;
            if (kIsNightTime(weatherdatetime)) {
                kDebug() << "found weather data for night" << weatherdataindex;
                // because there may not be data for the next 12-hours in that time data the
                // filling of the weather data picks any data for the next 12-hours because the
                // applet is coded around the day and night cycle, this ends up being best guess
                // (which is what the forecast is anyway)
                if (m_weatherdata[weatherdataindex].nighttemperature.isEmpty()) {
                    m_weatherdata[weatherdataindex].nighttemperature = weatherdatamap.value("instant").toMap().value("details").toMap().value("air_temperature").toString();
                }
                if (m_weatherdata[weatherdataindex].nighticon.isEmpty()) {
                    m_weatherdata[weatherdataindex].nighticon = weatherdatamap.value("next_12_hours").toMap().value("summary").toMap().value("symbol_code").toString();
                }
            } else {
                kDebug() << "found weather data for day" << weatherdataindex;
                if (m_weatherdata[weatherdataindex].daytemperature.isEmpty()) {
                    m_weatherdata[weatherdataindex].daytemperature = weatherdatamap.value("instant").toMap().value("details").toMap().value("air_temperature").toString();
                }
                if (m_weatherdata[weatherdataindex].dayicon.isEmpty()) {
                    m_weatherdata[weatherdataindex].dayicon = weatherdatamap.value("next_12_hours").toMap().value("summary").toMap().value("symbol_code").toString();
                }
            }
        }
        // qDebug() << Q_FUNC_INFO << weatherdatetime;
    }

    // HACK: because during night data for the day may not be provided (i.e. the historical data for
    // the day period) it is saved and restored here - it's a day-0 hack, what a thing! see
    // "Parameters for a time period" at https://api.met.no/doc/ForecastJSON
    const QString day0id = kMakeID(weatherjoburl, utc0);
    if (m_weatherdata[0].isValid()) {
        kDebug() << "saving day0 data for" << weatherjoburl << day0id;
        KSettings ksettings("kweatherdata", KSettings::SimpleConfig);
        ksettings.setString(day0id, m_weatherdata[0].toString());
    } else {
        kDebug() << "using cached day0 data for" << weatherjoburl << day0id;
        KSettings ksettings("kweatherdata", KSettings::SimpleConfig);
        const KWeatherData kweatherdata = KWeatherData::fromString(ksettings.string(day0id));
        // check in case there is no saved data and the current data is semi-valid (night part
        // only)
        if (kweatherdata.isValid()) {
            m_weatherdata[0] = kweatherdata;
        }
    }

    // qDebug() << Q_FUNC_INFO << m_weatherdata;
    m_lastupdate = utc0.day();
    m_weather->setBusy(false);
    slotUpdateWidgets();
    m_timer->start();
}

void WeatherWidget::slotUpdateWidgets()
{
    const QDateTime localnow = QDateTime::currentDateTime();
    // day change detection
    const int utcday = localnow.toUTC().date().day();
    if (m_lastupdate != utcday) {
        kDebug() << "setting up due to day change" << m_lastupdate << utcday;
        setup(m_source, m_latitude, m_longitude, m_tempunit);
        return;
    }

    const bool isnighttime = kIsNightTime(localnow);
    const QString weathericonname = (isnighttime ? m_weatherdata[0].nighticon : m_weatherdata[0].dayicon);
    // qDebug() << Q_FUNC_INFO << localnow << isnighttime << weathericonname;

    const QDate localnowdate = localnow.date();
    const QLocale locale = KGlobal::locale()->toLocale();
    const QString day0dayname = i18n("Today");
    const QString day0nightname = i18n("Tonight");
    const QString day1dayname = locale.dayName(localnowdate.addDays(1).dayOfWeek(), QLocale::LongFormat);
    const QString day2dayname = locale.dayName(localnowdate.addDays(2).dayOfWeek(), QLocale::LongFormat);
    const QString day3dayname = locale.dayName(localnowdate.addDays(3).dayOfWeek(), QLocale::LongFormat);
    const QString day4dayname = locale.dayName(localnowdate.addDays(4).dayOfWeek(), QLocale::LongFormat);
    kUpdateIconWidget(m_day0iconwidget, m_weatherdata[0], false, m_tempunit, day0dayname);
    kUpdateIconWidget(m_night0iconwidget, m_weatherdata[0], true, m_tempunit, day0nightname);
    kUpdateIconWidget(m_day1iconwidget, m_weatherdata[1], false, m_tempunit, day1dayname);
    kUpdateIconWidget(m_night1iconwidget, m_weatherdata[1], true, m_tempunit, day1dayname);
    kUpdateIconWidget(m_day2iconwidget, m_weatherdata[2], false, m_tempunit, day2dayname);
    kUpdateIconWidget(m_night2iconwidget, m_weatherdata[2], true, m_tempunit, day2dayname);
    kUpdateIconWidget(m_day3iconwidget, m_weatherdata[3], false, m_tempunit, day3dayname);
    kUpdateIconWidget(m_night3iconwidget, m_weatherdata[3], true, m_tempunit, day3dayname);
    kUpdateIconWidget(m_day4iconwidget, m_weatherdata[4], false, m_tempunit, day4dayname);
    kUpdateIconWidget(m_night4iconwidget, m_weatherdata[4], true, m_tempunit, day4dayname);

    const QIcon weathericon = kDisplayIcon(weathericonname, isnighttime);
    m_weather->setPopupIcon(weathericon);
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setImage(weathericon);
    plasmatooltip.setMainText(QString::fromLatin1("<center>%1</center>").arg(m_forecastframe->text()));
    plasmatooltip.setSubText(QString::fromLatin1("<center>%1</center>").arg(kDisplayCondition(weathericonname)));
    Plasma::ToolTipManager::self()->setContent(m_weather, plasmatooltip);
}

WeatherApplet::WeatherApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_weatherwidget(nullptr),
    m_tempunit(s_defaulttempunit),
    m_tempunitbox(nullptr),
    m_locationbox(nullptr),
    m_latitude(KTimeZone::UNKNOWN),
    m_latitudeinput(nullptr),
    m_longitude(KTimeZone::UNKNOWN),
    m_longitudeinput(nullptr),
    m_spacer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_weather");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setPopupIcon(s_defaultweathericon);

    m_weatherwidget = new WeatherWidget(this);
}

WeatherApplet::~WeatherApplet()
{
    delete m_weatherwidget;
}

void WeatherApplet::init()
{
    KConfigGroup configgroup = config();
    m_tempunit = static_cast<KTemperature::KTempUnit>(configgroup.readEntry("weatherTempUnit", static_cast<int>(s_defaulttempunit)));
    m_location = configgroup.readEntry("weatherLocation", QString());
    m_latitude = configgroup.readEntry("weatherLatitude", KTimeZone::UNKNOWN);
    m_longitude = configgroup.readEntry("weatherLongitude", KTimeZone::UNKNOWN);
    QString source;
    if (!m_location.isEmpty()) {
        source = kDisplayZoneName(m_location);
    } else if (m_latitude != KTimeZone::UNKNOWN && m_longitude != KTimeZone::UNKNOWN) {
        source = i18n("Custom");
    }
    m_weatherwidget->setup(source, m_latitude, m_longitude, m_tempunit);
}

void WeatherApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QGridLayout* widgetlayout = new QGridLayout(widget);
    QLabel* tempunitlabel = new QLabel(widget);
    tempunitlabel->setText(i18n("Temperature unit:"));
    widgetlayout->addWidget(tempunitlabel, 0, 0);
    m_tempunitbox = new QComboBox(widget);
    for (int i = 0; i < KTemperature::UnitCount; i++) {
        const KTemperature::KTempUnit tempunit = static_cast<KTemperature::KTempUnit>(i);
        m_tempunitbox->addItem(KTemperature::unitDescription(tempunit), tempunit);
    }
    m_tempunitbox->setCurrentIndex(static_cast<int>(m_tempunit));
    widgetlayout->addWidget(m_tempunitbox, 0, 1);
    QLabel* locationlabel = new QLabel(widget);
    locationlabel->setText(i18n("Location:"));
    widgetlayout->addWidget(locationlabel, 1, 0);
    m_locationbox = new QComboBox(widget);
    QMap<QString,QString> sortedzones;
    foreach (const KTimeZone &ktimezone, KSystemTimeZones::zones()) {
        const QString ktimezonename = ktimezone.name();
        if (ktimezonename == QLatin1String("UTC")) {
            continue;
        }
        sortedzones.insert(kDisplayZoneName(ktimezonename), ktimezonename);
    }
    m_locationbox->addItem(i18n("Automatic"));
    m_locationbox->addItem(i18n("Custom"));
    QMapIterator<QString,QString> sortedzonesiter(sortedzones);
    while (sortedzonesiter.hasNext()) {
        sortedzonesiter.next();
        m_locationbox->addItem(sortedzonesiter.key(), sortedzonesiter.value());
    }
    widgetlayout->addWidget(m_locationbox, 1, 1, 1, 1);
    m_latitudeinput = new KDoubleNumInput(widget);
    m_latitudeinput->setSliderEnabled(true);
    m_latitudeinput->setLabel(i18n("Latitude:"));
    m_latitudeinput->setValue(m_latitude);
    widgetlayout->addWidget(m_latitudeinput, 2, 0, 1, 2);
    m_longitudeinput = new KDoubleNumInput(widget);
    m_longitudeinput->setSliderEnabled(true);
    m_longitudeinput->setLabel(i18n("Longitude:"));
    m_longitudeinput->setValue(m_longitude);
    widgetlayout->addWidget(m_longitudeinput, 3, 0, 1, 2);
    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addItem(m_spacer, 4, 0, 1, 2);
    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("Weather"), "weather-clear");

    if (!m_location.isEmpty()) {
        const int locationindex = m_locationbox->findData(m_location);
        // first is automatic, then custom and then timezones
        if (locationindex < 2) {
            kWarning() << "could not find location index" << m_location;
        } else {
            m_locationbox->setCurrentIndex(locationindex);
        }
    } else if (m_latitude != KTimeZone::UNKNOWN && m_longitude != KTimeZone::UNKNOWN) {
        // custom location
        m_locationbox->setCurrentIndex(1);
    }
    slotCheckLocation();

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_tempunitbox, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(m_locationbox, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(m_locationbox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCheckLocation()));
    connect(m_latitudeinput, SIGNAL(valueChanged(double)), parent, SLOT(settingsModified()));
    connect(m_longitudeinput, SIGNAL(valueChanged(double)), parent, SLOT(settingsModified()));
}

QGraphicsWidget* WeatherApplet::graphicsWidget()
{
    return m_weatherwidget;
}

void WeatherApplet::slotCheckLocation()
{
    const int locationindex = m_locationbox->currentIndex();
    if (locationindex == 1) {
        m_latitudeinput->setVisible(true);
        m_latitudeinput->setRange(-90.0, 90.0);
        m_latitudeinput->setValue((!m_location.isEmpty() || m_latitude == KTimeZone::UNKNOWN) ? 0.0 : m_latitude);
        m_longitudeinput->setVisible(true);
        m_longitudeinput->setRange(-180.0, 180.0);
        m_longitudeinput->setValue((!m_location.isEmpty() || m_longitude == KTimeZone::UNKNOWN) ? 0.0 : m_longitude);
    } else {
        m_latitudeinput->setVisible(false);
        m_latitudeinput->setRange(KTimeZone::UNKNOWN, KTimeZone::UNKNOWN);
        m_latitudeinput->setValue(KTimeZone::UNKNOWN);
        m_longitudeinput->setVisible(false);
        m_longitudeinput->setRange(KTimeZone::UNKNOWN, KTimeZone::UNKNOWN);
        m_longitudeinput->setValue(KTimeZone::UNKNOWN);
    }
}

void WeatherApplet::slotConfigAccepted()
{
    Q_ASSERT(m_tempunitbox != nullptr);
    Q_ASSERT(m_locationbox != nullptr);
    Q_ASSERT(m_latitudeinput != nullptr);
    Q_ASSERT(m_longitudeinput != nullptr);
    const int tempindex = m_tempunitbox->currentIndex();
    m_tempunit = static_cast<KTemperature::KTempUnit>(tempindex);
    const int locationindex = m_locationbox->currentIndex();
    QString source;
    // the location is saved only when it is timezone
    m_location.clear();
    if (locationindex == 0) {
        source = i18n("Unknown");
        m_latitude = KTimeZone::UNKNOWN;
        m_longitude = KTimeZone::UNKNOWN;
    } else if (locationindex == 1) {
        source = i18n("Custom");
        m_latitude = m_latitudeinput->value();
        m_longitude = m_longitudeinput->value();
    } else {
        bool foundtimezone = false;
        const QString ktimezonename = m_locationbox->itemData(locationindex).toString();
        foreach (const KTimeZone &ktimezone, KSystemTimeZones::zones()) {
            if (ktimezone.name() == ktimezonename) {
                foundtimezone = true;
                source = m_locationbox->itemText(locationindex);
                m_location = ktimezone.name();
                m_latitude = ktimezone.latitude();
                m_longitude = ktimezone.longitude();
                break;
            }
        }
        if (!foundtimezone) {
            kWarning() << "could not find timezone" << ktimezonename;
        }
    }
    KConfigGroup configgroup = config();
    configgroup.writeEntry("weatherTempUnit", tempindex);
    configgroup.writeEntry("weatherLocation", m_location);
    configgroup.writeEntry("weatherLatitude", m_latitude);
    configgroup.writeEntry("weatherLongitude", m_longitude);
    m_weatherwidget->setup(source, m_latitude, m_longitude, m_tempunit);
    emit configNeedsSaving();
}

K_EXPORT_PLASMA_APPLET(weather, WeatherApplet)

#include "moc_weather.cpp"
#include "weather.moc"
