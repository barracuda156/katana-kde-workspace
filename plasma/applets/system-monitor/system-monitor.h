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

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <QColor>
#include <QComboBox>
#include <KConfigDialog>
#include <KLineEdit>
#include <KTimeEdit>
#include <KIntNumInput>
#include <KColorButton>
#include <Plasma/PopupApplet>

class SystemMonitorWidget;

class SystemMonitor : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    SystemMonitor(QObject *parent, const QVariantList &args);
    ~SystemMonitor();

    // Plasma::Applet reimplementations
    void init() final;
    void createConfigurationInterface(KConfigDialog *parent) final;
    // Plasma::PopupApplet reimplementation
    QGraphicsWidget* graphicsWidget() final;

private Q_SLOTS:
    void slotConfigAccepted();

private:
    friend SystemMonitorWidget;
    SystemMonitorWidget *m_systemmonitorwidget;
    QString m_hostname;
    int m_port;
    int m_update;
    QColor m_cpucolor;
    QColor m_receivercolor;
    QColor m_transmittercolor;
    int m_temperatureunit;
    KLineEdit* m_hostnameedit;
    KIntNumInput* m_portbox;
    KTimeEdit* m_updateedit;
    KColorButton* m_cpubutton;
    KColorButton* m_receiverbutton;
    KColorButton* m_transmitterbutton;
    QComboBox* m_temperaturebox;
};

#endif // SYSTEM_MONITOR_H
