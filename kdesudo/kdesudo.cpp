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

#include "kdesudo.h"

#include <QtCore/QDataStream>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpassworddialog.h>
#include <kpushbutton.h>
#include <kshell.h>
#include <kstandarddirs.h>
#include <kwindowsystem.h>
#include <ktemporaryfile.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>
#include <cstdlib>

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
    bool attach = args->isSet("attach");

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

    m_dialog = new KPasswordDialog();
    m_dialog->setDefaultButton(KDialog::Ok);

    if (attach) {
        const WId winid = args->getOption("attach").toULong();
        KWindowSystem::setMainWindow(m_dialog, winid);
    }

    m_process = new QProcess(this);

    /* load the icon */
    m_dialog->setPixmap(icon);

    // Parsins args

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

    connect(m_process, SIGNAL(readyReadStandardOutput()),
            this, SLOT(parseOutput()));

    connect(m_process, SIGNAL(readyReadStandardError()),
            this, SLOT(parseOutput()));

    connect(m_process, SIGNAL(finished(int)),
            this, SLOT(procExited(int)));

    connect(m_dialog, SIGNAL(gotPassword(const QString & , bool)),
            this, SLOT(pushPassword(const QString &)));

    connect(m_dialog, SIGNAL(rejected()),
            this, SLOT(slotCancel()));

    // Generate the xauth cookie and put it in a tempfile
    // set the environment variables to reflect that.
    // Default cookie-timeout is 60 sec. .
    // 'man xauth' for more info on xauth cookies.
    m_tmpName = KTemporaryFile::filePath("/tmp/kdesudo-XXXXXXXXXX-xauth");

    const QString disp = QString::fromLocal8Bit(qgetenv("DISPLAY"));
    if (disp.isEmpty()) {
        kError() << "$DISPLAY is not set.";
        exit(1);
    }

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
    m_process->setProcessEnvironment(processEnv);

    QStringList processArgs;
    {
        // Do not cache credentials to avoid security risks caused by the fact
        // that kdesudo could be invoked from anyting inside the user session
        // potentially in such a way that it uses the cached credentials of a
        // previously kdesudo run in that same scope.
        processArgs << "-k";
        if (changeUID) {
            processArgs << "-H" << "-S" << "-p" << "passprompt";

            if (!runas.isEmpty()) {
                processArgs << "-u" << runas;
            }
            processArgs << "--";
        }

        if (realtime) {
            processArgs << "nice" << "-n" << "10";
            m_dialog->addCommentLine(i18n("Priority:"), i18n("realtime:") +
                                     QChar(' ') + QString("50/100"));
            processArgs << "--";
        } else if (priority) {
            QString n = args->getOption("p");
            int intn = atoi(n.toUtf8());
            intn = (intn * 40 / 100) - (20 + 0.5);

            processArgs << "nice" << "-n" << QString::number(intn);
            m_dialog->addCommentLine(i18n("Priority:"), n + QString("/100"));
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
        }

        else if (args->count()) {
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
            m_dialog->addCommentLine(i18n("Command:"), cmd);
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
            m_dialog->setPrompt(defaultComment.arg(appname));
        } else {
            m_dialog->setPrompt(defaultComment.arg(cmd));
        }
    } else {
        m_dialog->setPrompt(comment);
    }

    m_process->setProcessChannelMode(QProcess::MergedChannels);

    m_process->start("sudo", processArgs);
}

KdeSudo::~KdeSudo()
{
    delete m_dialog;
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

void KdeSudo::parseOutput()
{
    QString strOut = m_process->readAllStandardOutput();

    static int badpass = 0;

    if (strOut.contains("try again")) {
        badpass++;
        if (badpass == 1) {
            m_dialog->addCommentLine(i18n("<b>Warning: </b>"), i18n("<b>Incorrect password, please try again.</b>"));
            m_dialog->show();
        } else if (badpass == 2) {
            m_dialog->show();
        } else {
            error(i18n("Wrong password! Exiting..."));
        }
    // NOTE: "command not found" comes from `sudo` while "No such file or directory" comes from
    // either `nice` or `dbus-run-session`
    } else if (strOut.contains("command not found") || strOut.contains("No such file or directory")) {
        error(i18n("Command not found!"));
    } else if (strOut.contains("is not in the sudoers file")) {
        error(i18n("Your username is unknown to sudo!"));
    } else if (strOut.contains("is not allowed to execute")) {
        error(i18n("Your user is not allowed to run the specified command!"));
    } else if (strOut.contains("is not allowed to run sudo on")) {
        error(i18n("Your user is not allowed to run sudo on this host!"));
    } else if (strOut.contains("may not run sudo on")) {
        error(i18n("Your user is not allowed to run sudo on this host!"));
    } else if (strOut.contains("passprompt") || strOut.contains("PIN (CHV2)")) {
        m_dialog->setPassword(QString());
        m_dialog->show();
    } else {
        const QByteArray bytesOut = strOut.toLocal8Bit();
        fprintf(stdout, "%s", bytesOut.constData());
    }
}

void KdeSudo::procExited(int exitCode)
{
    if (!m_error) {
        KApplication::kApplication()->exit(exitCode);
    }
}

void KdeSudo::pushPassword(const QString &pwd)
{
    m_process->write(pwd.toLocal8Bit() + "\n");
}

void KdeSudo::slotCancel()
{
    KApplication::kApplication()->exit(1);
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
