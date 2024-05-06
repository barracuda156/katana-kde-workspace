/*
 *   Copyright 2006 Aaron Seigo <aseigo@kde.org>
 *   Copyright 2010 Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
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

#include "plasmaapp.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QtDBus/QtDBus>
#include <QElapsedTimer>
#include <QTimer>
#include <QVBoxLayout>
#include <QTextStream>

#include <KAction>
#include <KCrash>
#include <KDebug>
#include <KCmdLineArgs>
#include <KGlobalAccel>
#include <KGlobalSettings>
#include <KNotification>
#include <KWindowSystem>
#include <KService>
#include <KIconLoader>
#include <KStandardDirs>
#include <KActionCollection>
#include <KToolInvocation>
#include <KConfigSkeleton>
#include <KDesktopFile>
#include <KShell>

#include <Plasma/AbstractToolBox>
#include <Plasma/Containment>
#include <Plasma/Dialog>
#include <Plasma/Wallpaper>
#include <Plasma/WindowEffects>
#include <Plasma/Package>

#include <plasmagenericshell/backgrounddialog.h>

#include "appadaptor.h"
#include "controllerwindow.h"
#include "desktopcorona.h"
#include "desktopview.h"
#include "panelshadows.h"
#include "panelview.h"
#include "toolbutton.h"
#include "shutdowndlg.h"
#include "kworkspace/kdisplaymanager.h"

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

static const QString s_defaultwm = QString::fromLatin1("kwin");
static const QStringList s_defaultwmcommands = QStringList() << s_defaultwm;

static void addInformationForApplet(QTextStream &stream, Plasma::Applet *applet)
{
    if (applet->isContainment()) {
        stream << "Containment - ";
    } else {
        stream << "Applet - ";
    }
    stream << applet->name() << ":\n";

    stream << "Plugin Name: " << applet->pluginName() << '\n';
    stream << "Category: " << applet->category() << '\n';

    // runtime info
    stream << "Failed To Launch: " << applet->hasFailedToLaunch() << '\n';
    const QRect rect = applet->screenRect();
    stream << "ScreenRect: " << rect.x() << ',' << rect.y() << ' ' << rect.width() << 'x' << rect.height() << '\n';
    stream << "FormFactor: " << applet->formFactor() << '\n';

    stream << "Config Group Name: " << applet->config().name() << '\n';

    stream << '\n'; // insert a blank line
}

PlasmaApp* PlasmaApp::self()
{
    if (!kapp) {
        return new PlasmaApp();
    }

    return qobject_cast<PlasmaApp*>(kapp);
}

PlasmaApp::PlasmaApp()
    : KUniqueApplication(),
    m_corona(nullptr),
    m_panelHidden(0),
    m_phase(0),
    m_klauncher(nullptr),
    m_kcminit(nullptr),
    m_wmproc(nullptr),
    m_startupsuspend(0),
    m_dialogActive(false),
    m_sdtype(KWorkSpace::ShutdownTypeNone)
{
    kDebug() << "!!{} STARTUP TIME" << QTime().msecsTo(QTime::currentTime()) << "plasma app ctor start" << "(line:" << __LINE__ << ")";

    KGlobal::locale()->insertCatalog("libplasma");
    KGlobal::locale()->insertCatalog("plasmagenericshell");
    KCrash::setFlags(KCrash::AutoRestart | KCrash::Log);

    m_panelViewCreationTimer.setSingleShot(true);
    m_panelViewCreationTimer.setInterval(0);

    m_desktopViewCreationTimer.setSingleShot(true);
    m_desktopViewCreationTimer.setInterval(0);

    new PlasmaAppAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/App", this);

    KGlobal::setAllowQuit(true);
    KGlobal::ref();

    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* action = actionCollection->addAction("Capture the desktop");
    action->setText(i18n("Capture the desktop"));
    action->setGlobalShortcut(QKeySequence(Qt::Key_Print));
    connect(action, SIGNAL(triggered(bool)), SLOT(captureDesktop()));

    action = actionCollection->addAction("Capture the current window");
    action->setText(i18n("Capture the current window"));
    action->setGlobalShortcut(QKeySequence(Qt::CTRL+Qt::Key_Print));
    connect(action, SIGNAL(triggered(bool)), SLOT(captureCurrentWindow()));

    action = actionCollection->addAction("Log Out");
    action->setText(i18n("Log Out"));
    action->setGlobalShortcut(QKeySequence(Qt::ALT+Qt::CTRL+Qt::Key_Delete));
    connect(action, SIGNAL(triggered(bool)), SLOT(defaultLogout()));

    action = actionCollection->addAction("Log Out Without Confirmation");
    action->setText(i18n("Log Out Without Confirmation"));
    action->setGlobalShortcut(QKeySequence(Qt::ALT+Qt::CTRL+Qt::SHIFT+Qt::Key_Delete));
    connect(action, SIGNAL(triggered(bool)), SLOT(logoutWithoutConfirmation()));

    action = actionCollection->addAction("Halt Without Confirmation");
    action->setText(i18n("Halt Without Confirmation"));
    action->setGlobalShortcut(QKeySequence(Qt::ALT+Qt::CTRL+Qt::SHIFT+Qt::Key_PageDown));
    connect(action, SIGNAL(triggered(bool)), SLOT(haltWithoutConfirmation()));

    action = actionCollection->addAction("Reboot Without Confirmation");
    action->setText(i18n("Reboot Without Confirmation"));
    action->setGlobalShortcut(QKeySequence(Qt::ALT+Qt::CTRL+Qt::SHIFT+Qt::Key_PageUp));
    connect(action, SIGNAL(triggered(bool)), SLOT(rebootWithoutConfirmation()));

    QTimer::singleShot(0, this, SLOT(setupDesktop()));
    kDebug() << "!!{} STARTUP TIME" << QTime().msecsTo(QTime::currentTime()) << "plasma app ctor end" << "(line:" << __LINE__ << ")";

    m_klauncher = new QDBusInterface(
        QLatin1String("org.kde.klauncher"),
        QLatin1String("/KLauncher"),
        QLatin1String("org.kde.KLauncher"),
        QDBusConnection::sessionBus(),
        this
    );
    // TODO: m_klauncher->call("setLaunchEnv", "SESSION_MANAGER", "TODO");

    
    m_kcminit = new QDBusInterface(
        QLatin1String("org.kde.kcminit"),
        QLatin1String("/kcminit"),
        QLatin1String("org.kde.KCMInit"),
        QDBusConnection::sessionBus(),
        this
    );

    KGlobal::dirs()->addResourceType("windowmanagers", "data", "plasma/windowmanagers");

    QStringList wmcommands;
    if (qgetenv("KDE_FAILSAFE").toInt() != 1) {
        KConfig cfg("plasmarc", KConfig::NoGlobals);
        KConfigGroup config(&cfg, "General");
        const QString wmname = config.readEntry("windowManager", s_defaultwm);
        if (wmname != s_defaultwm) {
            KDesktopFile wmfile("windowmanagers", wmname + ".desktop");
            if (!wmfile.noDisplay() && wmfile.tryExec()) {
                wmcommands = KShell::splitArgs(wmfile.desktopGroup().readEntry("Exec"));
            }
        }
    }
    if (wmcommands.isEmpty()) {
        wmcommands = s_defaultwmcommands;
    }
    const QString wmprog = wmcommands.takeFirst();
    m_wmproc = new QProcess(this);
    m_wmproc->start(wmprog, wmcommands);
    m_wmproc->waitForStarted(4000);

    QTimer::singleShot(100, this, SLOT(nextPhase()));
}

PlasmaApp::~PlasmaApp()
{
    cleanup();

    if (m_corona) {
        m_corona->saveLayout(KStandardDirs::locateLocal("config", "plasma-desktoprc"));

        // save the mapping of Views to Containments at the moment
        // of application exit so we can restore that when we start again.
        KConfigGroup viewIds(KGlobal::config(), "ViewIds");
        viewIds.deleteGroup();

        foreach (PanelView *v, m_panels) {
            if (v->containment()) {
                viewIds.writeEntry(QString::number(v->containment()->id()), v->id());
            }
        }

        foreach (DesktopView *v, m_desktops) {
            if (v->containment()) {
                viewIds.writeEntry(QString::number(v->containment()->id()), v->id());
            }
        }

        QList<DesktopView*> desktops = m_desktops;
        m_desktops.clear();
        qDeleteAll(desktops);

        QList<PanelView*> panels = m_panels;
        m_panels.clear();
        qDeleteAll(panels);

        delete m_corona;
        m_corona = nullptr;

        delete m_panelShadows;
        m_panelShadows = nullptr;

        //TODO: This manual sync() should not be necessary. Remove it when
        // KConfig was fixed
        KGlobal::config()->sync();
    }
    KGlobal::deref();
}

void PlasmaApp::setupDesktop()
{
#ifdef Q_WS_X11
    Atom atoms[5];
    const char * const atomNames[] = {"XdndAware", "XdndEnter", "XdndFinished", "XdndPosition", "XdndStatus"};
    XInternAtoms(QX11Info::display(), const_cast<char **>(atomNames), 5, False, atoms);
    m_XdndAwareAtom = atoms[0];
    m_XdndEnterAtom = atoms[1];
    m_XdndFinishedAtom = atoms[2];
    m_XdndPositionAtom = atoms[3];
    m_XdndStatusAtom = atoms[4];
    const int xdndversion = 5;
    m_XdndVersionAtom = (Atom)xdndversion;
#endif

    m_panelShadows = new PanelShadows();

    // this line initializes the corona.
    corona();

    DesktopTracker *tracker = DesktopTracker::self();
    connect(tracker, SIGNAL(screenRemoved(DesktopTracker::Screen)), SLOT(screenRemoved(DesktopTracker::Screen)));
    connect(tracker, SIGNAL(screenAdded(DesktopTracker::Screen)), SLOT(screenAdded(DesktopTracker::Screen)));

    // free the memory possibly occupied by the background image of the
    // root window - login managers will typically set one
    QPalette palette;
    palette.setColor(desktop()->backgroundRole(), Qt::black);
    desktop()->setPalette(palette);

    kDebug() << "!!{} STARTUP TIME" << QTime().msecsTo(QTime::currentTime()) << "Plasma App SetupDesktop()" << "(line:" << __LINE__ << ")";

    // now connect up the creation timers and start them to get the views created
    connect(&m_panelViewCreationTimer, SIGNAL(timeout()), this, SLOT(createWaitingPanels()));
    connect(&m_desktopViewCreationTimer, SIGNAL(timeout()), this, SLOT(createWaitingDesktops()));
    m_panelViewCreationTimer.start();
    m_desktopViewCreationTimer.start();
}

void PlasmaApp::syncConfig()
{
    KGlobal::config()->sync();
}

void PlasmaApp::panelHidden(bool hidden)
{
    if (hidden) {
        ++m_panelHidden;
        //kDebug() << "panel hidden" << m_panelHidden;
    } else {
        --m_panelHidden;
        if (m_panelHidden < 0) {
            kDebug() << "panelHidden(false) called too many times!";
            m_panelHidden = 0;
        }
        //kDebug() << "panel unhidden" << m_panelHidden;
    }
}

QList<PanelView*> PlasmaApp::panelViews() const
{
    return m_panels;
}

PanelShadows *PlasmaApp::panelShadows() const
{
    return m_panelShadows;
}

ControllerWindow *PlasmaApp::showWidgetExplorer(int screen, Plasma::Containment *containment)
{
    return showController(screen, containment, true);
}

ControllerWindow *PlasmaApp::showController(int screen, Plasma::Containment *containment, bool widgetExplorerMode)
{
    if (!containment) {
        kDebug() << "no containment";
        return 0;
    }

    QWeakPointer<ControllerWindow> controllerPtr = m_widgetExplorers.value(screen);
    ControllerWindow *controller = controllerPtr.data();

    if (!controller) {
        //kDebug() << "controller not found for screen" << screen;
        controllerPtr = controller = new ControllerWindow(0);
        m_widgetExplorers.insert(screen, controllerPtr);
    }

    controller->setContainment(containment);
    if (!containment || containment->screen() != screen) {
        controller->setScreen(screen);
    }

    controller->setLocation(containment->location());

    if (widgetExplorerMode) {
        controller->showWidgetExplorer();
    }

    controller->show();
    Plasma::WindowEffects::slideWindow(controller, controller->location());
    QTimer::singleShot(0, controller, SLOT(activate()));
    return controller;
}

void PlasmaApp::hideController(int screen)
{
    QWeakPointer<ControllerWindow> controller = m_widgetExplorers.value(screen);
    if (controller) {
        controller.data()->hide();
    }
}

#ifdef Q_WS_X11
PanelView *PlasmaApp::findPanelForTrigger(WId trigger) const
{
    foreach (PanelView *panel, m_panels) {
        if (panel->unhideTrigger() == trigger) {
            return panel;
        }
    }

    return 0;
}

bool PlasmaApp::x11EventFilter(XEvent *event)
{
    if (m_panelHidden > 0 &&
        (event->type == ClientMessage ||
         (event->xany.send_event != True && (event->type == EnterNotify ||
                                             event->type == MotionNotify)))) {

        /*
        if (event->type == ClientMessage) {
            kDebug() << "client message with" << event->xclient.message_type << m_XdndEnterAtom << event->xcrossing.window;
        }
        */

        bool dndEnter = false;
        bool dndPosition = false;
        if (event->type == ClientMessage) {
            dndEnter = event->xclient.message_type == m_XdndEnterAtom;
            if (!dndEnter) {
                dndPosition = event->xclient.message_type == m_XdndPositionAtom;
                if (!dndPosition) {
                    //kDebug() << "FAIL!";
                    return KUniqueApplication::x11EventFilter(event);
                }
            } else {
                //kDebug() << "on enter" << event->xclient.data.l[0];
            }
        }

        PanelView *panel = findPanelForTrigger(event->xcrossing.window);
        //kDebug() << "panel?" << panel << ((dndEnter || dndPosition) ? "Drag and drop op" : "Mouse move op");
        if (panel) {
            if (dndEnter || dndPosition) {
                QPoint p;

                const unsigned long *l = (const unsigned long *)event->xclient.data.l;
                if (dndPosition) {
                    p = QPoint((l[2] & 0xffff0000) >> 16, l[2] & 0x0000ffff);
                } else {
                    p = QCursor::pos();
                }

                XClientMessageEvent response;
                response.type = ClientMessage;
                response.window = l[0];
                response.format = 32;
                response.data.l[0] = panel->winId(); //event->xcrossing.window;

                if (panel->hintOrUnhide(p, true)) {
                    response.message_type = m_XdndFinishedAtom;
                    response.data.l[1] = 0; // flags
                    response.data.l[2] = XNone;
                } else {
                    response.message_type = m_XdndStatusAtom;
                    response.data.l[1] = 0; // flags
                    response.data.l[2] = 0; // x, y
                    response.data.l[3] = 0; // w, h
                    response.data.l[4] = 0; // action
                }

                XSendEvent(QX11Info::display(), l[0], False, NoEventMask, (XEvent*)&response);
            } else if (event->type == EnterNotify) {
                panel->hintOrUnhide(QPoint(-1, -1));
                //kDebug() << "entry";
            //FIXME: this if it was possible to avoid the polling
            /*} else if (event->type == LeaveNotify) {
                panel->unhintHide();
            */
            } else if (event->type == MotionNotify) {
                XMotionEvent *motion = (XMotionEvent*)event;
                //kDebug() << "motion" << motion->x << motion->y << panel->location();
                panel->hintOrUnhide(QPoint(motion->x_root, motion->y_root));
            }
        }
    }

    return KUniqueApplication::x11EventFilter(event);
}
#endif

