/*
 * Copyright (c) 2007      Gustavo Pichorim Boiko <gustavo.boiko@kdemail.net>
 * Copyright (c) 2002,2003 Hamish Rodda <rodda@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <QCoreApplication>
#include <KConfig>
#include <KConfigGroup>
#include <KProcess>
#include <KDebug>

int main(int argc, char *argv[])
{
    // application instance for events processing
    QCoreApplication app(argc, argv);

    KConfig config("krandrrc");
    KConfigGroup group = config.group("Display");
    const bool applyonstartup = group.readEntry("ApplyOnStartup", false);
    if (applyonstartup) {
        const QStringList commands = group.readEntry("StartupCommands").split("\n");
        foreach (const QString &command, commands) {
            KProcess kproc;
            kproc.setShellCommand(command);
            kproc.start();
            if (!kproc.waitForStarted() || !kproc.waitForFinished()) {
                kWarning() << kproc.readAllStandardError();
            }
        }
    }
    return 0;
}
