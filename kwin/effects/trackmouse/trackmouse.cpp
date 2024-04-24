/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

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

#include <config-kwin.h>

#include "trackmouse.h"

// KConfigSkeleton
#include "trackmouseconfig.h"

#include <kwinxrenderutils.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <KLocalizedString>
#include <kdebug.h>

#ifdef KWIN_BUILD_COMPOSITE
#include <xcbutils.h>
#endif

#include <math.h>

namespace KWin
{

TrackMouseEffect::TrackMouseEffect()
    : m_active(false)
    , m_angle(0)
{
#ifdef KWIN_BUILD_COMPOSITE
    m_picture[0] = m_picture[1] = 0;
    if ( effects->compositingType() == XRenderCompositing)
        m_angleBase = M_PI_2;
#endif
    m_mousePolling = false;
    KActionCollection *actionCollection = new KActionCollection(this);
    m_action = static_cast< KAction* >(actionCollection->addAction("TrackMouse"));
    m_action->setText(i18n("Track mouse"));
    m_action->setGlobalShortcut(QKeySequence());

    connect(m_action, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
                     SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
    if (m_mousePolling)
        effects->stopMousePolling();
    for (int i = 0; i < 2; ++i) {
#ifdef KWIN_BUILD_COMPOSITE
        delete m_picture[i]; m_picture[i] = 0;
#endif
    }
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    m_modifiers = 0;
    TrackMouseConfig::self()->readConfig();
    if (TrackMouseConfig::shift())
        m_modifiers |= Qt::ShiftModifier;
    if (TrackMouseConfig::alt())
        m_modifiers |= Qt::AltModifier;
    if (TrackMouseConfig::control())
        m_modifiers |= Qt::ControlModifier;
    if (TrackMouseConfig::meta())
        m_modifiers |= Qt::MetaModifier;

    if (m_modifiers) {
        if (!m_mousePolling)
            effects->startMousePolling();
        m_mousePolling = true;
    } else if (m_mousePolling) {
            effects->stopMousePolling();
        m_mousePolling = false;
    }
}

void TrackMouseEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_active) {
        QTime t = QTime::currentTime();
        m_angle = ((t.second() % 4) * m_angleBase) + (t.msec() / 1000.0 * m_angleBase);
        m_lastRect[0].moveCenter(cursorPos());
        m_lastRect[1].moveCenter(cursorPos());
        data.paint |= m_lastRect[0].adjusted(-1,-1,1,1);
    }
    effects->prePaintScreen(data, time);
}

void TrackMouseEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (!m_active)
        return;

#ifdef KWIN_BUILD_COMPOSITE
    if ( effects->compositingType() == XRenderCompositing && m_picture[0] && m_picture[1]) {
        float sine = sin(m_angle);
        const float cosine = cos(m_angle);
        for (int i = 0; i < 2; ++i) {
            if (i) sine = -sine;
            const float dx = m_size[i].width()/2.0;
            const float dy = m_size[i].height()/2.0;
            const xcb_render_picture_t picture = *m_picture[i];
            xcb_render_transform_t xform = {
                KWIN_DOUBLE_TO_FIXED( cosine ), KWIN_DOUBLE_TO_FIXED( -sine ), KWIN_DOUBLE_TO_FIXED( dx - cosine*dx + sine*dy ),
                KWIN_DOUBLE_TO_FIXED( sine ), KWIN_DOUBLE_TO_FIXED( cosine ), KWIN_DOUBLE_TO_FIXED( dy - sine*dx - cosine*dy ),
                KWIN_DOUBLE_TO_FIXED( 0.0 ), KWIN_DOUBLE_TO_FIXED( 0.0 ), KWIN_DOUBLE_TO_FIXED( 1.0 )
            };
            xcb_render_set_picture_transform(connection(), picture, xform);
            xcb_render_set_picture_filter(connection(), picture, 8, "bilinear", 0, NULL);
            const QRect &rect = m_lastRect[i];
            xcb_render_composite(connection(), XCB_RENDER_PICT_OP_OVER, picture, XCB_RENDER_PICTURE_NONE,
                                 effects->xrenderBufferPicture(), 0, 0, 0, 0,
                                 qRound((rect.x()+rect.width()/2.0)*data.xScale() - rect.width()/2.0 + data.xTranslation()),
                                 qRound((rect.y()+rect.height()/2.0)*data.yScale() - rect.height()/2.0 + data.yTranslation()),
                                 rect.width(), rect.height());
        }
    }
#endif
}

void TrackMouseEffect::postPaintScreen()
{
    if (m_active) {
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    }
    effects->postPaintScreen();
}

bool TrackMouseEffect::init()
{
#ifdef KWIN_BUILD_COMPOSITE
    if (!m_picture[0]) {
        loadTexture();
        if (!m_picture[0])
            return false;
    }
#else
    loadTexture();
    return false;
#endif
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    m_active = true;
    m_angle = 0;
    return true;
}

void TrackMouseEffect::toggle()
{
    if (m_mousePolling)
        return;

    if (m_active) {
        m_active = false;
    } else if (!init()) {
        return;
    }
    effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
}

void TrackMouseEffect::slotMouseChanged(const QPoint&, const QPoint&,
                                        Qt::MouseButtons, Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (!m_mousePolling) // we didn't ask for it but maybe someone else did...
        return;
    if (m_modifiers && modifiers == m_modifiers) {
        if (!m_active && !init()) {
            return;
        }
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    } else if (m_active) {
        m_active = false;
        effects->addRepaint(m_lastRect[0].adjusted(-1,-1,1,1));
    }
}

void TrackMouseEffect::loadTexture()
{
    QString f[2] = {KGlobal::dirs()->findResource("appdata", "tm_outer.png"),
                    KGlobal::dirs()->findResource("appdata", "tm_inner.png")};
    if (f[0].isEmpty() || f[1].isEmpty())
        return;

    for (int i = 0; i < 2; ++i) {
#ifdef KWIN_BUILD_COMPOSITE
        if ( effects->compositingType() == XRenderCompositing) {
            QImage image(f[i]);
            m_picture[i] = new XRenderPicture(image);
            m_size[i] = image.size();
            m_lastRect[i].setSize(image.size());
        }
#endif
    }
}

bool TrackMouseEffect::isActive() const
{
    return m_active;
}

} // namespace
