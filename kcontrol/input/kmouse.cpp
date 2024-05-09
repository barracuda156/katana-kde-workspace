/*
 * kmouse.cpp
 *
 * Copyright (c) 1999 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
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

#include <config-X11.h>

#include <QFile>
#include <QX11Info>
#include <QApplication>
#include <KConfig>
#include <KConfigGroup>
#include <KGlobalSettings>
#include <KToolInvocation>
#include <KDebug>

#include "mousesettings.h"

#include <X11/Xlib.h>
#ifdef HAVE_XCURSOR
#  include <X11/Xcursor/Xcursor.h>
#endif

int main(int argc, char *argv[])
{
    // application instance for QX11Info::display()
    QApplication app(argc, argv);

    KConfig *config = new KConfig("kcminputrc", KConfig::NoGlobals);
    MouseSettings settings;
    settings.load(config);
    settings.apply(true); // force

    // NOTE: keep in sync with:
    // kcontrol/input/kapplymousetheme.cpp
#ifdef HAVE_XCURSOR
    KConfigGroup group = config->group("Mouse");
    const QByteArray theme = group.readEntry("cursorTheme", QByteArray(KDE_DEFAULT_CURSOR_THEME));
    const QByteArray size = group.readEntry("cursorSize", QByteArray());

    // Apply the KDE cursor theme to ourselves
    XcursorSetTheme(QX11Info::display(), theme.constData());

    if (!size.isEmpty()) {
        XcursorSetDefaultSize(QX11Info::display(), size.toUInt());
    }

    // Load the default cursor from the theme and apply it to the root window.
    Cursor handle = XcursorLibraryLoadCursor(QX11Info::display(), "left_ptr");
    XDefineCursor(QX11Info::display(), QX11Info::appRootWindow(), handle);
    XFreeCursor(QX11Info::display(), handle); // Don't leak the cursor

    // Tell klauncher to set the XCURSOR_THEME and XCURSOR_SIZE environment
    // variables when launching applications.
    KToolInvocation::self()->setLaunchEnv("XCURSOR_THEME", theme);
    if (!size.isEmpty()) {
        KToolInvocation::self()->setLaunchEnv("XCURSOR_SIZE", size);
    }
#endif
    delete config;
    return 0;
}