void PlasmaApp::screenRemoved(const DesktopTracker::Screen &screen)
{
    kDebug() << "@@@@" << screen.id;
    QMutableListIterator<DesktopView *> it(m_desktops);
    while (it.hasNext()) {
        DesktopView *view = it.next();
        if (view->screen() == screen.id) {
            // the screen was removed, so we'll destroy the
            // corresponding view
            kDebug() << "@@@@removing the view for screen" << screen.id;
            view->setContainment(0);
            it.remove();
            delete view;
        }
    }

#if 1
    /**
    UPDATE: this was linked to kephal events, which are not optimal, but it seems it may well
    have been the panel->migrateTo call due to a bug in libplasma fixed in e2108ed. so let's try
    and re-enable this.
    NOTE: CURRENTLY UNSAFE DUE TO HOW KEPHAL (or rather, it seems, Qt?) PROCESSES EVENTS
          DURING XRANDR EVENTS. REVISIT IN 4.8!
          */
    const DesktopTracker::Screen primary = DesktopTracker::self()->primaryScreen();
    QList<DesktopTracker::Screen> screens = DesktopTracker::self()->screens();
    screens.removeAll(primary);

    // Now we process panels: if there is room on another screen for the panel,
    // we migrate the panel there, otherwise the view is deleted. The primary
    // screen is preferred in all cases.
    QMutableListIterator<PanelView*> pIt(m_panels);
    while (pIt.hasNext()) {
        PanelView *panel = pIt.next();
        if (panel->screen() == screen.id) {
            DesktopTracker::Screen moveTo;
            if (canRelocatePanel(panel, primary)) {
                moveTo = primary;
            } else {
                foreach (const DesktopTracker::Screen &screen, screens) {
                    if (canRelocatePanel(panel, screen)) {
                        moveTo = screen;
                        break;
                    }
                }
            }

            if (moveTo.id >= 0) {
                panel->migrateTo(moveTo.id);
            } else {
                pIt.remove();
                delete panel;
                continue;
            }
        }

        panel->updateStruts();
    }
#else
    QMutableListIterator<PanelView*> pIt(m_panels);
    while (pIt.hasNext()) {
        PanelView *panel = pIt.next();
        if (panel->screen() == screen.id) {
            pIt.remove();
            delete panel;
        }
    }
#endif
}

