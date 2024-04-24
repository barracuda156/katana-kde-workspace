/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include <config-X11.h>
#include <config-kwin.h>

#include "composite.h"
#include "compositingadaptor.h"

#include "utils.h"
#include "workspace.h"
#include "client.h"
#include "unmanaged.h"
#include "deleted.h"
#include "effects.h"
#include "overlaywindow.h"
#include "scene.h"
#include "scene_xrender.h"
#include "shadow.h"
#include "useractions.h"
#include "compositingprefs.h"
#include "xcbutils.h"

#include <stdio.h>

#include <QTextStream>
#include <QFile>
#include <QMenu>
#include <QtCore/qcoreevent.h>
#include <QDateTime>
#include <QDBusConnection>
#include <kaction.h>
#include <kactioncollection.h>
#include <KGlobal>
#include <KLocalizedString>
#include <KNotification>

#include <xcb/composite.h>
#include <xcb/damage.h>

Q_DECLARE_METATYPE(KWin::Compositor::SuspendReason)

namespace KWin
{

extern int currentRefreshRate();

KWIN_SINGLETON_FACTORY_VARIABLE(Compositor, s_compositor)

// static inline qint64 milliToNano(int milli) { return milli * 1000 * 1000; }
static inline qint64 nanoToMilli(int nano) { return nano / (1000*1000); }

Compositor::Compositor(QObject* workspace)
    : QObject(workspace)
    , m_suspended(options->isUseCompositing() ? NoReasonSuspend : UserSuspend)
    , cm_selection(NULL)
    , fpsInterval(0)
    , m_xrrRefreshRate(0)
    , forceUnredirectCheck(false)
    , m_finishing(false)
    , m_timeSinceLastVBlank(0)
    , m_scene(NULL)
{
    qRegisterMetaType<Compositor::SuspendReason>("Compositor::SuspendReason");
    new CompositingAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/Compositor", this);
    dbus.registerService("org.kde.kwin.Compositing");
    connect(&unredirectTimer, SIGNAL(timeout()), SLOT(delayedCheckUnredirect()));
    connect(&compositeResetTimer, SIGNAL(timeout()), SLOT(restart()));
    connect(workspace, SIGNAL(configChanged()), SLOT(slotConfigChanged()));
    connect(options, SIGNAL(unredirectFullscreenChanged()), SLOT(delayedCheckUnredirect()));
    unredirectTimer.setSingleShot(true);
    compositeResetTimer.setSingleShot(true);
    nextPaintReference.invalidate(); // Initialize the timer

    // 2 sec which should be enough to restart the compositor
    static const int compositorLostMessageDelay = 2000;

    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, SIGNAL(timeout()), SLOT(deleteUnusedSupportProperties()));

    // delay the call to setup by one event cycle
    // The ctor of this class is invoked from the Workspace ctor, that means before
    // Workspace is completely constructed, so calling Workspace::self() would result
    // in undefined behavior. This is fixed by using a delayed invocation.
    QMetaObject::invokeMethod(this, "setup", Qt::QueuedConnection);
}

Compositor::~Compositor()
{
    finish();
    deleteUnusedSupportProperties();
    if (cm_selection) {
        disconnect(cm_selection, 0, this, 0);
        delete cm_selection;
    }
    s_compositor = NULL;
}


