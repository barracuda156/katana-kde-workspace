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

#ifndef CALENDAR_H
#define CALENDAR_H

#include <QTimer>
#include <KConfigDialog>
#include <KCModuleProxy>
#include <Plasma/PopupApplet>
#include <Plasma/Svg>

class CalendarWidget;

class CalendarApplet : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    CalendarApplet(QObject *parent, const QVariantList &args);

    void init() final;
    QGraphicsWidget *graphicsWidget() final;
    void createConfigurationInterface(KConfigDialog *parent) final;

protected:
    void constraintsEvent(Plasma::Constraints constraints) final;
    void popupEvent(bool show) final;

private slots:
    void slotTimeout();
    void slotConfigAccepted();
    void slotLocaleChanged();

private:
    void updateIcon();
    void updateToolTip();

    CalendarWidget* m_calendarwidget;
    Plasma::Svg* m_svg;
    QTimer* m_timer;
    int m_day;
    int m_utcday;
    KCModuleProxy* m_kcmclockproxy;
};

K_EXPORT_PLASMA_APPLET(calendar, CalendarApplet)

#endif // CALENDAR_H
