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

#include "kcm_clock_helper.h"

#include <QFile>
#include <QDir>
#include <QProcess>
#include <ksystemtimezone.h>
#include <kstandarddirs.h>
#include <kdebug.h>

static const QString s_localtime = QString::fromLatin1("/etc/localtime");

KCMClockHelper::KCMClockHelper(const char* const helper, QObject *parent)
    : KAuthorization(helper, parent)
{
}

int KCMClockHelper::save(const QVariantMap &parameters)
{
    if (!parameters.contains("zone")) {
        return KAuthorization::HelperError;
    }

    if (parameters.contains("datetime")) {
        const QString datetime = parameters.value("datetime").toString();
        const QString timedatectlexe = KStandardDirs::findRootExe("timedatectl");
        if (!timedatectlexe.isEmpty()) {
            const QStringList timedatectlargs = QStringList()
                << "set-time" << datetime;
            if (QProcess::execute(timedatectlexe, timedatectlargs) != 0) {
                return 1;
            }
        } else {
            const QString hwclockexe = KStandardDirs::findRootExe("hwclock");
            const QStringList hwclockargs = QStringList()
                << "--set" << "--date" << datetime;
            if (QProcess::execute(hwclockexe, hwclockargs) != 0) {
                return 1;
            }
        }
    }

    const QString zone = parameters.value("zone").toString();
    const QString zonefile = KSystemTimeZones::zoneinfoDir() + QDir::separator() + zone;
    QFile::remove(s_localtime);
    if (!QFile::link(zonefile, s_localtime)) {
        return 2;
    }

    return KAuthorization::NoError;
}

K_AUTH_MAIN("org.kde.kcontrol.kcmclock", KCMClockHelper)
