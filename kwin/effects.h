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

#ifndef KWIN_EFFECTSIMPL_H
#define KWIN_EFFECTSIMPL_H

#include "kwineffects.h"
#include "scene.h"
#include "xcbutils.h"

#include <QStack>
#include <QHash>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

class KService;

namespace KWin
{

class Client;
class Compositor;
class Deleted;
class Unmanaged;

class EffectsHandlerImpl : public EffectsHandler
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Effects")
    Q_PROPERTY(QStringList activeEffects READ activeEffects)
    Q_PROPERTY(QStringList loadedEffects READ loadedEffects)
    Q_PROPERTY(QStringList listOfEffects READ listOfEffects)
public:
    EffectsHandlerImpl(Compositor *compositor, Scene *scene);
    virtual ~EffectsHandlerImpl();
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    /**
     * Special hook to perform a paintScreen but just with the windows on @p desktop.
     **/
    void paintDesktop(int desktop, int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void postPaintWindow(EffectWindow* w);
    virtual void paintEffectFrame(EffectFrame* frame, QRegion region, double opacity, double frameOpacity);

    Effect *provides(Effect::Feature ef);

    virtual void drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);

    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList);

    virtual void activateWindow(EffectWindow* c);
    virtual EffectWindow* activeWindow() const;
    virtual void moveWindow(EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0);
    virtual void windowToDesktop(EffectWindow* w, int desktop);
    virtual void windowToScreen(EffectWindow* w, int screen);
    virtual void setShowingDesktop(bool showing);

    virtual int currentDesktop() const;
    virtual int numberOfDesktops() const;
    virtual void setCurrentDesktop(int desktop);
    virtual void setNumberOfDesktops(int desktops);
    virtual QSize desktopGridSize() const;
    virtual int desktopGridWidth() const;
    virtual int desktopGridHeight() const;
    virtual int workspaceWidth() const;
    virtual int workspaceHeight() const;
    virtual int desktopAtCoords(QPoint coords) const;
    virtual QPoint desktopGridCoords(int id) const;
    virtual QPoint desktopCoords(int id) const;
    virtual int desktopAbove(int desktop = 0, bool wrap = true) const;
    virtual int desktopToRight(int desktop = 0, bool wrap = true) const;
    virtual int desktopBelow(int desktop = 0, bool wrap = true) const;
    virtual int desktopToLeft(int desktop = 0, bool wrap = true) const;
    virtual QString desktopName(int desktop) const;
    virtual bool optionRollOverDesktops() const;

    virtual int displayWidth() const;
    virtual int displayHeight() const;
    virtual QPoint cursorPos() const;
    virtual bool grabKeyboard(Effect* effect);
    virtual void ungrabKeyboard();
    // not performing XGrabPointer
    virtual void startMouseInterception(Effect *effect, Qt::CursorShape shape);
    virtual void stopMouseInterception(Effect *effect);
    virtual void* getProxy(QString name);
    virtual void startMousePolling();
    virtual void stopMousePolling();
    virtual EffectWindow* findWindow(WId id) const;
    virtual EffectWindowList stackingOrder() const;
    virtual void setElevatedWindow(EffectWindow* w, bool set);

    virtual void setActiveFullScreenEffect(Effect* e);
    virtual Effect* activeFullScreenEffect() const;

    virtual void addRepaintFull();
    virtual void addRepaint(const QRect& r);
    virtual void addRepaint(const QRegion& r);
    virtual void addRepaint(int x, int y, int w, int h);
    virtual int activeScreen() const;
    virtual int numScreens() const;
    virtual int screenNumber(const QPoint& pos) const;
    virtual QRect clientArea(clientAreaOption, int screen, int desktop) const;
    virtual QRect clientArea(clientAreaOption, const EffectWindow* c) const;
    virtual QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const;
    virtual double animationTimeFactor() const;
    virtual WindowQuadType newWindowQuadType();

    virtual void defineCursor(Qt::CursorShape shape);
    virtual bool checkInputWindowEvent(XEvent* e);
    virtual void checkInputWindowStacking();

    virtual void reserveElectricBorder(ElectricBorder border, Effect *effect);
    virtual void unreserveElectricBorder(ElectricBorder border, Effect *effect);

    virtual unsigned long xrenderBufferPicture();
    virtual void reconfigure();
    virtual void registerPropertyType(long atom, bool reg);
    virtual QByteArray readRootProperty(long atom, long type, int format) const;
    virtual void deleteRootProperty(long atom) const;
    virtual xcb_atom_t announceSupportProperty(const QByteArray& propertyName, Effect* effect);
    virtual void removeSupportProperty(const QByteArray& propertyName, Effect* effect);

    virtual bool hasDecorationShadows() const;

    virtual EffectFrame* effectFrame(bool staticSize, const QPoint& position, Qt::Alignment alignment) const;

    virtual QVariant kwinOption(KWinOption kwopt);

    // internal (used by kwin core or compositing code)
    void startPaint();
    void grabbedKeyboardEvent(QKeyEvent* e);
    bool hasKeyboardGrab() const;
    void desktopResized(const QSize &size);

    virtual void reloadEffect(Effect *effect);
    QStringList loadedEffects() const;
    QStringList listOfEffects() const;

    QList<EffectWindow*> elevatedWindows() const;
    QStringList activeEffects() const;

    /**
     * @returns Whether we are currently in a desktop rendering process triggered by paintDesktop hook
     **/
    bool isDesktopRendering() const {
        return m_desktopRendering;
    }
    /**
     * @returns the desktop currently being rendered in the paintDesktop hook.
     **/
    int currentRenderedDesktop() const {
        return m_currentRenderedDesktop;
    }

