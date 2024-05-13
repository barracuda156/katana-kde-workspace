/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "kdisplaymanager.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMetaType>
#include <klocale.h>
#include <kuser.h>
#include <kdebug.h>

// for reference:
// https://www.freedesktop.org/software/systemd/man/org.freedesktop.login1.html
// https://consolekit2.github.io/ConsoleKit2/#Manager
// https://github.com/GNOME/gdm/blob/main/daemon/gdm-local-display-factory.xml

struct systemdSession
{
    QString session_id;
    uint user_id;
    QString user_name;
    QString seat_id;
    QDBusObjectPath session_object;
};
Q_DECLARE_METATYPE(systemdSession);
Q_DECLARE_METATYPE(QList<systemdSession>);

QDBusArgument& operator<<(QDBusArgument &argument, const systemdSession &systemdsession)
{
    argument.beginStructure();
    argument << systemdsession.session_id << systemdsession.user_id;
    argument << systemdsession.user_name << systemdsession.seat_id;
    argument << systemdsession.session_object;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument &argument, systemdSession &systemdsession)
{
    argument.beginStructure();
    argument >> systemdsession.session_id >> systemdsession.user_id;
    argument >> systemdsession.user_name >> systemdsession.seat_id;
    argument >> systemdsession.session_object;
    argument.endStructure();
    return argument;
}


class KDisplayManagerPrivate
{
public:
    KDisplayManagerPrivate();

    QDBusInterface m_login1;
    QDBusInterface m_consolekit;
};

