/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include "config-kwin.h"

#include "effects.h"

#include "effectsadaptor.h"
#include "decorations.h"
#include "deleted.h"
#include "client.h"
#include "cursor.h"
#include "group.h"
#include "scene_xrender.h"
#include "unmanaged.h"
#ifdef KWIN_BUILD_SCREENEDGES
#include "screenedge.h"
#endif
#include "screens.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include "composite.h"
#include "xcbutils.h"

#include "effects/diminactive/diminactive.h"
#include "effects/dimscreen/dimscreen.h"
#include "effects/highlightwindow/highlightwindow.h"
#include "effects/logout/logout.h"
#include "effects/magnifier/magnifier.h"
#include "effects/minimizeanimation/minimizeanimation.h"
#include "effects/mousemark/mousemark.h"
#include "effects/presentwindows/presentwindows.h"
#include "effects/resize/resize.h"
#include "effects/showfps/showfps.h"
#include "effects/showpaint/showpaint.h"
#include "effects/slide/slide.h"
#include "effects/slideback/slideback.h"
#include "effects/slidingpopups/slidingpopups.h"
#include "effects/snaphelper/snaphelper.h"
#include "effects/taskbarthumbnail/taskbarthumbnail.h"
#include "effects/thumbnailaside/thumbnailaside.h"
#include "effects/trackmouse/trackmouse.h"
#include "effects/windowgeometry/windowgeometry.h"
#include "effects/zoom/zoom.h"
#include "effects/startupfeedback/startupfeedback.h"

#include <QMetaProperty>
#include <QDBusServiceWatcher>
#include <QDBusPendingCallWatcher>

#include <KDebug>
#include <KDesktopFile>
#include <KConfigGroup>
#include <KGlobal>
#include <KStandardDirs>
#include <KService>
#include <KServiceTypeTrader>
#include <KPluginInfo>

#include <assert.h>

namespace KWin
{

//---------------------
// Static

static QByteArray readWindowProperty(Window win, long atom, long type, int format)
{
    int len = 32768;
    for (;;) {
        unsigned char* data;
        Atom rtype;
        int rformat;
        unsigned long nitems, after;
        if (XGetWindowProperty(QX11Info::display(), win,
                              atom, 0, len, False, AnyPropertyType,
                              &rtype, &rformat, &nitems, &after, &data) == Success) {
            if (after > 0) {
                XFree(data);
                len *= 2;
                continue;
            }
            if (long(rtype) == type && rformat == format) {
                int bytelen = format == 8 ? nitems : format == 16 ? nitems * sizeof(short) : nitems * sizeof(long);
                QByteArray ret(reinterpret_cast< const char* >(data), bytelen);
                XFree(data);
                return ret;
            } else { // wrong format, type or something
                XFree(data);
                return QByteArray();
            }
        } else // XGetWindowProperty() failed
            return QByteArray();
    }
}

static void deleteWindowProperty(Window win, long int atom)
{
    XDeleteProperty(QX11Info::display(), win, atom);
}

//---------------------

EffectsHandlerImpl::EffectsHandlerImpl(Compositor *compositor, Scene *scene)
    : EffectsHandler(scene->compositingType())
    , keyboard_grab_effect(NULL)
    , fullscreen_effect(0)
    , next_window_quad_type(EFFECT_QUAD_TYPE_START)
    , m_compositor(compositor)
    , m_scene(scene)
    , m_desktopRendering(false)
    , m_currentRenderedDesktop(0)
{
    new EffectsAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/Effects", this);
    dbus.registerService("org.kde.kwin.Effects");
    // init is important, otherwise causes crashes when quads are build before the first painting pass start
    m_currentBuildQuadsIterator = m_activeEffects.constEnd();

    Workspace *ws = Workspace::self();
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(ws, SIGNAL(currentDesktopChanged(int,KWin::Client*)), SLOT(slotDesktopChanged(int,KWin::Client*)));
    connect(ws, SIGNAL(desktopPresenceChanged(KWin::Client*,int)), SLOT(slotDesktopPresenceChanged(KWin::Client*,int)));
    connect(ws, SIGNAL(clientAdded(KWin::Client*)), this, SLOT(slotClientAdded(KWin::Client*)));
    connect(ws, SIGNAL(unmanagedAdded(KWin::Unmanaged*)), this, SLOT(slotUnmanagedAdded(KWin::Unmanaged*)));
    connect(ws, SIGNAL(clientActivated(KWin::Client*)), this, SLOT(slotClientActivated(KWin::Client*)));
    connect(ws, SIGNAL(deletedRemoved(KWin::Deleted*)), this, SLOT(slotDeletedRemoved(KWin::Deleted*)));
    connect(vds, SIGNAL(countChanged(uint,uint)), SIGNAL(numberDesktopsChanged(uint)));
    connect(Cursor::self(), SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    connect(ws, SIGNAL(propertyNotify(long)), this, SLOT(slotPropertyNotify(long)));
    connect(ws, SIGNAL(stackingOrderChanged()), SIGNAL(stackingOrderChanged()));
#ifdef KWIN_BUILD_SCREENEDGES
    connect(ScreenEdges::self(), SIGNAL(approaching(ElectricBorder,qreal,QRect)), SIGNAL(screenEdgeApproaching(ElectricBorder,qreal,QRect)));
#endif
    // connect all clients
    foreach (const Client *c, ws->clientList()) {
        setupClientConnections(c);
    }
    foreach (const Unmanaged *u, ws->unmanagedList()) {
        setupUnmanagedConnections(u);
    }
    reconfigure();
}

EffectsHandlerImpl::~EffectsHandlerImpl()
{
    if (keyboard_grab_effect != NULL) {
        ungrabKeyboard();
    }
    QStringList loaded_names;
    foreach (const EffectPair &ep, loaded_effects) {
        loaded_names.append(ep.first);
    }
    foreach (const QString &en, loaded_names) {
        unloadEffect(en);
    }
}

void EffectsHandlerImpl::setupClientConnections(const Client* c)
{
    connect(c, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), this, SLOT(slotWindowClosed(KWin::Toplevel*)));
    connect(c, SIGNAL(clientMaximizedStateChanged(KWin::Client*,KDecorationDefines::MaximizeMode)), this, SLOT(slotClientMaximized(KWin::Client*,KDecorationDefines::MaximizeMode)));
    connect(c, SIGNAL(clientStartUserMovedResized(KWin::Client*)), this, SLOT(slotClientStartUserMovedResized(KWin::Client*)));
    connect(c, SIGNAL(clientStepUserMovedResized(KWin::Client*,QRect)), this, SLOT(slotClientStepUserMovedResized(KWin::Client*,QRect)));
    connect(c, SIGNAL(clientFinishUserMovedResized(KWin::Client*)), this, SLOT(slotClientFinishUserMovedResized(KWin::Client*)));
    connect(c, SIGNAL(opacityChanged(KWin::Toplevel*,qreal)), this, SLOT(slotOpacityChanged(KWin::Toplevel*,qreal)));
    connect(c, SIGNAL(clientMinimized(KWin::Client*,bool)), this, SLOT(slotClientMinimized(KWin::Client*,bool)));
    connect(c, SIGNAL(clientUnminimized(KWin::Client*,bool)), this, SLOT(slotClientUnminimized(KWin::Client*,bool)));
    connect(c, SIGNAL(modalChanged()), this, SLOT(slotClientModalityChanged()));
    connect(c, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), this, SLOT(slotGeometryShapeChanged(KWin::Toplevel*,QRect)));
    connect(c, SIGNAL(paddingChanged(KWin::Toplevel*,QRect)), this, SLOT(slotPaddingChanged(KWin::Toplevel*,QRect)));
    connect(c, SIGNAL(damaged(KWin::Toplevel*,QRect)), this, SLOT(slotWindowDamaged(KWin::Toplevel*,QRect)));
    connect(c, SIGNAL(propertyNotify(KWin::Toplevel*,long)), this, SLOT(slotPropertyNotify(KWin::Toplevel*,long)));
}

