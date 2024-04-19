/*
 *   Copyright 2006, 2007 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2010 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2,
 *   or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef PLASMA_APP_H
#define PLASMA_APP_H

#include <QtCore/qtimer.h>
#include <QtCore/qsharedpointer.h>

#include <KUniqueApplication>
#include <Plasma/Plasma>
#include <plasma/packagemetadata.h>

#ifdef Q_WS_X11
// for Atom
#include <X11/Xlib.h>
#include <fixx11h.h>
#endif

#include "desktoptracker.h"

namespace Plasma
{
    class Containment;
    class Corona;
    class Dialog;
} // namespace Plasma

class ControllerWindow;
class DesktopView;
class DesktopCorona;
class PanelShadows;
class PanelView;

class PlasmaApp : public KUniqueApplication
{
    Q_OBJECT
public:
    ~PlasmaApp();

    static PlasmaApp *self();

    DesktopCorona *corona(bool createIfMissing = true);

    /**
     * Should be called when a panel hides or unhides itself
     */
    void panelHidden(bool hidden);

    /**
     * Returns the PanelViews
     */
    QList<PanelView*> panelViews() const;
    PanelShadows *panelShadows() const;

    ControllerWindow *showWidgetExplorer(int screen, Plasma::Containment *c);
    void hideController(int screen);

    void prepareContainment(Plasma::Containment *containment);

    static bool isPanelContainment(Plasma::Containment *containment);

#ifdef Q_WS_X11
    Atom m_XdndAwareAtom;
    Atom m_XdndEnterAtom;
    Atom m_XdndFinishedAtom;
    Atom m_XdndPositionAtom;
    Atom m_XdndStatusAtom;
    Atom m_XdndVersionAtom;
#endif

public Q_SLOTS:
    // DBUS interface. if you change these methods, you MUST run:
    // qdbuscpp2xml plasmaapp.h -o dbus/org.kde.plasma.App.xml
    void createWaitingPanels();
    void createWaitingDesktops();
    void createView(Plasma::Containment *containment);

    QString supportInformation() const;

protected:
#ifdef Q_WS_X11
    PanelView *findPanelForTrigger(WId trigger) const;
    bool x11EventFilter(XEvent *event);
#endif
    void setControllerVisible(bool show);

private:
    PlasmaApp();
    DesktopView* viewForScreen(int screen, int desktop) const;
    ControllerWindow *showController(int screen, Plasma::Containment *c, bool widgetExplorerMode);
    bool canRelocatePanel(PanelView * view, const DesktopTracker::Screen &screen);
    PanelView *createPanelView(Plasma::Containment *containment);

private Q_SLOTS:
    void setupDesktop();
    void containmentAdded(Plasma::Containment *containment);
    void containmentScreenOwnerChanged(int, int, Plasma::Containment*);
    void syncConfig();
    void panelRemoved(QObject* panel);
    void screenRemoved(const DesktopTracker::Screen &screen);
    void screenAdded(const DesktopTracker::Screen &screen);
    void configureContainment(Plasma::Containment*);
    void setWmClass(WId id);
    void relocatePanels();
    void captureDesktop();
    void captureCurrentWindow();

private:
    DesktopCorona *m_corona;
    PanelShadows *m_panelShadows;

    QList<PanelView*> m_panels;
    QList<QWeakPointer<Plasma::Containment> > m_panelsWaiting;
    QList<QWeakPointer<Plasma::Containment> > m_panelRelocationCandidates;

    QList<DesktopView*> m_desktops;
    QList<QWeakPointer<Plasma::Containment> > m_desktopsWaiting;

    QTimer m_panelViewCreationTimer;
    QTimer m_desktopViewCreationTimer;
    int m_panelHidden;
    QHash<int, QWeakPointer<ControllerWindow> > m_widgetExplorers;
};

#endif // multiple inclusion guard
