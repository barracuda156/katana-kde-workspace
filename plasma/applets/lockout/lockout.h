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

#ifndef LOCKOUT_H
#define LOCKOUT_H

#include <QGraphicsLinearLayout>
#include <QCheckBox>
#include <QSpacerItem>
#include <Plasma/Applet>
#include <Plasma/IconWidget>
#include <KConfigDialog>
#include <KMessageWidget>

class LockoutDialog;

class LockoutApplet : public Plasma::Applet
{
    Q_OBJECT
public:
    enum DoWhat {
        DoNothing = 0,
        DoSwitch = 1,
        // shutdown is asked for by plasma-desktop
        DoToRam = 2,
        DoToDisk = 3,
        DoHybrid = 4
    };

    LockoutApplet(QObject *parent, const QVariantList &args);
    ~LockoutApplet();

    // Plasma::Applet reimplementations
    void init() final;
    void createConfigurationInterface(KConfigDialog *parent) final;

protected:
    // Plasma::Applet reimplementation
    void constraintsEvent(Plasma::Constraints constraints) final;

private Q_SLOTS:
    void slotUpdateButtons();
    void slotUpdateToolTips();
    void slotSwitch();
    void slotShutdown();
    void slotToRam();
    void slotToDisk();
    void slotHybrid();
    void slotDoIt();
    void slotCheckButtons();
    void slotConfigAccepted();

private:
    void updateSizes();

    QGraphicsLinearLayout* m_layout;
    Plasma::IconWidget* m_switchwidget;
    Plasma::IconWidget* m_shutdownwidget;
    Plasma::IconWidget* m_toramwidget;
    Plasma::IconWidget* m_todiskwidget;
    Plasma::IconWidget* m_hybridwidget;
    bool m_showswitch;
    bool m_showshutdown;
    bool m_showtoram;
    bool m_showtodisk;
    bool m_showhybrid;
    bool m_confirmswitch;
    bool m_confirmshutdown;
    bool m_confirmtoram;
    bool m_confirmtodisk;
    bool m_confirmhybrid;
    KMessageWidget* m_buttonsmessage;
    QCheckBox* m_switchbox;
    QCheckBox* m_shutdownbox;
    QCheckBox* m_torambox;
    QCheckBox* m_todiskbox;
    QCheckBox* m_hybridbox;
    QSpacerItem* m_spacer;
    QCheckBox* m_switchconfirmbox;
    QCheckBox* m_shutdownconfirmbox;
    QCheckBox* m_toramconfirmbox;
    QCheckBox* m_todiskconfirmbox;
    QCheckBox* m_hybridconfirmbox;
    QSpacerItem* m_spacer2;
    LockoutDialog* m_dialog;
    LockoutApplet::DoWhat m_dowhat;
};

K_EXPORT_PLASMA_APPLET(lockout, LockoutApplet)

#endif // LOCKOUT_H
