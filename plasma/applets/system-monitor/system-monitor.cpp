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

#include "system-monitor.h"
#include "ksysguard/ksgrd/SensorClient.h"
#include "ksysguard/ksgrd/SensorManager.h"

#include <QMutex>
#include <QTimer>
#include <QGraphicsGridLayout>
#include <QGraphicsLinearLayout>
#include <KUnitConversion>
#include <Plasma/Theme>
#include <Plasma/Frame>
#include <Plasma/SignalPlotter>
#include <Plasma/Meter>
#include <KDebug>

static const QString s_sensorshostname = QString::fromLatin1("localhost");
static const int s_monitorsid = -1;
static const int s_updatetimeout = 1000;
static const QSizeF s_minimumvisualizersize = QSizeF(120, 70);
static const QSizeF s_minimummetersize = QSizeF(70, 70);

enum KSensorType {
    UnknownSensor = 0,
    CPUSensor = 1,
    NetReceiverSensor = 2,
    NetTransmitterSensor = 3,
    PartitionFreeSensor = 4,
    PartitionUsedSensor = 5,
    ThermalSensor = 6
};

static KSensorType kSensorType(const QByteArray &sensor)
{
    // qDebug() << Q_FUNC_INFO << sensor;
    // the only CPU sensor required
    if (sensor == "cpu/system/TotalLoad") {
        return KSensorType::CPUSensor;
    // any network receiver or transmitter except loopback
    } else if (sensor.startsWith("network/interfaces/") && sensor.endsWith("/receiver/data")) {
        if (sensor.contains("/interfaces/lo/")) {
            return KSensorType::UnknownSensor;
        }
        return KSensorType::NetReceiverSensor;
    } else if (sensor.startsWith("network/interfaces/") && sensor.endsWith("/transmitter/data")) {
        if (sensor.contains("/interfaces/lo/")) {
            return KSensorType::UnknownSensor;
        }
        return KSensorType::NetTransmitterSensor;
    // any partition
    } else if (sensor.startsWith("partitions/") && sensor.endsWith("/freespace")) {
        return KSensorType::PartitionFreeSensor;
    } else if (sensor.startsWith("partitions/") && sensor.endsWith("/usedspace")) {
        return KSensorType::PartitionUsedSensor;
    // any thermal zone
    } else if (sensor.startsWith("acpi/Thermal_Zone/")) {
        return KSensorType::ThermalSensor;
    }
    return KSensorType::UnknownSensor;
}

static QByteArray kNetID(const QByteArray &sensor)
{
    if (sensor.endsWith("/receiver/data")) {
        return sensor.mid(0, sensor.size() - 14);
    } else if (sensor.endsWith("/transmitter/data")) {
        return sensor.mid(0, sensor.size() - 17);
    }
    kWarning() << "invalid network sensor" << sensor;
    return sensor;
}

static QByteArray kPartitionID(const QByteArray &sensor)
{
    if (sensor.endsWith("/freespace") || sensor.endsWith("/usedspace")) {
        return sensor.mid(0, sensor.size() - 10);
    }
    kWarning() << "invalid partition sensor" << sensor;
    return sensor;
}

static QByteArray kThermalID(const QByteArray &sensor)
{
    if (sensor.endsWith("/Temperature")) {
        return sensor.mid(0, sensor.size() - 12);
    }
    kWarning() << "invalid thermal sensor" << sensor;
    return sensor;
}

static QString kSensorDisplayString(const QByteArray &sensor)
{
    const int indexofslash = sensor.lastIndexOf('/');
    if (indexofslash >= 0) {
        return sensor.mid(indexofslash + 1, sensor.size() - indexofslash - 1);
    }
    return sensor;
}

static void kSetupFrame(Plasma::Frame* plasmaframe)
{
    plasmaframe->setFrameShadow(Plasma::Frame::Sunken);
    plasmaframe->setMinimumSize(s_minimumvisualizersize);
    plasmaframe->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    QGraphicsLinearLayout* plasmaframelayout = new QGraphicsLinearLayout(plasmaframe);
    plasmaframelayout->setContentsMargins(0, 0, 0, 0);
    plasmaframe->setLayout(plasmaframelayout);
}

static void kAddItem(QGraphicsWidget *parent, QGraphicsWidget *widget)
{
    QGraphicsLinearLayout* parentlayout = static_cast<QGraphicsLinearLayout*>(parent->layout());
    Q_ASSERT(parentlayout);
    parentlayout->addItem(widget);
}

