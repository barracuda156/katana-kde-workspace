/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Michael Zanetti <michael_zanetti@gmx.net>

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

#ifndef KWIN_SLIDEBACK_H
#define KWIN_SLIDEBACK_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class SlideBackEffect
    : public Effect
{
    Q_OBJECT
public:
    SlideBackEffect();

    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintWindow(EffectWindow* w);

    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void postPaintScreen();
    virtual bool isActive() const;

public Q_SLOTS:
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowUnminimized(KWin::EffectWindow *w);
    void slotStackingOrderChanged();

private:

    WindowMotionManager motionManager;
    EffectWindowList usableOldStackingOrder;
    EffectWindowList oldStackingOrder;
    EffectWindowList coveringWindows;
    EffectWindowList elevatedList;
    EffectWindow *m_justMapped, *m_upmostWindow;
    QHash<EffectWindow *, QRect> destinationList;
    QList <QRegion> clippedRegions;

    QRect getSlideDestination(const QRect &windowUnderGeometry, const QRect &windowOverGeometry);
    bool isWindowUsable(EffectWindow *w);
    bool intersects(EffectWindow *windowUnder, const QRect &windowOverGeometry);
    EffectWindowList usableWindows(const EffectWindowList &allWindows);
    QRect getModalGroupGeometry(EffectWindow *w);
    void windowRaised(EffectWindow *w);

};

} // namespace

#endif
