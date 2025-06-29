/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
 
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.
 
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
 
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kateappadaptor.h"

#include "kateapp.h"
#include "katesession.h"
#include "katedocmanager.h"
#include "katemainwindow.h"
#include "moc_kateappadaptor.cpp"

#include <kdebug.h>
#include <kwindowsystem.h>

KateAppAdaptor::KateAppAdaptor (KateApp *app)
    : QDBusAbstractAdaptor( app )
    , m_app (app)
{}

void KateAppAdaptor::activate ()
{
  KateMainWindow *win = m_app->activeMainWindow();
  if (!win)
    return;

  win->show();
  win->activateWindow();
  win->raise();
  
#ifdef Q_WS_X11
  KWindowSystem::forceActiveWindow (win->winId ());
  KWindowSystem::raiseWindow (win->winId ());
  KWindowSystem::demandAttention (win->winId ());
#endif
}

bool KateAppAdaptor::openUrl (QString url, QString encoding)
{
  kDebug () << "openURL";
  return m_app->openUrl (url, encoding);
}

//-----------
QString KateAppAdaptor::tokenOpenUrl (QString url, QString encoding)
{
  kDebug () << "openURL";
  KTextEditor::Document *doc=m_app->openDocUrl (url, encoding);
  if (!doc) return QString("ERROR");
  return QString::number((qptrdiff)doc);
}
//--------

bool KateAppAdaptor::setCursor (int line, int column)
{
  return m_app->setCursor (line, column);
}

bool KateAppAdaptor::openInput (QString text)
{
  return m_app->openInput (text);
}

bool KateAppAdaptor::activateSession (QString session)
{
  m_app->sessionManager()->activateSession (m_app->sessionManager()->giveSession (session));

  return true;
}

QString KateAppAdaptor::activeSession()
{
  return m_app->sessionManager()->activeSession()->sessionName();
}

void KateAppAdaptor::emitExiting ()
{
  emit exiting (); 
}

void KateAppAdaptor::emitDocumentClosed(const QString& token)
{
  documentClosed(token);
}

// kate: space-indent on; indent-width 2; replace-tabs on;