// TODO: hardcoded
static QColor kCPUVisualizerColor()
{
    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
}

static QColor kNetReceiverVisualizerColor()
{
    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::VisitedLinkColor);
}

static QColor kNetTransmitterVisualizerColor()
{
    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::LinkColor);
}

class SystemMonitorNet : public Plasma::Frame
{
    Q_OBJECT
public:
    SystemMonitorNet(QGraphicsWidget *parent, const QByteArray &netid);

    QByteArray netID() const;
    void resetSample();
    void addReceiveSample(const float value);
    void addTransmitSample(const float value);

private:
    Plasma::SignalPlotter* m_netplotter;
    const QByteArray m_netid;
    QList<double> m_netsample;
};

SystemMonitorNet::SystemMonitorNet(QGraphicsWidget *parent, const QByteArray &netid)
    : Plasma::Frame(parent),
    m_netplotter(nullptr),
    m_netid(netid)
{
    kSetupFrame(this);

    m_netplotter = new Plasma::SignalPlotter(this);
    m_netplotter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_netplotter->setTitle(kSensorDisplayString(m_netid));
    m_netplotter->setUnit("KiB/s");
    m_netplotter->setShowTopBar(true);
    m_netplotter->setShowLabels(true);
    m_netplotter->setShowVerticalLines(false);
    m_netplotter->setShowHorizontalLines(false);
    m_netplotter->setThinFrame(false);
    m_netplotter->setUseAutoRange(true);
    m_netplotter->setStackPlots(true);
    m_netplotter->addPlot(kNetReceiverVisualizerColor());
    m_netplotter->addPlot(kNetTransmitterVisualizerColor());
    kAddItem(this, m_netplotter);
}

QByteArray SystemMonitorNet::netID() const
{
    return m_netid;
}

void SystemMonitorNet::resetSample()
{
    m_netsample.clear();
    m_netsample.reserve(2);
    m_netsample.append(0.0);
    m_netsample.append(0.0);
}

void SystemMonitorNet::addReceiveSample(const float value)
{
    m_netsample[0] = double(value);
    m_netplotter->addSample(m_netsample);
}

void SystemMonitorNet::addTransmitSample(const float value)
{
    m_netsample[1] = double(value);
    m_netplotter->addSample(m_netsample);
}


class SystemMonitorPartition : public Plasma::Meter
{
    Q_OBJECT
public:
    SystemMonitorPartition(QGraphicsWidget *parent, const QByteArray &partitionid);

    QByteArray partitionID() const;
    void resetSpace();
    void setFreeSpace(const float value);
    void setUsedSpace(const float value);

protected:
     void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) final;

private:
    void calculateValues();

    const QByteArray m_partitionid;
    QString m_partitiondisplaystring;
    int m_partitionvalues[2];
};

SystemMonitorPartition::SystemMonitorPartition(QGraphicsWidget *parent, const QByteArray &partitionid)
    : Plasma::Meter(parent),
    m_partitionid(partitionid),
    m_partitiondisplaystring(kSensorDisplayString(m_partitionid))
{
    resetSpace();
    setMeterType(Plasma::Meter::BarMeterHorizontal);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    setMinimum(0);
    setMaximum(0);
    if (m_partitiondisplaystring.isEmpty()) {
        m_partitiondisplaystring = QLatin1String("root");
    }
}

QByteArray SystemMonitorPartition::partitionID() const
{
    return m_partitionid;
}

void SystemMonitorPartition::resetSpace()
{
    m_partitionvalues[0] = -1;
    m_partitionvalues[1] = -1;
}

void SystemMonitorPartition::setFreeSpace(const float value)
{
    m_partitionvalues[0] = qRound(value / 1024.0);
    calculateValues();
}

void SystemMonitorPartition::setUsedSpace(const float value)
{
    m_partitionvalues[1] = qRound(value / 1024.0);
    calculateValues();
}

void SystemMonitorPartition::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Plasma::Meter::paint(p, option, widget);
    p->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    QFontMetricsF pfmetrics(p->font());
    const QRectF rect(QPointF(0, 0), size());
    const QString pstring = pfmetrics.elidedText(m_partitiondisplaystring, Qt::ElideRight, rect.width());
    p->drawText(rect, Qt::AlignCenter, pstring);
}

void SystemMonitorPartition::calculateValues()
{
    if (m_partitionvalues[0] != -1 && m_partitionvalues[1] != -1) {
        setMaximum(m_partitionvalues[0] + m_partitionvalues[1]);
        setValue(m_partitionvalues[1]);
    }
}