void EffectsHandlerImpl::setupUnmanagedConnections(const Unmanaged* u)
{
    connect(u, SIGNAL(windowClosed(KWin::Toplevel*,KWin::Deleted*)), this, SLOT(slotWindowClosed(KWin::Toplevel*)));
    connect(u, SIGNAL(opacityChanged(KWin::Toplevel*,qreal)), this, SLOT(slotOpacityChanged(KWin::Toplevel*,qreal)));
    connect(u, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), this, SLOT(slotGeometryShapeChanged(KWin::Toplevel*,QRect)));
    connect(u, SIGNAL(paddingChanged(KWin::Toplevel*,QRect)), this, SLOT(slotPaddingChanged(KWin::Toplevel*,QRect)));
    connect(u, SIGNAL(damaged(KWin::Toplevel*,QRect)), this, SLOT(slotWindowDamaged(KWin::Toplevel*,QRect)));
    connect(u, SIGNAL(propertyNotify(KWin::Toplevel*,long)), this, SLOT(slotPropertyNotify(KWin::Toplevel*,long)));
}

void EffectsHandlerImpl::reconfigure()
{
    const KService::List offers = KServiceTypeTrader::self()->query(QString("KWin/Effect"), QString());
    QStringList effectsToBeLoaded;
    QStringList checkDefault;
    KConfigGroup conf(KGlobal::config(), "Plugins");

    // First unload necessary effects
    foreach (const KService::Ptr & service, offers) {
        KPluginInfo plugininfo(service);
        plugininfo.load(conf);

        if (plugininfo.isPluginEnabledByDefault()) {
            const QString key = plugininfo.pluginName() + QString::fromLatin1("Enabled");
            if (!conf.hasKey(key))
                checkDefault.append(plugininfo.pluginName());
        }

        bool isloaded = isEffectLoaded(plugininfo.pluginName());
        bool shouldbeloaded = plugininfo.isPluginEnabled();
        if (!shouldbeloaded && isloaded)
            unloadEffect(plugininfo.pluginName());
        if (shouldbeloaded)
            effectsToBeLoaded.append(plugininfo.pluginName());
    }
    QStringList newLoaded;
    // Then load those that should be loaded
    foreach (const QString & effectName, effectsToBeLoaded) {
        if (!isEffectLoaded(effectName)) {
            if (loadEffect(effectName, checkDefault.contains(effectName)))
                newLoaded.append(effectName);
        }
    }
    foreach (const EffectPair & ep, loaded_effects) {
        if (!newLoaded.contains(ep.first))    // don't reconfigure newly loaded effects
            ep.second->reconfigure(Effect::ReconfigureAll);
    }
}

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->prePaintScreen(data, time);
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->paintScreen(mask, region, data);
        --m_currentPaintScreenIterator;
    } else
        m_scene->finalPaintScreen(mask, region, data);
}

void EffectsHandlerImpl::paintDesktop(int desktop, int mask, QRegion region, ScreenPaintData &data)
{
    if (desktop < 1 || desktop > numberOfDesktops()) {
        return;
    }
    m_currentRenderedDesktop = desktop;
    m_desktopRendering = true;
    // save the paint screen iterator
    EffectsIterator savedIterator = m_currentPaintScreenIterator;
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    effects->paintScreen(mask, region, data);
    // restore the saved iterator
    m_currentPaintScreenIterator = savedIterator;
    m_desktopRendering = false;
}

void EffectsHandlerImpl::postPaintScreen()
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->postPaintScreen();
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->prePaintWindow(w, data, time);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

void EffectsHandlerImpl::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->paintWindow(w, mask, region, data);
        --m_currentPaintWindowIterator;
    } else
        m_scene->finalPaintWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    if (m_currentPaintEffectFrameIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintEffectFrameIterator++)->paintEffectFrame(frame, region, opacity, frameOpacity);
        --m_currentPaintEffectFrameIterator;
    } else {
        const EffectFrameImpl* frameImpl = static_cast<const EffectFrameImpl*>(frame);
        frameImpl->finalRender(region, opacity, frameOpacity);
    }
}

void EffectsHandlerImpl::postPaintWindow(EffectWindow* w)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->postPaintWindow(w);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

Effect *EffectsHandlerImpl::provides(Effect::Feature ef)
{
    for (int i = 0; i < loaded_effects.size(); ++i)
        if (loaded_effects.at(i).second->provides(ef))
            return loaded_effects.at(i).second;
    return NULL;
}

void EffectsHandlerImpl::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_currentDrawWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentDrawWindowIterator++)->drawWindow(w, mask, region, data);
        --m_currentDrawWindowIterator;
    } else
        m_scene->finalDrawWindow(static_cast<EffectWindowImpl*>(w), mask, region, data);
}

void EffectsHandlerImpl::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    static bool initIterator = true;
    if (initIterator) {
        m_currentBuildQuadsIterator = m_activeEffects.constBegin();
        initIterator = false;
    }
    if (m_currentBuildQuadsIterator != m_activeEffects.constEnd()) {
        (*m_currentBuildQuadsIterator++)->buildQuads(w, quadList);
        --m_currentBuildQuadsIterator;
    }
    if (m_currentBuildQuadsIterator == m_activeEffects.constBegin())
        initIterator = true;
}

bool EffectsHandlerImpl::hasDecorationShadows() const
{
    return decorationPlugin()->hasShadows();
}

