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

#include <QTimer>
#include <QGraphicsLinearLayout>
#include <Plasma/Theme>
#include <Plasma/Frame>
#include <Plasma/SignalPlotter>
#include <KDebug>

typedef QList<QByteArray> SystemMonitorSensors;

static const int s_monitorsid = -1;
static const char* const s_systemuptimesensor = "system/uptime";
static const char* const s_cpusystemloadsensor = "cpu/system/TotalLoad";
// TODO: hardcoded
static const char* const s_netreceiverdatasensor = "network/interfaces/eno1/receiver/data";
static const char* const s_nettransmitterdatasensor = "network/interfaces/eno1/transmitter/data";
static const int s_updatetimeout = 1000;
static const QSizeF s_minimumvisualizersize = QSizeF(120, 70);

static Plasma::Frame* kMakeFrame(QGraphicsWidget *parent)
{
    Plasma::Frame* plasmaframe = new Plasma::Frame(parent);
    plasmaframe->setFrameShadow(Plasma::Frame::Sunken);
    plasmaframe->setMinimumSize(s_minimumvisualizersize);
    plasmaframe->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    QGraphicsLinearLayout* plasmaframelayout = new QGraphicsLinearLayout(plasmaframe);
    plasmaframelayout->setContentsMargins(0, 0, 0, 0);
    plasmaframe->setLayout(plasmaframelayout);
    return plasmaframe;
}

void kAddItem(QGraphicsWidget *parent, QGraphicsWidget *widget)
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

class SystemMonitorClient : public QObject, public KSGRD::SensorClient
{
    Q_OBJECT
public:
    SystemMonitorClient(QObject *parent);
    ~SystemMonitorClient();

    SystemMonitorSensors sensors() const;
    void requestValue(const QByteArray &sensor) const;

Q_SIGNALS:
    void sensorValue(const QByteArray &sensor, const float value);

private Q_SLOTS:
    void slotUpdate();

protected:
    void answerReceived(int id, const QList<QByteArray> &answer) final;
    void sensorLost(int id) final;

private:
    SystemMonitorSensors m_sensors;
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

SystemMonitorSensors SystemMonitorClient::sensors() const
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
    const QString sensorname = QString::fromLatin1(sensor.constData(), sensor.size());
    KSGRD::SensorMgr->sendRequest("localhost", sensorname, (KSGRD::SensorClient*)this, sensorid);
}

void SystemMonitorClient::slotUpdate()
{
    KSGRD::SensorMgr->sendRequest("localhost", "monitors", (KSGRD::SensorClient*)this, s_monitorsid);
}

void SystemMonitorClient::answerReceived(int id, const QList<QByteArray> &answer)
{
    if (id == s_monitorsid) {
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
    SystemMonitor* m_systemmonitor;
    QGraphicsLinearLayout* m_layout;
    SystemMonitorClient* m_systemmonitorclient;
    Plasma::Frame* m_cpuframe;
    Plasma::SignalPlotter* m_cpuplotter;
    Plasma::Frame* m_netframe;
    Plasma::SignalPlotter* m_netplotter;
    QList<double> m_netsample;
};


SystemMonitorWidget::SystemMonitorWidget(SystemMonitor* systemmonitor)
    : QGraphicsWidget(systemmonitor),
    m_systemmonitor(systemmonitor),
    m_layout(nullptr),
    m_systemmonitorclient(nullptr),
    m_cpuframe(nullptr),
    m_cpuplotter(nullptr),
    m_netframe(nullptr),
    m_netplotter(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_systemmonitorclient = new SystemMonitorClient(this);
    connect(
        m_systemmonitorclient, SIGNAL(sensorValue(QByteArray,float)),
        this, SLOT(slotSensorValue(QByteArray,float))
    );

    m_cpuframe = kMakeFrame(this);
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

    m_netframe = kMakeFrame(this);
    m_netplotter = new Plasma::SignalPlotter(m_netframe);
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
    kAddItem(m_netframe, m_netplotter);
    m_layout->addItem(m_netframe);

    setLayout(m_layout);
}

SystemMonitorWidget::~SystemMonitorWidget()
{
}

void SystemMonitorWidget::slotUpdateLayout()
{
    QTimer::singleShot(s_updatetimeout, this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::slotRequestValues()
{
    m_netsample.clear();
    m_netsample.append(0.0);
    m_netsample.append(0.0);
    m_systemmonitorclient->requestValue(s_systemuptimesensor);
    m_systemmonitorclient->requestValue(s_cpusystemloadsensor);
    m_systemmonitorclient->requestValue(s_netreceiverdatasensor);
    m_systemmonitorclient->requestValue(s_nettransmitterdatasensor);
    QTimer::singleShot(s_updatetimeout, this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::slotSensorValue(const QByteArray &sensor, const float value)
{
    if (sensor == s_cpusystemloadsensor) {
        m_cpuplotter->addSample(QList<double>() << double(value));
    } else if (sensor == s_netreceiverdatasensor) {
        m_netsample[0] = double(value);
        m_netplotter->addSample(m_netsample);
    } else if (sensor == s_nettransmitterdatasensor) {
        m_netsample[1] = double(value);
        m_netplotter->addSample(m_netsample);
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
