/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_PRESENTWINDOWS_H
#define KWIN_PRESENTWINDOWS_H

#include "presentwindows_proxy.h"

#include <kwineffects.h>
#include <QKeySequence>
#include <QTimer>
#include <QGraphicsView>

namespace KWin
{

/**
 * Expose-like effect which shows all windows on current desktop side-by-side,
 *  letting the user select active window.
 **/
class PresentWindowsEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int layoutMode READ layoutMode)
    Q_PROPERTY(bool showCaptions READ isShowCaptions)
    Q_PROPERTY(bool showIcons READ isShowIcons)
    Q_PROPERTY(bool ignoreMinimized READ isIgnoreMinimized)
    Q_PROPERTY(int accuracy READ accuracy)
    Q_PROPERTY(bool fillGaps READ isFillGaps)
    Q_PROPERTY(int fadeDuration READ fadeDuration)
    Q_PROPERTY(bool showPanel READ isShowPanel)
    Q_PROPERTY(int leftButtonWindow READ leftButtonWindow)
    Q_PROPERTY(int rightButtonWindow READ rightButtonWindow)
    Q_PROPERTY(int middleButtonWindow READ middleButtonWindow)
    Q_PROPERTY(int leftButtonDesktop READ leftButtonDesktop)
    Q_PROPERTY(int middleButtonDesktop READ middleButtonDesktop)
    Q_PROPERTY(int rightButtonDesktop READ rightButtonDesktop)
    Q_PROPERTY(bool dragToClose READ isDragToClose)
    // TODO: electric borders
private:
    // Structures
    struct WindowData {
        bool visible;
        bool deleted;
        bool referenced;
        double opacity;
        double highlight;
        EffectFrame* textFrame;
        EffectFrame* iconFrame;
    };
    typedef QHash<EffectWindow*, WindowData> DataHash;
    struct GridSize {
        int columns;
        int rows;
    };

public:
    PresentWindowsEffect();
    virtual ~PresentWindowsEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void* proxy();

    // Screen painting
    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData &data);
    virtual void postPaintScreen();

    // Window painting
    virtual void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time);
    virtual void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data);

    // User interaction
    virtual bool borderActivated(ElectricBorder border);
    virtual void windowInputMouseEvent(QEvent *e);
    virtual void grabbedKeyboardEvent(QKeyEvent *e);
    virtual bool isActive() const;

    enum { LayoutNatural, LayoutRegularGrid, LayoutFlexibleGrid }; // Layout modes
    enum PresentWindowsMode {
        ModeAllDesktops, // Shows windows of all desktops
        ModeCurrentDesktop, // Shows windows on current desktop
        ModeSelectedDesktop, // Shows windows of selected desktop via property (m_desktop)
        ModeWindowGroup, // Shows windows selected via property
        ModeWindowClass // Shows all windows of same class as selected class
    };
    enum WindowMouseAction {
        WindowNoAction = 0, // Nothing
        WindowActivateAction = 1, // Activates the window and deactivates the effect
        WindowExitAction = 2, // Deactivates the effect without activating new window
        WindowToCurrentDesktopAction = 3, // Brings window to current desktop
        WindowToAllDesktopsAction = 4, // Brings window to all desktops
        WindowMinimizeAction = 5 // Minimize the window
    };
    enum DesktopMouseAction {
        DesktopNoAction = 0, // nothing
        DesktopActivateAction = 1, // Activates the window and deactivates the effect
        DesktopExitAction = 2, // Deactivates the effect without activating new window
        DesktopShowDesktopAction = 3 // Minimizes all windows
    };

    // for properties
    int layoutMode() const {
        return m_layoutMode;
    }
    bool isShowCaptions() const {
        return m_showCaptions;
    }
    bool isShowIcons() const {
        return m_showIcons;
    }
    bool isIgnoreMinimized() const {
        return m_ignoreMinimized;
    }
    int accuracy() const {
        return m_accuracy;
    }
    bool isFillGaps() const {
        return m_fillGaps;
    }
    int fadeDuration() const {
        return m_fadeDuration;
    }
    bool isShowPanel() const {
        return m_showPanel;
    }
    int leftButtonWindow() const {
        return m_leftButtonWindow;
    }
    int rightButtonWindow() const {
        return m_rightButtonWindow;
    }
    int middleButtonWindow() const {
        return m_middleButtonWindow;
    }
    int leftButtonDesktop() const {
        return m_leftButtonDesktop;
    }
    int middleButtonDesktop() const {
        return m_middleButtonDesktop;
    }
    int rightButtonDesktop() const {
        return m_rightButtonDesktop;
    }
    bool isDragToClose() const {
        return m_dragToClose;
    }