// start another painting pass
void EffectsHandlerImpl::startPaint()
{
    m_activeEffects.clear();
    m_activeEffects.reserve(loaded_effects.count());
    foreach (const KWin::EffectPair &it, loaded_effects) {
        if (it.second->isActive()) {
            m_activeEffects << it.second;
        }
    }
    m_currentDrawWindowIterator = m_activeEffects.constBegin();
    m_currentPaintWindowIterator = m_activeEffects.constBegin();
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    m_currentPaintEffectFrameIterator = m_activeEffects.constBegin();
}

void EffectsHandlerImpl::slotClientMaximized(KWin::Client *c, KDecorationDefines::MaximizeMode maxMode)
{
    bool horizontal = false;
    bool vertical = false;
    switch (maxMode) {
    case KDecorationDefines::MaximizeHorizontal:
        horizontal = true;
        break;
    case KDecorationDefines::MaximizeVertical:
        vertical = true;
        break;
    case KDecorationDefines::MaximizeFull:
        horizontal = true;
        vertical = true;
        break;
    case KDecorationDefines::MaximizeRestore: // fall through
    default:
        // default - nothing to do
        break;
    }
    if (EffectWindowImpl *w = c->effectWindow()) {
        emit windowMaximizedStateChanged(w, horizontal, vertical);
    }
}

void EffectsHandlerImpl::slotClientStartUserMovedResized(Client *c)
{
    emit windowStartUserMovedResized(c->effectWindow());
}

void EffectsHandlerImpl::slotClientFinishUserMovedResized(Client *c)
{
    emit windowFinishUserMovedResized(c->effectWindow());
}

void EffectsHandlerImpl::slotClientStepUserMovedResized(Client* c, const QRect& geometry)
{
    emit windowStepUserMovedResized(c->effectWindow(), geometry);
}

void EffectsHandlerImpl::slotOpacityChanged(Toplevel *t, qreal oldOpacity)
{
    if (t->opacity() == oldOpacity || !t->effectWindow()) {
        return;
    }
    emit windowOpacityChanged(t->effectWindow(), oldOpacity, (qreal)t->opacity());
}

void EffectsHandlerImpl::slotClientAdded(Client *c)
{
    if (c->readyForPainting())
        slotClientShown(c);
    else
        connect(c, SIGNAL(windowShown(KWin::Toplevel*)), SLOT(slotClientShown(KWin::Toplevel*)));
}

void EffectsHandlerImpl::slotUnmanagedAdded(Unmanaged *u)
{
    // it's never initially ready but has synthetic 50ms delay
    connect(u, SIGNAL(windowShown(KWin::Toplevel*)), SLOT(slotUnmanagedShown(KWin::Toplevel*)));
}

void EffectsHandlerImpl::slotClientShown(KWin::Toplevel *t)
{
    Q_ASSERT(qobject_cast<Client*>(t));
    Client *c = qobject_cast<Client*>(t);
    setupClientConnections(c);
    if (!c->tabGroup()) // the "window" has already been there
        emit windowAdded(c->effectWindow());
}

void EffectsHandlerImpl::slotUnmanagedShown(KWin::Toplevel *t)
{   // regardless, unmanaged windows are -yet?- not synced anyway
    Q_ASSERT(qobject_cast<Unmanaged*>(t));
    Unmanaged *u = qobject_cast<Unmanaged*>(t);
    setupUnmanagedConnections(u);
    emit windowAdded(u->effectWindow());
}

void EffectsHandlerImpl::slotDeletedRemoved(KWin::Deleted *d)
{
    emit windowDeleted(d->effectWindow());
    elevated_windows.removeAll(d->effectWindow());
}

void EffectsHandlerImpl::slotWindowClosed(KWin::Toplevel *c)
{
    c->disconnect(this);
    emit windowClosed(c->effectWindow());
}

void EffectsHandlerImpl::slotClientActivated(KWin::Client *c)
{
    emit windowActivated(c ? c->effectWindow() : NULL);
}

void EffectsHandlerImpl::slotClientMinimized(Client *c, bool animate)
{
    // TODO: notify effects even if it should not animate?
    if (animate) {
        emit windowMinimized(c->effectWindow());
    }
}

void EffectsHandlerImpl::slotClientUnminimized(Client* c, bool animate)
{
    // TODO: notify effects even if it should not animate?
    if (animate) {
        emit windowUnminimized(c->effectWindow());
    }
}

void EffectsHandlerImpl::slotClientModalityChanged()
{
    emit windowModalityChanged(static_cast<Client*>(sender())->effectWindow());
}

void EffectsHandlerImpl::slotDesktopChanged(int old, Client *c)
{
    const int newDesktop = VirtualDesktopManager::self()->current();
    if (old != 0 && newDesktop != old) {
        emit desktopChanged(old, newDesktop, c ? c->effectWindow() : 0);
        // TODO: remove in 4.10
        emit desktopChanged(old, newDesktop);
    }
}

void EffectsHandlerImpl::slotDesktopPresenceChanged(Client *c, int old)
{
    if (!c->effectWindow()) {
        return;
    }
    emit desktopPresenceChanged(c->effectWindow(), old, c->desktop());
}

void EffectsHandlerImpl::slotWindowDamaged(Toplevel* t, const QRect& r)
{
    if (!t->effectWindow()) {
        // can happen during tear down of window
        return;
    }
    emit windowDamaged(t->effectWindow(), r);
}

void EffectsHandlerImpl::slotGeometryShapeChanged(Toplevel* t, const QRect& old)
{
    // during late cleanup effectWindow() may be already NULL
    // in some functions that may still call this
    if (t == NULL || t->effectWindow() == NULL)
        return;
    emit windowGeometryShapeChanged(t->effectWindow(), old);
}

void EffectsHandlerImpl::slotPaddingChanged(Toplevel* t, const QRect& old)
{
    // during late cleanup effectWindow() may be already NULL
    // in some functions that may still call this
    if (t == NULL || t->effectWindow() == NULL)
        return;
    emit windowPaddingChanged(t->effectWindow(), old);
}

void EffectsHandlerImpl::setActiveFullScreenEffect(Effect* e)
{
    fullscreen_effect = e;
    m_compositor->checkUnredirect();
}

Effect* EffectsHandlerImpl::activeFullScreenEffect() const
{
    return fullscreen_effect;
}

bool EffectsHandlerImpl::grabKeyboard(Effect* effect)
{
    if (keyboard_grab_effect != NULL)
        return false;
    bool ret = grabXKeyboard();
    if (!ret)
        return false;
    keyboard_grab_effect = effect;
    return true;
}