public Q_SLOTS:
    // slots for D-Bus interface
    Q_SCRIPTABLE void reconfigureEffect(const QString& name);
    Q_SCRIPTABLE bool loadEffect(const QString& name, bool checkDefault = false);
    Q_SCRIPTABLE void toggleEffect(const QString& name);
    Q_SCRIPTABLE void unloadEffect(const QString& name);
    Q_SCRIPTABLE bool isEffectLoaded(const QString& name) const;
    Q_SCRIPTABLE QString supportInformation(const QString& name) const;
    Q_SCRIPTABLE QString debug(const QString& name, const QString& parameter = QString()) const;

protected Q_SLOTS:
    void slotDesktopChanged(int old, KWin::Client *withClient);
    void slotDesktopPresenceChanged(KWin::Client *c, int old);
    void slotClientAdded(KWin::Client *c);
    void slotClientShown(KWin::Toplevel*);
    void slotUnmanagedAdded(KWin::Unmanaged *u);
    void slotUnmanagedShown(KWin::Toplevel*);
    void slotWindowClosed(KWin::Toplevel *c);
    void slotClientActivated(KWin::Client *c);
    void slotDeletedRemoved(KWin::Deleted *d);
    void slotClientMaximized(KWin::Client *c, KDecorationDefines::MaximizeMode maxMode);
    void slotClientStartUserMovedResized(KWin::Client *c);
    void slotClientStepUserMovedResized(KWin::Client *c, const QRect &geometry);
    void slotClientFinishUserMovedResized(KWin::Client *c);
    void slotOpacityChanged(KWin::Toplevel *t, qreal oldOpacity);
    void slotClientMinimized(KWin::Client *c, bool animate);
    void slotClientUnminimized(KWin::Client *c, bool animate);
    void slotClientModalityChanged();
    void slotGeometryShapeChanged(KWin::Toplevel *t, const QRect &old);
    void slotPaddingChanged(KWin::Toplevel *t, const QRect &old);
    void slotWindowDamaged(KWin::Toplevel *t, const QRect& r);
    void slotPropertyNotify(KWin::Toplevel *t, long atom);
    void slotPropertyNotify(long atom);

protected:
    void effectsChanged();
    void setupClientConnections(const KWin::Client *c);
    void setupUnmanagedConnections(const KWin::Unmanaged *u);

    Effect* keyboard_grab_effect;
    Effect* fullscreen_effect;
    QList<EffectWindow*> elevated_windows;
    QHash< long, int > registered_atoms;
    int next_window_quad_type;

private:
    typedef QVector< Effect*> EffectsList;
    typedef EffectsList::const_iterator EffectsIterator;
    EffectsList m_activeEffects;
    EffectsIterator m_currentDrawWindowIterator;
    EffectsIterator m_currentPaintWindowIterator;
    EffectsIterator m_currentPaintEffectFrameIterator;
    EffectsIterator m_currentPaintScreenIterator;
    EffectsIterator m_currentBuildQuadsIterator;
    typedef QHash< QByteArray, QList< Effect*> > PropertyEffectMap;
    PropertyEffectMap m_propertiesForEffects;
    QHash<QByteArray, qulonglong> m_managedProperties;
    Compositor *m_compositor;
    Scene *m_scene;
    bool m_desktopRendering;
    int m_currentRenderedDesktop;
    Xcb::Window m_mouseInterceptionWindow;
    QList<Effect*> m_grabbedMouseEffects;
};

