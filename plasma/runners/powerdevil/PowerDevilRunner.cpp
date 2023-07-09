/***************************************************************************
 *   Copyright 2008 by Dario Freddi <drf@kdemod.ath.cx>                    *
 *   Copyright 2008 by Sebastian Kügler <sebas@kde.org>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "PowerDevilRunner.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>
#include <KStandardDirs>
#include <KRun>
#include <Solid/PowerManagement>

PowerDevilRunner::PowerDevilRunner(QObject *parent, const QVariantList &args)
        : Plasma::AbstractRunner(parent, args),
          m_shortestCommand(1000)
{
    Q_UNUSED(args)

    setObjectName( QLatin1String("PowerDevil" ));
    setIgnoredTypes(Plasma::RunnerContext::Directory | Plasma::RunnerContext::File |
                    Plasma::RunnerContext::NetworkLocation | Plasma::RunnerContext::Help);
    updateStatus();

    // Let's define all the words that will eventually trigger a match in the runner.
    QStringList commands;
    commands << i18nc("Note this is a KRunner keyword", "suspend")
             << i18nc("Note this is a KRunner keyword", "hibernate")
             << i18nc("Note this is a KRunner keyword", "hybrid")
             << i18nc("Note this is a KRunner keyword", "to disk")
             << i18nc("Note this is a KRunner keyword", "to ram");

    foreach (const QString &command, commands) {
        if (command.length() < m_shortestCommand) {
            m_shortestCommand = command.length();
        }
    }

    // Also receive state updates
    connect(
        Solid::PowerManagement::notifier(), SIGNAL(supportedSleepStatesChanged()),
        this, SLOT(updateStatus())
    );
}

PowerDevilRunner::~PowerDevilRunner()
{
}

void PowerDevilRunner::updateStatus()
{
    QList<Plasma::RunnerSyntax> syntaxes;
    syntaxes.append(Plasma::RunnerSyntax(i18nc("Note this is a KRunner keyword", "suspend"),
                     i18n("Lists system suspend (e.g. sleep, hibernate) options "
                          "and allows them to be activated")));

    QSet< Solid::PowerManagement::SleepState > states = Solid::PowerManagement::supportedSleepStates();

    if (states.contains(Solid::PowerManagement::SuspendState)) {
        Plasma::RunnerSyntax sleepSyntax(i18nc("Note this is a KRunner keyword", "sleep"),
                                         i18n("Suspends the system to RAM"));
        sleepSyntax.addExampleQuery(i18nc("Note this is a KRunner keyword", "to ram"));
        syntaxes.append(sleepSyntax);
    }

    if (states.contains(Solid::PowerManagement::HibernateState)) {
        Plasma::RunnerSyntax hibernateSyntax(i18nc("Note this is a KRunner keyword", "hibernate"),
                                         i18n("Suspends the system to disk"));
        hibernateSyntax.addExampleQuery(i18nc("Note this is a KRunner keyword", "to disk"));
        syntaxes.append(hibernateSyntax);
    }

    if (states.contains(Solid::PowerManagement::HybridSuspendState)) {
        Plasma::RunnerSyntax hybridSyntax(i18nc("Note this is a KRunner keyword", "hybrid"),
                                          i18n("Suspends the system to RAM and put the system in sleep mode"));
        syntaxes.append(hybridSyntax);
    }

    setSyntaxes(syntaxes);
}

void PowerDevilRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();
    if (term.length() < m_shortestCommand) {
        return;
    }

    QList<Plasma::QueryMatch> matches;
    if (term.compare(i18nc("Note this is a KRunner keyword", "suspend"), Qt::CaseInsensitive) == 0) {
        QSet< Solid::PowerManagement::SleepState > states = Solid::PowerManagement::supportedSleepStates();

        if (states.contains(Solid::PowerManagement::SuspendState)) {
            addSuspendMatch(Solid::PowerManagement::SuspendState, matches);
        }

        if (states.contains(Solid::PowerManagement::HibernateState)) {
            addSuspendMatch(Solid::PowerManagement::HibernateState, matches);
        }

        if (states.contains(Solid::PowerManagement::HybridSuspendState)) {
            addSuspendMatch(Solid::PowerManagement::HybridSuspendState, matches);
        }
    } else if (term.compare(i18nc("Note this is a KRunner keyword", "sleep"), Qt::CaseInsensitive) == 0 ||
               term.compare(i18nc("Note this is a KRunner keyword", "to ram"), Qt::CaseInsensitive) == 0) {
        addSuspendMatch(Solid::PowerManagement::SuspendState, matches);
    } else if (term.compare(i18nc("Note this is a KRunner keyword", "hibernate"), Qt::CaseInsensitive) == 0 ||
               term.compare(i18nc("Note this is a KRunner keyword", "to disk"), Qt::CaseInsensitive) == 0) {
        addSuspendMatch(Solid::PowerManagement::HibernateState, matches);
    } else if (term.compare(i18nc("Note this is a KRunner keyword", "hybrid"), Qt::CaseInsensitive) == 0) {
        addSuspendMatch(Solid::PowerManagement::HybridSuspendState, matches);
    }

    if (!matches.isEmpty()) {
        context.addMatches(term, matches);
    }
}

void PowerDevilRunner::addSuspendMatch(int value, QList<Plasma::QueryMatch> &matches)
{
    Plasma::QueryMatch match(this);
    match.setType(Plasma::QueryMatch::ExactMatch);

    switch ((Solid::PowerManagement::SleepState)value) {
        case Solid::PowerManagement::SuspendState: {
            match.setIcon(KIcon("system-suspend"));
            match.setText(i18n("Suspend to RAM"));
            match.setRelevance(1);
            break;
        }
        case Solid::PowerManagement::HibernateState: {
            match.setIcon(KIcon("system-suspend-hibernate"));
            match.setText(i18n("Suspend to Disk"));
            match.setRelevance(0.99);
            break;
        }
        case Solid::PowerManagement::HybridSuspendState: {
            match.setIcon(KIcon("system-suspend"));
            match.setText(i18n("Hybrid Suspend"));
            match.setRelevance(0.98);
            break;
        }
    }

    match.setData(value);
    match.setId("Suspend");
    matches.append(match);
}

void PowerDevilRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context)

    if (match.id().startsWith("PowerDevil_Suspend")) {
        switch ((Solid::PowerManagement::SleepState)match.data().toInt()) {
            case Solid::PowerManagement::SuspendState: {
                Solid::PowerManagement::requestSleep(Solid::PowerManagement::SuspendState);
                break;
            }
            case Solid::PowerManagement::HibernateState: {
                Solid::PowerManagement::requestSleep(Solid::PowerManagement::HibernateState);
                break;
            }
            case Solid::PowerManagement::HybridSuspendState: {
                Solid::PowerManagement::requestSleep(Solid::PowerManagement::HybridSuspendState);
                break;
            }
        }
    }
}

#include "moc_PowerDevilRunner.cpp"
