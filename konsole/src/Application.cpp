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

// Qt
#include <QtCore/qhash.h>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

// KDE
#include <KAction>
#include <KActionCollection>
#include <KCmdLineArgs>
#include <KDebug>
#include <KStandardDirs>

// Konsole
#include "SessionManager.h"
#include "ProfileManager.h"
#include "MainWindow.h"
#include "Session.h"
#include "ShellCommand.h"

using namespace Konsole;

Application::Application()
    : KApplication(),
    _backgroundInstance(nullptr)
{
    static bool firstInstance = true;

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    // handle session management
    if ((args->count() != 0) || !firstInstance || !isSessionRestored()) {
        // create a new window or use an existing one
        MainWindow* window = processWindowArgs(args);

        if (args->isSet("tabs-from-file")) {
            // create new session(s) as described in file
            processTabsFromFileArgs(args, window);
        } else {
            // select profile to use
            Profile::Ptr baseProfile = processProfileSelectArgs(args);

            // process various command-line options which cause a property of the
            // selected profile to be changed
            Profile::Ptr newProfile = processProfileChangeArgs(args, baseProfile);

            // create new session
            Session* session = window->createSession(newProfile, QString());

            if (!args->isSet("close")) {
                session->setAutoClose(false);
            }
        }

        // if the background-mode argument is supplied, start the background
        // session ( or bring to the front if it already exists )
        if (args->isSet("background-mode")) {
            startBackgroundMode(window);
        } else {
            // Qt constrains top-level windows which have not been manually
            // resized (via QWidget::resize()) to a maximum of 2/3rds of the
            //  screen size.
            //
            // This means that the terminal display might not get the width/
            // height it asks for.  To work around this, the widget must be
            // manually resized to its sizeHint().
            //
            // This problem only affects the first time the application is run.
            // run. After that KMainWindow will have manually resized the
            // window to its saved size at this point (so the Qt::WA_Resized
            // attribute will be set)
            if (!window->testAttribute(Qt::WA_Resized))
                window->resize(window->sizeHint());

            window->show();
        }
    }

    firstInstance = false;
    args->clear();
}

Application::~Application()
{
    SessionManager::instance()->closeAllSessions();
    ProfileManager::instance()->saveSettings();
}

MainWindow* Application::newMainWindow()
{
    MainWindow* window = new MainWindow();

    connect(window, SIGNAL(newWindowRequest(Profile::Ptr,QString)),
            this, SLOT(createWindow(Profile::Ptr,QString)));
    connect(window, SIGNAL(viewDetached(Session*)),
            this, SLOT(detachView(Session*)));

    return window;
}

void Application::createWindow(Profile::Ptr profile, const QString& directory)
{
    MainWindow* window = newMainWindow();
    window->createSession(profile, directory);
    window->show();
}

void Application::detachView(Session* session)
{
    MainWindow* window = newMainWindow();
    window->createView(session);
    // Since user is dragging and dropping, move dnd window to where
    // the user has the cursor (correct multiple monitor setups).
    window->move(QCursor::pos());
    window->show();
}

/* Documentation for tab file:
 *
 * ;; is the token separator
 * # at the beginning of line results in line being ignored.
 * supported tokens are title, command and profile.
 *
 * Note that the title is static and the tab will close when the
 * command is complete (do not use --noclose).  You can start new tabs.
 *
 * Examples:
title: This is the title;; command: ssh jupiter
title: Top this!;; command: top
#this line is comment
command: ssh  earth
profile: Zsh
*/
void Application::processTabsFromFileArgs(KCmdLineArgs* args,
        MainWindow* window)
{
    // Open tab configuration file
    const QString tabsFileName(args->getOption("tabs-from-file"));
    QFile tabsFile(tabsFileName);
    if (!tabsFile.open(QFile::ReadOnly)) {
        kWarning() << "ERROR: Cannot open tabs file "
                   << tabsFileName.toLocal8Bit().data();
        // TODO: this will not quit, exit code?
        quit();
    }

    unsigned int sessions = 0;
    while (!tabsFile.atEnd()) {
        QString lineString(tabsFile.readLine().trimmed());
        if ((lineString.isEmpty()) || (lineString[0] == '#'))
            continue;

        QHash<QString, QString> lineTokens;
        QStringList lineParts = lineString.split(";;", QString::SkipEmptyParts);

        for (int i = 0; i < lineParts.size(); ++i) {
            QString key = lineParts.at(i).section(':', 0, 0).trimmed().toLower();
            QString value = lineParts.at(i).section(':', 1, -1).trimmed();
            lineTokens[key] = value;
        }
        // should contain at least one of 'command' and 'profile'
        if (lineTokens.contains("command") || lineTokens.contains("profile")) {
            createTabFromArgs(args, window, lineTokens);
            sessions++;
        } else {
            kWarning() << "Each line should contain at least one of 'command' and 'profile'.";
        }
    }
    tabsFile.close();

    if (sessions < 1) {
        kWarning() << "No valid lines found in "
                   << tabsFileName.toLocal8Bit().data();
        quit();
    }
}

