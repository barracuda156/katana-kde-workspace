/***************************************************************************
                          kdesudo.cpp  -  the implementation of the
                                          admin granting sudo widget
                             -------------------
    begin                : Sam Feb 15 15:42:12 CET 2003
    copyright            : (C) 2003 by Robert Gruber
                                       <rgruber@users.sourceforge.net>
                           (C) 2007 by Martin Böhm <martin.bohm@kubuntu.org>
                                       Anthony Mercatante <tonio@kubuntu.org>
                                       Canonical Ltd (Jonathan Riddell
                                                      <jriddell@ubuntu.com>)

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "config.h"
#include "kdesudo.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <kiconloader.h>
#include <kicontheme.h>
#include <kglobal.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdesktopfile.h>
#include <kshell.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>
#include <kdebug.h>

#include <sys/stat.h>

#if defined(HAVE_PR_SET_DUMPABLE)
#  include <sys/prctl.h>
#elif defined(HAVE_PROCCTL)
#  include <unistd.h>
#  include <sys/procctl.h>
#endif

KdeSudo::KdeSudo(const QString &icon, const QString &appname)
    : QObject(),
    m_process(nullptr),
    m_error(false)
{
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    bool realtime = args->isSet("r");
    bool priority = args->isSet("p");
    bool showCommand = (!args->isSet("d"));
    bool changeUID = true;
    QString runas = args->getOption("u");
    QString cmd;

    if (!args->isSet("c") && !args->count()) {
        KMessageBox::information(
            nullptr,
            i18n(
                "No command arguments supplied!\n"
                "Usage: kdesudo [-u <runas>] <command>\n"
                "KdeSudo will now exit..."
            )
        );
        exit(0);
    }

    m_process = new QProcess(this);

    /* Get the comment out of cli args */
    QString comment = args->getOption("comment");

    if (args->isSet("f")) {
        // If file is writeable, do not change uid
        QString file = args->getOption("f");
        if (!file.isEmpty()) {
            if (file.at(0) != '/') {
                KStandardDirs dirs;
                file = dirs.findResource("config", file);
                if (file.isEmpty()) {
                    kError() << "Config file not found: " << file;
                    exit(1);
                }
            }
            QFileInfo fi(file);
            if (!fi.exists()) {
                kError() << "File does not exist: " << file;
                exit(1);
            }
            if (fi.isWritable()) {
                changeUID = false;
            }
        }
    }

    connect(
        m_process, SIGNAL(finished(int)),
        this, SLOT(procExited(int))
    );

    const QString disp = QString::fromLocal8Bit(qgetenv("DISPLAY"));
    if (disp.isEmpty()) {
        kError() << "$DISPLAY is not set.";
        exit(1);
    }

    const QString kaskpass = KStandardDirs::findExe("kaskpass");
    if (kaskpass.isEmpty()) {
        error(i18n("Could not find kaskpass program"));
        // no application loop yet
        exit(1);
        return;
    }

    // Generate the xauth cookie and put it in a tempfile
    // set the environment variables to reflect that.
    // Default cookie-timeout is 60 sec. .
    // 'man xauth' for more info on xauth cookies.
    m_tmpName = KTemporaryFile::filePath("/tmp/kdesudo-XXXXXXXXXX-xauth");

    // Create two processes, one for each xauth call
    QProcess xauth_ext;
    QProcess xauth_merge;

    // This makes "xauth extract - $DISPLAY | xauth -f /tmp/kdesudo-... merge -"
    xauth_ext.setStandardOutputProcess(&xauth_merge);

    // Start the first
    xauth_ext.start("xauth", QStringList() << "extract" << "-" << disp, QIODevice::ReadOnly);
    if (!xauth_ext.waitForStarted()) {
        return;
    }

    // Start the second
    xauth_merge.start("xauth", QStringList() << "-f" << m_tmpName << "merge" << "-", QIODevice::WriteOnly);
    if (!xauth_merge.waitForStarted()) {
        return;
    }

    // If they ended, close it all
    if (!xauth_merge.waitForFinished()) {
        return;
    }
    xauth_merge.close();

    if (!xauth_ext.waitForFinished()) {
        return;
    }
    xauth_ext.close();

    // non root users need to be able to read the xauth file.
    // the xauth file is deleted when kdesudo exits. security?
    if (!runas.isEmpty() && runas != "root" && QFile::exists(m_tmpName)) {
        chmod(QFile::encodeName(m_tmpName), 0644);
    }

    QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
    processEnv.insert("LANG", "C");
    processEnv.insert("LC_ALL", "C");
    processEnv.insert("DISPLAY", disp);
    processEnv.insert("XAUTHORITY", m_tmpName);
    processEnv.insert("SUDO_ASKPASS", kaskpass);
    processEnv.insert("KASKPASS_CALLER", "KdeSudo");
    processEnv.insert("KASKPASS_ICON", icon);
    if (args->isSet("attach")) {
        processEnv.insert("KASKPASS_MAINWINDOW", args->getOption("attach"));
    }

    QStringList processArgs;
    {
        // Do not cache credentials to avoid security risks caused by the fact
        // that kdesudo could be invoked from anyting inside the user session
        // potentially in such a way that it uses the cached credentials of a
        // previously kdesudo run in that same scope.
        processArgs << "-k";
        // Preserve all possible kaskpass environment variables
        processArgs << "--preserve-env=KASKPASS_ICON,KASKPASS_MAINWINDOW,KASKPASS_LABEL0,KASKPASS_COMMENT0,KASKPASS_LABEL1,KASKPASS_COMMENT1,KASKPASS_PROMPT";
        // Always use kaskpass
        processArgs << "-A";
        if (changeUID) {
            processArgs << "-H";

            if (!runas.isEmpty()) {
                processArgs << "-u" << runas;
            }
            processArgs << "--";
        }

        if (realtime) {
            processArgs << "nice" << "-n" << "10";
            processEnv.insert("KASKPASS_LABEL0", i18n("Priority:"));
            processEnv.insert("KASKPASS_COMMENT0", QString::fromLatin1("50/100"));
            processArgs << "--";
        } else if (priority) {
            QString n = args->getOption("p");
            int intn = atoi(n.toUtf8());
            intn = (intn * 40 / 100) - (20 + 0.5);

            processArgs << "nice" << "-n" << QString::number(intn);
            processEnv.insert("KASKPASS_LABEL0", i18n("Priority:"));
            processEnv.insert("KASKPASS_COMMENT0", QString::fromLatin1("%1/100").arg(n));
            processArgs << "--";
        }

        if (args->isSet("dbus")) {
            processArgs << "dbus-run-session";
        }

        if (args->isSet("c")) {
            QString command = args->getOption("c");
            cmd += command;
            processArgs << "sh";
            processArgs << "-c";
            processArgs << command;
        } else if (args->count()) {
            for (int i = 0; i < args->count(); i++) {
                if ((!args->isSet("c")) && (i == 0)) {
                    QStringList argsSplit = KShell::splitArgs(args->arg(i));
                    for (int j = 0; j < argsSplit.count(); j++) {
                        processArgs << validArg(argsSplit[j]);
                        if (j == 0) {
                            cmd += validArg(argsSplit[j]) + QChar(' ');
                        } else {
                            cmd += KShell::quoteArg(validArg(argsSplit[j])) + QChar(' ');
                        }
                    }
                } else {
                    processArgs << validArg(args->arg(i));
                    cmd += validArg(args->arg(i)) + QChar(' ');
                }
            }
        }
        // strcmd needs to be defined
        if (showCommand && !cmd.isEmpty()) {
            processEnv.insert("KASKPASS_LABEL1", i18n("Command:"));
            processEnv.insert("KASKPASS_COMMENT1", cmd);
        }
    }

    if (comment.isEmpty()) {
        QString defaultComment = "<b>%1</b> " + i18n("needs administrative privileges. ");

        if (runas.isEmpty() || runas == "root") {
            defaultComment += i18n("Please enter your password.");
        } else {
            defaultComment += i18n("Please enter password for <b>%1</b>.", runas);
        }

        if (!appname.isEmpty()) {
            processEnv.insert("KASKPASS_PROMPT", defaultComment.arg(appname));
        } else {
            processEnv.insert("KASKPASS_PROMPT", defaultComment.arg(cmd));
        }
    } else {
        processEnv.insert("KASKPASS_PROMPT", comment);
    }

    m_process->setProcessEnvironment(processEnv);

    m_process->start("sudo", processArgs);
}

KdeSudo::~KdeSudo()
{
    if (m_process) {
        m_process->terminate();
        m_process->waitForFinished(3000);
    }
    if (!m_tmpName.isEmpty()) {
        QFile::remove(m_tmpName);
    }
}

void KdeSudo::error(const QString &msg)
{
    m_error = true;
    KMessageBox::error(nullptr, msg);
    KApplication::kApplication()->exit(1);
}

void KdeSudo::procExited(int exitCode)
{
    if (!m_error) {
        KApplication::kApplication()->exit(exitCode);
    }
}

QString KdeSudo::validArg(QString arg)
{
    QChar firstChar = arg.at(0);
    QChar lastChar = arg.at(arg.length() - 1);

    if ((firstChar == '"' && lastChar == '"') || (firstChar == '\'' && lastChar == '\'')) {
        arg = arg.remove(0, 1);
        arg = arg.remove(arg.length() - 1, 1);
    }
    return arg;
}

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
