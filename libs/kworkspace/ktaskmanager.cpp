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

#include "ktaskmanager.h"

#include <QMutex>
#include <QX11Info>
#include <KGlobal>
#include <KWindowSystem>
#include <KDebug>
#include <netwm.h>

static bool kIsTaskWindow(const WId window)
{
    const KWindowInfo kwindowinfo = KWindowSystem::windowInfo(window, NET::WMWindowType | NET::WMState);
    NET::WindowType kwindowtype = kwindowinfo.windowType(
        NET::NormalMask | NET::DesktopMask | NET::DockMask |
        NET::ToolbarMask | NET::MenuMask | NET::DialogMask |
        NET::UtilityMask | NET::SplashMask
    );
    if (kwindowtype != NET::Normal && kwindowtype != NET::Unknown &&
        kwindowtype != NET::Dialog && kwindowtype != NET::Utility) {
        return false;
    }
    if (kwindowinfo.state() & NET::SkipTaskbar) {
        return false;
    }
    return true;
}

class KTaskManagerPrivate
{
public:
    KTaskManagerPrivate(KTaskManager *ktaskmanager);

    QList<WId> tasks;

    void _k_slotNewWindow(const WId window);
    void _k_slotChangedWindow(const WId window);
    void _k_slotRemovedWindow(const WId window);

private:
    QMutex m_mutex;
    KTaskManager* m_taskmanager;
};

KTaskManagerPrivate::KTaskManagerPrivate(KTaskManager *ktaskmanager)
    : m_taskmanager(ktaskmanager)
{
    foreach (const WId window, KWindowSystem::stackingOrder()) {
        if (!kIsTaskWindow(window)) {
            continue;
        }
        kDebug() << "adding task window" << window;
        tasks.append(window);
    }
}

void KTaskManagerPrivate::_k_slotNewWindow(const WId window)
{
    if (!kIsTaskWindow(window)) {
        return;
    }
    QMutexLocker locker(&m_mutex);
    kDebug() << "new task window" << window;
    tasks.append(window);
    emit m_taskmanager->taskAdded(window);
}

void KTaskManagerPrivate::_k_slotChangedWindow(const WId window)
{
    QMutexLocker locker(&m_mutex);
    if (tasks.contains(window)) {
        kDebug() << "changed task window" << window;
        emit m_taskmanager->taskChanged(window);
    }
}

void KTaskManagerPrivate::_k_slotRemovedWindow(const WId window)
{
    QMutexLocker locker(&m_mutex);
    const int indexofwindow = tasks.indexOf(window);
    if (indexofwindow >= 0) {
        kDebug() << "removed task window" << window;
        tasks.removeAt(indexofwindow);
        emit m_taskmanager->taskRemoved(window);
    }
}


K_GLOBAL_STATIC(KTaskManager, globalktaskmanager)

KTaskManager::KTaskManager(QObject *parent)
    : QObject(parent),
    d(new KTaskManagerPrivate(this))
{
    connect(
        KWindowSystem::self(), SIGNAL(windowAdded(WId)),
        this, SLOT(_k_slotNewWindow(WId))
    );
    connect(
        KWindowSystem::self(), SIGNAL(windowChanged(WId)),
        this, SLOT(_k_slotChangedWindow(WId))
    );
    connect(
        KWindowSystem::self(), SIGNAL(windowRemoved(WId)),
        this, SLOT(_k_slotRemovedWindow(WId))
    );
}

KTaskManager::~KTaskManager()
{
    delete d;
}

QList<WId> KTaskManager::tasks() const
{
    return d->tasks;
}

bool KTaskManager::isActive(const WId task)
{
    const WId activewindow = KWindowSystem::activeWindow();
    return (task == activewindow || KWindowSystem::transientFor(task) == activewindow);
}

bool KTaskManager::demandsAttention(const WId task)
{
    KWindowInfo kwindowinfo = KWindowSystem::windowInfo(
        task,
        NET::WMState | NET::XAWMState
    );
    if (kwindowinfo.hasState(NET::DemandsAttention)) {
        return true;
    }
    kwindowinfo = KWindowSystem::windowInfo(
        KWindowSystem::transientFor(task),
        NET::WMState | NET::XAWMState
    );
    return kwindowinfo.hasState(NET::DemandsAttention);
}

void KTaskManager::activateRaiseOrIconify(const WId task)
{
    if (isActive(task)) {
        KWindowSystem::minimizeWindow(task);
        return;
    }
    KWindowSystem::activateWindow(task);
    KWindowSystem::raiseWindow(task);
}

KTaskManager* KTaskManager::self()
{
    return globalktaskmanager;
}

#include "moc_ktaskmanager.cpp"
