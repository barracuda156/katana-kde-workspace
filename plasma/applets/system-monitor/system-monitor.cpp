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
#include <Plasma/SignalPlotter>
#include <KDebug>

typedef QList<QByteArray> SystemMonitorSensors;

static const int s_monitorsid = -1;
static const char* const s_systemuptimesensor = "system/uptime";
static const char* const s_cpusystemloadsensor = "cpu/system/TotalLoad";
static const int s_updatetimeout = 1000;

static QColor kCPUVisualizerColor()
{
    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
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

private Q_SLOTS:
    void slotRequestValues();
    void slotSensorValue(const QByteArray &sensor, const float value);

private:
    SystemMonitor* m_systemmonitor;
    QGraphicsLinearLayout* m_layout;
    SystemMonitorClient* m_systemmonitorclient;
    Plasma::SignalPlotter* m_cpuplotter;
};


SystemMonitorWidget::SystemMonitorWidget(SystemMonitor* systemmonitor)
    : QGraphicsWidget(systemmonitor),
    m_systemmonitor(systemmonitor),
    m_layout(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_systemmonitorclient = new SystemMonitorClient(this);
    connect(
        m_systemmonitorclient, SIGNAL(sensorValue(QByteArray,float)),
        this, SLOT(slotSensorValue(QByteArray,float))
    );
    m_cpuplotter = new Plasma::SignalPlotter(this);
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
    m_layout->addItem(m_cpuplotter);
    setLayout(m_layout);

    slotRequestValues();
}

SystemMonitorWidget::~SystemMonitorWidget()
{
}

void SystemMonitorWidget::slotRequestValues()
{
    m_systemmonitorclient->requestValue(s_systemuptimesensor);
    m_systemmonitorclient->requestValue(s_cpusystemloadsensor);
    QTimer::singleShot(s_updatetimeout, this, SLOT(slotRequestValues()));
}

void SystemMonitorWidget::slotSensorValue(const QByteArray &sensor, const float value)
{
    if (sensor == s_cpusystemloadsensor) {
        m_cpuplotter->addSample(QList<double>() << double(value));
    }
}


SystemMonitor::SystemMonitor(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_systemmonitorwidget(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_system-monitor");
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    m_systemmonitorwidget = new SystemMonitorWidget(this);
    setPopupIcon("utilities-system-monitor");
}

SystemMonitor::~SystemMonitor()
{
    delete m_systemmonitorwidget;
}

void SystemMonitor::init()
{
}

QGraphicsWidget *SystemMonitor::graphicsWidget()
{
    return m_systemmonitorwidget;
}

K_EXPORT_PLASMA_APPLET(system-monitor_applet, SystemMonitor)

#include "moc_system-monitor.cpp"
#include "system-monitor.moc"
