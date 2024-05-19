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

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QAction>
#include <KBookmarkManager>
#include <KConfigDialog>
#include <KPluginSelector>
#include <KSharedConfig>
#include <Plasma/PopupApplet>
#include <Plasma/RunnerManager>

class LauncherAppletWidget;

class LauncherApplet : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    LauncherApplet(QObject *parent, const QVariantList &args);

    void init() final;
    QGraphicsWidget* graphicsWidget() final;
    QList<QAction*> contextualActions() final;
    void createConfigurationInterface(KConfigDialog *parent) final;

    // internal
    void resetState();
    KBookmarkManager* bookmarkManager() const;
    Plasma::RunnerManager* runnerManager() const;

private Q_SLOTS:
    void slotEditMenu();
    void slotConfigAccepted();

private:
    friend LauncherAppletWidget;
    LauncherAppletWidget* m_launcherwidget;
    KBookmarkManager* m_bookmarkmanager;
    Plasma::RunnerManager* m_runnermanager;
    QAction* m_editmenuaction;
    KPluginSelector* m_selector;
    KSharedConfig::Ptr m_shareconfig;
    KConfigGroup m_configgroup;
};

K_EXPORT_PLASMA_APPLET(launcher, LauncherApplet)

#endif // LAUNCHER_H
