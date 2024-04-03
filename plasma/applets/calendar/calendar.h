/***************************************************************************
 *   Copyright 2008 by Davide Bettio <davide.bettio@kdemail.net>           *
 *   Copyright 2009 by John Layt <john@layt.net>                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef CALENDARTEST_H
#define CALENDARTEST_H

#include <QTimer>
#include <Plasma/PopupApplet>
#include <Plasma/Svg>

class CalendarWidget;

class CalendarApplet : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    CalendarApplet(QObject *parent, const QVariantList &args);

    void init() final;
    void constraintsEvent(Plasma::Constraints constraints) final;
    QGraphicsWidget *graphicsWidget() final;
    void popupEvent(bool show) final;

private slots:
    void slotCheckDate();

private:
    void paintIcon();

    CalendarWidget *m_calendarwidget;
    Plasma::Svg *m_svg;
    QTimer *m_timer;
    int m_day;
};

K_EXPORT_PLASMA_APPLET(calendar, CalendarApplet)

#endif