void PlasmaApp::screenAdded(const DesktopTracker::Screen &screen)
{
    foreach (Plasma::Containment *containment, corona()->containments()) {
        if (isPanelContainment(containment) && containment->screen() == screen.id) {
            m_panelsWaiting << containment;
            m_panelViewCreationTimer.start();
        }
    }

    foreach (PanelView *view, m_panels) {
        if (view->migratedFrom(screen.id)) {
            view->migrateTo(screen.id);
        }
    }
}

bool PlasmaApp::canRelocatePanel(PanelView * view, const DesktopTracker::Screen &screen)
{
    if (!view->containment()) {
        return false;
    }

    QRect newGeom = view->geometry();
    switch (view->location()) {
        case Plasma::TopEdge:
            newGeom.setY(screen.geom.y());
            newGeom.setX(view->offset());
            break;
        case Plasma::BottomEdge:
            newGeom.setY(screen.geom.bottom() - newGeom.height());
            newGeom.setX(view->offset());
            break;
        case Plasma::LeftEdge:
            newGeom.setX(screen.geom.left());
            newGeom.setY(view->offset());
            break;
        case Plasma::RightEdge:
            newGeom.setX(screen.geom.right() - newGeom.width());
            newGeom.setY(view->offset());
            break;
        default:
            break;
    }

    kDebug() << "testing:" << screen.id << view << view->geometry() << view->location() << newGeom;
    foreach (PanelView *pv, m_panels) {
        kDebug() << pv << pv->screen() << pv->screen() << pv->location() << pv->geometry();
        if (pv != view &&
            pv->screen() == screen.id &&
            pv->location() == view->location() &&
            pv->geometry().intersects(newGeom)) {
            return false;
        }
    }

    return true;
}