KDisplayManagerPrivate::KDisplayManagerPrivate()
    : m_login1("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus()),
    m_consolekit("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager", "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus())
{
}

KDisplayManager::KDisplayManager()
    : d(new KDisplayManagerPrivate())
{
    if (d->m_login1.isValid()) {
        qDBusRegisterMetaType<systemdSession>();
        qDBusRegisterMetaType<QList<systemdSession>>();
    }
}

KDisplayManager::~KDisplayManager()
{
    delete d;
}

bool KDisplayManager::canShutdown()
{
    if (d->m_login1.isValid()) {
        QDBusReply<QString> reply = d->m_login1.call("CanPowerOff");
        return (reply.value() == QLatin1String("yes"));
    }

    if (d->m_consolekit.isValid()) {
        QDBusReply<QString> reply = d->m_consolekit.call("CanPowerOff");
        return (reply.value() == QLatin1String("yes"));
    }

    return false;
}

void KDisplayManager::shutdown(KWorkSpace::ShutdownType shutdownType)
{
    if (shutdownType == KWorkSpace::ShutdownTypeNone) {
        return;
    }

    if (d->m_login1.isValid()) {
        if (shutdownType == KWorkSpace::ShutdownTypeReboot) {
            d->m_login1.call("Reboot", true);
        } else if (shutdownType == KWorkSpace::ShutdownTypeHalt) {
            d->m_login1.call("PowerOff", true);
        }
        return;
    }

    if (d->m_consolekit.isValid()) {
        if (shutdownType == KWorkSpace::ShutdownTypeReboot) {
            d->m_consolekit.call("Reboot", true);
        } else if (shutdownType == KWorkSpace::ShutdownTypeHalt) {
            d->m_consolekit.call("PowerOff", true);
        }
        return;
    }

    kWarning() << "Could not shutdown";
}

bool KDisplayManager::isSwitchable()
{
    QDBusInterface systemdiface(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
        QDBusConnection::systemBus()
    );
    if (systemdiface.isValid()) {
        QString systemdversion = systemdiface.property("Version").toString();
        const int dotindex = systemdversion.indexOf(QLatin1Char('.'));
        if (dotindex > 0) {
            systemdversion = systemdversion.left(dotindex);
        }
        // always allowed since 246
        return (systemdversion.toLongLong() >= 246);
    }

    if (d->m_consolekit.isValid()) {
        QDBusReply<QDBusObjectPath> reply = d->m_consolekit.call("GetCurrentSession");
        if (reply.isValid()) {
            QDBusInterface consolekitiface(
                "org.freedesktop.ConsoleKit", reply.value().path(), "org.freedesktop.ConsoleKit.Session",
                QDBusConnection::systemBus()
            );
            if (!consolekitiface.isValid()) {
                kWarning() << "Invalid session interface";
                return false;
            }
            QDBusReply<QDBusObjectPath> reply2 = consolekitiface.call("GetSeatId");
            if (!reply2.isValid()) {
                kWarning() << "Invalid session reply";
                return false;
            }
            QDBusInterface consolekitiface2(
                "org.freedesktop.ConsoleKit", reply2.value().path(), "org.freedesktop.ConsoleKit.Seat",
                QDBusConnection::systemBus()
            );
            if (!consolekitiface2.isValid()) {
                kWarning() << "Invalid seat interface";
                return false;
            }
            QDBusReply<bool> reply3 = consolekitiface2.call("CanActivateSessions");
            if (reply3.isValid()) {
                return reply3.value();
            }
            kWarning() << "Invalid seat reply";
            return false;
        }
        kWarning() << "Invalid session reply";
        return false;
    }
    return false;
}

void KDisplayManager::newSession()
{
    QDBusInterface gdmiface(
        "org.gnome.DisplayManager",
        "/org/gnome/DisplayManager/LocalDisplayFactory",
        "org.gnome.DisplayManager.LocalDisplayFactory",
        QDBusConnection::systemBus()
    );
    if (gdmiface.isValid()) {
        gdmiface.asyncCall("CreateTransientDisplay");
        return;
    }

    const QByteArray dmseatbytes = qgetenv("XDG_SEAT_PATH");
    QString dmseat = QString::fromLocal8Bit(dmseatbytes.constData(), dmseatbytes.size());
    if (dmseat.isEmpty()) {
        QDBusInterface dmiface(
            QString::fromLatin1("org.freedesktop.DisplayManager"),
            QString::fromLatin1("/org/freedesktop/DisplayManager"),
            QString::fromLatin1("org.freedesktop.DisplayManager"),
            QDBusConnection::systemBus()
        );
        if (dmiface.isValid()) {
            const QString username = KUser().loginName();
            const QList<QDBusObjectPath> dmsessions = qvariant_cast<QList<QDBusObjectPath>>(dmiface.property("Sessions"));
            // qDebug() << Q_FUNC_INFO << dmiface.property("Sessions");
            foreach (const QDBusObjectPath &dmsessionobj, dmsessions) {
                QDBusInterface dmsessioniface(
                    QString::fromLatin1("org.freedesktop.DisplayManager"),
                    dmsessionobj.path(),
                    QString::fromLatin1("org.freedesktop.DisplayManager.Session"),
                    QDBusConnection::systemBus()
                );
                if (!dmsessioniface.isValid()) {
                    kWarning() << "Display manager session interface is not valid";
                    continue;
                }
                const QString dmusername = dmsessioniface.property("UserName").toString();
                // qDebug() << Q_FUNC_INFO << dmusername << username;
                if (dmusername == username) {
                    const QDBusObjectPath dmsessionseat = qvariant_cast<QDBusObjectPath>(dmsessioniface.property("Seat"));
                    dmseat = dmsessionseat.path();
                    break;
                }
            }
        }
    }
    if (!dmseat.isEmpty()) {
        QDBusInterface lightdmiface(
            "org.freedesktop.DisplayManager",
            dmseat,
            "org.freedesktop.DisplayManager.Seat",
            QDBusConnection::systemBus()
        );
        if (lightdmiface.isValid()) {
            lightdmiface.asyncCall("SwitchToGreeter");
            return;
        }
    }

    kWarning() << "No way to start new session";
}

bool KDisplayManager::localSessions(SessList &list)
{
    list.clear();

    if (d->m_login1.isValid()) {
        QDBusReply<QList<systemdSession>> reply = d->m_login1.call("ListSessions");
        if (reply.isValid()) {
            // qDebug() << Q_FUNC_INFO << reply.value().size();
            foreach (const systemdSession &systemdsession, reply.value()) {
                QDBusInterface systemdiface(
                    "org.freedesktop.login1", systemdsession.session_object.path(), "org.freedesktop.login1.Session",
                    QDBusConnection::systemBus()
                );
                if (!systemdiface.isValid()) {
                    kWarning() << "Invalid session interface";
                    continue;
                }
                const bool isremote = systemdiface.property("Remote").toBool();
                if (isremote) {
                    continue;
                }
                const QString state = systemdiface.property("State").toString();
                if (state != QLatin1String("online") && state != QLatin1String("active")) {
                    continue;
                }
                SessEnt sessionentity;
                sessionentity.display = systemdiface.property("Display").toString();
                sessionentity.user = systemdiface.property("Name").toString();
                sessionentity.session = systemdiface.property("Desktop").toString();
                sessionentity.vt = systemdiface.property("VTNr").toInt();
                sessionentity.self = (sessionentity.display == qgetenv("DISPLAY"));
                sessionentity.tty = !systemdiface.property("TTY").toString().isEmpty();
#if 0
                qDebug() << Q_FUNC_INFO << sessionentity.display << sessionentity.user
                     << sessionentity.session << sessionentity.vt
                     << sessionentity.self << sessionentity.tty;
#endif
                list.append(sessionentity);
            }
            return true;
        }
        kWarning() << "Invalid login1 reply";
        return false;
    }

    if (d->m_consolekit.isValid()) {
        QDBusReply<QList<QDBusObjectPath>> reply = d->m_consolekit.call("GetSessions");
        if (reply.isValid()) {
            // qDebug() << Q_FUNC_INFO << reply.value().size();
            foreach (const QDBusObjectPath &consolekitsession, reply.value()) {
                QDBusInterface consolekitiface(
                    "org.freedesktop.ConsoleKit", consolekitsession.path(), "org.freedesktop.ConsoleKit.Session",
                    QDBusConnection::systemBus()
                );
                if (!consolekitiface.isValid()) {
                    kWarning() << "Invalid session interface";
                    continue;
                }
                const bool islocal = consolekitiface.property("is-local").toBool();
                if (!islocal) {
                    continue;
                }
                const QString state = consolekitiface.property("session-state").toString();
                if (state != QLatin1String("online") && state != QLatin1String("active")) {
                    continue;
                }
                SessEnt sessionentity;
                sessionentity.display = consolekitiface.property("x11-display").toString();
                sessionentity.user = KUser(K_UID(consolekitiface.property("unix-user").toUInt())).loginName();
                sessionentity.session = "<unknown>";
                sessionentity.vt = consolekitiface.property("VTNr").toInt();
                sessionentity.self = (sessionentity.display == qgetenv("DISPLAY"));
                sessionentity.tty = (consolekitiface.property("session-type").toString() == QLatin1String("tty"));
#if 0
                qDebug() << Q_FUNC_INFO << sessionentity.display << sessionentity.user
                     << sessionentity.session << sessionentity.vt
                     << sessionentity.self << sessionentity.tty;
#endif
                list.append(sessionentity);
            }
            return true;
        }
        kWarning() << "Invalid consolekit reply";
        return false;
    }

    return false;
}

QString KDisplayManager::sess2Str(const SessEnt &se)
{
    QString user, loc;
    if (se.tty) {
        user = i18nc("user: ...", "%1: TTY login", se.user);
        loc = se.vt ? QString("vt%1").arg(se.vt) : se.display ;
    } else {
        user =
            se.user.isEmpty() ?
                se.session.isEmpty() ?
                    i18nc("... location (TTY or X display)", "Unused") :
                    se.session == "<remote>" ?
                        i18n("X login on remote host") :
                        i18nc("... host", "X login on %1", se.session) :
                se.session == "<unknown>" ?
                    se.user :
                    i18nc("user: session type", "%1: %2",
                          se.user, se.session);
        loc =
            se.vt ?
                QString("%1, vt%2").arg(se.display).arg(se.vt) :
                se.display;
    }
    return i18nc("session (location)", "%1 (%2)", user, loc);
}

bool KDisplayManager::switchVT(int vt)
{
    if (d->m_login1.isValid()) {
        QDBusReply<QList<systemdSession>> reply = d->m_login1.call("ListSessions");
        if (reply.isValid()) {
            // qDebug() << Q_FUNC_INFO << reply.value().size();
            foreach (const systemdSession &systemdsession, reply.value()) {
                QDBusInterface systemdiface(
                    "org.freedesktop.login1", systemdsession.session_object.path(), "org.freedesktop.login1.Session",
                    QDBusConnection::systemBus()
                );
                if (!systemdiface.isValid()) {
                    kWarning() << "Invalid session interface";
                    continue;
                }
                const bool isremote = systemdiface.property("Remote").toBool();
                if (isremote) {
                    continue;
                }
                const int vtnr = systemdiface.property("VTNr").toInt();
                // qDebug() << Q_FUNC_INFO << vt << vtnr;
                if (vtnr != vt) {
                    continue;
                }
                systemdiface.asyncCall("Activate");
                return true;
            }
            kWarning() << "Could not find VT";
            return false;
        }
        kWarning() << "Invalid login1 reply";
        return false;
    }

    if (d->m_consolekit.isValid()) {
        QDBusReply<QList<QDBusObjectPath>> reply = d->m_consolekit.call("GetSessions");
        if (reply.isValid()) {
            // qDebug() << Q_FUNC_INFO << reply.value().size();
            foreach (const QDBusObjectPath &consolekitsession, reply.value()) {
                QDBusInterface consolekitiface(
                    "org.freedesktop.ConsoleKit", consolekitsession.path(), "org.freedesktop.ConsoleKit.Session",
                    QDBusConnection::systemBus()
                );
                if (!consolekitiface.isValid()) {
                    kWarning() << "Invalid session interface";
                    continue;
                }
                const bool islocal = consolekitiface.property("is-local").toBool();
                if (!islocal) {
                    continue;
                }
                const int vtnr = consolekitiface.property("VTNr").toInt();
                // qDebug() << Q_FUNC_INFO << vt << vtnr;
                if (vtnr != vt) {
                    continue;
                }
                consolekitiface.asyncCall("Activate");
                return true;
            }
            kWarning() << "Could not find VT";
            return false;
        }
        kWarning() << "Invalid consolekit reply";
        return false;
    }

    return false;
}