void Compositor::setup()
{
    if (hasScene())
        return;
    if (m_suspended) {
        kDebug(1212) << "Compositing is suspended, reason:" << m_suspended;
        return;
    } else if (!CompositingPrefs::compositingPossible()) {
        kError(1212) << "Compositing is not possible";
        return;
    }
    m_starting = true;

    if (!options->isCompositingInitialized()) {
        // options->reloadCompositingSettings(true) initializes the CompositingPrefs
        options->reloadCompositingSettings(true);
    }

    if (!cm_selection) {
        const int selection_sreen = DefaultScreen(display());
        char selection_name[ 100 ];
        ::memset(selection_name, '\0', sizeof(selection_name) * sizeof(char));
        ::sprintf(selection_name, "_NET_WM_CM_S%d", selection_sreen);
        cm_selection = new KSelectionOwner(selection_name, selection_sreen, this);
    }
    if (cm_selection->ownerWindow() == XNone) {
        cm_selection->claim(true);   // force claiming
        connect(cm_selection, SIGNAL(lostOwnership()), SLOT(finish()));
    }

    // There might still be a deleted around, needs to be cleared before creating the scene (BUG 333275)
    while (!Workspace::self()->deletedList().isEmpty()) {
        Workspace::self()->deletedList().first()->discard();
    }

    switch(options->compositingMode()) {
#ifdef KWIN_BUILD_COMPOSITE
    case XRenderCompositing:
        kDebug(1212) << "Initializing XRender compositing";
        m_scene = new SceneXrender(Workspace::self());
        break;
#endif
    default:
        kDebug(1212) << "No compositing enabled";
        m_starting = false;
        cm_selection->release();
        return;
    }
    if (m_scene == NULL || m_scene->initFailed()) {
        kError(1212) << "Failed to initialize compositing, compositing disabled";
        delete m_scene;
        m_scene = NULL;
        m_starting = false;
        cm_selection->release();
        return;
    }
    m_xrrRefreshRate = KWin::currentRefreshRate();
    fpsInterval = options->maxFpsInterval();
    m_timeSinceLastVBlank = fpsInterval - (options->vBlankTime() + 1); // means "start now" - we don't have even a slight idea when the first vsync will occur
    scheduleRepaint();
    xcb_composite_redirect_subwindows(connection(), rootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
    new EffectsHandlerImpl(this, m_scene);   // sets also the 'effects' pointer
    connect(effects, SIGNAL(screenGeometryChanged(QSize)), SLOT(addRepaintFull()));
    addRepaintFull();
    foreach (Client * c, Workspace::self()->clientList()) {
        c->setupCompositing();
        c->getShadow();
    }
    foreach (Client * c,  Workspace::self()->desktopList())
        c->setupCompositing();
    foreach (Unmanaged * c, Workspace::self()->unmanagedList()) {
        c->setupCompositing();
        c->getShadow();
    }

    emit compositingToggled(true);

    m_starting = false;

    // render at least once
    performCompositing();
}

void Compositor::scheduleRepaint()
{
    if (!compositeTimer.isActive())
        setCompositeTimer();
}

void Compositor::finish()
{
    if (!hasScene())
        return;
    m_finishing = true;
    foreach (Client * c, Workspace::self()->clientList())
        m_scene->windowClosed(c, NULL);
    foreach (Client * c, Workspace::self()->desktopList())
        m_scene->windowClosed(c, NULL);
    foreach (Unmanaged * c, Workspace::self()->unmanagedList())
        m_scene->windowClosed(c, NULL);
    foreach (Deleted * c, Workspace::self()->deletedList())
        m_scene->windowDeleted(c);
    foreach (Client * c, Workspace::self()->clientList())
        c->finishCompositing();
    foreach (Client * c, Workspace::self()->desktopList())
        c->finishCompositing();
    foreach (Unmanaged * c, Workspace::self()->unmanagedList())
        c->finishCompositing();
    foreach (Deleted * c, Workspace::self()->deletedList())
        c->finishCompositing();
    xcb_composite_unredirect_subwindows(connection(), rootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
    delete effects;
    effects = NULL;
    delete m_scene;
    m_scene = NULL;
    compositeTimer.stop();
    repaints_region = QRegion();
    for (ClientList::ConstIterator it = Workspace::self()->clientList().constBegin();
            it != Workspace::self()->clientList().constEnd();
            ++it) {
        // forward all opacity values to the frame in case there'll be other CM running
        if ((*it)->opacity() != 1.0) {
            NETWinInfo i(display(), (*it)->frameId(), rootWindow(), 0);
            i.setOpacity(static_cast< unsigned long >((*it)->opacity() * 0xffffffff));
        }
    }
    // discard all Deleted windows (#152914)
    while (!Workspace::self()->deletedList().isEmpty())
        Workspace::self()->deletedList().first()->discard();
    m_finishing = false;
    emit compositingToggled(false);
    if (cm_selection) {
        kDebug(1212) << "Releasing compositor selection";
        disconnect(cm_selection, 0, this, 0);
        cm_selection->release();
        delete cm_selection;
        cm_selection = 0;
    }
}

void Compositor::keepSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties.removeAll(atom);
}

void Compositor::removeSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties << atom;
    m_unusedSupportPropertyTimer.start();
}

