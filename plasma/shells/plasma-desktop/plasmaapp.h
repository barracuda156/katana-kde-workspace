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

#include <QTimer>
#include <QWeakPointer>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QProcess>

#include <KUniqueApplication>
#include <Plasma/Plasma>
#include <plasma/packagemetadata.h>

#ifdef Q_WS_X11
// for Atom
#include <X11/Xlib.h>
#include <fixx11h.h>
#endif

#include "desktoptracker.h"
#include "kworkspace/kworkspace.h"
#include "kapplication_interface.h"

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

    void saveClients();
    void restoreClients();
    void registerClient(const QString &client);
    void unregisterClient(const QString &client);
    void suspendStartup(const QString &app);
    void resumeStartup(const QString &app);
    void logout(int confirm, int sdtype);
    void wmChanged();

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

    void cleanup();
    void nextPhase();
    void defaultLogout();
    void logoutWithoutConfirmation();
    void haltWithoutConfirmation();
    void rebootWithoutConfirmation();
    void doLogout();
private Q_SLOTS:
    void clientSaved();
    void clientSaveCanceled();

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
    QTimer* m_phaseTimer;
    int m_phase;
    QDBusPendingReply<void> m_klauncherReply;
    QDBusPendingReply<void> m_kdedReply;
    QDBusInterface* m_klauncher;
    QDBusInterface* m_kded;
    QProcess* m_wmProc;
    int m_startupSuspend;
    bool m_dialogActive;
    KWorkSpace::ShutdownType m_sdtype;
    bool m_sessionManager;
    int m_waitingCount;
    QMap<QString,org::kde::KApplication*> m_clients;
};

#endif // multiple inclusion guard