void EffectsHandlerImpl::ungrabKeyboard()
{
    assert(keyboard_grab_effect != NULL);
    ungrabXKeyboard();
    keyboard_grab_effect = NULL;
}

void EffectsHandlerImpl::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (keyboard_grab_effect != NULL)
        keyboard_grab_effect->grabbedKeyboardEvent(e);
}

void EffectsHandlerImpl::startMouseInterception(Effect *effect, Qt::CursorShape shape)
{
    if (m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.append(effect);
    if (m_grabbedMouseEffects.size() != 1) {
        return;
    }
    // NOTE: it is intended to not perform an XPointerGrab on X11. See documentation in kwineffects.h
    // The mouse grab is implemented by using a full screen input only window
    if (!m_mouseInterceptionWindow.isValid()) {
        const QRect geo(0, 0, displayWidth(), displayHeight());
        const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
        const uint32_t values[] = {
            true,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
            Cursor::x11Cursor(shape)
        };
        m_mouseInterceptionWindow.reset(Xcb::createInputWindow(geo, mask, values));
    }
    m_mouseInterceptionWindow.map();
    m_mouseInterceptionWindow.raise();
    // Raise electric border windows above the input windows
    // so they can still be triggered.
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->ensureOnTop();
#endif
}

void EffectsHandlerImpl::stopMouseInterception(Effect *effect)
{
    if (!m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.removeAll(effect);
    if (m_grabbedMouseEffects.isEmpty()) {
        m_mouseInterceptionWindow.unmap();
#ifdef KWIN_BUILD_SCREENEDGES
        Workspace::self()->stackScreenEdgesUnderOverrideRedirect();
#endif
    }
}

void* EffectsHandlerImpl::getProxy(QString name)
{
    // All effects start with "kwin4_effect_", prepend it to the name
    name.prepend("kwin4_effect_");

    foreach (const EffectPair &it, loaded_effects) {
        if (it.first == name) {
            return it.second->proxy();
        }
    }

    return NULL;
}

void EffectsHandlerImpl::startMousePolling()
{
    Cursor::self()->startMousePolling();
}

void EffectsHandlerImpl::stopMousePolling()
{
    Cursor::self()->stopMousePolling();
}

bool EffectsHandlerImpl::hasKeyboardGrab() const
{
    return keyboard_grab_effect != NULL;
}

void EffectsHandlerImpl::desktopResized(const QSize &size)
{
    m_scene->screenGeometryChanged(size);
    if (m_mouseInterceptionWindow.isValid()) {
        m_mouseInterceptionWindow.setGeometry(QRect(0, 0, size.width(), size.height()));
    }
    emit screenGeometryChanged(size);
}

void EffectsHandlerImpl::slotPropertyNotify(Toplevel* t, long int atom)
{
    if (!registered_atoms.contains(atom))
        return;
    emit propertyNotify(t->effectWindow(), atom);
}

void EffectsHandlerImpl::slotPropertyNotify(long int atom)
{
    if (!registered_atoms.contains(atom))
        return;
    emit propertyNotify(NULL, atom);
}

void EffectsHandlerImpl::registerPropertyType(long atom, bool reg)
{
    if (reg)
        ++registered_atoms[ atom ]; // initialized to 0 if not present yet
    else {
        if (--registered_atoms[ atom ] == 0)
            registered_atoms.remove(atom);
    }
}

xcb_atom_t EffectsHandlerImpl::announceSupportProperty(const QByteArray &propertyName, Effect *effect)
{
    PropertyEffectMap::iterator it = m_propertiesForEffects.find(propertyName);
    if (it != m_propertiesForEffects.end()) {
        // property has already been registered for an effect
        // just append Effect and return the atom stored in m_managedProperties
        if (!it.value().contains(effect)) {
            it.value().append(effect);
        }
        return m_managedProperties.value(propertyName);
    }
    // get the atom for the propertyName
    ScopedCPointer<xcb_intern_atom_reply_t> atomReply(xcb_intern_atom_reply(connection(),
        xcb_intern_atom_unchecked(connection(), false, propertyName.size(), propertyName.constData()),
        NULL));
    if (atomReply.isNull()) {
        return XCB_ATOM_NONE;
    }
    m_compositor->keepSupportProperty(atomReply->atom);
    // announce property on root window
    unsigned char dummy = 0;
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, rootWindow(), atomReply->atom, atomReply->atom, 8, 1, &dummy);
    // TODO: add to _NET_SUPPORTED
    m_managedProperties.insert(propertyName, atomReply->atom);
    m_propertiesForEffects.insert(propertyName, QList<Effect*>() << effect);
    registerPropertyType(atomReply->atom, true);
    return atomReply->atom;
}

void EffectsHandlerImpl::removeSupportProperty(const QByteArray &propertyName, Effect *effect)
{
    PropertyEffectMap::iterator it = m_propertiesForEffects.find(propertyName);
    if (it == m_propertiesForEffects.end()) {
        // property is not registered - nothing to do
        return;
    }
    if (!it.value().contains(effect)) {
        // property is not registered for given effect - nothing to do
        return;
    }
    it.value().removeAll(effect);
    if (!it.value().isEmpty()) {
        // property still registered for another effect - nothing further to do
        return;
    }
    const xcb_atom_t atom = m_managedProperties.take(propertyName);
    registerPropertyType(atom, false);
    m_propertiesForEffects.remove(propertyName);
    m_compositor->removeSupportProperty(atom); // delayed removal
}

QByteArray EffectsHandlerImpl::readRootProperty(long atom, long type, int format) const
{
    return readWindowProperty(rootWindow(), atom, type, format);
}

void EffectsHandlerImpl::deleteRootProperty(long atom) const
{
    deleteWindowProperty(rootWindow(), atom);
}

void EffectsHandlerImpl::activateWindow(EffectWindow* c)
{
    if (Client* cl = qobject_cast< Client* >(static_cast<EffectWindowImpl*>(c)->window()))
        Workspace::self()->activateClient(cl, true);
}

EffectWindow* EffectsHandlerImpl::activeWindow() const
{
    return Workspace::self()->activeClient() ? Workspace::self()->activeClient()->effectWindow() : NULL;
}

void EffectsHandlerImpl::moveWindow(EffectWindow* w, const QPoint& pos, bool snap, double snapAdjust)
{
    Client* cl = qobject_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (!cl || !cl->isMovable())
        return;

    if (snap)
        cl->move(Workspace::self()->adjustClientPosition(cl, pos, true, snapAdjust));
    else
        cl->move(pos);
}

void EffectsHandlerImpl::windowToDesktop(EffectWindow* w, int desktop)
{
    Client* cl = qobject_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock())
        Workspace::self()->sendClientToDesktop(cl, desktop, true);
}

