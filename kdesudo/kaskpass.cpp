/*
    This file is part of the KDE project
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#include "config.h"

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kpassworddialog.h>
#include <kwindowsystem.h>

#include <stdio.h>

#if defined(HAVE_PR_SET_DUMPABLE)
#  include <sys/prctl.h>
#elif defined(HAVE_PROCCTL)
#  include <unistd.h>
#  include <sys/procctl.h>
#endif

static QString kLocalString(const QByteArray &bytes)
{
    return QString::fromLocal8Bit(bytes.constData(), bytes.size());
}

static QByteArray kLocalBytes(const QString &string)
{
    return string.toLocal8Bit();
}

int main(int argc, char **argv)
{
#if defined(HAVE_PR_SET_DUMPABLE)
    prctl(PR_SET_DUMPABLE, 0);
#elif defined(HAVE_PROCCTL)
    int ctldata = PROC_TRACE_CTL_DISABLE;
    procctl(P_PID, ::getpid(), PROC_TRACE_CTL, &ctldata);
#endif

    KAboutData about(
        "kaskpass", 0, ki18n("KAskPass"),
        "1.0.0", ki18n("Password prompt for KDE"),
        KAboutData::License_GPL,
        ki18n("(C) 2024 Ivailo Monev")
    );
    const QByteArray caller = qgetenv("KASKPASS_CALLER");
    if (!caller.isEmpty()) {
        about.setAppName(caller.toLower());
        about.setProgramName(ki18n(caller.constData()));
    }
    KCmdLineArgs::init(&about);

    KApplication app;
    app.disableSessionManagement();

    KPasswordDialog dialog;
    dialog.setDefaultButton(KDialog::Ok);
    const QByteArray winid = qgetenv("KASKPASS_MAINWINDOW");
    if (!winid.isEmpty()) {
        KWindowSystem::setMainWindow(&dialog, static_cast<WId>(winid.toULong()));
    }
    const QByteArray icon = qgetenv("KASKPASS_ICON");
    if (!icon.isEmpty()) {
        dialog.setPixmap(QPixmap(kLocalString(icon)));
    }
    const QByteArray label0 = qgetenv("KASKPASS_LABEL0");
    if (!label0.isEmpty()) {
        const QByteArray comment0 = qgetenv("KASKPASS_COMMENT0");
        dialog.addCommentLine(kLocalString(label0), kLocalString(comment0));
    }
    const QByteArray label1 = qgetenv("KASKPASS_LABEL1");
    if (!label1.isEmpty()) {
        const QByteArray comment1 = qgetenv("KASKPASS_COMMENT1");
        dialog.addCommentLine(kLocalString(label1), kLocalString(comment1));
    }
    QByteArray prompt = qgetenv("KASKPASS_PROMPT");
    if (prompt.isEmpty()) {
        // in case it is used by something other than kdesudo
        prompt = qgetenv("SSH_ASKPASS_PROMPT");
    }
    if (!prompt.isEmpty()) {
        dialog.setPrompt(kLocalString(prompt));
    }

    const int result = dialog.exec();
    if (result == QDialog::Accepted) {
        const QByteArray pass = kLocalBytes(dialog.password());
        fprintf(stdout, "%s\n", pass.constData());
        fflush(stdout);
        return 0;
    }
    return 1;
}