void Application::createTabFromArgs(KCmdLineArgs* args, MainWindow* window,
                                    const QHash<QString, QString>& tokens)
{
    const QString& title = tokens["title"];
    const QString& command = tokens["command"];
    const QString& profile = tokens["profile"];
    const QString& workdir = tokens["workdir"];

    Profile::Ptr baseProfile;
    if (!profile.isEmpty()) {
        baseProfile = ProfileManager::instance()->loadProfile(profile);
    }
    if (!baseProfile) {
        // fallback to default profile
        baseProfile = ProfileManager::instance()->defaultProfile();
    }

    Profile::Ptr newProfile = Profile::Ptr(new Profile(baseProfile));
    newProfile->setHidden(true);

    // FIXME: the method of determining whether to use newProfile does not
    // scale well when we support more fields in the future
    bool shouldUseNewProfile = false;

    if (!command.isEmpty()) {
        newProfile->setProperty(Profile::Command,   command);
        newProfile->setProperty(Profile::Arguments, command.split(' '));
        shouldUseNewProfile = true;
    }

    if (!title.isEmpty()) {
        newProfile->setProperty(Profile::LocalTabTitleFormat, title);
        newProfile->setProperty(Profile::RemoteTabTitleFormat, title);
        shouldUseNewProfile = true;
    }

    if (args->isSet("workdir")) {
        newProfile->setProperty(Profile::Directory, args->getOption("workdir"));
        shouldUseNewProfile = true;
    }

    if (!workdir.isEmpty()) {
        newProfile->setProperty(Profile::Directory, workdir);
        shouldUseNewProfile = true;
    }

    // Create the new session
    Profile::Ptr theProfile = shouldUseNewProfile ? newProfile :  baseProfile;
    Session* session = window->createSession(theProfile, QString());

    if (!args->isSet("close")) {
        session->setAutoClose(false);
    }

    if (!window->testAttribute(Qt::WA_Resized)) {
        window->resize(window->sizeHint());
    }

    // FIXME: this ugly hack here is to make the session start running, so that
    // its tab title is displayed as expected.
    //
    // This is another side effect of the commit fixing BKO 176902.
    window->show();
    window->hide();
}

MainWindow* Application::processWindowArgs(KCmdLineArgs* args)
{
    MainWindow* window = 0;
    if (args->isSet("new-tab")) {
        QListIterator<QWidget*> iter(topLevelWidgets());
        iter.toBack();
        while (iter.hasPrevious()) {
            window = qobject_cast<MainWindow*>(iter.previous());
            if (window != 0)
                break;
        }
    }

    if (window == 0) {
        window = newMainWindow();

        // override default menubar visibility
        if (args->isSet("show-menubar")) {
            window->setMenuBarInitialVisibility(true);
        }
        if (args->isSet("hide-menubar")) {
            window->setMenuBarInitialVisibility(false);
        }
        if (args->isSet("fullscreen")) {
            window->viewFullScreen(true);
        }

        // override default tabbbar visibility
        // FIXME: remove those magic number
        // see ViewContainer::NavigationVisibility
        if (args->isSet("show-tabbar")) {
            // always show
            window->setNavigationVisibility(0);
        }
        if (args->isSet("hide-tabbar")) {
            // never show
            window->setNavigationVisibility(2);
        }
    }
    return window;
}