void Compositor::deleteUnusedSupportProperties()
{
    if (m_starting) {
        // currently still starting the compositor
        m_unusedSupportPropertyTimer.start();
        return;
    }
    if (m_finishing) {
        // still shutting down, a restart might follow
        m_unusedSupportPropertyTimer.start();
        return;
    }
    foreach (const xcb_atom_t &atom, m_unusedSupportProperties) {
        // remove property from root window
        XDeleteProperty(QX11Info::display(), rootWindow(), atom);
    }
}

void Compositor::slotConfigChanged()
{
    if (!m_suspended) {
        setup();
        if (effects)   // setupCompositing() may fail
            effects->reconfigure();
        addRepaintFull();
    } else {
        finish();
    }
}

void Compositor::slotReinitialize()
{
    // Reparse config. Config options will be reloaded by setup()
    KGlobal::config()->reparseConfiguration();

    // Restart compositing
    finish();
    // resume compositing if suspended
    m_suspended = NoReasonSuspend;
    options->setCompositingInitialized(false);
    setup();

    if (effects) { // setup() may fail
        effects->reconfigure();
    }
}

// for the shortcut
void Compositor::slotToggleCompositing()
{
    if (m_suspended) { // direct user call; clear all bits
        resume(AllReasonSuspend);
    } else { // but only set the user one (sufficient to suspend)
        suspend(UserSuspend);
    }
}

// for the dbus call
void Compositor::toggleCompositing()
{
    slotToggleCompositing(); // TODO only operate on script level here?
    if (m_suspended) {
        // when disabled show a shortcut how the user can get back compositing
        QString shortcut, message;
        if (KAction* action = qobject_cast<KAction*>(Workspace::self()->actionCollection()->action("Suspend Compositing")))
            shortcut = action->globalShortcut().toString(QKeySequence::NativeText);
        if (!shortcut.isEmpty()) {
            // display notification only if there is the shortcut
            message = i18n("Desktop effects have been suspended by another application.<br/>"
                           "You can resume using the '%1' shortcut.", shortcut);
            KNotification::event("kwin/compositingsuspendeddbus", message);
        }
    }
}

void Compositor::updateCompositeBlocking()
{
    updateCompositeBlocking(NULL);
}

void Compositor::updateCompositeBlocking(Client *c)
{
    if (c) { // if c == 0 we just check if we can resume
        if (c->isBlockingCompositing()) {
            if (!(m_suspended & BlockRuleSuspend)) // do NOT attempt to call suspend(true); from within the eventchain!
                QMetaObject::invokeMethod(this, "suspend", Qt::QueuedConnection, Q_ARG(Compositor::SuspendReason, BlockRuleSuspend));
        }
    }
    else if (m_suspended & BlockRuleSuspend) {  // lost a client and we're blocked - can we resume?
        bool resume = true;
        for (ClientList::ConstIterator it = Workspace::self()->clientList().constBegin(); it != Workspace::self()->clientList().constEnd(); ++it) {
            if ((*it)->isBlockingCompositing()) {
                resume = false;
                break;
            }
        }
        if (resume) { // do NOT attempt to call suspend(false); from within the eventchain!
            QMetaObject::invokeMethod(this, "resume", Qt::QueuedConnection, Q_ARG(Compositor::SuspendReason, BlockRuleSuspend));
        }
    }
}

void Compositor::suspend(Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended |= reason;
    finish();
}

void Compositor::resume(Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended &= ~reason;
    setup(); // signal "toggled" is eventually emitted from within setup
}

void Compositor::setCompositing(bool active)
{
    if (active) {
        resume(ScriptSuspend);
    } else {
        suspend(ScriptSuspend);
    }
}

void Compositor::restart()
{
    if (hasScene()) {
        finish();
        QTimer::singleShot(0, this, SLOT(setup()));
    }
}

void Compositor::addRepaint(int x, int y, int w, int h)
{
    if (!hasScene())
        return;
    repaints_region += QRegion(x, y, w, h);
    scheduleRepaint();
}

