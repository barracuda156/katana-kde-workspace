/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "Application.h"
#include "MainWindow.h"

// OS specific
#include <kde_file.h>

// KDE
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>
#include <KDebug>

#define KONSOLE_VERSION "2.14.2"

using Konsole::Application;

// fill the KAboutData structure with information about contributors to Konsole.
void fillAboutData(KAboutData& aboutData);

// fill the KCmdLineOptions object with konsole specific options.
void fillCommandLineOptions(KCmdLineOptions& options);

// restore sessions saved by KDE.
void restoreSession(Application& app);

// ***
// Entry point into the Konsole terminal application.
// ***
int main(int argc, char** argv)
{
    KAboutData about("konsole",
                     0,
                     ki18nc("@title", "Konsole"),
                     KONSOLE_VERSION,
                     ki18nc("@title", "Terminal emulator"),
                     KAboutData::License_GPL_V2
                    );
    fillAboutData(about);

    KCmdLineArgs::init(argc, argv, &about);
    KCmdLineArgs::addStdCmdLineOptions();  // Qt and KDE options
    KCmdLineOptions konsoleOptions; // Konsole options
    fillCommandLineOptions(konsoleOptions);
    KCmdLineArgs::addCmdLineOptions(konsoleOptions);

    // check for arguments to print help or other information to the
    // terminal, quit if such an argument was found
    if (Application::processHelpArgs(KCmdLineArgs::parsedArgs())) {
        return 0;
    }

    // create a new application instance
    Application app;
    app.enableSessionManagement();

    // make sure the d&d popup menu provided by libkonq get translated.
    KGlobal::locale()->insertCatalog("libkonq");

    restoreSession(app);
    return app.exec();
}

void fillCommandLineOptions(KCmdLineOptions& options)
{
    options.add("profile <name>",
                ki18nc("@info:shell", "Name of profile to use for new Konsole instance"));
    options.add("fallback-profile",
                ki18nc("@info:shell", "Use the internal FALLBACK profile"));
    options.add("workdir <dir>",
                ki18nc("@info:shell", "Set the initial working directory of the new tab or"
                      " window to 'dir'"));
    options.add("hold");
    options.add("noclose",
                ki18nc("@info:shell", "Do not close the initial session automatically when it"
                      " ends."));
    options.add("new-tab",
                ki18nc("@info:shell", "Create a new tab in an existing window rather than"
                      " creating a new window"));
    options.add("tabs-from-file <file>",
                ki18nc("@info:shell", "Create tabs as specified in given tabs configuration"
                      " file"));
    options.add("background-mode",
                ki18nc("@info:shell", "Start Konsole in the background and bring to the front"
                      " when Ctrl+Shift+F12 (by default) is pressed"));
    options.add("separate", ki18n("Run in a separate process"));
    options.add("show-menubar", ki18nc("@info:shell", "Show the menubar, overriding the default setting"));
    options.add("hide-menubar", ki18nc("@info:shell", "Hide the menubar, overriding the default setting"));
    options.add("show-tabbar", ki18nc("@info:shell", "Show the tabbar, overriding the default setting"));
    options.add("hide-tabbar", ki18nc("@info:shell", "Hide the tabbar, overriding the default setting"));
    options.add("fullscreen", ki18nc("@info:shell", "Start Konsole in fullscreen mode"));
    options.add("notransparency",
                ki18nc("@info:shell", "Disable transparent backgrounds, even if the system"
                      " supports them."));
    options.add("list-profiles", ki18nc("@info:shell", "List the available profiles"));
    options.add("list-profile-properties",
                ki18nc("@info:shell", "List all the profile properties names and their type"
                      " (for use with -p)"));
    options.add("p <property=value>",
                ki18nc("@info:shell", "Change the value of a profile property."));
    options.add("!e <cmd>",
                ki18nc("@info:shell", "Command to execute. This option will catch all following"
                      " arguments, so use it as the last option."));
    options.add("+[args]", ki18nc("@info:shell", "Arguments passed to command"));
}