class SystemMonitorThermal : public Plasma::Meter
{
    Q_OBJECT
public:
    SystemMonitorThermal(QGraphicsWidget *parent, const QByteArray &thermalid);

    QByteArray thermalID() const;
    void setSensorValue(const float value);

private:
    const QByteArray m_thermalid;
    const QString m_thermaldisplaystring;
};

SystemMonitorThermal::SystemMonitorThermal(QGraphicsWidget *parent, const QByteArray &thermalid)
    : Plasma::Meter(parent),
    m_thermalid(thermalid),
    m_thermaldisplaystring(kSensorDisplayString(m_thermalid))
{
    setMeterType(Plasma::Meter::AnalogMeter);
    setMinimumSize(s_minimummetersize);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setLabel(0, m_thermaldisplaystring);
    // NOTE: units is celsius, see:
    // https://www.kernel.org/doc/Documentation/thermal/sysfs-api.txt
    setMinimum(0);
    // emergency is usually 2x less
    setMaximum(200);
}

QByteArray SystemMonitorThermal::thermalID() const
{
    return m_thermalid;
}

void SystemMonitorThermal::setSensorValue(const float value)
{
    const QString valuestring = KTemperature(double(value), KTemperature::Celsius).toString();
    setLabel(0, QString::fromLatin1("%1 - %2").arg(m_thermaldisplaystring).arg(valuestring));
    setValue(qRound(value));
}


class SystemMonitorClient : public QObject, public KSGRD::SensorClient
{
    Q_OBJECT
public:
    SystemMonitorClient(QObject *parent);
    ~SystemMonitorClient();

    QList<QByteArray> sensors() const;
    void requestValue(const QByteArray &sensor);

Q_SIGNALS:
    void sensorsChanged();
    void sensorValue(const QByteArray &sensor, const float value);

private Q_SLOTS:
    void slotUpdate();

protected:
    void answerReceived(int id, const QList<QByteArray> &answer) final;
    void sensorLost(int id) final;

private:
    QList<QByteArray> m_sensors;
};

SystemMonitorClient::SystemMonitorClient(QObject *parent)
    : QObject(parent)
{
    KSGRD::SensorMgr = new KSGRD::SensorManager(this);
    KSGRD::SensorMgr->engage(s_sensorshostname, "", "ksysguardd");

    connect(KSGRD::SensorMgr, SIGNAL(update()), this, SLOT(slotUpdate()));
    slotUpdate();
}

SystemMonitorClient::~SystemMonitorClient()
{
}

QList<QByteArray> SystemMonitorClient::sensors() const
{
    return m_sensors;
}

void SystemMonitorClient::requestValue(const QByteArray &sensor)
{
    const int sensorid = m_sensors.indexOf(sensor);
    if (sensorid < 0) {
        // this can actually happen if there was no answer yet for "monitors"
        kWarning() << "unmapped sensor" << sensor;
        return;
    }
    const QString sensorstring = QString::fromLatin1(sensor.constData(), sensor.size());
    KSGRD::SensorMgr->sendRequest(s_sensorshostname, sensorstring, this, sensorid);
}

void SystemMonitorClient::slotUpdate()
{
    KSGRD::SensorMgr->sendRequest(s_sensorshostname, "monitors", this, s_monitorsid);
}

void SystemMonitorClient::answerReceived(int id, const QList<QByteArray> &answer)
{
    if (id == s_monitorsid) {
        m_sensors.clear();
        foreach (const QByteArray &sensoranswer, answer) {
            const QList<QByteArray> splitsensoranswer = sensoranswer.split('\t');
            if (splitsensoranswer.size() != 2) {
                kWarning() << "invalid sensor answer" << sensoranswer;
                continue;
            }
            const QByteArray sensortype = splitsensoranswer.at(1);
            if (sensortype != "integer" && sensortype != "float") {
                continue;
            }
            const QByteArray sensorname = splitsensoranswer.at(0);
            kDebug() << "mapping sensor" << sensorname << sensortype;
            // pre-sort, order of occurance matters
            const KSensorType ksensortype = kSensorType(sensorname);
            switch (ksensortype) {
                case KSensorType::PartitionFreeSensor:
                case KSensorType::PartitionUsedSensor: {
                    m_sensors.append(sensorname);
                    break;
                }
                default: {
                    m_sensors.prepend(sensorname);
                    break;
                }
            }
        }
        emit sensorsChanged();
    } else if (id < m_sensors.size()) {
        foreach (const QByteArray &sensoranswer, answer) {
            const QByteArray sensorname = m_sensors.at(id);
            const float sensorvalue = sensoranswer.toFloat();
            kDebug() << "got sensor value" << id << sensorname << sensorvalue;
            emit sensorValue(sensorname, sensorvalue);
        }
    } else {
        kWarning() << "invalid sensor ID" << id;
    }
        
}