DesktopView* PlasmaApp::viewForScreen(int screen, int desktop) const
{
    foreach (DesktopView *view, m_desktops) {
        if (view->containment()) {
            kDebug() << "comparing" << view->containment()->screen() << screen;
        }
        if (view->containment() && view->containment()->screen() == screen && (desktop < 0 || view->containment()->desktop() == desktop)) {
            return view;
        }
    }

    return 0;
}

DesktopCorona* PlasmaApp::corona(bool createIfMissing)
{
    if (!m_corona && createIfMissing) {
        QElapsedTimer t;
        t.start();
        DesktopCorona *c = new DesktopCorona(this);
        connect(c, SIGNAL(containmentAdded(Plasma::Containment*)),
                this, SLOT(containmentAdded(Plasma::Containment*)));
        connect(c, SIGNAL(configSynced()), this, SLOT(syncConfig()));
        connect(c, SIGNAL(screenOwnerChanged(int,int,Plasma::Containment*)),
                this, SLOT(containmentScreenOwnerChanged(int,int,Plasma::Containment*)));

        foreach (DesktopView *view, m_desktops) {
            connect(c, SIGNAL(screenOwnerChanged(int,int,Plasma::Containment*)),
                    view, SLOT(screenOwnerChanged(int,int,Plasma::Containment*)));
        }

        //actions!
        c->updateShortcuts();

        m_corona = c;
        c->setItemIndexMethod(QGraphicsScene::NoIndex);
        c->initializeLayout();

        kDebug() << " ------------------------------------------>" << t.elapsed();
    }

    return m_corona;
}

