/*  This file is part of the KDE project
    Copyright (C) 2006-2007 Matthias Kretz <kretz@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <KStandardDirs>

#include <sys/stat.h>

int main(int argc, char *argv[])
{
    // application instance for events processing
    QCoreApplication app(argc, argv);

    // can't use KGlobalSettings::desktopPath() here, since it returns the home dir
    // if the desktop folder doesn't exist.
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktopPath.isEmpty()) {
        desktopPath = QDir::homePath() + "/Desktop";
    }

    // Create the desktop folder if it doesn't exist
    bool desktopIsEmpty = false;
    const QDir desktopDir(desktopPath);
    if (!desktopDir.exists()) {
        ::mkdir(QFile::encodeName(desktopPath), S_IRWXU);
        desktopIsEmpty = true;
    } else {
        desktopIsEmpty = desktopDir.entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty();
    }

    if (desktopIsEmpty) {
        // Copy the desktop links and .directory file
        const QStringList links = KGlobal::dirs()->findAllResources("data", "kcm_desktoppaths/*", KStandardDirs::NoDuplicates);
        foreach (const QString &link, links) {
            QString linkname = link.mid(link.lastIndexOf('/'));
            // NOTE: special case for the .directory file, it is .desktop file but hidden one
            if (linkname.startsWith(QLatin1String("/directory."))) {
                linkname = QString::fromLatin1("/.directory");
            // NOTE: .desktop file but extension does not matter and is better chopped
            } else if (linkname.endsWith(QLatin1String(".desktop"))) {
                linkname.chop(8);
            }
            QFile::copy(link, desktopPath + linkname);
        }
    }
    return 0;
}
