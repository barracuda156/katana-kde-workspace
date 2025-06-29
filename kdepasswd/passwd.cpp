/* vi: ts=8 sts=4 sw=4
 *
 * This file is part of the KDE project, module kdesu.
 * Copyright (C) 1999,2000 Geert Jansen <jansen@kde.org>

 Permission to use, copy, modify, and distribute this software
 and its documentation for any purpose and without fee is hereby
 granted, provided that the above copyright notice appear in all
 copies and that both that the copyright notice and this
 permission notice and warranty disclaimer appear in supporting
 documentation, and that the name of the author not be used in
 advertising or publicity pertaining to distribution of the
 software without specific, written prior permission.

 The author disclaim all warranties with regard to this
 software, including all implied warranties of merchantability
 and fitness.  In no event shall the author be liable for any
 special, indirect or consequential damages or any damages
 whatsoever resulting from loss of use, data or profits, whether
 in an action of contract, negligence or other tortious action,
 arising out of or in connection with the use or performance of
 this software.

 * passwd.cpp: Change a user's password.
 */

#include "passwd.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <kdebug.h>
#include <kstandarddirs.h>
#include <kuser.h>

PasswdProcess::PasswdProcess(const QString &user)
{
    uid_t userid = -1;

    if (user.isEmpty()) {
        const KUser kuser(::getuid());
        if (!kuser.isValid()) {
            kDebug(1512) << "You don't exist!\n";
            return;
        }
        m_User = kuser.loginName();
    } else {
        const KUser kuser(user);
        if (!kuser.isValid()) {
            kDebug(1512) << "User " << user << "does not exist.\n";
            return;
        }
        m_User = user;
    }
    bOtherUser = (userid != ::getuid());
}


PasswdProcess::~PasswdProcess()
{
}

int PasswdProcess::checkCurrent(const char *oldpass)
{
    return exec(oldpass, 0L, 1);
}

int PasswdProcess::exec(const char *oldpass, const char *newpass, int check)
{
    if (m_User.isEmpty()) {
        return -1;
    }
    //    if (check)
    //        setTerminal(true);

    // Try to set the default locale to make the parsing of the output
    // of `passwd' easier.
    ::setenv("LANG","C", true /* override */);

    QList<QByteArray> args;
    if (bOtherUser) {
        args += m_User.toLocal8Bit();
    }
    int ret = PtyProcess::exec("passwd", args);
    if (ret < 0) {
        kDebug(1512) << "Passwd not found!\n";
        return PasswdNotFound;
    }

    ret = ConversePasswd(oldpass, newpass, check);
    if (ret < 0) {
        kDebug(1512) << "Conversation with passwd failed. pid = " << pid() << ret;
    }

    if ((waitForChild() != 0) && !check) {
        return PasswordNotGood;
    }

    return ret;
}


/*
 * The tricky thing is to make this work with a lot of different passwd
 * implementations. We _don't_ want implementation specific routines.
 * Return values: -1 = unknown error, 0 = ok, >0 = error code.
 */

int PasswdProcess::ConversePasswd(const char *oldpass, const char *newpass, int check)
{
    QByteArray line, errline;
    int state = 0;

    while (state != 7) {
        line = readLine();
        if (line.isNull()) {
            return -1;
        }

        if (state == 0 && isPrompt(line, "new")) {
            // If root is changing a user's password,
            // passwd can't prompt for the original password.
            // Therefore, we have to start at state=2.
            state = 2;
        }

        switch (state) {
            case 0: {
                // Eat garbage, wait for prompt
                m_Error += line+'\n';
                if (isPrompt(line, "password")) {
                    WaitSlave();
                    write(fd(), oldpass, strlen(oldpass));
                    write(fd(), "\n", 1);
                    state++;
                    break;
                }
                if (m_bTerminal) {
                    fputs(line, stdout);
                }
                break;
            }

            case 1:
            case 3:
            case 6: {
                // Wait for \n
                if (line.isEmpty()) {
                    state++;
                    break;
                }
                // error
                return -1;
            }

            case 2: {
                m_Error = "";
                if( line.contains("again")) {
                    m_Error = line;
                    kill(m_Pid, SIGKILL);
                    waitForChild();
                    return PasswordIncorrect;
                }
                // Wait for second prompt.
                errline = line;  // use first line for error message
                while (!isPrompt(line, "new")) {
                    line = readLine();
                    if (line.isNull()) {
                        // We didn't get the new prompt so assume incorrect password.
                        if (m_bTerminal) {
                            fputs(errline, stdout);
                        }
                        m_Error = errline;
                        return PasswordIncorrect;
                    }
                }

                // we have the new prompt
                if (check) {
                    kill(m_Pid, SIGKILL);
                    waitForChild();
                    return 0;
                }
                WaitSlave();
                write(fd(), newpass, strlen(newpass));
                write(fd(), "\n", 1);
                state++;
                break;
            }

            case 4: {
                // Wait for third prompt
                if (isPrompt(line, "re")) {
                    WaitSlave();
                    write(fd(), newpass, strlen(newpass));
                    write(fd(), "\n", 1);
                    state += 2;
                    break;
                }
                // Warning or error about the new password
                if (m_bTerminal) {
                    fputs(line, stdout);
                }
                m_Error = line + '\n';
                state++;
                break;
            }

            case 5: {
                // Wait for either a "Reenter password" or a "Enter password" prompt
                if (isPrompt(line, "re")) {
                    WaitSlave();
                    write(fd(), newpass, strlen(newpass));
                    write(fd(), "\n", 1);
                    state++;
                    break;
                } else if (isPrompt(line, "password")) {
                    kill(m_Pid, SIGKILL);
                    waitForChild();
                    return PasswordNotGood;
                }
                if (m_bTerminal) {
                    fputs(line, stdout);
                }
                m_Error += line + '\n';
                break;
            }
        }
    }

    // Are we ok or do we still get an error thrown at us?
    m_Error = "";
    state = 0;
    while (state != 1) {
        line = readLine();
        if (line.isNull()) {
            // No more input... OK
            return 0;
        }
        if (isPrompt(line, "password")) {
            // Uh oh, another prompt. Not good!
            kill(m_Pid, SIGKILL);
            waitForChild();
            return PasswordNotGood;
        }
        m_Error += line + '\n'; // Collect error message
    }

    kDebug(1512) << "Conversation ended successfully.\n";
    return 0;
}

bool PasswdProcess::isPrompt(const QByteArray &line, const char *word)
{
    unsigned i, j, colon;
    unsigned int lineLength(line.length());
    for (i = 0,j = 0, colon = 0; i < lineLength; ++i) {
        if (line[i] == ':') {
            j = i; ++colon;
            continue;
        }
        if (!isspace(line[i])) {
            ++j;
        }
    }

    if ((colon != 1) || (line[j] != ':')) {
        return false;
    }
    if (word == 0L) {
        return true;
    }
    return line.toLower().contains(word);
}
