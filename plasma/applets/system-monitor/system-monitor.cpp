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
#include <QGridLayout>
#include <QLabel>
#include <QGraphicsGridLayout>
#include <QGraphicsLinearLayout>
#include <KUnitConversion>
#include <Plasma/Theme>
#include <Plasma/Label>
#include <Plasma/Frame>
#include <Plasma/SignalPlotter>
#include <Plasma/Meter>
#include <KDebug>

static const QString s_hostname = QString::fromLatin1("localhost");
static const int s_port = -1;
static const int s_monitorsid = -1;
static const int s_update = 1; // 1 sec
static const QSizeF s_minimumvisualizersize = QSizeF(120, 70);
static const QSizeF s_minimummetersize = QSizeF(70, 70);
static const int s_textoffset = 10;

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
    } else if (sensor.startsWith("network/interfaces/")) {
        if (sensor.contains("/interfaces/lo/")) {
            return KSensorType::UnknownSensor;
        }
        if (sensor.endsWith("/receiver/data")) {
            return KSensorType::NetReceiverSensor;
        } else if (sensor.endsWith("/transmitter/data")) {
            return KSensorType::NetTransmitterSensor;
        }
        return KSensorType::UnknownSensor;
    // any partition
    } else if (sensor.startsWith("partitions/")) {
        if (sensor.endsWith("/freespace")) {
            return KSensorType::PartitionFreeSensor;
        } else if (sensor.endsWith("/usedspace")) {
            return KSensorType::PartitionUsedSensor;
        }
        return KSensorType::UnknownSensor;
    // any thermal zone or lmsensor except fans
    } else if (sensor.startsWith("acpi/Thermal_Zone/") || sensor.startsWith("lmsensors/")) {
        if (sensor.contains("fan")) {
            return KSensorType::UnknownSensor;
        }
        return KSensorType::ThermalSensor;
    }
    return KSensorType::UnknownSensor;
}

static int kWeightSensor(const QByteArray &sensor)
{
    // CPU sensors go first
    if (sensor.contains("/Package_")) {
        return 0;
    }
    // GPU sensors after
    if (sensor.contains("-pci-") && sensor.contains("/temp")) {
        return 1;
    }
    // anything else
    return 2;
}