void fillAboutData(KAboutData& aboutData)
{
    aboutData.setProgramIconName("utilities-terminal");

    aboutData.addAuthor(ki18nc("@info:credit", "Kurt Hindenburg"),
                        ki18nc("@info:credit", "General maintainer, bug fixes and general"
                               " improvements"),
                        "kurt.hindenburg@gmail.com");
    aboutData.addAuthor(ki18nc("@info:credit", "Robert Knight"),
                        ki18nc("@info:credit", "Previous maintainer, ported to KDE4"),
                        "robertknight@gmail.com");
    aboutData.addAuthor(ki18nc("@info:credit", "Lars Doelle"),
                        ki18nc("@info:credit", "Original author"),
                        "lars.doelle@on-line.de");
    aboutData.addCredit(ki18nc("@info:credit", "Jekyll Wu"),
                        ki18nc("@info:credit", "Bug fixes and general improvements"),
                        "adaptee@gmail.com");
    aboutData.addCredit(ki18nc("@info:credit", "Waldo Bastian"),
                        ki18nc("@info:credit", "Bug fixes and general improvements"),
                        "bastian@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Stephan Binner"),
                        ki18nc("@info:credit", "Bug fixes and general improvements"),
                        "binner@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Thomas Dreibholz"),
                        ki18nc("@info:credit", "General improvements"),
                        "dreibh@iem.uni-due.de");
    aboutData.addCredit(ki18nc("@info:credit", "Chris Machemer"),
                        ki18nc("@info:credit", "Bug fixes"),
                        "machey@ceinetworks.com");
    aboutData.addCredit(ki18nc("@info:credit", "Francesco Cecconi"),
                        ki18nc("@info:credit", "Bug fixes"),
                        "francesco.cecconi@gmail.com");
    aboutData.addCredit(ki18nc("@info:credit", "Stephan Kulow"),
                        ki18nc("@info:credit", "Solaris support and history"),
                        "coolo@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Alexander Neundorf"),
                        ki18nc("@info:credit", "Bug fixes and improved startup performance"),
                        "neundorf@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Peter Silva"),
                        ki18nc("@info:credit", "Marking improvements"),
                        "Peter.A.Silva@gmail.com");
    aboutData.addCredit(ki18nc("@info:credit", "Lotzi Boloni"),
                        ki18nc("@info:credit", "Embedded Konsole\n"
                               "Toolbar and session names"),
                        "boloni@cs.purdue.edu");
    aboutData.addCredit(ki18nc("@info:credit", "David Faure"),
                        ki18nc("@info:credit", "Embedded Konsole\n"
                               "General improvements"),
                        "faure@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Antonio Larrosa"),
                        ki18nc("@info:credit", "Visual effects"),
                        "larrosa@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Matthias Ettrich"),
                        ki18nc("@info:credit", "Code from the kvt project\n"
                               "General improvements"),
                        "ettrich@kde.org");
    aboutData.addCredit(ki18nc("@info:credit", "Warwick Allison"),
                        ki18nc("@info:credit", "Schema and text selection improvements"),
                        "warwick@troll.no");
    aboutData.addCredit(ki18nc("@info:credit", "Dan Pilone"),
                        ki18nc("@info:credit", "SGI port"),
                        "pilone@slac.com");
    aboutData.addCredit(ki18nc("@info:credit", "Kevin Street"),
                        ki18nc("@info:credit", "FreeBSD port"),
                        "street@iname.com");
    aboutData.addCredit(ki18nc("@info:credit", "Sven Fischer"),
                        ki18nc("@info:credit", "Bug fixes"),
                        "herpes@kawo2.renditionwth-aachen.de");
    aboutData.addCredit(ki18nc("@info:credit", "Dale M. Flaven"),
                        ki18nc("@info:credit", "Bug fixes"),
                        "dflaven@netport.com");
    aboutData.addCredit(ki18nc("@info:credit", "Martin Jones"),
                        ki18nc("@info:credit", "Bug fixes"),
                        "mjones@powerup.com.au");
    aboutData.addCredit(ki18nc("@info:credit", "Lars Knoll"),
                        ki18nc("@info:credit", "Bug fixes"),
                        "knoll@mpi-hd.mpg.de");
    aboutData.addCredit(ki18nc("@info:credit", "Thanks to many others.\n"));
}

void restoreSession(Application& app)
{
    if (app.isSessionRestored()) {
        int n = 1;
        while (KMainWindow::canBeRestored(n))
            app.newMainWindow()->restore(n++);
    }
}

