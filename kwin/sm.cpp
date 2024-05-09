/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "sm.h"

#include <unistd.h>
#include <stdlib.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kuser.h>

#include "workspace.h"
#include "client.h"
#include <kapplication.h>
#include <kdebug.h>

namespace KWin
{

// Workspace

/*!
  Stores the current session in the config file

  \sa loadSessionInfo()
 */
void Workspace::storeSession(KConfig* config)
{
    KConfigGroup cg(config, "Session");
    int count =  0;

    for (ClientList::Iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* c = (*it);
        QByteArray sessionId = c->sessionId();
        QByteArray wmCommand = c->wmCommand();
        if (sessionId.isEmpty()) {
            if (wmCommand.isEmpty()) {
                continue;
            }
        }
        count++;
        storeClient(cg, count, c);
    }
    cg.writeEntry("count", count);
    cg.writeEntry("desktop", VirtualDesktopManager::self()->current());
}

void Workspace::storeClient(KConfigGroup &cg, int num, Client *c)
{
    c->setSessionInteract(false); //make sure we get the real values
    QString n = QString::number(num);
    cg.writeEntry(QString("sessionId") + n, c->sessionId());
    cg.writeEntry(QString("windowRole") + n, c->windowRole());
    cg.writeEntry(QString("wmCommand") + n, c->wmCommand());
    cg.writeEntry(QString("resourceName") + n, c->resourceName());
    cg.writeEntry(QString("resourceClass") + n, c->resourceClass());
    cg.writeEntry(QString("geometry") + n, QRect(c->calculateGravitation(true), c->clientSize()));   // FRAME
    cg.writeEntry(QString("restore") + n, c->geometryRestore());
    cg.writeEntry(QString("fsrestore") + n, c->geometryFSRestore());
    cg.writeEntry(QString("maximize") + n, (int) c->maximizeMode());
    cg.writeEntry(QString("fullscreen") + n, (int) c->fullScreenMode());
    cg.writeEntry(QString("desktop") + n, c->desktop());;
    // the config entry is called "iconified" for back. comp. reasons
    // (kconf_update script for updating session files would be too complicated)
    cg.writeEntry(QString("iconified") + n, c->isMinimized());
    cg.writeEntry(QString("opacity") + n, c->opacity());
    // the config entry is called "sticky" for back. comp. reasons
    cg.writeEntry(QString("sticky") + n, c->isOnAllDesktops());
    cg.writeEntry(QString("shaded") + n, c->isShade());
    // the config entry is called "staysOnTop" for back. comp. reasons
    cg.writeEntry(QString("staysOnTop") + n, c->keepAbove());
    cg.writeEntry(QString("keepBelow") + n, c->keepBelow());
    cg.writeEntry(QString("skipTaskbar") + n, c->skipTaskbar(true));
    cg.writeEntry(QString("skipPager") + n, c->skipPager());
    cg.writeEntry(QString("skipSwitcher") + n, c->skipSwitcher());
    // not really just set by user, but name kept for back. comp. reasons
    cg.writeEntry(QString("userNoBorder") + n, c->noBorder());
    cg.writeEntry(QString("windowType") + n, windowTypeToTxt(c->windowType()));
    cg.writeEntry(QString("shortcut") + n, c->shortcut().toString());
    cg.writeEntry(QString("active") + n, c->isActive());
    cg.writeEntry(QString("stackingOrder") + n, unconstrained_stacking_order.indexOf(c));
    // KConfig doesn't support long so we need to live with less precision on 64-bit systems
    cg.writeEntry(QString("tabGroup") + n, static_cast<int>(reinterpret_cast<long>(c->tabGroup())));
}

/*!
  Loads the session information from the config file.

  \sa storeSession()
 */
void Workspace::loadSessionInfo()
{
    session.clear();
    KConfigGroup cg(kapp->sessionConfig(), "Session");

    int count =  cg.readEntry("count", 0);
    for (int i = 1; i <= count; i++) {
        QString n = QString::number(i);
        SessionInfo* info = new SessionInfo();
        session.append(info);
        info->sessionId = cg.readEntry(QString("sessionId") + n, QString()).toLatin1();
        info->windowRole = cg.readEntry(QString("windowRole") + n, QString()).toLatin1();
        info->wmCommand = cg.readEntry(QString("wmCommand") + n, QString()).toLatin1();
        info->resourceName = cg.readEntry(QString("resourceName") + n, QString()).toLatin1();
        info->resourceClass = cg.readEntry(QString("resourceClass") + n, QString()).toLower().toLatin1();
        info->geometry = cg.readEntry(QString("geometry") + n, QRect());
        info->restore = cg.readEntry(QString("restore") + n, QRect());
        info->fsrestore = cg.readEntry(QString("fsrestore") + n, QRect());
        info->maximized = cg.readEntry(QString("maximize") + n, 0);
        info->fullscreen = cg.readEntry(QString("fullscreen") + n, 0);
        info->desktop = cg.readEntry(QString("desktop") + n, 0);
        info->minimized = cg.readEntry(QString("iconified") + n, false);
        info->opacity = cg.readEntry(QString("opacity") + n, 1.0);
        info->onAllDesktops = cg.readEntry(QString("sticky") + n, false);
        info->shaded = cg.readEntry(QString("shaded") + n, false);
        info->keepAbove = cg.readEntry(QString("staysOnTop") + n, false);
        info->keepBelow = cg.readEntry(QString("keepBelow") + n, false);
        info->skipTaskbar = cg.readEntry(QString("skipTaskbar") + n, false);
        info->skipPager = cg.readEntry(QString("skipPager") + n, false);
        info->skipSwitcher = cg.readEntry(QString("skipSwitcher") + n, false);
        info->noBorder = cg.readEntry(QString("userNoBorder") + n, false);
        info->windowType = txtToWindowType(cg.readEntry(QString("windowType") + n, QString()).toLatin1());
        info->shortcut = cg.readEntry(QString("shortcut") + n, QString());
        info->active = cg.readEntry(QString("active") + n, false);
        info->stackingOrder = cg.readEntry(QString("stackingOrder") + n, -1);
        info->tabGroup = cg.readEntry(QString("tabGroup") + n, 0);
        info->tabGroupClient = NULL;
    }
}

/*!
  Returns a SessionInfo for client \a c. The returned session
  info is removed from the storage. It's up to the caller to delete it.

  This function is called when a new window is mapped and must be managed.
  We try to find a matching entry in the session.

  May return 0 if there's no session info for the client.
 */
SessionInfo* Workspace::takeSessionInfo(Client* c)
{
    SessionInfo *realInfo = 0;
    QByteArray sessionId = c->sessionId();
    QByteArray windowRole = c->windowRole();
    QByteArray wmCommand = c->wmCommand();
    QByteArray resourceName = c->resourceName();
    QByteArray resourceClass = c->resourceClass();

    // First search ``session''
    if (! sessionId.isEmpty()) {
        // look for a real session managed client (algorithm suggested by ICCCM)
        foreach (SessionInfo *info, session) {
            if (info->sessionId == sessionId && sessionInfoWindowTypeMatch(c, info)) {
                if (! windowRole.isEmpty()) {
                    if (info->windowRole == windowRole) {
                        realInfo = info;
                        session.removeAll(info);
                        break;
                    }
                } else {
                    if (info->windowRole.isEmpty()
                            && info->resourceName == resourceName
                            && info->resourceClass == resourceClass) {
                        realInfo = info;
                        session.removeAll(info);
                        break;
                    }
                }
            }
        }
    } else {
        // look for a sessioninfo with matching features.
        foreach (SessionInfo * info, session) {
            if (info->resourceName == resourceName
                    && info->resourceClass == resourceClass
                    && sessionInfoWindowTypeMatch(c, info)) {
                if (wmCommand.isEmpty() || info->wmCommand == wmCommand) {
                    realInfo = info;
                    session.removeAll(info);
                    break;
                }
            }
        }
    }

    // Set tabGroupClient for other clients in the same group
    if (realInfo && realInfo->tabGroup) {
        foreach (SessionInfo * info, session) {
            if (!info->tabGroupClient && info->tabGroup == realInfo->tabGroup)
                info->tabGroupClient = c;
        }
    }

    return realInfo;
}

bool Workspace::sessionInfoWindowTypeMatch(Client* c, SessionInfo* info)
{
    if (info->windowType == -2) {
        // undefined (not really part of NET::WindowType)
        return !c->isSpecialWindow();
    }
    return info->windowType == c->windowType();
}

static const char* const window_type_names[] = {
    "Unknown", "Normal" , "Desktop", "Dock", "Toolbar", "Menu", "Dialog",
    "TopMenu", "Utility", "Splash"
};
// change also the two functions below when adding new entries

const char* Workspace::windowTypeToTxt(NET::WindowType type)
{
    if (type >= NET::Unknown && type <= NET::Splash)
        return window_type_names[ type + 1 ]; // +1 (unknown==-1)
    if (type == -2)   // undefined (not really part of NET::WindowType)
        return "Undefined";
    kFatal(1212) << "Unknown Window Type" ;
    return NULL;
}

NET::WindowType Workspace::txtToWindowType(const char* txt)
{
    for (int i = NET::Unknown;
            i <= NET::Splash;
            ++i)
        if (qstrcmp(txt, window_type_names[ i + 1 ]) == 0)     // +1
            return static_cast< NET::WindowType >(i);
    return static_cast< NET::WindowType >(-2);   // undefined
}

} // namespace

#include "moc_sm.cpp"