class EffectWindowImpl : public EffectWindow
{
    Q_OBJECT
public:
    explicit EffectWindowImpl(Toplevel *toplevel);
    virtual ~EffectWindowImpl();

    virtual void enablePainting(int reason);
    virtual void disablePainting(int reason);
    virtual bool isPaintingEnabled();

    virtual void refWindow();
    virtual void unrefWindow();

    virtual const EffectWindowGroup* group() const;

    virtual QRegion shape() const;
    virtual QRect decorationInnerRect() const;
    virtual QByteArray readProperty(long atom, long type, int format) const;
    virtual void deleteProperty(long atom) const;

    virtual EffectWindow* findModal();
    virtual EffectWindowList mainWindows() const;

    virtual WindowQuadList buildQuads(bool force = false) const;

    virtual void referencePreviousWindowPixmap();
    virtual void unreferencePreviousWindowPixmap();

    const Toplevel* window() const;
    Toplevel* window();

    void setWindow(Toplevel* w);   // internal
    void setSceneWindow(Scene::Window* w);   // internal
    const Scene::Window* sceneWindow() const; // internal
    Scene::Window* sceneWindow(); // internal

    void elevate(bool elevate);

    void setData(int role, const QVariant &data);
    QVariant data(int role) const;

private:
    Toplevel* toplevel;
    Scene::Window* sw; // This one is used only during paint pass.
    QHash<int, QVariant> dataMap;
};

class EffectWindowGroupImpl
    : public EffectWindowGroup
{
public:
    explicit EffectWindowGroupImpl(Group* g);
    virtual EffectWindowList members() const;
private:
    Group* group;
};

class EffectFrameImpl
    : public QObject, public EffectFrame
{
    Q_OBJECT
public:
    explicit EffectFrameImpl(bool staticSize = true, QPoint position = QPoint(-1, -1),
                             Qt::Alignment alignment = Qt::AlignCenter);
    virtual ~EffectFrameImpl();

    virtual void free();
    virtual void render(QRegion region = infiniteRegion(), double opacity = 1.0, double frameOpacity = 1.0);
    virtual Qt::Alignment alignment() const;
    virtual void setAlignment(Qt::Alignment alignment);
    virtual const QFont& font() const;
    virtual void setFont(const QFont& font);
    virtual const QRect& geometry() const;
    virtual void setGeometry(const QRect& geometry, bool force = false);
    virtual const QPixmap& icon() const;
    virtual void setIcon(const QPixmap& icon);
    virtual const QSize& iconSize() const;
    virtual void setIconSize(const QSize& size);
    virtual void setPosition(const QPoint& point);
    virtual const QString& text() const;
    virtual void setText(const QString& text);
    bool isStatic() const {
        return m_static;
    };
    void finalRender(QRegion region, double opacity, double frameOpacity) const;
private:
    Q_DISABLE_COPY(EffectFrameImpl)   // As we need to use Qt slots we cannot copy this class
    void align(QRect &geometry);   // positions geometry around m_point respecting m_alignment
    void autoResize(); // Auto-resize if not a static size

    // Position
    bool m_static;
    QPoint m_point;
    Qt::Alignment m_alignment;
    QRect m_geometry;

    // Contents
    QString m_text;
    QFont m_font;
    QPixmap m_icon;
    QSize m_iconSize;

    Scene::EffectFrame* m_sceneFrame;
};

inline
QList<EffectWindow*> EffectsHandlerImpl::elevatedWindows() const
{
    return elevated_windows;
}


inline
EffectWindowGroupImpl::EffectWindowGroupImpl(Group* g)
    : group(g)
{
}

EffectWindow* effectWindow(Toplevel* w);
EffectWindow* effectWindow(Scene::Window* w);

inline
const Scene::Window* EffectWindowImpl::sceneWindow() const
{
    return sw;
}

inline
Scene::Window* EffectWindowImpl::sceneWindow()
{
    return sw;
}

inline
const Toplevel* EffectWindowImpl::window() const
{
    return toplevel;
}

inline
Toplevel* EffectWindowImpl::window()
{
    return toplevel;
}


} // namespace

#endif