void Compositor::addRepaint(const QRect& r)
{
    if (!hasScene())
        return;
    repaints_region += r;
    scheduleRepaint();
}

void Compositor::addRepaint(const QRegion& r)
{
    if (!hasScene())
        return;
    repaints_region += r;
    scheduleRepaint();
}

void Compositor::addRepaintFull()
{
    if (!hasScene())
        return;
    repaints_region = QRegion(0, 0, displayWidth(), displayHeight());
    scheduleRepaint();
}

void Compositor::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == compositeTimer.timerId()) {
        performCompositing();
    } else
        QObject::timerEvent(te);
}

void Compositor::performCompositing()
{
    if (!isOverlayWindowVisible())
        return; // nothing is visible anyway

    // Create a list of all windows in the stacking order
    ToplevelList windows = Workspace::self()->xStackingOrder();
    ToplevelList damaged;

    // Reset the damage state of each window and fetch the damage region
    // without waiting for a reply
    foreach (Toplevel *win, windows) {
        if (win->resetAndFetchDamage())
            damaged << win;
    }

    if (damaged.count() > 0)
        xcb_flush(connection());

    // Move elevated windows to the top of the stacking order
    foreach (EffectWindow *c, static_cast<EffectsHandlerImpl *>(effects)->elevatedWindows()) {
        Toplevel* t = static_cast< EffectWindowImpl* >(c)->window();
        windows.removeAll(t);
        windows.append(t);
    }

    // Get the replies
    foreach (Toplevel *win, damaged) {
        win->getDamageRegionReply();
    }

    if (repaints_region.isEmpty() && !windowRepaintsPending()) {
        m_scene->idle();
        m_timeSinceLastVBlank = fpsInterval - (options->vBlankTime() + 1); // means "start now"
        // Note: It would seem here we should undo suspended unredirect, but when scenes need
        // it for some reason, e.g. transformations or translucency, the next pass that does not
        // need this anymore and paints normally will also reset the suspended unredirect.
        // Otherwise the window would not be painted normally anyway.
        compositeTimer.stop();
        return;
    }

    // skip windows that are not yet ready for being painted
    // TODO ?
    // this cannot be used so carelessly - needs protections against broken clients, the window
    // should not get focus before it's displayed, handle unredirected windows properly and so on.
    QMutableListIterator<Toplevel*> windows_it(windows);
    while (windows_it.hasNext()) {
        const Toplevel *t = windows_it.next();
        if (!t->readyForPainting())
            windows_it.remove();
    }

    QRegion repaints = repaints_region;
    // clear all repaints, so that post-pass can add repaints for the next repaint
    repaints_region = QRegion();

    m_timeSinceLastVBlank = m_scene->paint(repaints, windows);

    compositeTimer.stop(); // stop here to ensure *we* cause the next repaint schedule - not some effect through m_scene->paint()

    // Trigger at least one more pass even if there would be nothing to paint, so that scene->idle()
    // is called the next time. If there would be nothing pending, it will not restart the timer and
    // scheduleRepaint() would restart it again somewhen later, called from functions that
    // would again add something pending.
    scheduleRepaint();
}

bool Compositor::windowRepaintsPending() const
{
    foreach (const Toplevel * c, Workspace::self()->clientList()) {
        if (!c->repaints().isEmpty())
            return true;
    }
    foreach (const Toplevel * c, Workspace::self()->desktopList()) {
        if (!c->repaints().isEmpty())
            return true;
    }
    foreach (const Toplevel * c, Workspace::self()->unmanagedList()) {
        if (!c->repaints().isEmpty())
            return true;
    }
    foreach (const Toplevel * c, Workspace::self()->deletedList()) {
        if (!c->repaints().isEmpty())
            return true;
    }
    return false;
}

void Compositor::setCompositeResetTimer(int msecs)
{
    compositeResetTimer.start(msecs);
}

void Compositor::setCompositeTimer()
{
    if (!hasScene())  // should not really happen, but there may be e.g. some damage events still pending
        return;

    uint waitTime = 1;
    // w/o blocking vsync we just jump to the next demanded tick
    if (fpsInterval > m_timeSinceLastVBlank) {
        waitTime = nanoToMilli(fpsInterval - m_timeSinceLastVBlank);
        if (!waitTime) {
            waitTime = 1; // will ensure we don't block out the eventloop - the system's just not faster ...
        }
    } else {
        waitTime = 1; // ... "0" would be sufficient, but the compositor isn't the WMs only task
    }
    compositeTimer.start(qMin(waitTime, 250u), this); // force 4fps minimum
}

