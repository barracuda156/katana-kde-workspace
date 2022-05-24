/*
  * This file is part of the KDE project
  * Copyright (C) 2009 Shaun Reich <shaun.reich@kdemail.net>
  * Copyright (C) 2006-2008 Rafael Fernández López <ereslibre@kde.org>
  * Copyright (C) 2001 George Staikos <staikos@kde.org>
  * Copyright (C) 2000 Matej Koss <koss@miesto.sk>
  *                    David Faure <faure@kde.org>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Library General Public
  * License version 2 as published by the Free Software Foundation.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Library General Public License for more details.
  *
  * You should have received a copy of the GNU Library General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
*/

#include "uiserver.h"
#include "uiserver_p.h"

#include "progresslistmodel.h"

#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kdebug.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>

int main(int argc, char **argv)
{
    //  GS 5/2001 - I changed the name to "KDE" to make it look better
    //              in the titles of dialogs which are displayed.
    KAboutData aboutdata("kuiserver", "kdelibs4", ki18n("Job Manager"),
                         "0.8", ki18n("KDE Job Manager"),
                         KAboutData::License_GPL_V2, ki18n("(C) 2000-2009, KDE Team"));

    aboutdata.addAuthor(ki18n("Shaun Reich"), ki18n("Maintainer"), "shaun.reich@kdemail.net");
    aboutdata.addAuthor(ki18n("Rafael Fernández López"), ki18n("Former Maintainer"), "ereslibre@kde.org");
    aboutdata.addAuthor(ki18n("David Faure"), ki18n("Former maintainer"), "faure@kde.org");
    aboutdata.addAuthor(ki18n("Matej Koss"), ki18n("Developer"), "koss@miesto.sk");

    KCmdLineArgs::init(argc, argv, &aboutdata);

    KApplication app;
    // This app is started automatically, no need for session management
    app.disableSessionManagement();
    app.setQuitOnLastWindowClosed(false);

    QDBusConnection session = QDBusConnection::sessionBus();
    if (!session.isConnected()) {
        kWarning() << "No DBUS session-bus found. Check if you have started the DBUS server.";
        return 1;
    }
    QDBusReply<bool> sessionReply = session.interface()->isServiceRegistered("org.kde.kuiserver");
    if (sessionReply.isValid() && sessionReply.value() == true) {
        kWarning() << "Another instance of kuiserver is already running!";
        return 2;
    }

    ProgressListModel model;

    return app.exec();
}
