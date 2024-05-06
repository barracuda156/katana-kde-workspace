/*
  Copyright (c) 1999 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <config-workspace.h>

#include "main.h"

#include <unistd.h>

#include <QFile>
#include <QTimer>
#include <QLibrary>
#include <QtDBus/QtDBus>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kservice.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocale.h>
#include <kservicetypetrader.h>
#include <kglobalsettings.h>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <QtGui/qx11info_x11.h>
#endif

static int ready[ 2 ];
static bool startup = false;

static void sendReady()
{
  if( ready[ 1 ] == -1 )
    return;
  char c = 0;
  write( ready[ 1 ], &c, 1 );
  close( ready[ 1 ] );
  ready[ 1 ] = -1;
}

static void waitForReady()
{
  char c = 1;
  close( ready[ 1 ] );
  read( ready[ 0 ], &c, 1 );
  close( ready[ 0 ] );
}

bool KCMInit::runModule(const QString &libName, KService::Ptr service)
{
    QLibrary lib(libName);
    if (lib.load()) {
        QVariant tmp = service->property("X-KDE-Init-Symbol", QVariant::String);
        QString kcminit;
        if ( tmp.isValid() ) {
            kcminit = tmp.toString();
            if( !kcminit.startsWith( QLatin1String( "kcminit_" ) ) )
                kcminit = "kcminit_" + kcminit;
        } else {
            kcminit = "kcminit_" + libName;
        }

        // get the kcminit_ function
        void *init = lib.resolve(kcminit.toUtf8());
        if (init) {
            // initialize the module
            kDebug() << "Initializing " << libName << ": " << kcminit;

            void (*func)() = (void(*)())init;
            func();
            return true;
        } else {
            kDebug() << "Module" << libName << "does not actually have a kcminit function";
        }
    }
    return false;
}

void KCMInit::runModules( int phase )
{
  for(KService::List::Iterator it = list.begin();
      it != list.end();
      ++it) {
      KService::Ptr service = (*it);

      QString library = service->library();
      if (library.isEmpty()) {
          continue; // Skip
      }

      QVariant vphase = service->property("X-KDE-Init-Phase", QVariant::Int );
      int libphase = 1;
      if( vphase.isValid() )
          libphase = vphase.toInt();

      if( phase != -1 && libphase != phase )
          continue;

      // try to load the library
      if (!alreadyInitialized.contains(library)) {
          runModule(library, service);
          alreadyInitialized.append(library);
      }
  }
}

KCMInit::KCMInit( KCmdLineArgs* args )
{
  QDBusConnection::sessionBus().registerObject("/kcminit", this,
      QDBusConnection::ExportScriptableSlots|QDBusConnection::ExportScriptableSignals);
  QString arg;
  if (args->count() == 1) {
    arg = args->arg(0);
  }

  if (args->isSet("list"))
  {
    list = KServiceTypeTrader::self()->query( "KCModuleInit" );

    for(KService::List::Iterator it = list.begin();
        it != list.end();
        ++it)
    {
      KService::Ptr service = (*it);
      if (service->library().isEmpty())
        continue; // Skip
      printf("%s\n", QFile::encodeName(service->desktopEntryName()).data());
    }
    return;
  }

  if (!arg.isEmpty()) {

    QString module = arg;
    if (!module.endsWith(".desktop"))
       module += ".desktop";

    KService::Ptr serv = KService::serviceByStorageId( module );
    if ( !serv || serv->library().isEmpty() ) {
      kError() << i18n("Module %1 not found", module);
      return;
    } else
      list.append(serv);

  } else {

    // locate the desktop files
    list = KServiceTypeTrader::self()->query( "KCModuleInit" );

  }

  if( startup )
  {
     runModules( 0 );
     sendReady();
     QTimer::singleShot( 300 * 1000, qApp, SLOT(quit())); // just in case
     qApp->exec(); // wait for runPhase1() and runPhase2()
  }
  else
     runModules( -1 ); // all phases
}

KCMInit::~KCMInit()
{
  sendReady();
}

void KCMInit::runPhase1()
{
  runModules( 1 );
  emit phase1Done();
}

void KCMInit::runPhase2()
{
  runModules( 2 );
  emit phase2Done();
  qApp->exit( 0 );
}

int main(int argc, char *argv[])
{
  // during KDE startup only important kcm's are started very early in the login process,
  // the rest is delayed, so fork and make parent return after the initial phase
  pipe( ready );
  if( fork() != 0 )
  {
      waitForReady();
      return 0;
  }
  close( ready[ 0 ] );

  startup = ( strcmp( argv[ 0 ], "kcminit_startup" ) == 0 ); // started from startkde?
  KAboutData aboutData( "kcminit", "kcminit", ki18n("KCMInit"),
                        "",
                        ki18n("KCMInit - runs startup initialization for Control Modules."));

  KCmdLineArgs::init(argc, argv, &aboutData);

  KCmdLineOptions options;
  options.add("list", ki18n("List modules that are run at startup"));
  options.add("+module", ki18n("Configuration module to run"));
  KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication app;
  QDBusConnection::sessionBus().interface()->registerService( "org.kde.kcminit",
      QDBusConnectionInterface::DontQueueService );
  KCMInit kcminit( KCmdLineArgs::parsedArgs());
  return 0;
}

#include "moc_main.cpp"