void EffectsHandlerImpl::windowToScreen(EffectWindow* w, int screen)
{
    Client* cl = qobject_cast< Client* >(static_cast<EffectWindowImpl*>(w)->window());
    if (cl && !cl->isDesktop() && !cl->isDock())
        Workspace::self()->sendClientToScreen(cl, screen);
}

void EffectsHandlerImpl::setShowingDesktop(bool showing)
{
    Workspace::self()->setShowingDesktop(showing);
}

int EffectsHandlerImpl::currentDesktop() const
{
    return VirtualDesktopManager::self()->current();
}

int EffectsHandlerImpl::numberOfDesktops() const
{
    return VirtualDesktopManager::self()->count();
}

void EffectsHandlerImpl::setCurrentDesktop(int desktop)
{
    VirtualDesktopManager::self()->setCurrent(desktop);
}

void EffectsHandlerImpl::setNumberOfDesktops(int desktops)
{
    VirtualDesktopManager::self()->setCount(desktops);
}

QSize EffectsHandlerImpl::desktopGridSize() const
{
    return VirtualDesktopManager::self()->grid().size();
}

int EffectsHandlerImpl::desktopGridWidth() const
{
    return desktopGridSize().width();
}

int EffectsHandlerImpl::desktopGridHeight() const
{
    return desktopGridSize().height();
}

int EffectsHandlerImpl::workspaceWidth() const
{
    return desktopGridWidth() * displayWidth();
}

int EffectsHandlerImpl::workspaceHeight() const
{
    return desktopGridHeight() * displayHeight();
}

int EffectsHandlerImpl::desktopAtCoords(QPoint coords) const
{
    return VirtualDesktopManager::self()->grid().at(coords);
}

QPoint EffectsHandlerImpl::desktopGridCoords(int id) const
{
    return VirtualDesktopManager::self()->grid().gridCoords(id);
}

QPoint EffectsHandlerImpl::desktopCoords(int id) const
{
    QPoint coords = VirtualDesktopManager::self()->grid().gridCoords(id);
    if (coords.x() == -1)
        return QPoint(-1, -1);
    return QPoint(coords.x() * displayWidth(), coords.y() * displayHeight());
}

int EffectsHandlerImpl::desktopAbove(int desktop, bool wrap) const
{
    return getDesktop<DesktopAbove>(desktop, wrap);
}

int EffectsHandlerImpl::desktopToRight(int desktop, bool wrap) const
{
    return getDesktop<DesktopRight>(desktop, wrap);
}

int EffectsHandlerImpl::desktopBelow(int desktop, bool wrap) const
{
    return getDesktop<DesktopBelow>(desktop, wrap);
}

int EffectsHandlerImpl::desktopToLeft(int desktop, bool wrap) const
{
    return getDesktop<DesktopLeft>(desktop, wrap);
}

QString EffectsHandlerImpl::desktopName(int desktop) const
{
    return VirtualDesktopManager::self()->name(desktop);
}

bool EffectsHandlerImpl::optionRollOverDesktops() const
{
    return options->isRollOverDesktops();
}

double EffectsHandlerImpl::animationTimeFactor() const
{
    return options->animationTimeFactor();
}

WindowQuadType EffectsHandlerImpl::newWindowQuadType()
{
    return WindowQuadType(next_window_quad_type++);
}

int EffectsHandlerImpl::displayWidth() const
{
    return KWin::displayWidth();
}

int EffectsHandlerImpl::displayHeight() const
{
    return KWin::displayHeight();
}

EffectWindow* EffectsHandlerImpl::findWindow(WId id) const
{
    if (Client* w = Workspace::self()->findClient(WindowMatchPredicate(id)))
        return w->effectWindow();
    if (Unmanaged* w = Workspace::self()->findUnmanaged(WindowMatchPredicate(id)))
        return w->effectWindow();
    return NULL;
}

EffectWindowList EffectsHandlerImpl::stackingOrder() const
{
    ToplevelList list = Workspace::self()->xStackingOrder();
    EffectWindowList ret;
    foreach (Toplevel *it, list) {
        if (EffectWindow *ew = effectWindow(it))
            ret.append(ew);
    }
    return ret;
}

void EffectsHandlerImpl::setElevatedWindow(EffectWindow* w, bool set)
{
    elevated_windows.removeAll(w);
    if (set)
        elevated_windows.append(w);
}

void EffectsHandlerImpl::addRepaintFull()
{
    m_compositor->addRepaintFull();
}

void EffectsHandlerImpl::addRepaint(const QRect& r)
{
    m_compositor->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(const QRegion& r)
{
    m_compositor->addRepaint(r);
}

void EffectsHandlerImpl::addRepaint(int x, int y, int w, int h)
{
    m_compositor->addRepaint(x, y, w, h);
}

int EffectsHandlerImpl::activeScreen() const
{
    return screens()->current();
}

int EffectsHandlerImpl::numScreens() const
{
    return screens()->count();
}

int EffectsHandlerImpl::screenNumber(const QPoint& pos) const
{
    return screens()->number(pos);
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, int screen, int desktop) const
{
    return Workspace::self()->clientArea(opt, screen, desktop);
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const EffectWindow* c) const
{
    const Toplevel* t = static_cast< const EffectWindowImpl* >(c)->window();
    if (const Client* cl = qobject_cast< const Client* >(t))
        return Workspace::self()->clientArea(opt, cl);
    else
        return Workspace::self()->clientArea(opt, t->geometry().center(), VirtualDesktopManager::self()->current());
}

QRect EffectsHandlerImpl::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return Workspace::self()->clientArea(opt, p, desktop);
}

void EffectsHandlerImpl::defineCursor(Qt::CursorShape shape)
{
    if (!m_mouseInterceptionWindow.isValid()) {
        return;
    }
    m_mouseInterceptionWindow.defineCursor(Cursor::x11Cursor(shape));
}

