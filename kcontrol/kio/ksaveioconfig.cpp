/*
   Copyright (C) 2001 Dawit Alemayehu <adawit@kde.org>

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

// Own
#include "ksaveioconfig.h"

// Qt
#include <QtDBus/QtDBus>

// KDE
#include <kconfig.h>
#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kconfiggroup.h>
#include <kio/ioslave_defaults.h>

class KSaveIOConfigPrivate
{
public:
  KSaveIOConfigPrivate ();
  ~KSaveIOConfigPrivate ();

  KConfig* config;
};

K_GLOBAL_STATIC(KSaveIOConfigPrivate, d)

KSaveIOConfigPrivate::KSaveIOConfigPrivate ()
                     : config(0)
{
}

KSaveIOConfigPrivate::~KSaveIOConfigPrivate ()
{
  delete config;
}

static KConfig* config()
{
  if (!d->config)
     d->config = new KConfig("kioslaverc", KConfig::NoGlobals);

  return d->config;
}

void KSaveIOConfig::reparseConfiguration ()
{
  delete d->config;
  d->config = 0;
}

void KSaveIOConfig::setConnectTimeout( int _timeout )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry("ConnectTimeout", qMax(MIN_TIMEOUT_VALUE,_timeout));
  cfg.sync();
}

void KSaveIOConfig::setResponseTimeout( int _timeout )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry("ResponseTimeout", qMax(MIN_TIMEOUT_VALUE,_timeout));
  cfg.sync();
}


void KSaveIOConfig::setMarkPartial( bool _mode )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry( "MarkPartial", _mode );
  cfg.sync();
}

void KSaveIOConfig::setMinimumKeepSize( int _size )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry( "MinimumKeepSize", _size );
  cfg.sync();
}

void KSaveIOConfig::setAutoResume( bool _mode )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry( "AutoResume", _mode );
  cfg.sync();
}

void KSaveIOConfig::updateRunningIOSlaves (QWidget *parent)
{
  // Inform all running io-slaves about the changes...
  // if we cannot update, ioslaves inform the end user...
  QDBusMessage message = QDBusMessage::createSignal("/KIO/Scheduler", "org.kde.KIO.Scheduler", "reparseSlaveConfiguration");
  message << QString();
  if (!QDBusConnection::sessionBus().send(message))
  {
    KMessageBox::information (parent,
                              i18n("You have to restart the running applications "
                                   "for these changes to take effect."),
                              i18nc("@title:window", "Update Failed"));
  }
}
