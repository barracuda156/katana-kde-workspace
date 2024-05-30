/***************************************************************************
 *   Copyright (C) 2010 by Peter Penz <peter.penz19@gmail.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "filenamesearchprotocol.h"

#include <KComponentData>
#include <KDirLister>
#include <KFileItem>
#include <KIO/NetAccess>
#include <KIO/Job>
#include <KUrl>
#include <ktemporaryfile.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QRegExp>
#include <QTextStream>

FileNameSearchProtocol::FileNameSearchProtocol( const QByteArray &app ) :
    SlaveBase("search", app),
    m_checkContent(""),
    m_checkType(""),
    m_regExp(0)
{
}

FileNameSearchProtocol::~FileNameSearchProtocol()
{
    cleanup();
}

void FileNameSearchProtocol::listDir(const KUrl& url)
{
    cleanup();

    const KUrl directory = KUrl(url.queryItemValue("url"));
    // Don't try to iterate the pseudo filesystem directories of Linux
    if (directory.path() == QLatin1String("/dev")
        || directory.path() == QLatin1String("/proc")
        || directory.path() == QLatin1String("/sys")) {
        finished();
        return;
    }

    m_checkContent = url.queryItemValue("checkContent");

    m_literal = url.queryItemValue("literal");

    m_checkType = url.queryItemValue("checkType");


    QString search = url.queryItemValue("search");
    if (!search.isEmpty() && m_literal == "yes") {
        search = QRegExp::escape(search);
    }

    if (!search.isEmpty()) {
        m_regExp = new QRegExp(search, Qt::CaseInsensitive);
    }

    // Get all items of the directory
    KDirLister *dirLister = new KDirLister();
    dirLister->setAutoUpdate(false);
    dirLister->setAutoErrorHandlingEnabled(false, 0);

    QEventLoop eventLoop;
    QObject::connect(dirLister, SIGNAL(canceled()), &eventLoop, SLOT(quit()));
    QObject::connect(dirLister, SIGNAL(completed()), &eventLoop, SLOT(quit()));
    dirLister->openUrl(directory, true);
    eventLoop.exec();

    // Visualize all items that match the search pattern
    QList<KUrl> pendingDirs;
    const KFileItemList items = dirLister->items();
    foreach (const KFileItem& item, items) {
        bool addItem = false;
        if (!m_regExp || item.name().contains(*m_regExp)) {
            addItem = true;
            if (!m_checkType.isEmpty()) {
                addItem = false;
                const QStringList types = m_checkType.split(";");
                const KSharedPtr<KMimeType> mime = item.mimeTypePtr();
                foreach (const QString& t, types) {
                    if (mime->is(t)) {
                        addItem = true;
                    }
                }
            }
        } else if (!m_checkContent.isEmpty() && item.mimeTypePtr()->is(QLatin1String("text/plain"))) {
            addItem = contentContainsPattern(item.url());
        }

        if (addItem) {
            KIO::UDSEntry entry = item.entry();
            entry.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, item.url().fileName());
            entry.insert(KIO::UDSEntry::UDS_URL, item.url().url());
            entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, item.mimetype());
            listEntry(entry, false);
        }
    }
    listEntry(KIO::UDSEntry(), true);

    delete dirLister;
    dirLister = 0;

    cleanup();
    finished();
}

bool FileNameSearchProtocol::contentContainsPattern(const KUrl& fileName) const
{
    Q_ASSERT(m_regExp);

    QString path;
    KTemporaryFile tempFile;

    if (fileName.isLocalFile()) {
        path = fileName.path();
    } else if (tempFile.open()) {
        KIO::Job* getJob = KIO::file_copy(fileName,
                                          tempFile.fileName(),
                                          -1,
                                          KIO::Overwrite | KIO::HideProgressInfo);
        if (!KIO::NetAccess::synchronousRun(getJob, 0)) {
            // The non-local file could not be downloaded
            return false;
        }
        path = tempFile.fileName();
    } else {
        // No temporary file could be created for downloading non-local files
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
         return false;
    }

     QTextStream in(&file);
     while (!in.atEnd()) {
         const QString line = in.readLine();
         if (line.contains(*m_regExp)) {
             return true;
         }
     }

     return false;
}

void FileNameSearchProtocol::cleanup()
{
    delete m_regExp;
    m_regExp = 0;
}

int main( int argc, char **argv )
{
    KComponentData instance("kio_search");
    QCoreApplication app(argc, argv);

    if (argc != 2) {
        fprintf(stderr, "Usage: kio_filenamesearch protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

    FileNameSearchProtocol slave(argv[1]);
    slave.dispatchLoop();

    return 0;
}