void SystemMonitorClient::sensorLost(int id)
{
    kDebug() << "sensor lost" << id;
    slotUpdate();
}


class SystemMonitorWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    SystemMonitorWidget(SystemMonitor* systemmonitor);
    ~SystemMonitorWidget();

public Q_SLOTS:
    void slotUpdateLayout();

private Q_SLOTS:
    void slotRequestValues();
    void slotSensorValue(const QByteArray &sensor, const float value);

private:
    QMutex m_mutex;
    SystemMonitor* m_systemmonitor;
    QGraphicsGridLayout* m_layout;
    SystemMonitorClient* m_systemmonitorclient;
    Plasma::Frame* m_cpuframe;
    Plasma::SignalPlotter* m_cpuplotter;
    QList<SystemMonitorNet*> m_netmonitors;
    QList<SystemMonitorPartition*> m_partitionmonitors;
    QList<SystemMonitorThermal*> m_thermalmonitors;
    QList<QByteArray> m_requestsensors;
};


SystemMonitorWidget::SystemMonitorWidget(SystemMonitor* systemmonitor)
    : QGraphicsWidget(systemmonitor),
    m_systemmonitor(systemmonitor),
    m_layout(nullptr),
    m_systemmonitorclient(nullptr),
    m_cpuframe(nullptr),
    m_cpuplotter(nullptr)
{
    m_layout = new QGraphicsGridLayout(this);
    m_systemmonitorclient = new SystemMonitorClient(this);
    connect(
        m_systemmonitorclient, SIGNAL(sensorValue(QByteArray,float)),
        this, SLOT(slotSensorValue(QByteArray,float))
    );

    m_cpuframe = new Plasma::Frame(this);
    kSetupFrame(m_cpuframe);
    m_cpuplotter = new Plasma::SignalPlotter(m_cpuframe);
    m_cpuplotter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_cpuplotter->setTitle(i18n("CPU"));
    m_cpuplotter->setUnit("%");
    m_cpuplotter->setShowTopBar(true);
    m_cpuplotter->setShowLabels(true);
    m_cpuplotter->setShowVerticalLines(false);
    m_cpuplotter->setShowHorizontalLines(false);
    m_cpuplotter->setThinFrame(false);
    m_cpuplotter->setUseAutoRange(false);
    m_cpuplotter->setVerticalRange(0.0, 100.0);
    m_cpuplotter->addPlot(kCPUVisualizerColor());
    kAddItem(m_cpuframe, m_cpuplotter);
    m_layout->addItem(m_cpuframe, 0, 0);
    m_layout->setColumnStretchFactor(0, 2);

    setLayout(m_layout);

    connect(
        m_systemmonitorclient, SIGNAL(sensorsChanged()),
        this, SLOT(slotUpdateLayout())
    );
}

SystemMonitorWidget::~SystemMonitorWidget()
{
}