Profile::Ptr Application::processProfileSelectArgs(KCmdLineArgs* args)
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    if (args->isSet("profile")) {
        Profile::Ptr profile = ProfileManager::instance()->loadProfile(
                                   args->getOption("profile"));
        if (profile)
            return profile;
    } else if (args->isSet("fallback-profile")) {
        Profile::Ptr profile = ProfileManager::instance()->loadProfile("FALLBACK/");
        if (profile)
            return profile;
    }

    return defaultProfile;
}

bool Application::processHelpArgs(KCmdLineArgs* args)
{
    if (args->isSet("list-profiles")) {
        QStringList paths = ProfileManager::instance()->availableProfilePaths();
        foreach(const QString& path, paths) {
            QFileInfo info(path);
            const QByteArray base = info.completeBaseName().toLocal8Bit();
            printf("%s\n", base.constData());
        }
        return true;
    } else if (args->isSet("list-profile-properties")) {
        Profile::Ptr tempProfile = ProfileManager::instance()->defaultProfile();
        const QStringList names = tempProfile->propertiesInfoList();
        foreach(const QString& name, names) {
            const QByteArray namebytes = name.toLocal8Bit();
            printf("%s\n", namebytes.constData());
        }
        return true;
    }
    return false;
}

Profile::Ptr Application::processProfileChangeArgs(KCmdLineArgs* args, Profile::Ptr baseProfile)
{
    bool shouldUseNewProfile = false;

    Profile::Ptr newProfile = Profile::Ptr(new Profile(baseProfile));
    newProfile->setHidden(true);

    // change the initial working directory
    if (args->isSet("workdir")) {
        newProfile->setProperty(Profile::Directory, args->getOption("workdir"));
        shouldUseNewProfile = true;
    }

    // temporary changes to profile options specified on the command line
    foreach(const QString & value , args->getOptionList("p")) {
        ProfileCommandParser parser;

        QHashIterator<Profile::Property, QVariant> iter(parser.parse(value));
        while (iter.hasNext()) {
            iter.next();
            newProfile->setProperty(iter.key(), iter.value());
        }

        shouldUseNewProfile = true;
    }

    // run a custom command
    if (args->isSet("e")) {
        QString commandExec = args->getOption("e");
        QStringList commandArguments;

        // Note: KCmdLineArgs::count() return the number of arguments
        // that aren't options.
        if (args->count() == 0 && KStandardDirs::findExe(commandExec).isEmpty()) {
            // Example: konsole -e "man ls"
            ShellCommand shellCommand(args->getOption("e"));
            commandExec = shellCommand.command();
            commandArguments = shellCommand.arguments();
        } else {
            // Example: konsole -e man ls
            commandArguments << commandExec;
            for ( int i = 0 ; i < args->count() ; i++ )
                commandArguments << args->arg(i);
        }

        if (commandExec.startsWith(QLatin1String("./")))
            commandExec = QDir::currentPath() + commandExec.mid(1);

        newProfile->setProperty(Profile::Command, commandExec);
        newProfile->setProperty(Profile::Arguments, commandArguments);

        shouldUseNewProfile = true;
    }

    if (shouldUseNewProfile) {
        return newProfile;
    } else {
        return baseProfile;
    }
}

void Application::startBackgroundMode(MainWindow* window)
{
    if (_backgroundInstance) {
        return;
    }

    KAction* action = window->actionCollection()->addAction("toggle-background-window");
    action->setObjectName(QLatin1String("Konsole Background Mode"));
    action->setText(i18n("Toggle Background Window"));
    action->setGlobalShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F12));

    connect(action, SIGNAL(triggered()),
            this, SLOT(toggleBackgroundInstance()));

    _backgroundInstance = window;
}

void Application::toggleBackgroundInstance()
{
    Q_ASSERT(_backgroundInstance);

    if (!_backgroundInstance->isVisible()) {
        _backgroundInstance->show();
        // ensure that the active terminal display has the focus. Without
        // this, an odd problem occurred where the focus widget would change
        // each time the background instance was shown
        _backgroundInstance->setFocus();
    } else {
        _backgroundInstance->hide();
    }
}

#include "moc_Application.cpp"

