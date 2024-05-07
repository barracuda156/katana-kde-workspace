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

#include "shellrunner.h"

#include <QWidget>
#include <QPushButton>
#include <KDebug>
#include <KIcon>
#include <KLocale>
#include <KRun>
#include <KShell>
#include <KStandardDirs>
#include <KToolInvocation>

ShellRunner::ShellRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    setObjectName( QLatin1String("Command" ));
    setPriority(AbstractRunner::HighestPriority);
    setIgnoredTypes(
        Plasma::RunnerContext::Directory | Plasma::RunnerContext::File |
        Plasma::RunnerContext::NetworkLocation | Plasma::RunnerContext::UnknownType
    );

    addSyntax(Plasma::RunnerSyntax(":q:", i18n("Finds commands that match :q:, using common shell syntax")));
}

void ShellRunner::match(Plasma::RunnerContext &context)
{
    if (context.type() == Plasma::RunnerContext::Executable ||
        context.type() == Plasma::RunnerContext::ShellCommand)  {
        const QString term = context.query();
        Plasma::QueryMatch match(this);
        match.setId(term);
        match.setData(term);
        match.setIcon(KIcon("system-run"));
        match.setText(i18n("Run %1", term));
        match.setRelevance(0.7);
        context.addMatch(match);
    }
}

void ShellRunner::run(const Plasma::QueryMatch &match)
{
    bool interminal = false;
    if (match.selectedAction() != nullptr) {
        interminal = match.selectedAction()->data().toBool();
    }
    const QString command = match.data().toString();
    if (interminal) {
        KToolInvocation::self()->invokeTerminal(command);
    } else {
        KRun::runCommand(command, nullptr);
    }
}

QList<QAction*> ShellRunner::actionsForMatch(const Plasma::QueryMatch &match)
{
    Q_UNUSED(match)
    QList<QAction*> result;
    QAction* matchaction = addAction("run_in_terminal", KIcon("utilities-terminal"), i18n("Run in &terminal window"));
    matchaction->setData(true);
    result.append(matchaction);
    return result;
}

#include "moc_shellrunner.cpp"
