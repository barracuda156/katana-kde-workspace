/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2009, 2010 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "logout.h"

#include <math.h>
#include <kdebug.h>
#include <KGlobal>
#include <KStandardDirs>

namespace KWin
{

LogoutEffect::LogoutEffect()
    : progress(0.0)
    , displayEffect(false)
    , logoutWindow(NULL)
    , logoutWindowPassed(false)
    , canDoPersistent(false)
{
    // Persistent effect
    logoutAtom = XInternAtom(display(), "_KDE_LOGGING_OUT", False);
    effects->registerPropertyType(logoutAtom, true);

    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
    connect(effects, SIGNAL(propertyNotify(KWin::EffectWindow*,long)), this, SLOT(slotPropertyNotify(KWin::EffectWindow*,long)));
}

LogoutEffect::~LogoutEffect()
{
}

void LogoutEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (displayEffect) {
        progress = qMin(1.0, progress + time / animationTime(2000.0));
    } else if (progress > 0.0) {
        progress = qMax(0.0, progress - time / animationTime(500.0));
    }

    data.paint |= effects->clientArea(FullArea, 0, 0);
    effects->prePaintScreen(data, time);
}

void LogoutEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (progress > 0.0) {
        if (effects->compositingType() == KWin::XRenderCompositing) {
            // Since we can't do vignetting in XRender just do a stronger desaturation and darken
            if (w != logoutWindow && !logoutWindowPassed && !ignoredWindows.contains(w)) {
                data.multiplySaturation((1.0 - progress * 0.8));
                data.multiplyBrightness((1.0 - progress * 0.3));
            }
        }
        if (w == logoutWindow || ignoredWindows.contains(w)) {
            // HACK: All windows past the first ignored one should not be blurred as it affects the
            // stacking order. All following windows are on top of the logout window and should not
            // be altered either
            logoutWindowPassed = true;
        }
    }
    effects->paintWindow(w, mask, region, data);
}

void LogoutEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);
}

void LogoutEffect::postPaintScreen()
{
    if (progress != 0.0 && progress != 1.0)
        effects->addRepaintFull();

    if (progress > 0.0)
        logoutWindowPassed = false;
    effects->postPaintScreen();
}

void LogoutEffect::slotWindowAdded(EffectWindow* w)
{
    if (isLogoutDialog(w)) {
        logoutWindow = w;
        progress = 0.0;
        displayEffect = true;
        ignoredWindows.clear();
        effects->addRepaintFull();
    } else if (canDoPersistent)
        // TODO: Add parent
        ignoredWindows.append(w);
}

void LogoutEffect::slotWindowClosed(EffectWindow* w)
{
    if (w == logoutWindow) {
        if (!canDoPersistent)
            displayEffect = false; // Fade back to normal
        effects->addRepaintFull();
    }
}

void LogoutEffect::slotWindowDeleted(EffectWindow* w)
{
    ignoredWindows.removeAll(w);
    if (w == logoutWindow)
        logoutWindow = NULL;
}

bool LogoutEffect::isLogoutDialog(EffectWindow* w)
{
    // TODO there should be probably a better way (window type?)
    if (w->windowClass() == "plasma-desktop plasma-desktop"
            && (w->windowRole() == "logoutdialog" || w->windowRole() == "logouteffect")) {
        return true;
    }
    return false;
}

void LogoutEffect::slotPropertyNotify(EffectWindow* w, long a)
{
    if (w || a != logoutAtom)
        return; // Not our atom

    QByteArray byteData = effects->readRootProperty(logoutAtom, logoutAtom, 8);
    if (byteData.length() < 1) {
        // Property was deleted
        displayEffect = false;
        return;
    }

    // using a compatible session manager therefore only terminate the effect when the
    // atom is deleted, not when the dialog is closed.
    canDoPersistent = true;
    effects->addRepaintFull();
}

bool LogoutEffect::isActive() const
{
    return progress != 0 || logoutWindow;
}

} // namespace
