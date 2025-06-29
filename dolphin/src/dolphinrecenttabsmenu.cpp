/***************************************************************************
 * Copyright (C) 2014 by Emmanuel Pescosta <emmanuelpescosta099@gmail.com> *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "dolphinrecenttabsmenu.h"

#include <KLocalizedString>
#include <KAcceleratorManager>
#include <KMimeType>
#include <KMenu>
#include <KIcon>
#include <kio/global.h>

DolphinRecentTabsMenu::DolphinRecentTabsMenu(QObject* parent) :
    KActionMenu(KIcon("edit-undo"), i18n("Recently Closed Tabs"), parent)
{
    setDelayed(false);
    setEnabled(false);

    m_clearListAction = new QAction(i18n("Empty Recently Closed Tabs"), this);
    m_clearListAction->setIcon(KIcon("edit-clear-list"));
    addAction(m_clearListAction);

    addSeparator();

    connect(menu(), SIGNAL(triggered(QAction*)),
            this, SLOT(handleAction(QAction*)));
}

void DolphinRecentTabsMenu::rememberClosedTab(const KUrl& primaryUrl, const KUrl& secondaryUrl)
{
    QAction* action = new QAction(menu());
    action->setText(primaryUrl.path());

    action->setIcon(QIcon(KIO::pixmapForUrl(primaryUrl)));

    KUrl::List urls;
    urls << primaryUrl;
    urls << secondaryUrl;
    action->setData(QVariant::fromValue(urls));

    // Add the closed tab menu entry after the separator and
    // "Empty Recently Closed Tabs" entry
    if (menu()->actions().size() == 2) {
        addAction(action);
    } else {
        insertAction(menu()->actions().at(2), action);
    }

    // Assure that only up to 6 closed tabs are shown in the menu.
    // 8 because of clear action + separator + 6 closed tabs
    if (menu()->actions().size() > 8) {
        removeAction(menu()->actions().last());
    }
    setEnabled(true);
    KAcceleratorManager::manage(menu());
}

void DolphinRecentTabsMenu::handleAction(QAction* action)
{
    if (action == m_clearListAction) {
        // Clear all actions except the "Empty Recently Closed Tabs"
        // action and the separator
        QList<QAction*> actions = menu()->actions();
        const int count = actions.size();
        for (int i = 2; i < count; ++i) {
            removeAction(actions.at(i));
        }
    } else {
        const KUrl::List urls = action->data().value<KUrl::List>();
        if (urls.count() == 2) {
            emit restoreClosedTab(urls.first(), urls.last());
        }
        removeAction(action);
        delete action;
        action = 0;
    }

    if (menu()->actions().count() <= 2) {
        setEnabled(false);
    }
}