bool Compositor::isActive()
{
    return !m_finishing && hasScene();
}

void Compositor::checkUnredirect()
{
    checkUnredirect(false);
}

// force is needed when the list of windows changes (e.g. a window goes away)
void Compositor::checkUnredirect(bool force)
{
    if (!hasScene() || m_scene->overlayWindow()->window() == None || !options->isUnredirectFullscreen())
        return;
    if (force)
        forceUnredirectCheck = true;
    if (!unredirectTimer.isActive())
        unredirectTimer.start(0);
}

void Compositor::delayedCheckUnredirect()
{
    if (!hasScene() || m_scene->overlayWindow()->window() == None || !(options->isUnredirectFullscreen() || sender() == options))
        return;
    ToplevelList list;
    bool changed = forceUnredirectCheck;
    foreach (Client * c, Workspace::self()->clientList())
        list.append(c);
    foreach (Unmanaged * c, Workspace::self()->unmanagedList())
        list.append(c);
    foreach (Toplevel * c, list) {
        if (c->updateUnredirectedState())
            changed = true;
    }
    // no desktops, no Deleted ones
    if (!changed)
        return;
    forceUnredirectCheck = false;
    // Cut out parts from the overlay window where unredirected windows are,
    // so that they are actually visible.
    QRegion reg(0, 0, displayWidth(), displayHeight());
    foreach (Toplevel * c, list) {
        if (c->unredirected())
            reg -= c->geometry();
    }
    m_scene->overlayWindow()->setShape(reg);
}

bool Compositor::checkForOverlayWindow(WId w) const
{
    if (!hasScene()) {
        // no scene, so it cannot be the overlay window
        return false;
    }
    if (!m_scene->overlayWindow()) {
        // no overlay window, it cannot be the overlay
        return false;
    }
    // and compare the window ID's
    return w == m_scene->overlayWindow()->window();
}

WId Compositor::overlayWindow() const
{
    if (!hasScene()) {
        return None;
    }
    if (!m_scene->overlayWindow()) {
        return None;
    }
    return m_scene->overlayWindow()->window();
}

bool Compositor::isOverlayWindowVisible() const
{
    if (!hasScene()) {
        return false;
    }
    if (!m_scene->overlayWindow()) {
        return false;
    }
    return m_scene->overlayWindow()->isVisible();
}

void Compositor::setOverlayWindowVisibility(bool visible)
{
    if (hasScene() && m_scene->overlayWindow()) {
        m_scene->overlayWindow()->setVisibility(visible);
    }
}

bool Compositor::isCompositingPossible() const
{
    return CompositingPrefs::compositingPossible();
}

QString Compositor::compositingNotPossibleReason() const
{
    return CompositingPrefs::compositingNotPossibleReason();
}

QString Compositor::compositingType() const
{
    if (!hasScene()) {
        return "none";
    }
    switch (m_scene->compositingType()) {
    case XRenderCompositing:
        return "xrender";
    case NoCompositing:
    default:
        return "none";
    }
}

/*****************************************************
 * Workspace
 ****************************************************/

bool Workspace::compositing() const
{
    return m_compositor && m_compositor->hasScene();
}

//****************************************
// Toplevel
//****************************************

