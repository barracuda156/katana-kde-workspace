/*
    This file is part of the KDE project
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

#ifndef SHUTDOWNDLG_H
#define SHUTDOWNDLG_H

#include <QGraphicsScene>
#include <QGraphicsWidget>
#include <QGraphicsGridLayout>
#include <QEventLoop>
#include <QTimer>
#include <Plasma/Dialog>
#include <Plasma/Label>
#include <Plasma/Separator>
#include <Plasma/IconWidget>
#include <Plasma/PushButton>
#include "kworkspace/kworkspace.h"

// The confirmation dialog
class KSMShutdownDlg : public Plasma::Dialog
{
    Q_OBJECT
public:
    static bool confirmShutdown(bool maysd, bool choose, KWorkSpace::ShutdownType &sdtype);

public Q_SLOTS:
    void slotLogout();
    void slotHalt();
    void slotReboot();
    void slotOk();
    void slotCancel();
    void slotTimeout();

protected:
    // Plasma::Dialog reimplementations
    void hideEvent(QHideEvent *event) final;
    bool eventFilter(QObject *watched, QEvent *event) final;

private:
    KSMShutdownDlg(QWidget *parent, bool maysd, bool choose, KWorkSpace::ShutdownType sdtype);
    ~KSMShutdownDlg();

    bool execDialog();
    void interrupt();

    QGraphicsScene* m_scene;
    QGraphicsWidget* m_widget;
    QGraphicsGridLayout* m_layout;
    Plasma::Label* m_titlelabel;
    Plasma::Separator* m_separator;
    Plasma::IconWidget* m_logoutwidget;
    Plasma::IconWidget* m_rebootwidget;
    Plasma::IconWidget* m_haltwidget;
    Plasma::PushButton* m_okbutton;
    Plasma::PushButton* m_cancelbutton;
    QEventLoop* m_eventloop;
    QTimer* m_timer;
    int m_second;
    KWorkSpace::ShutdownType m_shutdownType;
};

#endif // SHUTDOWNDLG_H
