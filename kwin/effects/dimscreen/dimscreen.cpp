/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008, 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include "dimscreen.h"

namespace KWin
{

DimScreenEffect::DimScreenEffect()
    : mActivated(false)
    , mActivateAnimation(false)
    , mDeactivateAnimation(false)
    , mWindow(nullptr)
{
    mCheck << "kdesu kdesu";
    mCheck << "kdesudo kdesudo";
    mCheck << "polkit-kde-manager polkit-kde-manager";
    mCheck << "polkit-kde-authentication-agent-1 polkit-kde-authentication-agent-1";
    mCheck << "pinentry pinentry";

    reconfigure(ReconfigureAll);
    connect(
        effects, SIGNAL(windowActivated(KWin::EffectWindow*)),
        this, SLOT(slotWindowActivated(KWin::EffectWindow*))
    );
}

void DimScreenEffect::reconfigure(ReconfigureFlags)
{
    mTimeline.setDuration(animationTime(250));
}

void DimScreenEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (mActivated && mActivateAnimation && !effects->activeFullScreenEffect()) {
        mTimeline.setCurrentTime(mTimeline.currentTime() + time);
    }
    if (mActivated && mDeactivateAnimation) {
        mTimeline.setCurrentTime(mTimeline.currentTime() - time);
    }
    if (mActivated && effects->activeFullScreenEffect()) {
        mTimeline.setCurrentTime(mTimeline.currentTime() - time);
    }
    if (mActivated && !mActivateAnimation && !mDeactivateAnimation &&
        !effects->activeFullScreenEffect() && mTimeline.currentValue() != 1.0) {
        mTimeline.setCurrentTime(mTimeline.currentTime() + time);
    }
    effects->prePaintScreen(data, time);
}

void DimScreenEffect::postPaintScreen()
{
    if (mActivated) {
        if (mActivateAnimation && mTimeline.currentValue() == 1.0) {
            mActivateAnimation = false;
            effects->addRepaintFull();
        }
        if (mDeactivateAnimation && mTimeline.currentValue() == 0.0) {
            mDeactivateAnimation = false;
            mActivated = false;
            effects->addRepaintFull();
        }
        // still animating
        if (mTimeline.currentValue() > 0.0 && mTimeline.currentValue() < 1.0)
            effects->addRepaintFull();
    }
    effects->postPaintScreen();
}

void DimScreenEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (mActivated && (w != mWindow) && w->isManaged()) {
        data.multiplyBrightness((1.0 - 0.33 * mTimeline.currentValue()));
        data.multiplySaturation((1.0 - 0.33 * mTimeline.currentValue()));
    }
    effects->paintWindow(w, mask, region, data);
}

void DimScreenEffect::slotWindowActivated(EffectWindow *w)
{
    if (!w) {
        return;
    }
    if (mCheck.contains(w->windowClass())) {
        mActivated = true;
        mActivateAnimation = true;
        mDeactivateAnimation = false;
        mWindow = w;
        effects->addRepaintFull();
    } else {
        if (mActivated) {
            mActivateAnimation = false;
            mDeactivateAnimation = true;
            effects->addRepaintFull();
        }
    }
}

bool DimScreenEffect::isActive() const
{
    return mActivated;
}

} // namespace