public slots:
    void setActive(bool active);
    void toggleActive()  {
        m_mode = ModeCurrentDesktop;
        setActive(!m_activated);
    }
    void toggleActiveAllDesktops()  {
        m_mode = ModeAllDesktops;
        setActive(!m_activated);
    }
    void toggleActiveClass();

    // slots for global shortcut changed
    // needed to toggle the effect
    void globalShortcutChanged(const QKeySequence& seq);
    void globalShortcutChangedAll(const QKeySequence& seq);
    void globalShortcutChangedClass(const QKeySequence& seq);
    // EffectsHandler
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowGeometryShapeChanged(KWin::EffectWindow *w, const QRect &old);
    // atoms
    void slotPropertyNotify(KWin::EffectWindow* w, long atom);

private slots:
    void screenCountChanged();

protected:
    // Window rearranging
    void rearrangeWindows();
    void calculateWindowTransformations(EffectWindowList windowlist, int screen,
                                        WindowMotionManager& motionManager, bool external = false);
    void calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);
    void calculateWindowTransformationsKompose(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);
    void calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);

    // Helper functions for window rearranging
    inline double aspectRatio(EffectWindow *w) {
        return w->width() / double(w->height());
    }
    inline int widthForHeight(EffectWindow *w, int height) {
        return int((height / double(w->height())) * w->width());
    }
    inline int heightForWidth(EffectWindow *w, int width) {
        return int((width / double(w->width())) * w->height());
    }
    bool isOverlappingAny(EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border);

    // Filter box
    void updateFilterFrame();

    // Helper functions
    bool isSelectableWindow(EffectWindow *w);
    bool isVisibleWindow(EffectWindow *w);
    void setHighlightedWindow(EffectWindow *w);
    EffectWindow* relativeWindow(EffectWindow *w, int xdiff, int ydiff, bool wrap) const;
    EffectWindow* findFirstWindow() const;

    // Helper functions for mouse actions
    void mouseActionWindow(WindowMouseAction& action);
    void mouseActionDesktop(DesktopMouseAction& action);

private:
    PresentWindowsEffectProxy m_proxy;
    friend class PresentWindowsEffectProxy;

    // User configuration settings
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_borderActivateAll;
    QList<ElectricBorder> m_borderActivateClass;
    int m_layoutMode;
    bool m_showCaptions;
    bool m_showIcons;
    int m_accuracy;
    bool m_fillGaps;
    double m_fadeDuration;
    bool m_showPanel;

    // Activation
    bool m_activated;
    bool m_ignoreMinimized;
    double m_decalOpacity;
    bool m_hasKeyboardGrab;
    PresentWindowsMode m_mode;
    int m_desktop;
    EffectWindowList m_selectedWindows;
    EffectWindow *m_managerWindow;
    QString m_class;
    bool m_needInitialSelection;

    // Window data
    WindowMotionManager m_motionManager;
    DataHash m_windowData;
    EffectWindow *m_highlightedWindow;

    // Grid layout info
    QList<GridSize> m_gridSizes;

    // Filter box
    EffectFrame* m_filterFrame;
    QString m_windowFilter;

    // Shortcut - needed to toggle the effect
    QKeySequence shortcut;
    QKeySequence shortcutAll;
    QKeySequence shortcutClass;

    // Atoms
    // Present windows for all windows of given desktop
    // -1 for all desktops
    long m_atomDesktop;
    // Present windows for group of window ids
    long m_atomWindows;

    // Mouse Actions
    WindowMouseAction m_leftButtonWindow;
    WindowMouseAction m_middleButtonWindow;
    WindowMouseAction m_rightButtonWindow;
    DesktopMouseAction m_leftButtonDesktop;
    DesktopMouseAction m_middleButtonDesktop;
    DesktopMouseAction m_rightButtonDesktop;

    // drag to close
    QPoint m_dragStart;
    bool m_dragInProgress;
    EffectWindow *m_dragWindow;
    QList<EffectFrame*> m_dropTargets;
    EffectFrame *m_highlightedDropTarget;
    bool m_dragToClose;
};

} // namespace

#endif
