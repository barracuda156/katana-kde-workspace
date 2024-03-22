/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#ifndef KDISPLAYMANAGER_H
#define KDISPLAYMANAGER_H

#include "kworkspace.h"
#include "kworkspace_export.h"

#include <QString>
#include <QList>

struct KWORKSPACE_EXPORT SessEnt
{
    QString display;
    QString user;
    QString session;
    int vt;
    bool self;
    bool tty;
};

typedef QList<SessEnt> SessList;

class KDisplayManagerPrivate;

class KWORKSPACE_EXPORT KDisplayManager
{
public:
    KDisplayManager();
    ~KDisplayManager();

    bool canShutdown();
    void shutdown(KWorkSpace::ShutdownType shutdownType);

    bool isSwitchable();
    void newSession();
    bool localSessions(SessList &list);
    bool switchVT(int vt);
    void lockSwitchVT(int vt);

    static QString sess2Str(const SessEnt &se);

private:
    Q_DISABLE_COPY(KDisplayManager);
    KDisplayManagerPrivate *d;

};

#endif // KDISPLAYMANAGER_H