bool EffectsHandlerImpl::checkInputWindowEvent(XEvent* e)
{
    if (e->type != ButtonPress && e->type != ButtonRelease && e->type != MotionNotify)
        return false;
    if (m_grabbedMouseEffects.isEmpty() || m_mouseInterceptionWindow != e->xany.window) {
        return false;
    }
    foreach (Effect *effect, m_grabbedMouseEffects) {
        switch(e->type) {
        case ButtonPress: {
            XButtonEvent* e2 = &e->xbutton;
            Qt::MouseButton button = x11ToQtMouseButton(e2->button);
            Qt::MouseButtons buttons = x11ToQtMouseButtons(e2->state) | button;
            QMouseEvent ev(QEvent::MouseButtonPress,
                            QPoint(e2->x, e2->y), QPoint(e2->x_root, e2->y_root),
                            button, buttons, x11ToQtKeyboardModifiers(e2->state));
            effect->windowInputMouseEvent(&ev);
            break; // --->
        }
        case ButtonRelease: {
            XButtonEvent* e2 = &e->xbutton;
            Qt::MouseButton button = x11ToQtMouseButton(e2->button);
            Qt::MouseButtons buttons = x11ToQtMouseButtons(e2->state) & ~button;
            QMouseEvent ev(QEvent::MouseButtonRelease,
                            QPoint(e2->x, e2->y), QPoint(e2->x_root, e2->y_root),
                            button, buttons, x11ToQtKeyboardModifiers(e2->state));
            effect->windowInputMouseEvent(&ev);
            break; // --->
        }
        case MotionNotify: {
            XMotionEvent* e2 = &e->xmotion;
            QMouseEvent ev(QEvent::MouseMove, QPoint(e2->x, e2->y), QPoint(e2->x_root, e2->y_root),
                            Qt::NoButton, x11ToQtMouseButtons(e2->state), x11ToQtKeyboardModifiers(e2->state));
            effect->windowInputMouseEvent(&ev);
            break; // --->
        }
        }
    }
    return true; // eat event
}

void EffectsHandlerImpl::checkInputWindowStacking()
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return;
    }
    m_mouseInterceptionWindow.raise();
    // Raise electric border windows above the input windows
    // so they can still be triggered. TODO: Do both at once.
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->ensureOnTop();
#endif
}

QPoint EffectsHandlerImpl::cursorPos() const
{
    return Cursor::pos();
}

void EffectsHandlerImpl::reserveElectricBorder(ElectricBorder border, Effect *effect)
{
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->reserve(border, effect, "borderActivated");
#else
    Q_UNUSED(border)
    Q_UNUSED(effect)
#endif
}

void EffectsHandlerImpl::unreserveElectricBorder(ElectricBorder border, Effect *effect)
{
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::self()->unreserve(border, effect);
#else
    Q_UNUSED(border)
    Q_UNUSED(effect)
#endif
}

unsigned long EffectsHandlerImpl::xrenderBufferPicture()
{
#ifdef KWIN_BUILD_COMPOSITE
    if (SceneXrender* s = qobject_cast< SceneXrender* >(m_scene))
        return s->bufferPicture();
#endif
    return None;
}

void EffectsHandlerImpl::toggleEffect(const QString& name)
{
    if (isEffectLoaded(name))
        unloadEffect(name);
    else
        loadEffect(name);
}

QStringList EffectsHandlerImpl::loadedEffects() const
{
    QStringList listModules;
    foreach (const EffectPair &it, loaded_effects) {
        listModules << it.first;
    }
    return listModules;
}

QStringList EffectsHandlerImpl::listOfEffects() const
{
    const KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect");
    QStringList listOfModules;
    // First unload necessary effects
    foreach (const KService::Ptr & service, offers) {
        KPluginInfo plugininfo(service);
        listOfModules << plugininfo.pluginName();
    }
    return listOfModules;
}

bool EffectsHandlerImpl::loadEffect(const QString& name, bool checkDefault)
{
    m_compositor->addRepaintFull();

    if (!name.startsWith(QLatin1String("kwin4_effect_")))
        kWarning(1212) << "Effect names usually have kwin4_effect_ prefix" ;

    // Make sure a single effect won't be loaded multiple times
    foreach (const EffectPair &it, loaded_effects) {
        if (it.first == name) {
            kDebug(1212) << "EffectsHandler::loadEffect : Effect already loaded : " << name;
            return true;
        }
    }


    kDebug(1212) << "Trying to load " << name;
    QString internalname = name.toLower();

    QString constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg(internalname);
    KService::List offers = KServiceTypeTrader::self()->query("KWin/Effect", constraint);
    if (offers.isEmpty()) {
        kError(1212) << "Couldn't find effect " << name;
        return false;
    }
    KService::Ptr service = offers.first();

    Effect* effect = 0;
    // builtins first
    if (internalname == "kwin4_effect_presentwindows") {
        effect = new PresentWindowsEffect();
    } else if (internalname == "kwin4_effect_slidingpopups") {
        effect = new SlidingPopupsEffect();
    } else if (internalname == "kwin4_effect_taskbarthumbnail") {
        effect = new TaskbarThumbnailEffect();
    } else if (internalname == "kwin4_effect_diminactive") {
        effect = new DimInactiveEffect();
    } else if (internalname == "kwin4_effect_dimscreen") {
        effect = new DimScreenEffect();
    } else if (internalname == "kwin4_effect_highlightwindow") {
        effect = new HighlightWindowEffect();
    } else if (internalname == "kwin4_effect_minimizeanimation") {
        effect = new MinimizeAnimationEffect();
    } else if (internalname == "kwin4_effect_resize") {
        effect = new ResizeEffect();
    } else if (internalname == "kwin4_effect_showfps") {
        effect = new ShowFpsEffect();
    } else if (internalname == "kwin4_effect_showpaint") {
        effect = new ShowPaintEffect();
    } else if (internalname == "kwin4_effect_slide") {
        effect = new SlideEffect();
    } else if (internalname == "kwin4_effect_slideback") {
        effect = new SlideBackEffect();
    } else if (internalname == "kwin4_effect_thumbnailaside") {
        effect = new ThumbnailAsideEffect();
    } else if (internalname == "kwin4_effect_windowgeometry") {
        effect = new WindowGeometryEffect();
    } else if (internalname == "kwin4_effect_zoom") {
        effect = new ZoomEffect();
    } else if (internalname == "kwin4_effect_logout") {
        effect = new LogoutEffect();
    } else if (internalname == "kwin4_effect_magnifier") {
        effect = new MagnifierEffect();
    } else if (internalname == "kwin4_effect_mousemark") {
        effect = new MouseMarkEffect();
    } else if (internalname == "kwin4_effect_snaphelper") {
        effect = new SnapHelperEffect();
    } else if (internalname == "kwin4_effect_trackmouse") {
        effect = new TrackMouseEffect();
    } else if (internalname == "kwin4_effect_startupfeedback") {
        effect = new StartupFeedbackEffect();
    }

    if (effect) {
        bool enabledByDefault = service->property("X-KDE-PluginInfo-EnabledByDefault").toBool();
        if (checkDefault && !enabledByDefault) {
            kDebug(1212) << "Disabled internal effect" << name;
            delete effect;
            return false;
        }

        kDebug(1212) << "Internal effect has been loaded" << name;
        loaded_effects.append(EffectPair(name, effect));
        effectsChanged();
        return true;
    }

    kWarning() << "Invalid effect" << name;
    return false;
}

