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

#ifndef _kateapp_adaptor_h_
#define _kateapp_adaptor_h_

#include <QtDBus/QDBusAbstractAdaptor>

class KateApp;

class KateAppAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Kate.Application")
    Q_PROPERTY(QString activeSession READ activeSession)
  public:
    KateAppAdaptor (KateApp *app);

    /**
     * emit the exiting signal
     */
    void emitExiting ();
    void emitDocumentClosed(const QString& token);
    
  public Q_SLOTS:
    /**
     * open a file with given url and encoding
     * will get view created
     * @param url url of the file
     * @param encoding encoding name
     * @return success
     */
    bool openUrl (QString url, QString encoding);
    
    /**
     * open a file with given url and encoding
     * will get view created
     * @param url url of the file
     * @param encoding encoding name
     * @return token or ERROR
     */
    QString tokenOpenUrl (QString url, QString encoding);

    /**
     * set cursor of active view in active main window
     * @param line line for cursor
     * @param column column for cursor
     * @return success
     */
    bool setCursor (int line, int column);

    /**
     * helper to handle stdin input
     * open a new document/view, fill it with the text given
     * @param text text to fill in the new doc/view
     * @return success
     */
    bool openInput (QString text);

    /**
     * activate a given session
     * @param session session name
     * @return success
     */
    bool activateSession (QString session);

    /**
     * activate this kate instance
     */
    void activate ();
    
  Q_SIGNALS:
    /**
     * Notify the world that this kate instance is exiting.
     * All apps should stop using the dbus interface of this instance after this signal got emitted.
     */
    void exiting ();
    void documentClosed(const QString& token);
  public:
    QString activeSession();
  private:
    KateApp *m_app;
};

#endif
// kate: space-indent on; indent-width 2; replace-tabs on;