bool PlasmaApp::isPanelContainment(Plasma::Containment *containment)
{
    if (!containment) {
        return false;
    }

    Plasma::Containment::Type t = containment->containmentType();

    return t == Plasma::Containment::PanelContainment ||
           t == Plasma::Containment::CustomPanelContainment;

}

void PlasmaApp::createView(Plasma::Containment *containment)
{
    kDebug() << "!!{} STARTUP TIME" << QTime().msecsTo(QTime::currentTime()) << "Plasma App createView() start" << "(line:" << __LINE__ << ")";
    kDebug() << "Containment name:" << containment->name()
             << "| type" << containment->containmentType()
             <<  "| screen:" << containment->screen()
             <<  "| desktop:" << containment->desktop()
             << "| geometry:" << containment->geometry()
             << "| zValue:" << containment->zValue();

    // find the mapping of View to Containment, if any,
    // so we can restore things on start.

    if (isPanelContainment(containment)) {
        m_panelsWaiting << containment;
        m_panelViewCreationTimer.start();
    } else if (containment->screen() > -1 && containment->screen() < m_corona->numScreens()) {
        m_desktopsWaiting.append(containment);
        m_desktopViewCreationTimer.start();
    }
}

void PlasmaApp::setWmClass(WId id)
{
#ifdef Q_WS_X11
    XClassHint classHint;
    classHint.res_name = const_cast<char*>("Plasma");
    classHint.res_class = const_cast<char*>("Plasma");
    XSetClassHint(QX11Info::display(), id, &classHint);
#endif
}

