/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2019 Ivailo Monev <xakepa10@gmail.com>

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

#include "startupfeedback.h"
#include "client.h"
#include "effects.h"
// KDE
#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KStartupInfo>
#include <KApplication>

namespace KWin
{

StartupEventNotifier::StartupEventNotifier(xcb_window_t window)
    : QWidget(nullptr),
    m_window(window)
{
    kapp->installX11EventFilter(this);
}

StartupEventNotifier::~StartupEventNotifier()
{
    kapp->removeX11EventFilter(this);
}

bool StartupEventNotifier::x11Event(XEvent *xevent)
{
    // qDebug() << Q_FUNC_INFO << xevent->type << xevent->xany.window << m_window;
    if (xevent->type == ButtonRelease && xevent->xany.window == m_window) {
        emit interrupt();
        return true;
    }
    return false;
}


StartupFeedbackEffect::StartupFeedbackEffect()
    : m_startupInfo(new KStartupInfo(KStartupInfo::CleanOnCantDetect, this))
    , m_startups(0)
    , m_notifier(nullptr)
    , m_type(StartupFeedbackEffect::PassiveFeedback)
    , m_cursor(Qt::WaitCursor)
{
    reconfigure(ReconfigureAll);

    connect(
        m_startupInfo, SIGNAL(gotNewStartup(KStartupInfoId,KStartupInfoData)),
        this, SLOT(gotNewStartup(KStartupInfoId,KStartupInfoData))
    );
    connect(
        m_startupInfo, SIGNAL(gotRemoveStartup(KStartupInfoId,KStartupInfoData)),
        this, SLOT(gotRemoveStartup(KStartupInfoId,KStartupInfoData))
    );
}

StartupFeedbackEffect::~StartupFeedbackEffect()
{
    stop();
}

void StartupFeedbackEffect::reconfigure(Effect::ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    const bool oldactive = isActive();
    if (oldactive) {
        stop();
    }

    KConfig conf("klaunchrc", KConfig::NoGlobals);
    KConfigGroup c = conf.group("FeedbackStyle");
    const bool busyCursor = c.readEntry("BusyCursor", true);
    c = conf.group("BusyCursorSettings");
    const int timeout = c.readEntry("Timeout", 10);
    m_startupInfo->setTimeout(qMax(timeout, 1));
    if (!busyCursor) {
        m_type = StartupFeedbackEffect::NoFeedback;
    } else {
        m_type = StartupFeedbackEffect::PassiveFeedback;
    }
    if (oldactive) {
        start();
    }
}

void StartupFeedbackEffect::gotNewStartup(const KStartupInfoId& id, const KStartupInfoData& data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);

    m_startups++;
    if (!isActive()) {
        start();
    }
}

void StartupFeedbackEffect::gotRemoveStartup(const KStartupInfoId& id, const KStartupInfoData& data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);

    m_startups--;
    if (m_startups <= 0) {
        m_startups = 0;
        stop();
    }
}

void StartupFeedbackEffect::start()
{
    if (m_type == StartupFeedbackEffect::NoFeedback) {
        return;
    }

    const xcb_window_t window = rootWindow();
    m_notifier = new StartupEventNotifier(window);
    connect(m_notifier, SIGNAL(interrupt()), this, SLOT(stop()));
    ScopedCPointer<xcb_grab_pointer_reply_t> grabPointer(
        xcb_grab_pointer_reply(
            connection(),
            xcb_grab_pointer_unchecked(
                connection(), true, window,
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_WINDOW_NONE,
                m_cursor.handle(), XCB_TIME_CURRENT_TIME
            ),
            NULL
        )
    );
    if (grabPointer.isNull() || grabPointer->status != XCB_GRAB_STATUS_SUCCESS) {
        kWarning() << "could not grab pointer";
        delete m_notifier;
        m_notifier = nullptr;
    }
}

void StartupFeedbackEffect::stop()
{
    switch(m_type) {
        case StartupFeedbackEffect::PassiveFeedback: {
            if (m_notifier) {
                delete m_notifier;
                m_notifier = nullptr;
                xcb_ungrab_pointer(connection(), XCB_TIME_CURRENT_TIME);
            }
            break;
        }
        case StartupFeedbackEffect::NoFeedback: {
            break;
        }
        default: {
            // impossible
            break;
        }
    }
}

bool StartupFeedbackEffect::isActive() const
{
    return (m_notifier != nullptr);
}

} // namespace
