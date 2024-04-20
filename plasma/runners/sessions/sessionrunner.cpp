/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
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

#include "sessionrunner.h"

#include <KDebug>
#include <KLocale>
#include <KMessageBox>

#include "kworkspace/kworkspace.h"

#include "screensaver_interface.h"

SessionRunner::SessionRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    setObjectName(QLatin1String("Sessions"));
    setPriority(LowPriority);
    setIgnoredTypes(
        Plasma::RunnerContext::Directory | Plasma::RunnerContext::File |
        Plasma::RunnerContext::NetworkLocation
    );

    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("log out command", "logout"),
            i18n("Logs out, exiting the current desktop session")
        )
    );
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("shutdown computer command", "shutdown"),
            i18n("Turns off the computer")
        )
    );

    Plasma::RunnerSyntax rebootSyntax(
        i18nc("restart computer command", "restart"),
        i18n("Reboots the computer")
    );
    rebootSyntax.addExampleQuery(i18nc("restart computer command", "reboot"));
    addSyntax(rebootSyntax);

    m_triggerWord = i18nc("switch user command", "switch");
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("switch user command", "switch :q:"),
            i18n("Switches to the active session for the user :q:, or lists all active sessions if :q: is not provided")
        )
    );

    Plasma::RunnerSyntax fastUserSwitchSyntax(
        i18n("switch user"),
        i18n("Starts a new session as a different user")
    );
    fastUserSwitchSyntax.addExampleQuery(i18n("new session"));
    addSyntax(fastUserSwitchSyntax);

    addSyntax(Plasma::RunnerSyntax("SESSIONS", i18n("Lists all sessions")));
}

void SessionRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();
    if (term.size() < 3) {
        return;
    }

    QString user;
    bool matchUser = false;
    QList<Plasma::QueryMatch> matches;
    // first compare with SESSIONS
    bool listAll = (
        term.compare("SESSIONS", Qt::CaseInsensitive) == 0 ||
        term.compare(i18nc("User sessions", "sessions"), Qt::CaseInsensitive) == 0
    );

    if (!listAll) {
        // is it the "switch" user command?
        if (term.startsWith(m_triggerWord, Qt::CaseInsensitive)) {
            user = term.right(term.size() - m_triggerWord.length()).trimmed();
            listAll = user.isEmpty();
            matchUser = !listAll;
        // it's not SESSION or "switch <something>", try some other possibilities
        } else if (term.compare(i18nc("log out command", "logout"), Qt::CaseInsensitive) == 0 ||
            term.compare(QLatin1String("logout"), Qt::CaseInsensitive) == 0 ||
            term.compare(i18n("log out"), Qt::CaseInsensitive) == 0 ||
            term.compare(QLatin1String("log out"), Qt::CaseInsensitive) == 0) {
            Plasma::QueryMatch match(this);
            match.setText(i18nc("log out command","Logout"));
            match.setIcon(KIcon("system-log-out"));
            match.setData(LogoutAction);
            match.setRelevance(0.9);
            matches << match;
        } else if (term.compare(i18nc("restart computer command", "restart"), Qt::CaseInsensitive) == 0 ||
            term.compare(QLatin1String("restart"), Qt::CaseInsensitive) == 0 ||
            term.compare(i18nc("restart computer command", "reboot"), Qt::CaseInsensitive) == 0 ||
            term.compare(QLatin1String("reboot"), Qt::CaseInsensitive) == 0) {
            Plasma::QueryMatch match(this);
            match.setText(i18n("Restart the computer"));
            match.setIcon(KIcon("system-reboot"));
            match.setData(RestartAction);
            match.setRelevance(0.9);
            matches << match;
        } else if (term.compare(i18nc("shutdown computer command", "shutdown"), Qt::CaseInsensitive) == 0 ||
            term.compare(QLatin1String("shutdown"), Qt::CaseInsensitive) == 0) {
            Plasma::QueryMatch match(this);
            match.setText(i18n("Shutdown the computer"));
            match.setIcon(KIcon("system-shutdown"));
            match.setData(ShutdownAction);
            match.setRelevance(0.9);
            matches << match;
        }
    }

    //kDebug() << "session switching to" << (listAll ? "all sessions" : term);
    bool switchUser = listAll ||
                      term.compare(i18n("switch user"), Qt::CaseInsensitive) == 0 ||
                      term.compare(i18n("new session"), Qt::CaseInsensitive) == 0;

    if (switchUser && dm.isSwitchable()) {
        Plasma::QueryMatch match(this);
        match.setIcon(KIcon("system-switch-user"));
        match.setText(i18n("New Session"));
        matches << match;
    }

    // now add the active sessions
    if (listAll || matchUser) {
        SessList sessions;
        dm.localSessions(sessions);

        foreach (const SessEnt& session, sessions) {
            if (!session.vt || session.self) {
                continue;
            }

            QString name = KDisplayManager::sess2Str(session);
            qreal relevance = 0.0;

            if (listAll) {
                relevance = 1.0;
            } else if (matchUser) {
                if (name.compare(user, Qt::CaseInsensitive) == 0) {
                    // we need an elif branch here because we don't
                    // want the last conditional to be checked if !listAll
                    relevance = 1.0;
                } else if (name.contains(user, Qt::CaseInsensitive)) {
                    relevance = 0.7;
                }
            }

            if (relevance != 0.0) {
                Plasma::QueryMatch match(this);
                match.setRelevance(relevance);
                match.setIcon(KIcon("user-identity"));
                match.setText(name);
                match.setData(QString::number(session.vt));
                matches << match;
            }
        }
    }

    context.addMatches(matches);
}

void SessionRunner::run(const Plasma::QueryMatch &match)
{
    if (match.data().type() == QVariant::Int) {
        KWorkSpace::ShutdownType type = KWorkSpace::ShutdownTypeDefault;

        switch (match.data().toInt()) {
            case LogoutAction: {
                type = KWorkSpace::ShutdownTypeNone;
                break;
            }
            case RestartAction: {
                type = KWorkSpace::ShutdownTypeReboot;
                break;
            }
            case ShutdownAction: {
                type = KWorkSpace::ShutdownTypeHalt;
                break;
            }
        }

        if (type != KWorkSpace::ShutdownTypeDefault) {
            KWorkSpace::ShutdownConfirm confirm = KWorkSpace::ShutdownConfirmDefault;
            KWorkSpace::requestShutDown(confirm, type);
            return;
        }
    }

    if (!match.data().toString().isEmpty()) {
        dm.switchVT(match.data().toString().toInt());
        return;
    }

    int result = KMessageBox::warningContinueCancel(
        nullptr,
        i18n(
            "<p>The current session will be hidden "
            "and a new login screen will be displayed.<p/>"
            "<p>Switch between sessions can be done by pressing "
            "Ctrl, Alt and the appropriate F%1-%2 key at the same time.</p>",
            1, 12
        ),
        i18n("Warning - New Session"),
        KGuiItem(i18n("&Start New Session"), "fork"),
        KStandardGuiItem::cancel(),
        ":confirmNewSession",
        KMessageBox::PlainCaption | KMessageBox::Notify
    );

    if (result == KMessageBox::Cancel) {
        return;
    }

    dm.newSession();
}

#include "moc_sessionrunner.cpp"