void SystemMonitorWidget::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (SystemMonitorNet* netmonitor, m_netmonitors) {
        m_layout->removeItem(netmonitor);
    }
    qDeleteAll(m_netmonitors);
    m_netmonitors.clear();
    foreach (SystemMonitorPartition* partitionmonitor, m_partitionmonitors) {
        m_layout->removeItem(partitionmonitor);
    }
    qDeleteAll(m_partitionmonitors);
    m_partitionmonitors.clear();
    foreach (SystemMonitorThermal* thermalmonitor, m_thermalmonitors) {
        m_layout->removeItem(thermalmonitor);
    }
    qDeleteAll(m_thermalmonitors);
    m_thermalmonitors.clear();

    m_requestsensors.clear();
    foreach (const QByteArray &sensor, m_systemmonitorclient->sensors()) {
        const KSensorType ksensortype = kSensorType(sensor);
        if (ksensortype != KSensorType::UnknownSensor) {
            m_requestsensors.append(sensor);
            kDebug() << "monitoring" << sensor << ksensortype;
        }
        if (ksensortype == KSensorType::NetReceiverSensor) {
            SystemMonitorNet* netmonitor = new SystemMonitorNet(this, kNetID(sensor));
            m_layout->addItem(netmonitor, 1, 0);
            m_netmonitors.append(netmonitor);
        }

        if (ksensortype == KSensorType::PartitionFreeSensor) {
            SystemMonitorPartition* partitionmonitor = new SystemMonitorPartition(this, kPartitionID(sensor));
            m_layout->addItem(partitionmonitor, m_layout->rowCount(), 0, 1, 2);
            m_partitionmonitors.append(partitionmonitor);
        }
    }

    foreach (const QByteArray &sensor, m_systemmonitorclient->sensors()) {
        const KSensorType ksensortype = kSensorType(sensor);
        if (ksensortype == KSensorType::ThermalSensor) {
            // bottom row is reserved, thermal monitors are aligned to the CPU monitor + the
            // network monitors count
            if (m_thermalmonitors.size() >= (m_netmonitors.size() + 1)) {
                kWarning() << "not enough space for thermal sensor" << sensor;
                continue;
            }
            SystemMonitorThermal* thermalmonitor = new SystemMonitorThermal(this, kThermalID(sensor));
            m_layout->addItem(thermalmonitor, m_thermalmonitors.size(), 1);
            m_thermalmonitors.append(thermalmonitor);
        }
    }

    QTimer::singleShot(s_updatetimeout, this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::slotRequestValues()
{
    QMutexLocker locker(&m_mutex);
    foreach (SystemMonitorNet* netmonitor, m_netmonitors) {
        netmonitor->resetSample();
    }
    foreach (SystemMonitorPartition* partitionmonitor, m_partitionmonitors) {
        partitionmonitor->resetSpace();
    }
    locker.unlock();
    foreach (const QByteArray &request, m_requestsensors) {
        m_systemmonitorclient->requestValue(request);
    }
    QTimer::singleShot(s_updatetimeout, this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::slotSensorValue(const QByteArray &sensor, const float value)
{
    const KSensorType ksensortype = kSensorType(sensor);
    switch (ksensortype) {
        case KSensorType::CPUSensor: {
            m_cpuplotter->addSample(QList<double>() << double(value));
            break;
        }
        case KSensorType::NetReceiverSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray netid = kNetID(sensor);
            foreach (SystemMonitorNet* netmonitor, m_netmonitors) {
                if (netmonitor->netID() == netid) {
                    netmonitor->addReceiveSample(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::NetTransmitterSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray netid = kNetID(sensor);
            foreach (SystemMonitorNet* netmonitor, m_netmonitors) {
                if (netmonitor->netID() == netid) {
                    netmonitor->addTransmitSample(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::PartitionFreeSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray partitionid = kPartitionID(sensor);
            foreach (SystemMonitorPartition* partitionmonitor, m_partitionmonitors) {
                if (partitionmonitor->partitionID() == partitionid) {
                    partitionmonitor->setFreeSpace(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::PartitionUsedSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray partitionid = kPartitionID(sensor);
            foreach (SystemMonitorPartition* partitionmonitor, m_partitionmonitors) {
                if (partitionmonitor->partitionID() == partitionid) {
                    partitionmonitor->setUsedSpace(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::ThermalSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray thermalid = kThermalID(sensor);
            foreach (SystemMonitorThermal* thermalmonitor, m_thermalmonitors) {
                if (thermalmonitor->thermalID() == thermalid) {
                    thermalmonitor->setSensorValue(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::UnknownSensor: {
            kWarning() << "got value for unknown sensor" << sensor;
            break;
        }
    }
}


SystemMonitor::SystemMonitor(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_systemmonitorwidget(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_system-monitor");
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    m_systemmonitorwidget = new SystemMonitorWidget(this);
    setPopupIcon("utilities-system-monitor");
}

SystemMonitor::~SystemMonitor()
{
    delete m_systemmonitorwidget;
}

void SystemMonitor::init()
{
    QTimer::singleShot(500, m_systemmonitorwidget, SLOT(slotUpdateLayout()));
}

void SystemMonitor::createConfigurationInterface(KConfigDialog *parent)
{
    // TODO: implement
}

QGraphicsWidget *SystemMonitor::graphicsWidget()
{
    return m_systemmonitorwidget;
}

K_EXPORT_PLASMA_APPLET(system-monitor_applet, SystemMonitor)

#include "moc_system-monitor.cpp"
#include "system-monitor.moc"
