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

#ifndef SESSIONRUNNER_H
#define SESSIONRUNNER_H

#include <kworkspace/kdisplaymanager.h>
#include <Plasma/AbstractRunner>


/**
 * This class provides matches for running sessions as well as
 * an action to start a new session, etc.
 */
class SessionRunner : public Plasma::AbstractRunner
{
    Q_OBJECT

public:
    enum {
        LogoutAction = 1,
        ShutdownAction,
        RestartAction
    };

    SessionRunner(QObject *parent, const QVariantList &args);

    void match(Plasma::RunnerContext &context);
    void run(const Plasma::QueryMatch &action);

private:
    QString m_triggerWord;
    KDisplayManager dm;
};

K_EXPORT_PLASMA_RUNNER(sessions, SessionRunner)

#endif