bool Toplevel::setupCompositing()
{
    if (!compositing())
        return false;

    if (damage_handle != XCB_NONE)
        return false;

    damage_handle = xcb_generate_id(connection());
    xcb_damage_create(connection(), damage_handle, frameId(), XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

    damage_region = QRegion(0, 0, width(), height());
    effect_window = new EffectWindowImpl(this);
    unredirect = false;

    Compositor::self()->checkUnredirect(true);
    Compositor::self()->scene()->windowAdded(this);

    // With unmanaged windows there is a race condition between the client painting the window
    // and us setting up damage tracking.  If the client wins we won't get a damage event even
    // though the window has been painted.  To avoid this we mark the whole window as damaged
    // and schedule a repaint immediately after creating the damage object.
    if (qobject_cast<Unmanaged*>(this))
        addDamageFull();

    return true;
}

void Toplevel::finishCompositing()
{
    if (damage_handle == XCB_NONE)
        return;
    Compositor::self()->checkUnredirect(true);
    if (effect_window->window() == this) { // otherwise it's already passed to Deleted, don't free data
        discardWindowPixmap();
        delete effect_window;
    }

    xcb_damage_destroy(connection(), damage_handle);

    damage_handle = XCB_NONE;
    damage_region = QRegion();
    repaints_region = QRegion();
    effect_window = NULL;
}

void Toplevel::discardWindowPixmap()
{
    addDamageFull();
    if (effectWindow() != NULL && effectWindow()->sceneWindow() != NULL)
        effectWindow()->sceneWindow()->pixmapDiscarded();
}

void Toplevel::damageNotifyEvent()
{
    m_isDamaged = true;

    // Note: The rect is supposed to specify the damage extents,
    //       but we don't know it at this point. No one who connects
    //       to this signal uses the rect however.
    emit damaged(this, QRect());
}

bool Toplevel::compositing() const
{
    return Workspace::self()->compositing();
}

void Client::damageNotifyEvent()
{
#ifdef HAVE_XSYNC
    if (syncRequest.isPending && isResize()) {
        emit damaged(this, QRect());
        m_isDamaged = true;
        return;
    }

    if (!ready_for_painting) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest.counter == None)   // cannot detect complete redraw, consider done now
            setReadyForPainting();
    }
#else
    if (!ready_for_painting)
        setReadyForPainting();
#endif

    Toplevel::damageNotifyEvent();
}

bool Toplevel::resetAndFetchDamage()
{
    if (!m_isDamaged)
        return false;

    xcb_connection_t *conn = connection();

    // Create a new region and copy the damage region to it,
    // resetting the damaged state.
    xcb_xfixes_region_t region = xcb_generate_id(conn);
    xcb_xfixes_create_region(conn, region, 0, 0);
    xcb_damage_subtract(conn, damage_handle, 0, region);

    // Send a fetch-region request and destroy the region
    m_regionCookie = xcb_xfixes_fetch_region_unchecked(conn, region);
    xcb_xfixes_destroy_region(conn, region);

    m_isDamaged = false;
    m_damageReplyPending = true;

    return m_damageReplyPending;
}

void Toplevel::getDamageRegionReply()
{
    if (!m_damageReplyPending)
        return;

    m_damageReplyPending = false;

    // Get the fetch-region reply
    xcb_xfixes_fetch_region_reply_t *reply =
            xcb_xfixes_fetch_region_reply(connection(), m_regionCookie, 0);

    if (!reply)
        return;

    // Convert the reply to a QRegion
    int count = xcb_xfixes_fetch_region_rectangles_length(reply);
    QRegion region;

    if (count > 1 && count < 16) {
        xcb_rectangle_t *rects = xcb_xfixes_fetch_region_rectangles(reply);

        QVector<QRect> qrects;
        qrects.reserve(count);

        for (int i = 0; i < count; i++)
            qrects << QRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height);

        region.setRects(qrects.constData(), count);
    } else
        region += QRect(reply->extents.x, reply->extents.y,
                        reply->extents.width, reply->extents.height);

    damage_region += region;
    repaints_region += region;

    free(reply);
}

void Toplevel::addDamageFull()
{
    if (!compositing())
        return;

    damage_region = rect();
    repaints_region |= rect();

    emit damaged(this, rect());
}

void Toplevel::resetDamage()
{
    damage_region = QRegion();
}