static bool kSensorSort(const QByteArray &first, const QByteArray &second)
{
    return (kWeightSensor(first) <= kWeightSensor(second));
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

class SystemMonitorCPU : public Plasma::Frame
{
    Q_OBJECT
public:
    SystemMonitorCPU(QGraphicsWidget *parent);
};

SystemMonitorCPU::SystemMonitorCPU(QGraphicsWidget *parent)
    : Plasma::Frame(parent)
{
    kSetupFrame(this);
}


class SystemMonitorNet : public Plasma::Frame
{
    Q_OBJECT
public:
    SystemMonitorNet(QGraphicsWidget *parent, const QByteArray &netid,
                     const QColor &receivercolor, const QColor &transmittercolor);

    QByteArray netID() const;
    void resetSample();
    void addReceiveSample(const double value);
    void addTransmitSample(const double value);

private:
    Plasma::SignalPlotter* m_netplotter;
    const QByteArray m_netid;
    QList<double> m_netsample;
};

SystemMonitorNet::SystemMonitorNet(QGraphicsWidget *parent, const QByteArray &netid,
                                   const QColor &receivercolor, const QColor &transmittercolor)
    : Plasma::Frame(parent),
    m_netplotter(nullptr),
    m_netid(netid)
{
    kSetupFrame(this);

    m_netplotter = new Plasma::SignalPlotter(this);
    m_netplotter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_netplotter->setUnit("KiB/s");
    m_netplotter->setTitle(kSensorDisplayString(m_netid));
    m_netplotter->setShowLabels(true);
    m_netplotter->setShowVerticalLines(false);
    m_netplotter->setShowHorizontalLines(false);
    m_netplotter->setThinFrame(false);
    m_netplotter->setUseAutoRange(true);
    m_netplotter->setStackPlots(true);
    m_netplotter->addPlot(receivercolor);
    m_netplotter->addPlot(transmittercolor);
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

void SystemMonitorNet::addReceiveSample(const double value)
{
    m_netsample[0] = value;
    m_netplotter->addSample(m_netsample);
}

void SystemMonitorNet::addTransmitSample(const double value)
{
    m_netsample[1] = value;
    m_netplotter->addSample(m_netsample);
}


class SystemMonitorPartition : public Plasma::Meter
{
    Q_OBJECT
public:
    SystemMonitorPartition(QGraphicsWidget *parent, const QByteArray &partitionid);

    QByteArray partitionID() const;
    void resetSpace();
    void setFreeSpace(const double value);
    void setUsedSpace(const double value);

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

void SystemMonitorPartition::setFreeSpace(const double value)
{
    m_partitionvalues[0] = qRound(value / 1024.0);
    calculateValues();
}

void SystemMonitorPartition::setUsedSpace(const double value)
{
    m_partitionvalues[1] = qRound(value / 1024.0);
    calculateValues();
}

void SystemMonitorPartition::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Plasma::Meter::paint(p, option, widget);

    const QRectF rect(QPointF(0, 0), size());
    p->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    QFontMetricsF pfmetrics(p->font());
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

private:
    const QByteArray m_thermalid;
};

SystemMonitorThermal::SystemMonitorThermal(QGraphicsWidget *parent, const QByteArray &thermalid)
    : Plasma::Meter(parent),
    m_thermalid(thermalid)
{
    setMeterType(Plasma::Meter::AnalogMeter);
    setMinimumSize(s_minimummetersize);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setLabel(0, kSensorDisplayString(m_thermalid));
    // NOTE: values of thermal zones are in celsius, see:
    // https://www.kernel.org/doc/Documentation/thermal/sysfs-api.txt
    setMinimum(0);
    setMaximum(110);
}

QByteArray SystemMonitorThermal::thermalID() const
{
    return m_thermalid;
}

class SystemMonitorClient : public QObject, public KSGRD::SensorClient
{
    Q_OBJECT
public:
    SystemMonitorClient(QObject *parent);

    bool setup(const QString &hostname, const int port);
    QList<QByteArray> sensors() const;
    void requestValue(const QByteArray &sensor);

Q_SIGNALS:
    void sensorsChanged();
    void sensorValue(const QByteArray &sensor, const double value);

private Q_SLOTS:
    void slotUpdate();

protected:
    void answerReceived(int id, const QList<QByteArray> &answer) final;
    void sensorLost(int id) final;

private:
    QString m_hostname;
    QList<QByteArray> m_sensors;
};

SystemMonitorClient::SystemMonitorClient(QObject *parent)
    : QObject(parent),
    m_hostname(s_hostname)
{
    KSGRD::SensorMgr = new KSGRD::SensorManager(this);
    connect(KSGRD::SensorMgr, SIGNAL(update()), this, SLOT(slotUpdate()));
}

bool SystemMonitorClient::setup(const QString &hostname, const int port)
{
    if (KSGRD::SensorMgr->isConnected(m_hostname)) {
        KSGRD::SensorMgr->disengage(m_hostname);
    }
    m_hostname = hostname;
    kDebug() << "connecting to sensor manager on" << m_hostname << port;
    const bool result = KSGRD::SensorMgr->engage(m_hostname, "", "ksysguardd", port);
    if (!result) {
        kWarning() << "could not connect to sensor manager on" << m_hostname << port;
    }
    slotUpdate();
    return result;
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
    KSGRD::SensorMgr->sendRequest(m_hostname, sensorstring, this, sensorid);
}

void SystemMonitorClient::slotUpdate()
{
    // even if setup does not fail the manager may not answer
    m_sensors.clear();
    emit sensorsChanged();
    KSGRD::SensorMgr->sendRequest(m_hostname, "monitors", this, s_monitorsid);
}

void SystemMonitorClient::answerReceived(int id, const QList<QByteArray> &answer)
{
    if (id == s_monitorsid) {
        m_sensors.clear();
        // pre-sort, order of occurance matters and thermal sensors are not even received in sorted
        // order
        QList<QByteArray> thermalsensors;
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
            const KSensorType ksensortype = kSensorType(sensorname);
            switch (ksensortype) {
                case KSensorType::ThermalSensor: {
                    kDebug() << "mapping thermal sensor" << sensorname << sensortype;
                    thermalsensors.append(sensorname);
                    break;
                }
                case KSensorType::PartitionFreeSensor:
                case KSensorType::PartitionUsedSensor: {
                    kDebug() << "mapping sensor to back" << sensorname << sensortype;
                    m_sensors.append(sensorname);
                    break;
                }
                case KSensorType::CPUSensor:
                case KSensorType::NetReceiverSensor:
                case KSensorType::NetTransmitterSensor: {
                    kDebug() << "mapping sensor to front" << sensorname << sensortype;
                    m_sensors.prepend(sensorname);
                    break;
                }
                case KSensorType::UnknownSensor: {
                    break;
                }
            }
        }
        if (!thermalsensors.isEmpty()) {
            qStableSort(thermalsensors.begin(), thermalsensors.end(), kSensorSort);
            m_sensors.append(thermalsensors);
        }
        // qDebug() << Q_FUNC_INFO << m_sensors;
        emit sensorsChanged();
    } else if (id < m_sensors.size()) {
        foreach (const QByteArray &sensoranswer, answer) {
            const QByteArray sensorname = m_sensors.at(id);
            bool ok = false;
            const double sensorvalue = sensoranswer.toDouble(&ok);
            if (Q_UNLIKELY(!ok)) {
                kWarning() << "sensor value conversion failed" << sensorname << sensoranswer;
            } else {
                kDebug() << "got sensor value" << id << sensorname << sensorvalue;
                emit sensorValue(sensorname, sensorvalue);
            }
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

    void setupMonitors(const QString &hostname, const int port, const int update,
                       const QColor &cpucolor,
                       const QColor &receivercolor, const QColor &transmittercolor);

public Q_SLOTS:
    void slotUpdateLayout();

private Q_SLOTS:
    void slotRequestValues();
    void slotSensorValue(const QByteArray &sensor, const double value);

private:
    QMutex m_mutex;
    SystemMonitor* m_systemmonitor;
    QGraphicsGridLayout* m_layout;
    SystemMonitorClient* m_systemmonitorclient;
    Plasma::Label* m_label;
    SystemMonitorCPU* m_cpuframe;
    Plasma::SignalPlotter* m_cpuplotter;
    QList<SystemMonitorNet*> m_netmonitors;
    QList<SystemMonitorPartition*> m_partitionmonitors;
    QList<SystemMonitorThermal*> m_thermalmonitors;
    QTimer* m_updatetimer;
    QList<QByteArray> m_requestsensors;
    QColor m_receivercolor;
    QColor m_transmittercolor;
};


SystemMonitorWidget::SystemMonitorWidget(SystemMonitor* systemmonitor)
    : QGraphicsWidget(systemmonitor),
    m_systemmonitor(systemmonitor),
    m_layout(nullptr),
    m_systemmonitorclient(nullptr),
    m_label(nullptr),
    m_cpuframe(nullptr),
    m_cpuplotter(nullptr)
{
    m_systemmonitorclient = new SystemMonitorClient(this);
    m_layout = new QGraphicsGridLayout(this);

    m_label = new Plasma::Label(this);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_label->setText(i18n("No sensors"));
    m_label->setAlignment(Qt::AlignCenter);

    m_cpuframe = new SystemMonitorCPU(this);
    m_cpuplotter = new Plasma::SignalPlotter(m_cpuframe);
    m_cpuplotter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_cpuplotter->setUnit("%");
    m_cpuplotter->setTitle(i18n("CPU"));
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

    m_updatetimer = new QTimer(this);
    // the time is in seconds, has to be in ms for the timer
    m_updatetimer->setInterval(s_update * 1000);
    connect(m_updatetimer, SIGNAL(timeout()), this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::setupMonitors(const QString &hostname, const int port, const int update,
                                        const QColor &cpucolor,
                                        const QColor &receivercolor, const QColor &transmittercolor)
{
    m_updatetimer->stop();
    m_updatetimer->setInterval(qMax(update, s_update) * 1000);
    disconnect(m_systemmonitorclient, 0, this, 0);
    m_systemmonitor->setBusy(true);
    if (!m_systemmonitorclient->setup(hostname, port)) {
        slotUpdateLayout();
        const QString errorstring = i18n("Could not connect to: %1 on port %2", hostname, QString::number(port));
        m_systemmonitor->showMessage(KIcon("dialog-error"), errorstring, Plasma::MessageButton::ButtonOk);
        return;
    }
    m_cpuplotter->removePlot(0);
    m_cpuplotter->addPlot(cpucolor);
    m_receivercolor = receivercolor;
    m_transmittercolor = transmittercolor;
    slotUpdateLayout();
    connect(
        m_systemmonitorclient, SIGNAL(sensorValue(QByteArray,double)),
        this, SLOT(slotSensorValue(QByteArray,double))
    );
    connect(
        m_systemmonitorclient, SIGNAL(sensorsChanged()),
        this, SLOT(slotUpdateLayout())
    );
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
    m_cpuframe->setVisible(true);
    foreach (const QByteArray &sensor, m_systemmonitorclient->sensors()) {
        const KSensorType ksensortype = kSensorType(sensor);
        Q_ASSERT(ksensortype != KSensorType::UnknownSensor);
        m_requestsensors.append(sensor);
        if (ksensortype == KSensorType::NetReceiverSensor) {
            const QByteArray knetid = kNetID(sensor);
            kDebug() << "creating net monitor for" << knetid;
            SystemMonitorNet* netmonitor = new SystemMonitorNet(
                this, knetid,
                m_receivercolor, m_transmittercolor
            );
            m_layout->addItem(netmonitor, 1, 0);
            m_netmonitors.append(netmonitor);
        }

        if (ksensortype == KSensorType::PartitionFreeSensor) {
            const QByteArray kpartitionid = kPartitionID(sensor);
            kDebug() << "creating partition monitor for" << kpartitionid;
            SystemMonitorPartition* partitionmonitor = new SystemMonitorPartition(
                this, kpartitionid
            );
            m_layout->addItem(partitionmonitor, m_layout->rowCount(), 0, 1, 2);
            m_partitionmonitors.append(partitionmonitor);
        }
    }

    foreach (const QByteArray &sensor, m_systemmonitorclient->sensors()) {
        const KSensorType ksensortype = kSensorType(sensor);
        if (ksensortype == KSensorType::ThermalSensor) {
            // bottom is reserved, thermal monitors are aligned to the CPU monitor + the network
            // monitors count
            if (m_thermalmonitors.size() >= (m_netmonitors.size() + 1)) {
                kWarning() << "not enough space for thermal sensor" << sensor;
                continue;
            }
            const QByteArray thermalid = kThermalID(sensor);
            kDebug() << "creating thermal monitor for" << thermalid;
            SystemMonitorThermal* thermalmonitor = new SystemMonitorThermal(
                this, thermalid
            );
            m_layout->addItem(thermalmonitor, m_thermalmonitors.size(), 1);
            m_thermalmonitors.append(thermalmonitor);
        }
    }

    if (m_requestsensors.isEmpty()) {
        // no sensors were reported
        m_cpuframe->setVisible(false);
        // put the label in a cell that is unlikely to be taken at any point
        m_layout->addItem(m_label, 100, 0);
        m_label->setVisible(true);
    } else {
        m_label->setVisible(false);
        m_layout->removeItem(m_label);
        m_cpuframe->setVisible(true);
    }

    // immediate update in case the update time is long
    locker.unlock();
    slotRequestValues();
    // and start polling for updates
    m_updatetimer->start();
    m_systemmonitor->setBusy(false);
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
}

void SystemMonitorWidget::slotSensorValue(const QByteArray &sensor, const double value)
{
    const KSensorType ksensortype = kSensorType(sensor);
    switch (ksensortype) {
        case KSensorType::CPUSensor: {
            m_cpuplotter->addSample(QList<double>() << value);
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
                    thermalmonitor->setValue(qRound(value));
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
    m_systemmonitorwidget(nullptr),
    m_hostname(s_hostname),
    m_port(s_port),
    m_update(s_update),
    m_hostnameedit(nullptr),
    m_portbox(nullptr),
    m_updateedit(nullptr),
    m_cpubutton(nullptr),
    m_receiverbutton(nullptr),
    m_transmitterbutton(nullptr),
    m_spacer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_system-monitor");
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    m_systemmonitorwidget = new SystemMonitorWidget(this);
    setPopupIcon("utilities-system-monitor");
    // NOTE: no check has to be done if it is installed
    setAssociatedApplication("ksysguard");
}

SystemMonitor::~SystemMonitor()
{
    delete m_systemmonitorwidget;
}

void SystemMonitor::init()
{
    slotThemeChanged();
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(slotThemeChanged()));
}

void SystemMonitor::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QGridLayout* widgetlayout = new QGridLayout(widget);
    QLabel* hostnamelabel = new QLabel(widget);
    hostnamelabel->setText(i18n("Hostname:"));
    widgetlayout->addWidget(hostnamelabel, 0, 0, Qt::AlignRight);
    m_hostnameedit = new KLineEdit(widget);
    m_hostnameedit->setText(m_hostname);
    widgetlayout->addWidget(m_hostnameedit, 0, 1);

    QLabel* portlabel = new QLabel(widget);
    portlabel->setText(i18n("Port:"));
    widgetlayout->addWidget(portlabel, 1, 0, Qt::AlignRight);
    m_portbox = new KIntNumInput(widget);
    m_portbox->setMinimum(s_port);
    m_portbox->setMaximum(USHRT_MAX);
    m_portbox->setSpecialValueText(i18n("Default"));
    m_portbox->setValue(m_port);
    widgetlayout->addWidget(m_portbox, 1, 1);

    QLabel* updatelabel = new QLabel(widget);
    updatelabel->setText(i18n("Update interval:"));
    widgetlayout->addWidget(updatelabel, 2, 0, Qt::AlignRight);
    m_updateedit = new KTimeEdit(widget);
    m_updateedit->setMinimumTime(QTime(0, 0, 1));
    m_updateedit->setTime(QTime(0, 0, 0).addSecs(m_update));
    widgetlayout->addWidget(m_updateedit, 2, 1);

    QLabel* cpulabel = new QLabel(widget);
    cpulabel->setText(i18n("CPU color:"));
    widgetlayout->addWidget(cpulabel, 3, 0, Qt::AlignRight);
    const QColor defaultcpucolor = kCPUVisualizerColor();
    QColor cpucolor = m_cpucolor;
    if (!cpucolor.isValid()) {
        cpucolor = defaultcpucolor;
    }
    m_cpubutton = new KColorButton(widget);
    m_cpubutton->setDefaultColor(defaultcpucolor);
    m_cpubutton->setColor(cpucolor);
    widgetlayout->addWidget(m_cpubutton, 3, 1);

    QLabel* receiverlabel = new QLabel(widget);
    receiverlabel->setText(i18n("Receiver color:"));
    widgetlayout->addWidget(receiverlabel, 4, 0, Qt::AlignRight);
    const QColor defaultreceivercolor = kNetReceiverVisualizerColor();
    QColor receivercolor = m_receivercolor;
    if (!receivercolor.isValid()) {
        receivercolor = defaultreceivercolor;
    }
    m_receiverbutton = new KColorButton(widget);
    m_receiverbutton->setDefaultColor(defaultreceivercolor);
    m_receiverbutton->setColor(receivercolor);
    widgetlayout->addWidget(m_receiverbutton, 4, 1);

    QLabel* transmitterlabel = new QLabel(widget);
    transmitterlabel->setText(i18n("Transmitter color:"));
    widgetlayout->addWidget(transmitterlabel, 5, 0, Qt::AlignRight);
    const QColor defaulttransmittercolor = kNetTransmitterVisualizerColor();
    QColor transmittercolor = m_transmittercolor;
    if (!transmittercolor.isValid()) {
        transmittercolor = defaulttransmittercolor;
    }
    m_transmitterbutton = new KColorButton(widget);
    m_transmitterbutton->setDefaultColor(defaulttransmittercolor);
    m_transmitterbutton->setColor(transmittercolor);
    widgetlayout->addWidget(m_transmitterbutton, 5, 1);

    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addItem(m_spacer, 6, 0, 1, 2);

    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("System Monitor"), "utilities-system-monitor");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_hostnameedit, SIGNAL(textChanged(QString)), parent, SLOT(settingsModified()));
    connect(m_portbox, SIGNAL(valueChanged(int)), parent, SLOT(settingsModified()));
    connect(m_updateedit, SIGNAL(timeChanged(QTime)), parent, SLOT(settingsModified()));
    connect(m_cpubutton, SIGNAL(changed(QColor)), parent, SLOT(settingsModified()));
    connect(m_receiverbutton, SIGNAL(changed(QColor)), parent, SLOT(settingsModified()));
    connect(m_transmitterbutton, SIGNAL(changed(QColor)), parent, SLOT(settingsModified()));
}

QGraphicsWidget *SystemMonitor::graphicsWidget()
{
    return m_systemmonitorwidget;
}

void SystemMonitor::slotConfigAccepted()
{
    Q_ASSERT(m_hostnameedit != nullptr);
    Q_ASSERT(m_portbox != nullptr);
    Q_ASSERT(m_updateedit != nullptr);
    Q_ASSERT(m_cpubutton != nullptr);
    Q_ASSERT(m_receiverbutton != nullptr);
    Q_ASSERT(m_transmitterbutton != nullptr);
    m_hostname = m_hostnameedit->text();
    m_port = m_portbox->value();
    m_update = QTime(0, 0, 0).secsTo(m_updateedit->time());
    m_cpucolor = m_cpubutton->color();
    m_receivercolor = m_receiverbutton->color();
    m_transmittercolor = m_transmitterbutton->color();
    KConfigGroup configgroup = config();
    configgroup.writeEntry("hostname", m_hostname);
    configgroup.writeEntry("port", m_port);
    configgroup.writeEntry("update", m_update);
    if (m_cpucolor == kCPUVisualizerColor()) {
        configgroup.writeEntry("cpucolor", QColor());
    } else {
        configgroup.writeEntry("cpucolor", m_cpucolor);
    }
    if (m_receivercolor == kNetReceiverVisualizerColor()) {
        configgroup.writeEntry("netreceivercolor", QColor());
    } else {
        configgroup.writeEntry("netreceivercolor", m_receivercolor);
    }
    if (m_transmittercolor == kNetTransmitterVisualizerColor()) {
        configgroup.writeEntry("nettransmittercolor", QColor());
    } else {
        configgroup.writeEntry("nettransmittercolor", m_transmittercolor);
    }
    emit configNeedsSaving();
    m_systemmonitorwidget->setupMonitors(
        m_hostname, m_port, m_update,
        m_cpucolor, m_receivercolor, m_transmittercolor
    );
}

void SystemMonitor::slotThemeChanged()
{
    KConfigGroup configgroup = config();
    m_hostname = configgroup.readEntry("hostname", s_hostname);
    m_port = configgroup.readEntry("port", s_port);
    m_update = configgroup.readEntry("update", s_update);
    m_cpucolor = configgroup.readEntry("cpucolor", QColor());
    if (!m_cpucolor.isValid()) {
        m_cpucolor = kCPUVisualizerColor();
    }
    m_receivercolor = configgroup.readEntry("netreceivercolor", QColor());
    if (!m_receivercolor.isValid()) {
        m_receivercolor = kNetReceiverVisualizerColor();
    }
    m_transmittercolor = configgroup.readEntry("nettransmittercolor", QColor());
    if (!m_transmittercolor.isValid()) {
        m_transmittercolor = kNetTransmitterVisualizerColor();
    }
    m_systemmonitorwidget->setupMonitors(
        m_hostname, m_port, m_update,
        m_cpucolor, m_receivercolor, m_transmittercolor
    );
}

K_EXPORT_PLASMA_APPLET(system-monitor_applet, SystemMonitor)

#include "moc_system-monitor.cpp"
#include "system-monitor.moc"
