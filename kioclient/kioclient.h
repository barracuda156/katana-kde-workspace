/* This file is part of the KDE project
   Copyright (C) 1999-2006 David Faure <faure@kde.org>

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

#ifndef KIOCLIENT_H
#define KIOCLIENT_H

#include <kapplication.h>
#include <kurl.h>
#include <kio/udsentry.h>
#include <kio/job.h>

class ClientApp : public KApplication
{
    Q_OBJECT
public:
    /** Parse command-line arguments and "do it" */
    static bool doIt();

private Q_SLOTS:
    void slotPrintData(KIO::Job *job, const QByteArray &data);
    void slotEntries(KIO::Job* job, const KIO::UDSEntryList& );
    void slotResult( KJob * );
    void slotDialogCanceled();

private:
    bool kde_open( const KUrl& url, const QString& mimeType );
    bool doCopy( int firstArg );
    bool doMove( int firstArg );
    bool doList( int firstArg );
    bool doRemove( int firstArg );

    static bool m_ok;
};

#endif