void EffectsHandlerImpl::unloadEffect(const QString& name)
{
    m_compositor->addRepaintFull();

    QMutableVectorIterator<EffectPair> it(loaded_effects);
    while (it.hasNext()) {
        const EffectPair &effect = it.next();
        if (effect.first == name) {
            Effect* effectptr = effect.second;
            kDebug(1212) << "EffectsHandler::unloadEffect : Unloading Effect : " << name;
            if (activeFullScreenEffect() == effectptr) {
                setActiveFullScreenEffect(0);
            }
            stopMouseInterception(effectptr);
            // remove support properties for the effect
            const QList<QByteArray> properties = m_propertiesForEffects.keys();
            foreach (const QByteArray &property, properties) {
                removeSupportProperty(property, effectptr);
            }
            it.remove();
            delete effectptr;
            effectsChanged();
            return;
        }
    }

    kDebug(1212) << "EffectsHandler::unloadEffect : Effect not loaded : " << name;
}

void EffectsHandlerImpl::reconfigureEffect(const QString& name)
{
    // effects use the global config for shortcuts and such, reload it
    KGlobal::config()->reparseConfiguration();
    foreach (KActionCollection* collection, KActionCollection::allCollections()) {
        collection->readSettings();
    }

    foreach (EffectPair &it, loaded_effects) {
        if (it.first == name) {
            it.second->reconfigure(Effect::ReconfigureAll);
            break;
        }
    }
}

bool EffectsHandlerImpl::isEffectLoaded(const QString& name) const
{
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it)
        if ((*it).first == name)
            return true;

    return false;
}

void EffectsHandlerImpl::reloadEffect(Effect *effect)
{
    QString effectName;
    foreach (const EffectPair &it, loaded_effects) {
        if (it.second == effect) {
            effectName = it.first;
            break;
        }
    }
    if (!effectName.isEmpty()) {
        unloadEffect(effectName);
        loadEffect(effectName);
    }
}

void EffectsHandlerImpl::effectsChanged()
{
    // it's possible to have a reconfigure and a quad rebuild between two paint cycles - bug #308201
    m_activeEffects.clear();
    m_activeEffects.reserve(loaded_effects.count());
}

QStringList EffectsHandlerImpl::activeEffects() const
{
    QStringList ret;
    foreach (const EffectPair &it, loaded_effects) {
        if (it.second->isActive()) {
            ret << it.first;
        }
    }
    return ret;
}

EffectFrame* EffectsHandlerImpl::effectFrame(bool staticSize, const QPoint& position, Qt::Alignment alignment) const
{
    return new EffectFrameImpl(staticSize, position, alignment);
}


QVariant EffectsHandlerImpl::kwinOption(KWinOption kwopt)
{
    switch (kwopt) {
    case CloseButtonCorner:
        return decorationPlugin()->closeButtonCorner();
#ifdef KWIN_BUILD_SCREENEDGES
    case SwitchDesktopOnScreenEdge:
        return ScreenEdges::self()->isDesktopSwitching();
    case SwitchDesktopOnScreenEdgeMovingWindows:
        return ScreenEdges::self()->isDesktopSwitchingMovingClients();
#endif
    default:
        return QVariant(); // an invalid one
    }
}

QString EffectsHandlerImpl::supportInformation(const QString &name) const
{
    if (!isEffectLoaded(name)) {
        return QString();
    }
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == name) {
            QString support((*it).first + ":\n");
            const QMetaObject *metaOptions = (*it).second->metaObject();
            for (int i=0; i<metaOptions->propertyCount(); ++i) {
                const QMetaProperty property = metaOptions->property(i);
                if (QLatin1String(property.name()) == "objectName") {
                    continue;
                }
                support.append(QLatin1String(property.name()) + ": " + (*it).second->property(property.name()).toString() + '\n');
            }
            return support;
        }
    }
    return QString();
}

QString EffectsHandlerImpl::debug(const QString& name, const QString& parameter) const
{
    QString internalName = name.startsWith("kwin4_effect_") ? name : "kwin4_effect_" + name;
    for (QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if ((*it).first == internalName) {
            return it->second->debug(parameter);
        }
    }
    return QString();
}

//****************************************
// EffectWindowImpl
//****************************************

EffectWindowImpl::EffectWindowImpl(Toplevel *toplevel)
    : EffectWindow(toplevel)
    , toplevel(toplevel)
    , sw(NULL)
{
}

EffectWindowImpl::~EffectWindowImpl()
{
}

bool EffectWindowImpl::isPaintingEnabled()
{
    return sceneWindow()->isPaintingEnabled();
}

void EffectWindowImpl::enablePainting(int reason)
{
    sceneWindow()->enablePainting(reason);
}

void EffectWindowImpl::disablePainting(int reason)
{
    sceneWindow()->disablePainting(reason);
}

const EffectWindowGroup* EffectWindowImpl::group() const
{
    if (Client* c = qobject_cast< Client* >(toplevel))
        return c->group()->effectGroup();
     // TODO
    return NULL;
}

void EffectWindowImpl::refWindow()
{
    if (Deleted* d = qobject_cast< Deleted* >(toplevel))
        return d->refWindow();
    // TODO
    kFatal() << "Something strange happened";
}

void EffectWindowImpl::unrefWindow()
{
    if (Deleted* d = qobject_cast< Deleted* >(toplevel))
        return d->unrefWindow();   // delays deletion in case
    // TODO
    kFatal() << "Something strange happened";
}

void EffectWindowImpl::setWindow(Toplevel* w)
{
    toplevel = w;
    setParent(w);
}

void EffectWindowImpl::setSceneWindow(Scene::Window* w)
{
    sw = w;
}

QRegion EffectWindowImpl::shape() const
{
    return sw ? sw->shape() : geometry();
}

QRect EffectWindowImpl::decorationInnerRect() const
{
    Client *client = qobject_cast<Client*>(toplevel);
    return client ? client->transparentRect() : contentsRect();
}

QByteArray EffectWindowImpl::readProperty(long atom, long type, int format) const
{
    return readWindowProperty(window()->window(), atom, type, format);
}

void EffectWindowImpl::deleteProperty(long int atom) const
{
    deleteWindowProperty(window()->window(), atom);
}

EffectWindow* EffectWindowImpl::findModal()
{
    if (Client* c = qobject_cast< Client* >(toplevel)) {
        if (Client* c2 = c->findModal())
            return c2->effectWindow();
    }
    return NULL;
}

template <typename T>
EffectWindowList getMainWindows(Toplevel *toplevel)
{
    T *c = static_cast<T*>(toplevel);
    EffectWindowList ret;
    ClientList mainclients = c->mainClients();
    foreach (Client *tmp, mainclients) {
        ret.append(tmp->effectWindow());
    }
    return ret;
}

