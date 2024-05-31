/* Copyright 2009  Jan Gerrit Marker <jangerrit@weiler-marker.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "killrunner.h"

#include <QAction>
#include <KDebug>
#include <KIcon>
#include <KUser>
#include <KMessageBox>

#include <signal.h>

KillRunner::KillRunner(QObject *parent, const QVariantList& args)
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args);
    setObjectName(QLatin1String("Kill Runner"));
    reloadConfiguration();
}

void KillRunner::reloadConfiguration()
{
#warning TODO: untranslated keyword match, why is it even translated when it is configurable?
    KConfigGroup grp = config();
    m_triggerWord.clear();
    if (grp.readEntry(CONFIG_USE_TRIGGERWORD, true)) {
        m_triggerWord = grp.readEntry(CONFIG_TRIGGERWORD, i18n("kill")) + ' ';
    }

    m_sorting = static_cast<KillRunnerSort>(grp.readEntry(CONFIG_SORTING, static_cast<int>(KillRunnerSort::NONE)));
    QList<Plasma::RunnerSyntax> syntaxes;
    syntaxes << Plasma::RunnerSyntax(m_triggerWord + ":q:",
                                     i18n("Terminate running applications whose names match the query."));
    setSyntaxes(syntaxes);
}

void KillRunner::match(Plasma::RunnerContext &context)
{
    QString term = context.query();
    const bool hasTrigger = !m_triggerWord.isEmpty();
    if (hasTrigger && !term.startsWith(m_triggerWord, Qt::CaseInsensitive)) {
        return;
    }

    term = term.right(term.length() - m_triggerWord.length());

    if (term.length() < 2)  {
        return;
    }

    QList<Plasma::QueryMatch> matches;
    KSysGuard::Processes *processes = new KSysGuard::Processes();
    processes->updateAllProcesses();
    const QList<KSysGuard::Process *> processlist = processes->getAllProcesses();
    processes->deleteLater();
    foreach (const KSysGuard::Process *process, processlist) {
        if (!context.isValid()) {
            return;
        }

        const QString name = process->name;
        if (!name.contains(term, Qt::CaseInsensitive)) {
            // Process doesn't match the search term
            continue;
        }

        const quint64 pid = process->pid;
        const qlonglong uid = process->uid;
        QString user;
        KUser kuser(uid);
        if (kuser.isValid()) {
            user = kuser.loginName();
        } else {
            kWarning() << "No user with UID" << uid << "was found";
            user = QLatin1String("root");
        }

        Plasma::QueryMatch match(this);
        match.setText(i18n("Terminate %1", name));
        match.setSubtext(i18n("Process ID: %1\nRunning as user: %2", QString::number(pid), user));
        match.setIcon(KIcon("application-exit"));
        match.setData(pid);
        match.setId(name);

        // Set the relevance
        switch (m_sorting) {
            case KillRunnerSort::CPU: {
                match.setRelevance((process->userUsage + process->sysUsage) / 100);
                break;
            }
            case KillRunnerSort::CPUI: {
                match.setRelevance(1 - (process->userUsage + process->sysUsage) / 100);
                break;
            }
            case KillRunnerSort::NONE: {
                match.setRelevance(name.compare(term, Qt::CaseInsensitive) == 0 ? 1 : 9);
                break;
            }
        }

        matches << match;
    }

    kDebug() << "match count is" << matches.count();
    context.addMatches(matches);
}

void KillRunner::run(const Plasma::QueryMatch &match)
{
    const quint64 pid = match.data().toULongLong();
    int sig = SIGKILL;
    if (match.selectedAction() != NULL) {
        sig = match.selectedAction()->data().toInt();
    }

    if (::kill(pid, sig) == -1) {
        const int savederrno = errno;
        KMessageBox::error(nullptr, qt_error_string(savederrno));
    }
}

QList<QAction*> KillRunner::actionsForMatch(const Plasma::QueryMatch &match)
{
    Q_UNUSED(match)
    QList<QAction*> ret;
    if (!action("SIGTERM")) {
        QAction* action = addAction("SIGTERM", KIcon("application-exit"), i18n("Send SIGTERM"));
        action->setData(int(SIGTERM));

        action = addAction("SIGKILL", KIcon("process-stop"), i18n("Send SIGKILL"));
        action->setData(int(SIGKILL));
    }
    ret << action("SIGTERM") << action("SIGKILL");
    return ret;
}

#include "moc_killrunner.cpp"
