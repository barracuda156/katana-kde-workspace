/***************************************************************************
                          kdesudo.cpp  -  description
                             -------------------
    begin                : Sam Feb 15 15:42:12 CET 2003
    copyright            : (C) 2003 by Robert Gruber
                                       <rgruber@users.sourceforge.net>
                           (C) 2007 by Martin Böhm <martin.bohm@kubuntu.org>
                                       Anthony Mercatante <tonio@kubuntu.org>
                                       Canonical Ltd (Jonathan Riddell
                                                      <jriddell@ubuntu.com>)
                           (C) 2009-2015 by Harald Sitter <sitter@kde.org>

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdesktopfile.h>
#include <kiconloader.h>
#include <kicontheme.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kapplication.h>

#include <config.h>

#if defined(HAVE_PR_SET_DUMPABLE)
#  include <sys/prctl.h>
#elif defined(HAVE_PROCCTL)
#  include <unistd.h>
#  include <sys/procctl.h>
#endif

#include "kdesudo.h"

int main(int argc, char **argv)
{
    // Disable tracing to prevent arbitrary apps reading password out of memory.
#if defined(HAVE_PR_SET_DUMPABLE)
    prctl(PR_SET_DUMPABLE, 0);
#elif defined(HAVE_PROCCTL)
    int ctldata = PROC_TRACE_CTL_DISABLE;
    procctl(P_PID, ::getpid(), PROC_TRACE_CTL, &ctldata);
#endif

    KAboutData about(
        "kdesudo", 0, ki18n("KdeSudo"),
        "3.4.2.3", ki18n("Sudo frontend for KDE"),
        KAboutData::License_GPL,
        ki18n("(C) 2007 - 2008 Anthony Mercatante")
    );

    about.addAuthor(ki18n("Robert Gruber"), KLocalizedString(),
                    "rgruber@users.sourceforge.net", "http://test.com");
    about.addAuthor(ki18n("Anthony Mercatante"), KLocalizedString(),
                    "tonio@ubuntu.com");
    about.addAuthor(ki18n("Martin Böhm"), KLocalizedString(),
                    "martin.bohm@kubuntu.org");
    about.addAuthor(ki18n("Jonathan Riddell"), KLocalizedString(),
                    "jriddell@ubuntu.com");
    about.addAuthor(ki18n("Harald Sitter"), KLocalizedString(),
                    "apachelogger@ubuntu.com");

    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add("u <runas>", ki18n("sets a runas user"));
    options.add("c <command>", ki18n("The command to execute"));
    options.add("i <icon name>", ki18n("Specify icon to use in the password dialog"));
    options.add("d", ki18n("Do not show the command to be run in the dialog"));
    options.add("p <priority>", ki18n("Process priority, between 0 and 100, 0 the lowest [50]"));
    options.add("r", ki18n("Use realtime scheduling"));
    options.add("f <file>", ki18n("Use target UID if <file> is not writeable"));
    options.add("nodbus", ki18n("Do not start a message bus"));
    options.add("comment <dialog text>", ki18n("The comment that should be "
                "displayed in the dialog"));
    options.add("attach <winid>", ki18n("Makes the dialog transient for an X app specified by winid"));
    options.add("desktop <desktop file>", ki18n("Manual override for automatic desktop file detection"));

    options.add("+command", ki18n("The command to execute"));

    KCmdLineArgs::addCmdLineOptions(options);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    KApplication a;
    a.disableSessionManagement();

    QString executable;
    QStringList executableList;
    KDesktopFile *desktopFile = nullptr;

    if (args->isSet("c")) {
        executable = args->getOption("c");
    }

    if (args->count() && executable.isEmpty()) {
        QString command = args->arg(0);
        QStringList commandlist = command.split(" ");
        executable = commandlist[0];
    }

    /* Have to make sure the executable is only the binary name */
    executableList = executable.split(" ");
    executable = executableList[0];

    executableList = executable.split("/");
    executable = executableList[executableList.count() - 1];

    /* Kubuntu has a bug in it - this is a workaround for it */
    KGlobal::dirs()->addResourceDir("apps", "/usr/share/applications/kde4");
    KGlobal::dirs()->addResourceDir("apps", "/usr/share/kde4/services");
    KGlobal::dirs()->addResourceDir("apps", "/usr/share/applications");

    QString path = getenv("PATH");
    QStringList pathList = path.split(":");
    for (int i = 0; i < pathList.count(); i++) {
        executable.remove(pathList[i]);
    }

    if (args->isSet("desktop")) {
        desktopFile = new KDesktopFile(args->getOption("desktop"));
    } else {
        desktopFile = new KDesktopFile(executable + ".desktop");
    }

    /* icon parsing */
    QString icon;
    if (args->isSet("i")) {
        icon = args->getOption("i");
    } else {
        QString iconName = desktopFile->readIcon();
        KIconLoader *loader = KIconLoader::global();
        icon = loader->iconPath(iconName, -1 * KIconLoader::StdSizes(KIconLoader::SizeHuge), true);
    }

    a.setQuitOnLastWindowClosed(false);
    KdeSudo kdesudo(icon, desktopFile->readName());
    return a.exec();
}
