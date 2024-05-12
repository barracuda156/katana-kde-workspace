/*
 *   Copyright 2009 by Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "launch.h"

#include <KDebug>
#include <KIcon>
#include <KToolInvocation>
#include <Plasma/Containment>

AppLauncher::AppLauncher(QObject *parent, const QVariantList &args)
    : Plasma::ContainmentActions(parent, args),
      m_action(new QAction(this))
{
    m_menu = new KMenu();
    connect(m_menu, SIGNAL(triggered(QAction*)), this, SLOT(switchTo(QAction*)));

    m_action->setMenu(m_menu);
}

AppLauncher::~AppLauncher()
{
    delete m_menu;
}

void AppLauncher::contextEvent(QEvent *event)
{
    makeMenu();
    m_menu->adjustSize();
    m_menu->exec(popupPosition(m_menu->size(), event));
}

QList<QAction*> AppLauncher::contextualActions()
{
    makeMenu();

    QList<QAction*> list;
    list << m_action;
    return list;
}

void AppLauncher::makeMenu()
{
    m_menu->clear();

    // add the whole kmenu
    KServiceGroup::Ptr group = KServiceGroup::root();
    if (group && group->isValid()) {
        m_menu->setTitle(group->caption());
        m_menu->setIcon(KIcon(group->icon()));
        addApp(m_menu, group);
    }
}

void AppLauncher::addApp(QMenu *menu, KServiceGroup::Ptr group)
{
    const QString name = group->name();
    if (group->noDisplay()) {
        kDebug() << "hidden group" << name;
        return;
    }

    foreach (const KServiceGroup::Ptr subGroup, group->groupEntries(KServiceGroup::NoOptions)) {
        if (subGroup->noDisplay() || subGroup->childCount() < 1) {
            continue;
        }
        QMenu *subMenu = menu->addMenu(KIcon(subGroup->icon()), subGroup->caption());
        addApp(subMenu, subGroup);
        if (subMenu->isEmpty()) {
            delete subMenu;
        }
    }

    foreach (const KService::Ptr app, group->serviceEntries(KServiceGroup::NoOptions)) {
        if (app->noDisplay()) {
            kDebug() << "hidden entry" << app->name();
            continue;
        }
        QAction *action = menu->addAction(KIcon(app->icon()), app->name());
        action->setData(app->entryPath());
    }
}

void AppLauncher::switchTo(QAction *action)
{
    const QString entrypath = action->data().toString();
    kDebug() << "running" << entrypath;
    KToolInvocation::self()->startServiceByStorageId(entrypath);
}

#include "moc_launch.cpp"