void PlasmaApp::createWaitingPanels()
{
    if (m_panelsWaiting.isEmpty()) {
        return;
    }

    const QList<QWeakPointer<Plasma::Containment> > containments = m_panelsWaiting;
    m_panelsWaiting.clear();

    foreach (QWeakPointer<Plasma::Containment> containmentPtr, containments) {
        Plasma::Containment *containment = containmentPtr.data();
        if (!containment) {
            continue;
        }

        foreach (PanelView *view, m_panels) {
            if (view->containment() == containment) {
                continue;
            }
        }

        if (containment->screen() < 0) {
            continue;
        }

        // try to relocate the panel if it is on a now-non-existent screen
        if (containment->screen() >= m_corona->numScreens()) {
            m_panelRelocationCandidates << containment;
            continue;
        }

        createPanelView(containment);
    }

    if (!m_panelRelocationCandidates.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(relocatePanels()));
    }
}

void PlasmaApp::relocatePanels()
{
    // we go through relocatables last so that all other panels can be set up first,
    // preventing panel creation ordering to trip up the canRelocatePanel algorithm
    const DesktopTracker::Screen primary = DesktopTracker::self()->primaryScreen();
    QList<DesktopTracker::Screen> screens = DesktopTracker::self()->screens();
    screens.removeAll(primary);

    foreach (QWeakPointer<Plasma::Containment> c, m_panelRelocationCandidates) {
        Plasma::Containment *containment = c.data();
        if (!containment) {
            continue;
        }

        DesktopTracker::Screen moveTo;
        PanelView *panelView = createPanelView(containment);
        if (canRelocatePanel(panelView, primary)) {
            moveTo = primary;
        } else {
            foreach (const DesktopTracker::Screen &screen, screens) {
                if (canRelocatePanel(panelView, screen)) {
                    moveTo = screen;
                    break;
                }
            }
        }

        if (moveTo.id >= 0) {
            panelView->migrateTo(moveTo.id);
        } else {
            m_panels.removeAll(panelView);
            delete panelView;
        }
    }

    m_panelRelocationCandidates.clear();
}

PanelView *PlasmaApp::createPanelView(Plasma::Containment *containment)
{
    KConfigGroup viewIds(KGlobal::config(), "ViewIds");
    const int id = viewIds.readEntry(QString::number(containment->id()), 0);
    PanelView *panelView = new PanelView(containment, id);

    connect(panelView, SIGNAL(destroyed(QObject*)), this, SLOT(panelRemoved(QObject*)));
    m_panels << panelView;
    panelView->show();
    setWmClass(panelView->winId());
    return panelView;
}

void PlasmaApp::createWaitingDesktops()
{
    const QList<QWeakPointer<Plasma::Containment> > containments = m_desktopsWaiting;
    m_desktopsWaiting.clear();

    foreach (QWeakPointer<Plasma::Containment> weakContainment, containments) {
        if (weakContainment) {
            Plasma::Containment *containment = weakContainment.data();
            KConfigGroup viewIds(KGlobal::config(), "ViewIds");
            const int id = viewIds.readEntry(QString::number(containment->id()), 0);

            const int screen = containment->screen();
            if (screen >= m_corona->numScreens() || screen < 0) {
                kDebug() << "not creating a view on screen" << screen << "as it does not exist";
                continue;
            }

            DesktopView *view = viewForScreen(screen, -1);

            if (view) {
                kDebug() << "already had a view for" << containment->screen() << containment->desktop();
                // we already have a view for this screen
                continue;
            }

            kDebug() << "creating a new view for" << containment->screen() << containment->desktop()
                     << "and we have" << m_corona->numScreens() << "screens";

            // we have a new screen. neat.
            view = new DesktopView(containment, id, 0);
            if (m_corona) {
                connect(m_corona, SIGNAL(screenOwnerChanged(int,int,Plasma::Containment*)),
                        view, SLOT(screenOwnerChanged(int,int,Plasma::Containment*)));
            }

            m_desktops.append(view);
            view->show();
            setWmClass(view->winId());
        }
    }
}

