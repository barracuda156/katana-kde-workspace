/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#include "config-unix.h"
#include "main.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif // HAVE_MALLOC_H

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QMessageBox>
#include <QEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <QFile>

#include <ksharedconfig.h>
#include <kglobal.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kcrash.h>
#include <kxerrorhandler.h>
#include <kdialog.h>
#include <kstandarddirs.h>
#include <kde_file.h>
#include <kdebug.h>
#include <KComboBox>
#include <QtGui/qx11info_x11.h>
#include <fixx11h.h>

#include "atoms.h"
#include "options.h"
#include "utils.h"
#include "effects.h"
#include "workspace.h"
#include "composite.h"
#include "xcbutils.h"

#include <signal.h>
#include <X11/Xproto.h>

namespace KWin
{

Options* options;

Atoms* atoms;

int screen_number = -1;

bool initting = false;

/**
 * Whether to run Xlib in synchronous mode and print backtraces for X errors.
 * Note that you most probably need to configure cmake with "-D__KDE_HAVE_GCC_VISIBILITY=0"
 * and -rdynamic in CXXFLAGS for kBacktrace() to work.
 */
static bool kwin_sync = false;

static QByteArray make_selection_atom(int screen_P)
{
    if (screen_P < 0)
        screen_P = DefaultScreen(display());
    char tmp[ 30 ];
    sprintf(tmp, "WM_S%d", screen_P);
    return QByteArray(tmp);
}

// errorMessage is only used ifndef NDEBUG, and only in one place.
// it might be worth reevaluating why this is used? I don't know.
#ifndef NDEBUG
/**
 * Outputs: "Error: <error> (<value>), Request: <request>(<value>), Resource: <value>"
 */
// This is copied from KXErrorHandler and modified to explicitly use known extensions
static QByteArray errorMessage(const XErrorEvent& event, Display* dpy)
{
    QByteArray ret;
    char tmp[256];
    char num[256];
    if (event.request_code < 128) {
        // Core request
        XGetErrorText(dpy, event.error_code, tmp, 255);
        // The explanation in parentheses just makes
        // it more verbose and is not really useful
        if (char* paren = strchr(tmp, '('))
            * paren = '\0';
        // The various casts are to get overloads non-ambiguous :-/
        ret = QByteArray("error: ") + (const char*)(tmp) + '[' + QByteArray::number(event.error_code) + ']';
        sprintf(num, "%d", event.request_code);
        XGetErrorDatabaseText(dpy, "XRequest", num, "<unknown>", tmp, 256);
        ret += QByteArray(", request: ") + (const char*)(tmp) + '[' + QByteArray::number(event.request_code) + ']';
        if (event.resourceid != 0)
            ret += QByteArray(", resource: 0x") + QByteArray::number(qlonglong(event.resourceid), 16);
    } else { // Extensions
        // XGetErrorText() currently has a bug that makes it fail to find text
        // for some errors (when error==error_base), also XGetErrorDatabaseText()
        // requires the right extension name, so it is needed to get info about
        // all extensions. However that is almost impossible:
        // - Xlib itself has it, but in internal data.
        // - Opening another X connection now can cause deadlock with server grabs.
        // - Fetching it at startup means a bunch of roundtrips.

        // KWin here explicitly uses known extensions.
        XGetErrorText(dpy, event.error_code, tmp, 255);
        int index = -1;
        int base = 0;
        QVector<Xcb::ExtensionData> extensions = Xcb::Extensions::self()->extensions();
        for (int i = 0; i < extensions.size(); ++i) {
            const Xcb::ExtensionData &extension = extensions.at(i);
            if (extension.errorBase != 0 &&
                    event.error_code >= extension.errorBase && (index == -1 || extension.errorBase > base)) {
                index = i;
                base = extension.errorBase;
            }
        }
        if (tmp == QString::number(event.error_code)) {
            // XGetErrorText() failed or it has a bug that causes not finding all errors, check ourselves
            if (index != -1) {
                snprintf(num, 255, "%s.%d", extensions.at(index).name.constData(), event.error_code - base);
                XGetErrorDatabaseText(dpy, "XProtoError", num, "<unknown>", tmp, 255);
            } else
                strcpy(tmp, "<unknown>");
        }
        if (char* paren = strchr(tmp, '('))
            * paren = '\0';
        if (index != -1)
            ret = QByteArray("error: ") + (const char*)(tmp) + '[' + extensions.at(index).name +
                  '+' + QByteArray::number(event.error_code - base) + ']';
        else
            ret = QByteArray("error: ") + (const char*)(tmp) + '[' + QByteArray::number(event.error_code) + ']';
        tmp[0] = '\0';
        for (int i = 0; i < extensions.size(); ++i)
            if (extensions.at(i).majorOpcode == event.request_code) {
                snprintf(num, 255, "%s.%d", extensions.at(i).name.constData(), event.minor_code);
                XGetErrorDatabaseText(dpy, "XRequest", num, "<unknown>", tmp, 255);
                ret += QByteArray(", request: ") + (const char*)(tmp) + '[' +
                       extensions.at(i).name + '+' + QByteArray::number(event.minor_code) + ']';
            }
        if (tmp[0] == '\0')   // Not found?
            ret += QByteArray(", request <unknown> [") + QByteArray::number(event.request_code) + ':'
                   + QByteArray::number(event.minor_code) + ']';
        if (event.resourceid != 0)
            ret += QByteArray(", resource: 0x") + QByteArray::number(qlonglong(event.resourceid), 16);
    }
    return ret;
}
#endif

static int x11ErrorHandler(Display* d, XErrorEvent* e)
{
    Q_UNUSED(d);
    bool ignore_badwindow = true; // Might be temporary

    if (initting && (e->request_code == X_ChangeWindowAttributes || e->request_code == X_GrabKey) &&
            e->error_code == BadAccess) {
        fputs(i18n("kwin: it looks like there's already a window manager running. kwin not started.\n").toLocal8Bit(), stderr);
        exit(1);
    }

    if (ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
        return 0;

#ifndef NDEBUG
    //fprintf( stderr, "kwin: X Error (%s)\n", KXErrorHandler::errorMessage( *e, d ).data());
    kWarning(1212) << "kwin: X Error (" << errorMessage(*e, d) << ")";
#endif

    if (kwin_sync) {
        const QByteArray trace = kBacktrace();
        fprintf(stderr, "%s\n", trace.constData());
    }

    return 0;
}

Application::Application()
    : KApplication()
    , owner(new KSelectionOwner(make_selection_atom(screen_number), screen_number, this))
{
    if (KCmdLineArgs::parsedArgs("qt")->isSet("sync")) {
        kwin_sync = true;
        XSynchronize(display(), True);
        kDebug(1212) << "Running KWin in sync mode";
    }
    setQuitOnLastWindowClosed(false);

    if (screen_number == -1)
        screen_number = DefaultScreen(display());
}

Application::~Application()
{
    disconnect(owner, 0, this, 0);
    delete Workspace::self();
    if (owner->ownerWindow() != None) { // If there was no --replace (no new WM)
        XSetInputFocus(display(), PointerRoot, RevertToPointerRoot, xTime());
        // Remove windowmanager privileges
        XSelectInput(display(), rootWindow(), PropertyChangeMask);
    }
    delete options;
    delete effects;
    delete atoms;
    owner->release();
    delete owner;
}

bool Application::setup()
{
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    KSharedConfig::Ptr config = KGlobal::config();
    if (!config->isImmutable() && args->isSet("lock")) {
        // TODO: This shouldn't be necessary
        //config->setReadOnly( true );
        config->reparseConfiguration();
    }

    if (!owner->claim(args->isSet("replace"))) {
        fputs(i18n("kwin: unable to claim manager selection, another wm running? (try using --replace)\n").toLocal8Bit(), stderr);
        return false;
    }
    connect(owner, SIGNAL(lostOwnership()), this, SLOT(lostSelection()), Qt::DirectConnection);

    KCrash::setFlags(KCrash::AutoRestart | KCrash::Log);

    initting = true; // Startup...
    // first load options - done internally by a different thread
    options = new Options;

    // Install X11 error handler
    XSetErrorHandler(x11ErrorHandler);

    // Check  whether another windowmanager is running
    XSelectInput(display(), rootWindow(), SubstructureRedirectMask);
    syncX(); // Trigger error now

    atoms = new Atoms;

//    initting = false; // TODO

    // create workspace.
    (void) new Workspace(isSessionRestored());

    syncX(); // Trigger possible errors, there's still a chance to abort

    initting = false; // Startup done, we are up and running now.
    return true;
}

bool Application::saveSession()
{
    Workspace::self()->sessionSaveStarted();
    RuleBook::self()->setUpdatesDisabled(true);
    Workspace::self()->storeSession(kapp->sessionConfig());
    kapp->sessionConfig()->sync();
    RuleBook::self()->setUpdatesDisabled(false);   // re-enable
    Workspace::self()->sessionSaveDone();
    return KApplication::saveSession();
}

void Application::lostSelection()
{
    KCrash::setFlags(0); // too late to restart now
    quit();
}

bool Application::x11EventFilter(XEvent* e)
{
    if (Workspace::self() && Workspace::self()->workspaceEvent(e))
        return true;
    return KApplication::x11EventFilter(e);
}

bool Application::notify(QObject* o, QEvent* e)
{
    if (Workspace::self() && Workspace::self()->workspaceEvent(e))
        return true;
    return KApplication::notify(o, e);
}

} // namespace

static const char version[] = KWIN_VERSION_STRING;
static const char description[] = I18N_NOOP("KDE window manager");

int main(int argc, char * argv[])
{
#ifdef M_TRIM_THRESHOLD
    // Prevent fragmentation of the heap by malloc (glibc).
    //
    // The default threshold is 128*1024, which can result in a large memory usage
    // due to fragmentation especially if we use the raster graphicssystem. On the
    // otherside if the threshold is too low, free() starts to permanently ask the kernel
    // about shrinking the heap.
    const int pagesize = sysconf(_SC_PAGESIZE);
    mallopt(M_TRIM_THRESHOLD, 5*pagesize);
#endif // M_TRIM_THRESHOLD

    KAboutData aboutData(
        "kwin",                     // The program name used internally
        0,                          // The message catalog name. If null, program name is used instead
        ki18n("KWin"),              // A displayable program name string
        version,                    // The program version string
        ki18n(description),         // Short description of what the app does
        KAboutData::License_GPL,    // The license this code is released under
        ki18n("(c) 1999-2008, The KDE Developers"));   // Copyright Statement
    aboutData.addAuthor(ki18n("Matthias Ettrich"), KLocalizedString(), "ettrich@kde.org");
    aboutData.addAuthor(ki18n("Cristian Tibirna"), KLocalizedString(), "tibirna@kde.org");
    aboutData.addAuthor(ki18n("Daniel M. Duley"), KLocalizedString(), "mosfet@kde.org");
    aboutData.addAuthor(ki18n("Luboš Luňák"), KLocalizedString(), "l.lunak@kde.org");
    aboutData.addAuthor(ki18n("Martin Gräßlin"), ki18n("Maintainer"), "mgraesslin@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions args;
    args.add("lock", ki18n("Disable configuration options"));
    args.add("replace", ki18n("Replace already-running ICCCM2.0-compliant window manager"));
    KCmdLineArgs::addCmdLineOptions(args);

    KWin::Application a;
    a.enableSessionManagement();
    QDBusInterface plasma("org.kde.plasma-desktop", "/App", "local.PlasmaApp", QDBusConnection::sessionBus());
    if (plasma.isValid()) {
        plasma.call("suspendStartup", "kwin");
    }
    if (!a.setup()) {
        if (plasma.isValid()) {
            plasma.call("resumeStartup", "kwin");
        }
        a.exit(1);
        return 1;
    }

    // wait for the workspace initialization to complete before resuming
    while (KWin::Workspace::self() && KWin::Workspace::self()->initializing()) {
        a.processEvents();
    }
    // and the compositor
    while (KWin::Compositor::starting()) {
        a.processEvents();
    }
    // and one more iteration just in case
    a.processEvents();

    if (plasma.isValid()) {
        plasma.call("resumeStartup", "kwin");
    }
    KGlobal::locale()->insertCatalog("kwin_effects");

    fcntl(XConnectionNumber(KWin::display()), F_SETFD, 1);

    QString appname;
    if (KWin::screen_number == 0)
        appname = "org.kde.kwin";
    else
        appname.sprintf("org.kde.kwin-screen-%d", KWin::screen_number);

    QDBusConnection::sessionBus().interface()->registerService(
        appname, QDBusConnectionInterface::DontQueueService);

    return a.exec();
}

#include "moc_main.cpp"
