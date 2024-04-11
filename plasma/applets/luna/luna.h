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

#ifndef LUNA_H
#define LUNA_H

#include <QTimer>
#include <Plasma/Applet>
#include <Plasma/Svg>

class LunaApplet : public Plasma::Applet
{
    Q_OBJECT
public:
    LunaApplet(QObject *parent, const QVariantList &args);

    // Plasma::Applet reimplementations
    void init() final;
    void paintInterface(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        const QRect &contentsRect);
protected:
    // Plasma::Applet reimplementation
    void constraintsEvent(Plasma::Constraints constraints) final;

private Q_SLOTS:
    void slotTimeout();

private:
    Plasma::Svg* m_svg;
    QTimer* m_timer;
    int m_phase;
};

K_EXPORT_PLASMA_APPLET(luna, LunaApplet)

#endif // LUNA_H