void PlasmaApp::containmentAdded(Plasma::Containment *containment)
{
    if (isPanelContainment(containment)) {
        foreach (PanelView * panel, m_panels) {
            if (panel->containment() == containment) {
                kDebug() << "not creating second PanelView with existing Containment!!";
                return;
            }
        }
    }

    createView(containment);
}

void PlasmaApp::prepareContainment(Plasma::Containment *containment)
{
    if (!containment) {
        return;
    }

    disconnect(containment, 0, this, 0);
    connect(containment, SIGNAL(configureRequested(Plasma::Containment*)),
            this, SLOT(configureContainment(Plasma::Containment*)));

    if (isPanelContainment(containment)) {
        return;
    }

    if ((containment->containmentType() == Plasma::Containment::DesktopContainment ||
         containment->containmentType() == Plasma::Containment::CustomContainment)) {
        QAction *a = containment->action("remove");
        delete a; //activities handle removal now

        if (containment->containmentType() == Plasma::Containment::DesktopContainment) {
            foreach (QAction *action, m_corona->actions()) {
                containment->addToolBoxAction(action);
            }
        }
    }
}

void PlasmaApp::containmentScreenOwnerChanged(int wasScreen, int isScreen, Plasma::Containment *containment)
{
    Q_UNUSED(wasScreen)
    kDebug() << "@@@was" << wasScreen << "is" << isScreen << (QObject*)containment << m_desktops.count();

    if (isScreen < 0) {
        kDebug() << "@@@screen<0";
        return;
    }

    if (isPanelContainment(containment)) {
        kDebug() << "@@@isPanel";
        return;
    }

    foreach (DesktopView *view, m_desktops) {
        if (view->screen() == isScreen) {
            kDebug() << "@@@@found view" << view;
            return;
        }
    }

    kDebug() << "@@@@appending";
    m_desktopsWaiting.append(containment);
    m_desktopViewCreationTimer.start();
}

void PlasmaApp::configureContainment(Plasma::Containment *containment)
{
    const QString id = QString::number(containment->id()) + "settings" + containment->name();
    BackgroundDialog *configDialog = qobject_cast<BackgroundDialog*>(KConfigDialog::exists(id));

    if (configDialog) {
        configDialog->reloadConfig();
    } else {
        const QSize resolution = QApplication::desktop()->screenGeometry(containment->screen()).size();
        Plasma::View *view = viewForScreen(containment->screen(), containment->desktop());

        if (!view) {
            view = viewForScreen(desktop()->screenNumber(QCursor::pos()), containment->desktop());

            if (!view) {
                if (m_desktops.count() < 1) {
                    return;
                }

                view = m_desktops.at(0);
            }

        }

        KConfigSkeleton *nullManager = new KConfigSkeleton();
        configDialog = new BackgroundDialog(resolution, containment, view, 0, id, nullManager);
        configDialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(configDialog, SIGNAL(destroyed(QObject*)), nullManager, SLOT(deleteLater()));
    }

    configDialog->show();
    KWindowSystem::setOnDesktop(configDialog->winId(), KWindowSystem::currentDesktop());
    KWindowSystem::activateWindow(configDialog->winId());
}

void PlasmaApp::panelRemoved(QObject *panel)
{
    m_panels.removeAll((PanelView *)panel);
}

QString PlasmaApp::supportInformation() const
{
    QString streambuffer;
    QTextStream stream(&streambuffer);
    stream << "Plasma-desktop Support Information:\n"
           << "The following information should be used when requesting support.\n"
           << "It provides information about the currently running instance and which applets are used.\n"
           << "Please include the information provided underneath this introductory text along with "
           << "whatever you think may be relevant to the issue.\n\n";

    stream << "Version\n";
    stream << "=======\n";
    stream << "KDE SC version (runtime):\n";
    stream << KDE::versionString() << '\n';
    stream << "KDE SC version (compile):\n";
    stream << KDE_VERSION_STRING << '\n';
    stream << "Katie Version:\n";
    stream << qVersion() << '\n';

    stream << '\n' << "=========" << '\n';

    foreach (Plasma::Containment *containment, m_corona->containments()) {
        // a containment is also an applet so print standard applet information out
        addInformationForApplet(stream, containment);

        foreach (Plasma::Applet *applet, containment->applets()) {
            addInformationForApplet(stream, applet);
        }
    }

    return streambuffer;
}