void Toplevel::addRepaint(const QRect& r)
{
    if (!compositing()) {
        return;
    }
    repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addRepaint(int x, int y, int w, int h)
{
    QRect r(x, y, w, h);
    addRepaint(r);
}

void Toplevel::addRepaint(const QRegion& r)
{
    if (!compositing()) {
        return;
    }
    repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addLayerRepaint(const QRect& r)
{
    if (!compositing()) {
        return;
    }
    layer_repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addLayerRepaint(int x, int y, int w, int h)
{
    QRect r(x, y, w, h);
    addLayerRepaint(r);
}

void Toplevel::addLayerRepaint(const QRegion& r)
{
    if (!compositing())
        return;
    layer_repaints_region += r;
    emit needsRepaint();
}

void Toplevel::addRepaintFull()
{
    repaints_region = visibleRect().translated(-pos());
    emit needsRepaint();
}

void Toplevel::resetRepaints()
{
    repaints_region = QRegion();
    layer_repaints_region = QRegion();
}

void Toplevel::addWorkspaceRepaint(int x, int y, int w, int h)
{
    addWorkspaceRepaint(QRect(x, y, w, h));
}

void Toplevel::addWorkspaceRepaint(const QRect& r2)
{
    if (!compositing())
        return;
    Compositor::self()->addRepaint(r2);
}

bool Toplevel::updateUnredirectedState()
{
    assert(compositing());
    bool should = options->isUnredirectFullscreen() && shouldUnredirect() && !unredirectSuspend &&
                  !shape() && !hasAlpha() && opacity() == 1.0 &&
                  !static_cast<EffectsHandlerImpl*>(effects)->activeFullScreenEffect();
    if (should == unredirect)
        return false;
    static QElapsedTimer lastUnredirect;
    static const qint64 msecRedirectInterval = 100;
    if (!lastUnredirect.hasExpired(msecRedirectInterval)) {
        QTimer::singleShot(msecRedirectInterval, Compositor::self(), SLOT(checkUnredirect()));
        return false;
    }
    lastUnredirect.start();
    unredirect = should;
    if (unredirect) {
        kDebug(1212) << "Unredirecting:" << this;
        xcb_composite_unredirect_window(connection(), frameId(), XCB_COMPOSITE_REDIRECT_MANUAL);
    } else {
        kDebug(1212) << "Redirecting:" << this;
        xcb_composite_redirect_window(connection(), frameId(), XCB_COMPOSITE_REDIRECT_MANUAL);
        discardWindowPixmap();
    }
    return true;
}

void Toplevel::suspendUnredirect(bool suspend)
{
    if (unredirectSuspend == suspend)
        return;
    unredirectSuspend = suspend;
    Compositor::self()->checkUnredirect();
}

//****************************************
// Client
//****************************************

bool Client::setupCompositing()
{
    if (!Toplevel::setupCompositing()){
        return false;
    }
    updateVisibility(); // for internalKeep()
    if (isManaged()) {
        // only create the decoration when a client is managed
        updateDecoration(true, true);
    }
    return true;
}

void Client::finishCompositing()
{
    Toplevel::finishCompositing();
    updateVisibility();
    if (!deleting) {
        // only recreate the decoration if we are not shutting down completely
        updateDecoration(true, true);
    }
    // for safety in case KWin is just resizing the window
    s_haveResizeEffect = false;
}

bool Client::shouldUnredirect() const
{
    if (isActiveFullScreen()) {
        ToplevelList stacking = workspace()->xStackingOrder();
        for (int pos = stacking.count() - 1;
                pos >= 0;
                --pos) {
            Toplevel* c = stacking.at(pos);
            if (c == this)   // is not covered by any other window, ok to unredirect
                return true;
            if (c->geometry().intersects(geometry()))
                return false;
        }
        kFatal() << "Something strange happened";
    }
    return false;
}


//****************************************
// Unmanaged
//****************************************

bool Unmanaged::shouldUnredirect() const
{
    // it must cover whole display or one xinerama screen, and be the topmost there
    const int desktop = VirtualDesktopManager::self()->current();
    if (geometry() == workspace()->clientArea(FullArea, geometry().center(), desktop)
            || geometry() == workspace()->clientArea(ScreenArea, geometry().center(), desktop)) {
        ToplevelList stacking = workspace()->xStackingOrder();
        for (int pos = stacking.count() - 1;
                pos >= 0;
                --pos) {
            Toplevel* c = stacking.at(pos);
            if (c == this)   // is not covered by any other window, ok to unredirect
                return true;
            if (c->geometry().intersects(geometry()))
                return false;
        }
        kFatal() << "Something strange happened";
    }
    return false;
}

//****************************************
// Deleted
//****************************************

bool Deleted::shouldUnredirect() const
{
    return false;
}


} // namespace
