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

#ifndef WEATHER_H
#define WEATHER_H

#include <QComboBox>
#include <QSpacerItem>
#include <QLabel>
#include <KDoubleNumInput>
#include <KConfigDialog>
#include <KUnitConversion>
#include <Plasma/PopupApplet>

class WeatherWidget;

class WeatherApplet : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    WeatherApplet(QObject *parent, const QVariantList &args);
    ~WeatherApplet();

    // Plasma::Applet reimplementations
    void init() final;
    void createConfigurationInterface(KConfigDialog *parent) final;
    // Plasma::PopupApplet reimplementation
    QGraphicsWidget* graphicsWidget() final;

private Q_SLOTS:
    void slotCheckLocation();
    void slotConfigAccepted();

private:
    friend WeatherWidget;
    WeatherWidget *m_weatherwidget;
    KTemperature::KTempUnit m_tempunit;
    QComboBox* m_tempunitbox;
    QString m_location;
    QComboBox* m_locationbox;
    float m_latitude;
    QLabel* m_latitudelabel;
    KDoubleNumInput* m_latitudeinput;
    float m_longitude;
    QLabel* m_longitudelabel;
    KDoubleNumInput* m_longitudeinput;
    QSpacerItem* m_spacer;
};

#endif // WEATHER_H