void PlasmaApp::suspendStartup(const QString &app)
{
    // TODO: timeout for suspending
    m_startupsuspend++;
}

void PlasmaApp::resumeStartup(const QString &app)
{
    m_startupsuspend--;
}

void PlasmaApp::logout(int confirm, int sdtype)
{
    // TODO: prevent logout while initializing
    m_sdtype = static_cast<KWorkSpace::ShutdownType>(sdtype);
    if (confirm == KWorkSpace::ShutdownConfirmNo) {
        QTimer::singleShot(500, this, SLOT(doLogout()));
        return;
    }
    if (m_dialogActive) {
        return;
    }
    m_dialogActive = true;
    KApplication::kApplication()->updateUserTimestamp();
    const bool maysd = KDisplayManager().canShutdown();
    const bool choose = (m_sdtype == KWorkSpace::ShutdownTypeDefault);
    const bool logoutConfirmed = KSMShutdownDlg::confirmShutdown(maysd, choose, m_sdtype);
    if (logoutConfirmed) {
        QTimer::singleShot(500, this, SLOT(doLogout()));
    }
    m_dialogActive = false;
}

void PlasmaApp::wmChanged()
{
    // changing the window manager happens from the KCM, currently nothing to do when that happens
}

void PlasmaApp::captureDesktop()
{
    KToolInvocation::kdeinitExec("ksnapshot", QStringList() << "--fullscreen");
}

void PlasmaApp::captureCurrentWindow()
{
    KToolInvocation::kdeinitExec("ksnapshot", QStringList() << "--current");
}

void PlasmaApp::cleanup()
{
    if (m_klauncher) {
        m_klauncher->call("cleanup");
        m_klauncher->deleteLater();
        m_klauncher = nullptr;
    }

    if (m_wmproc && m_wmproc->state() != QProcess::NotRunning) {
        m_wmproc->kill();
        m_wmproc->waitForFinished();
        m_wmproc->deleteLater();
        m_wmproc = nullptr;
    }
}

void PlasmaApp::nextPhase()
{
    if (m_startupsuspend <= 0){ 
        switch (m_phase) {
            case 0: {
                static const QString kdedInterface = QString::fromLatin1("org.kde.kded");
                QDBusConnectionInterface* sessionInterface = QDBusConnection::sessionBus().interface();
                if (!sessionInterface->isServiceRegistered(kdedInterface)) {
                    sessionInterface->startService(kdedInterface);
                }
                m_klauncher->call("autoStart", int(0));
                m_kcminit->call("runPhase1");
                break;
            }
            case 1: {
                m_klauncher->call("autoStart", int(1));
                m_kcminit->call("runPhase2");
                break;
            }
            case 2: {
                m_klauncher->call("autoStart", int(2));
                break;
            }
        }
        m_phase++;
    }
    QTimer::singleShot(100, this, SLOT(nextPhase()));
}

void PlasmaApp::defaultLogout()
{
    logout(KWorkSpace::ShutdownConfirmYes, KWorkSpace::ShutdownTypeDefault);
}

void PlasmaApp::logoutWithoutConfirmation()
{
    logout(KWorkSpace::ShutdownConfirmNo, KWorkSpace::ShutdownTypeNone);
}

void PlasmaApp::haltWithoutConfirmation()
{
    logout(KWorkSpace::ShutdownConfirmNo, KWorkSpace::ShutdownTypeHalt);
}

void PlasmaApp::rebootWithoutConfirmation()
{
    logout(KWorkSpace::ShutdownConfirmNo, KWorkSpace::ShutdownTypeReboot);
}

void PlasmaApp::doLogout()
{
    cleanup();
    if (m_sdtype == KWorkSpace::ShutdownTypeDefault || m_sdtype == KWorkSpace::ShutdownTypeNone) {
        quit();
    } else {
        KDisplayManager().shutdown(m_sdtype);
    }
}

#include "moc_plasmaapp.cpp"
