/* This file is part of the KDE libraries
    Copyright (C) 1997 Matthias Kalle Dalheimer (kalle@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kworkspace.h"
#include "kdisplaymanager.h"

#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSocketNotifier>
#include <QtDBus/QtDBus>

#include <kapplication.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kuser.h>
#include <plasma_interface.h>

#include <stdlib.h> // getenv()
#include <sys/types.h>

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/SM/SMlib.h>
#include <fixx11h.h>
#endif

#ifdef Q_WS_X11
#define DISPLAY "DISPLAY"
#elif defined(Q_WS_QWS)
#define DISPLAY "QWS_DISPLAY"
#endif

#include "kworkspace_p.h"

namespace KWorkSpace
{
#ifdef Q_WS_X11
static void save_yourself_callback( SmcConn conn_P, SmPointer, int, Bool , int, Bool )
    {
    SmcSaveYourselfDone( conn_P, True );
    }

static void dummy_callback( SmcConn, SmPointer )
    {
    }
#endif
KRequestShutdownHelper::KRequestShutdownHelper()
    {
#ifdef Q_WS_X11
    SmcCallbacks calls;
    calls.save_yourself.callback = save_yourself_callback;
    calls.die.callback = dummy_callback;
    calls.save_complete.callback = dummy_callback;
    calls.shutdown_cancelled.callback = dummy_callback;
    char* id = NULL;
    char err[ 11 ];
    conn = SmcOpenConnection( NULL, NULL, 1, 0,
        SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask
        | SmcShutdownCancelledProcMask, &calls, NULL, &id, 10, err );
    if( id != NULL )
        free( id );
    if( conn == NULL )
        return; // no SM
    // set the required properties, mostly dummy values
    SmPropValue propvalue[ 5 ];
    SmProp props[ 5 ];
    propvalue[ 0 ].length = sizeof( unsigned char );
    unsigned char value0 = SmRestartNever; // so that this extra SM connection doesn't interfere
    propvalue[ 0 ].value = &value0;
    props[ 0 ].name = const_cast< char* >( SmRestartStyleHint );
    props[ 0 ].type = const_cast< char* >( SmCARD8 );
    props[ 0 ].num_vals = 1;
    props[ 0 ].vals = &propvalue[ 0 ];
    QByteArray username = KUser(KUser::UseEffectiveUID).loginName().toLocal8Bit();
    propvalue[ 1 ].length = username.size();
    propvalue[ 1 ].value = (SmPointer)( username.isEmpty() ? "" : username.data() );
    props[ 1 ].name = const_cast< char* >( SmUserID );
    props[ 1 ].type = const_cast< char* >( SmARRAY8 );
    props[ 1 ].num_vals = 1;
    props[ 1 ].vals = &propvalue[ 1 ];
    propvalue[ 2 ].length = 0;
    propvalue[ 2 ].value = (SmPointer)( "" );
    props[ 2 ].name = const_cast< char* >( SmRestartCommand );
    props[ 2 ].type = const_cast< char* >( SmLISTofARRAY8 );
    props[ 2 ].num_vals = 1;
    props[ 2 ].vals = &propvalue[ 2 ];
    propvalue[ 3 ].length = strlen( "requestshutdownhelper" );
    propvalue[ 3 ].value = (SmPointer)"requestshutdownhelper";
    props[ 3 ].name = const_cast< char* >( SmProgram );
    props[ 3 ].type = const_cast< char* >( SmARRAY8 );
    props[ 3 ].num_vals = 1;
    props[ 3 ].vals = &propvalue[ 3 ];
    propvalue[ 4 ].length = 0;
    propvalue[ 4 ].value = (SmPointer)( "" );
    props[ 4 ].name = const_cast< char* >( SmCloneCommand );
    props[ 4 ].type = const_cast< char* >( SmLISTofARRAY8 );
    props[ 4 ].num_vals = 1;
    props[ 4 ].vals = &propvalue[ 4 ];
    SmProp* p[ 5 ] = { &props[ 0 ], &props[ 1 ], &props[ 2 ], &props[ 3 ], &props[ 4 ] };
    SmcSetProperties( conn, 5, p );
    notifier = new QSocketNotifier( IceConnectionNumber( SmcGetIceConnection( conn )),
        QSocketNotifier::Read, this );
    connect( notifier, SIGNAL(activated(int)), SLOT(processData()));
#endif
    }

KRequestShutdownHelper::~KRequestShutdownHelper()
    {
#ifdef Q_WS_X11	    
    if( conn != NULL )
        {
        delete notifier;
        SmcCloseConnection( conn, 0, NULL );
        }
#endif
    }

void KRequestShutdownHelper::processData()
    {
#ifdef Q_WS_X11
    if( conn != NULL )
        IceProcessMessages( SmcGetIceConnection( conn ), 0, 0 );
#endif    
    }

bool KRequestShutdownHelper::requestShutdown( ShutdownConfirm confirm )
    {
#ifdef Q_WS_X11
    if( conn == NULL )
        return false;
    SmcRequestSaveYourself( conn, SmSaveBoth, True, SmInteractStyleAny,
        confirm == ShutdownConfirmNo, True );
    // flush the request
    IceFlush(SmcGetIceConnection(conn));
#endif    
    return true;
    }
#ifdef Q_WS_X11
static KRequestShutdownHelper* helper = NULL;

static void cleanup_sm()
{
    delete helper;
}
#endif

void requestShutDown(ShutdownConfirm confirm, ShutdownType sdtype)
{
#ifdef Q_WS_X11
    /*  use plasma's dcop interface if necessary  */
    if ( confirm == ShutdownConfirmYes ||
         sdtype != ShutdownTypeDefault )
    {
        local::PlasmaApp plasma("org.kde.plasma-desktop", "/App", QDBusConnection::sessionBus());
        plasma.logout((int)confirm,  (int)sdtype);
        return;
    }

    if( helper == NULL )
    {
        helper = new KRequestShutdownHelper();
        qAddPostRoutine(cleanup_sm);
    }
    helper->requestShutdown( confirm );
#endif
}

bool canShutDown( ShutdownConfirm confirm, ShutdownType sdtype )
{
#ifdef Q_WS_X11
    if ( confirm == ShutdownConfirmYes ||
         sdtype != ShutdownTypeDefault )
    {
        return KDisplayManager().canShutdown();
    }

    return true;
#else
    return false;
#endif
}

} // end namespace

#include "moc_kworkspace_p.cpp"
