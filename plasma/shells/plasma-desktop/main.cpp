/*
 *   Copyright 2006-2007 Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2,
 *   or (at your option) any later version.
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

#include <QDateTime>
#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>
#include <KIcon>
#include <KDebug>
#include <kdeversion.h>

#include "plasmaapp.h"

static const char description[] = I18N_NOOP( "The KDE desktop, panels and widgets workspace application." );

int main(int argc, char **argv)
{
    kDebug() << "!!{} STARTUP TIME" << QTime().msecsTo(QTime::currentTime()) << "START" << "(line:" << __LINE__ << ")";

    KAboutData aboutData("plasma-desktop", 0, ki18n("Plasma Desktop Shell"),
                         KDE_VERSION_STRING, ki18n(description), KAboutData::License_GPL,
                         ki18n("Copyright 2006-2009, The KDE Team"));
    aboutData.addAuthor(ki18n("Aaron J. Seigo"),
                        ki18n("Author and maintainer"),
                        "aseigo@kde.org");
    aboutData.addCredit(ki18n("John Lions"),
                        ki18n("In memory of his contributions, 1937-1998."),
                        0, "http://en.wikipedia.org/wiki/John_Lions");

    // plasma-desktop is the session manager
    ::unsetenv("SESSION_MANAGER");

    KCmdLineArgs::init(argc, argv, &aboutData);

    PlasmaApp *app = PlasmaApp::self();
    QApplication::setWindowIcon(KIcon("plasma"));

    int rc = app->exec();
    delete app;
    return rc;
}
