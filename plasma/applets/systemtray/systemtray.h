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

#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QMutex>
#include <QTimer>
#include <QGraphicsLinearLayout>
#include <Plasma/Applet>
#include <Plasma/IconWidget>

class SystemTrayApplet : public Plasma::Applet
{
    Q_OBJECT
public:
    SystemTrayApplet(QObject *parent, const QVariantList &args);
    ~SystemTrayApplet();

    // Plasma::Applet reimplementations
    void init() final;

protected:
    // Plasma::Applet reimplementation
    void constraintsEvent(Plasma::Constraints constraints) final;

private Q_SLOTS:
    void slotAppletDestroyed(Plasma::Applet *plasmaapplet);
    void slotUpdateVisibility();
    void slotShowHidden();

private:
    void updateLayout();
    void updateApplets(const Plasma::Constraints constraints);

    QMutex m_mutex;
    QGraphicsLinearLayout* m_layout;
    QList<Plasma::Applet*> m_applets;
    Plasma::IconWidget* m_arrowicon;
    bool m_showinghidden;
    QTimer* m_popuptimer;
};

#endif // SYSTEMTRAY_H