EffectWindowList EffectWindowImpl::mainWindows() const
{
    if (toplevel->isClient()) {
        return getMainWindows<Client>(toplevel);
    } else if (toplevel->isDeleted()) {
        return getMainWindows<Deleted>(toplevel);
    }
    return EffectWindowList();
}

WindowQuadList EffectWindowImpl::buildQuads(bool force) const
{
    return sceneWindow()->buildQuads(force);
}

void EffectWindowImpl::setData(int role, const QVariant &data)
{
    if (!data.isNull())
        dataMap[ role ] = data;
    else
        dataMap.remove(role);
}

QVariant EffectWindowImpl::data(int role) const
{
    if (!dataMap.contains(role))
        return QVariant();
    return dataMap[ role ];
}

EffectWindow* effectWindow(Toplevel* w)
{
    EffectWindowImpl* ret = w->effectWindow();
    return ret;
}

EffectWindow* effectWindow(Scene::Window* w)
{
    EffectWindowImpl* ret = w->window()->effectWindow();
    ret->setSceneWindow(w);
    return ret;
}

void EffectWindowImpl::elevate(bool elevate)
{
    effects->setElevatedWindow(this, elevate);
}

void EffectWindowImpl::referencePreviousWindowPixmap()
{
    if (sw) {
        sw->referencePreviousPixmap();
    }
}

void EffectWindowImpl::unreferencePreviousWindowPixmap()
{
    if (sw) {
        sw->unreferencePreviousPixmap();
    }
}

//****************************************
// EffectWindowGroupImpl
//****************************************


EffectWindowList EffectWindowGroupImpl::members() const
{
    EffectWindowList ret;
    foreach (Toplevel *client, group->members()) {
        ret.append(client->effectWindow());
    }
    return ret;
}

//****************************************
// EffectFrameImpl
//****************************************

EffectFrameImpl::EffectFrameImpl(bool staticSize, QPoint position, Qt::Alignment alignment)
    : QObject(0)
    , EffectFrame()
    , m_static(staticSize)
    , m_point(position)
    , m_alignment(alignment)
{
    if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_BUILD_COMPOSITE
        m_sceneFrame = new SceneXrender::EffectFrame(this);
#endif
    } else {
        // that should not happen and will definitely crash!
        m_sceneFrame = NULL;
    }
}

EffectFrameImpl::~EffectFrameImpl()
{
    delete m_sceneFrame;
}

const QFont& EffectFrameImpl::font() const
{
    return m_font;
}

void EffectFrameImpl::setFont(const QFont& font)
{
    if (m_font == font) {
        return;
    }
    m_font = font;
    QRect oldGeom = m_geometry;
    if (!m_text.isEmpty()) {
        autoResize();
    }
    if (oldGeom == m_geometry) {
        // Wasn't updated in autoResize()
        m_sceneFrame->freeTextFrame();
    }
}

void EffectFrameImpl::free()
{
    m_sceneFrame->free();
}

const QRect& EffectFrameImpl::geometry() const
{
    return m_geometry;
}

void EffectFrameImpl::setGeometry(const QRect& geometry, bool force)
{
    QRect oldGeom = m_geometry;
    m_geometry = geometry;
    if (m_geometry == oldGeom && !force) {
        return;
    }
    effects->addRepaint(oldGeom);
    effects->addRepaint(m_geometry);
    if (m_geometry.size() == oldGeom.size() && !force) {
        return;
    }

    free();
}

const QPixmap& EffectFrameImpl::icon() const
{
    return m_icon;
}

void EffectFrameImpl::setIcon(const QPixmap& icon)
{
    m_icon = icon;
    if (isCrossFade()) {
        m_sceneFrame->crossFadeIcon();
    }
    if (m_iconSize.isEmpty()) { // Set a size if we don't already have one
        setIconSize(m_icon.size());
    }
    m_sceneFrame->freeIconFrame();
}

const QSize& EffectFrameImpl::iconSize() const
{
    return m_iconSize;
}

void EffectFrameImpl::setIconSize(const QSize& size)
{
    if (m_iconSize == size) {
        return;
    }
    m_iconSize = size;
    autoResize();
    m_sceneFrame->freeIconFrame();
}

void EffectFrameImpl::render(QRegion region, double opacity, double frameOpacity)
{
    if (m_geometry.isEmpty()) {
        return; // Nothing to display
    }
    effects->paintEffectFrame(this, region, opacity, frameOpacity);
}

void EffectFrameImpl::finalRender(QRegion region, double opacity, double frameOpacity) const
{
    region = infiniteRegion();

    m_sceneFrame->render(region, opacity, frameOpacity);
}

Qt::Alignment EffectFrameImpl::alignment() const
{
    return m_alignment;
}


void
EffectFrameImpl::align(QRect &geometry)
{
    if (m_alignment & Qt::AlignLeft)
        geometry.moveLeft(m_point.x());
    else if (m_alignment & Qt::AlignRight)
        geometry.moveLeft(m_point.x() - geometry.width());
    else
        geometry.moveLeft(m_point.x() - geometry.width() / 2);
    if (m_alignment & Qt::AlignTop)
        geometry.moveTop(m_point.y());
    else if (m_alignment & Qt::AlignBottom)
        geometry.moveTop(m_point.y() - geometry.height());
    else
        geometry.moveTop(m_point.y() - geometry.height() / 2);
}


void EffectFrameImpl::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
    align(m_geometry);
    setGeometry(m_geometry);
}

void EffectFrameImpl::setPosition(const QPoint& point)
{
    m_point = point;
    QRect geometry = m_geometry; // this is important, setGeometry need call repaint for old & new geometry
    align(geometry);
    setGeometry(geometry);
}

const QString& EffectFrameImpl::text() const
{
    return m_text;
}

void EffectFrameImpl::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }
    if (isCrossFade()) {
        m_sceneFrame->crossFadeText();
    }
    m_text = text;
    QRect oldGeom = m_geometry;
    autoResize();
    if (oldGeom == m_geometry) {
        // Wasn't updated in autoResize()
        m_sceneFrame->freeTextFrame();
    }
}

void EffectFrameImpl::autoResize()
{
    if (m_static)
        return; // Not automatically resizing

    QRect geometry;
    // Set size
    if (!m_text.isEmpty()) {
        QFontMetrics metrics(m_font);
        geometry.setSize(metrics.size(0, m_text));
    }
    if (!m_icon.isNull() && !m_iconSize.isEmpty()) {
        geometry.setLeft(-m_iconSize.width());
        if (m_iconSize.height() > geometry.height())
            geometry.setHeight(m_iconSize.height());
    }

    align(geometry);
    setGeometry(geometry);
}

} // namespace
