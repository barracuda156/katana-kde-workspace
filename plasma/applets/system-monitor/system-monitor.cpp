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
#include <QGraphicsLinearLayout>
#include <Plasma/Theme>
#include <Plasma/Frame>
#include <Plasma/SignalPlotter>
#include <Plasma/Meter>
#include <KDebug>

static const int s_monitorsid = -1;
static const int s_updatetimeout = 1000;
static const QSizeF s_minimumvisualizersize = QSizeF(120, 70);

enum KSensorType {
    UnknownSensor = 0,
    CPUSensor = 1,
    NetReceiverSensor = 2,
    NetTransmitterSensor = 3,
    DiskFreeSensor = 4,
    DiskUsedSensor = 5
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
    // any partitions
    } else if (sensor.startsWith("partitions/") && sensor.endsWith("/freespace")) {
        return KSensorType::DiskFreeSensor;
    } else if (sensor.startsWith("partitions/") && sensor.endsWith("/usedspace")) {
        return KSensorType::DiskUsedSensor;
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
    QByteArray m_netid;
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
    m_netplotter->setTitle(i18n("Network"));
    m_netplotter->setUnit("KiB/s");
    m_netplotter->setShowTopBar(false);
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


class SystemMonitorClient : public QObject, public KSGRD::SensorClient
{
    Q_OBJECT
public:
    SystemMonitorClient(QObject *parent);
    ~SystemMonitorClient();

    QList<QByteArray> sensors() const;
    void requestValue(const QByteArray &sensor) const;

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
    KSGRD::SensorMgr->engage("localhost", "", "ksysguardd");

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

void SystemMonitorClient::requestValue(const QByteArray &sensor) const
{
    const int sensorid = m_sensors.indexOf(sensor);
    if (sensorid < 0) {
        // this can actually happen if there was no answer yet for "monitors"
        kWarning() << "unmapped sensor" << sensor;
        return;
    }
    const QString sensorstring = QString::fromLatin1(sensor.constData(), sensor.size());
    KSGRD::SensorMgr->sendRequest("localhost", sensorstring, (KSGRD::SensorClient*)this, sensorid);
}

void SystemMonitorClient::slotUpdate()
{
    KSGRD::SensorMgr->sendRequest("localhost", "monitors", (KSGRD::SensorClient*)this, s_monitorsid);
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
            m_sensors.append(sensorname);
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
    QGraphicsLinearLayout* m_layout;
    SystemMonitorClient* m_systemmonitorclient;
    Plasma::Frame* m_cpuframe;
    Plasma::SignalPlotter* m_cpuplotter;
    QList<SystemMonitorNet*> m_netplotters;
    QList<Plasma::Meter*> m_diskmeters;
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
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
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
    m_cpuplotter->setShowTopBar(false);
    m_cpuplotter->setShowLabels(true);
    m_cpuplotter->setShowVerticalLines(false);
    m_cpuplotter->setShowHorizontalLines(false);
    m_cpuplotter->setThinFrame(false);
    m_cpuplotter->setUseAutoRange(false);
    m_cpuplotter->setVerticalRange(0.0, 100.0);
    m_cpuplotter->addPlot(kCPUVisualizerColor());
    kAddItem(m_cpuframe, m_cpuplotter);
    m_layout->addItem(m_cpuframe);

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
    foreach (SystemMonitorNet* netplotter, m_netplotters) {
        m_layout->removeItem(netplotter);
    }
    qDeleteAll(m_netplotters);
    m_diskmeters.clear();
    foreach (Plasma::Meter* diskmeter, m_diskmeters) {
        m_layout->removeItem(diskmeter);
    }
    qDeleteAll(m_diskmeters);
    m_diskmeters.clear();
    
    m_requestsensors.clear();
    // TODO: pre-sort, order of occurance matters
    foreach (const QByteArray &sensor, m_systemmonitorclient->sensors()) {
        const KSensorType ksensortype = kSensorType(sensor);
        if (ksensortype != KSensorType::UnknownSensor) {
            m_requestsensors.append(sensor);
            kDebug() << "monitoring" << sensor << ksensortype;
        }
        if (ksensortype == KSensorType::NetReceiverSensor) {
            SystemMonitorNet* netplotter = new SystemMonitorNet(this, kNetID(sensor));
            m_layout->addItem(netplotter);
            m_netplotters.append(netplotter);
        }

        if (ksensortype == KSensorType::DiskFreeSensor) {
            Plasma::Meter* diskmeter = new Plasma::Meter(this);
            diskmeter->setMeterType(Plasma::Meter::BarMeterHorizontal);
            diskmeter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
            // TODO: custom class
            diskmeter->setProperty("_k_diskmeterid", kPartitionID(sensor));
            diskmeter->setMinimum(0);
            diskmeter->setMaximum(0);
            m_layout->addItem(diskmeter);
            m_diskmeters.append(diskmeter);
        }
    }

    QTimer::singleShot(s_updatetimeout, this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::slotRequestValues()
{
    QMutexLocker locker(&m_mutex);
    foreach (SystemMonitorNet* netplotter, m_netplotters) {
        netplotter->resetSample();
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
            foreach (SystemMonitorNet* netplotter, m_netplotters) {
                if (netplotter->netID() == netid) {
                    netplotter->addReceiveSample(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::NetTransmitterSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray netid = kNetID(sensor);
            foreach (SystemMonitorNet* netplotter, m_netplotters) {
                if (netplotter->netID() == netid) {
                    netplotter->addTransmitSample(value);
                    break;
                }
            }
            break;
        }
        case KSensorType::DiskFreeSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray partitionid = kPartitionID(sensor);
            foreach (Plasma::Meter* diskmeter, m_diskmeters) {
                const QByteArray diskmeterid = diskmeter->property("_k_diskmeterid").toByteArray();
                if (diskmeterid == partitionid) {
                    const int roundvalue = qRound(value);
                    const int maxvalue = qMax(roundvalue + diskmeter->value(), roundvalue);
                    diskmeter->setMaximum(maxvalue);
                    // qDebug() << Q_FUNC_INFO << "disk free" << sensor << roundvalue << maxvalue;
                    break;
                }
            }
            break;
        }
        case KSensorType::DiskUsedSensor: {
            QMutexLocker locker(&m_mutex);
            const QByteArray partitionid = kPartitionID(sensor);
            foreach (Plasma::Meter* diskmeter, m_diskmeters) {
                const QByteArray diskmeterid = diskmeter->property("_k_diskmeterid").toByteArray();
                if (diskmeterid == partitionid) {
                    const int roundvalue = qRound(value);
                    const int maxvalue = qMax(roundvalue, diskmeter->maximum());
                    diskmeter->setMaximum(maxvalue);
                    diskmeter->setValue(roundvalue);
                    // qDebug() << Q_FUNC_INFO << "disk used" << sensor << roundvalue << maxvalue;
                    break;
                }
            }
            break;
        }
        case KSensorType::UnknownSensor: {
            break;
        }
    }
    if (ksensortype == KSensorType::DiskFreeSensor || ksensortype == KSensorType::DiskUsedSensor) {
        // force an update, apparently sometimes the sensors have bogus values (e.g. / reported to
        // have zero free space, probably bug in ksysguard)
        update();
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
