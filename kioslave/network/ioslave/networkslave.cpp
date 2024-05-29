/*
    This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "networkslave.h"

#include <QCoreApplication>
#include <KMimeType>
#include <KComponentData>
#include <KDebug>
#include <kdemacros.h>

#include <sys/stat.h>

static QString urlForService(const KDNSSDService &kdnssdservice)
{
    // for compatibility and because there is no KIO slave to open rfb protocol
    if (kdnssdservice.url.startsWith(QLatin1String("rfb://"))) {
        QString result = kdnssdservice.url;
        result = result.replace(QLatin1String("rfb://"), QLatin1String("vnc://"));
        return result;
    } else if (kdnssdservice.url.startsWith(QLatin1String("sftp-ssh://"))) {
        QString result = kdnssdservice.url;
        result = result.replace(QLatin1String("sftp-ssh://"), QLatin1String("sftp://"));
        return result;
    }
    return kdnssdservice.url;
}

static QString mimeForService(const KDNSSDService &kdnssdservice)
{
    const QString servicemimetype = QString::fromLatin1("inode/vnd.kde.service.%1").arg(
        KUrl(kdnssdservice.url).protocol()
    );
    // qDebug() << Q_FUNC_INFO << servicemimetype;
    const KMimeType::Ptr kmimetypeptr = KMimeType::mimeType(servicemimetype);
    if (kmimetypeptr.isNull()) {
        return QString::fromLatin1("inode/vnd.kde.service.unknown");
    }
    return kmimetypeptr->name();
}

static QString iconForService(const QString &servicemimetype)
{
    const KMimeType::Ptr kmimetypeptr = KMimeType::mimeType(servicemimetype);
    if (kmimetypeptr.isNull()) {
        return QString::fromLatin1("unknown");
    }
    return kmimetypeptr->iconName();
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: kio_network app-socket\n");
        exit(-1);
    }

    KComponentData componentData("kio_network");
    QCoreApplication app(argc, argv);

    NetworkSlave slave(argv[1]);
    slave.dispatchLoop();

    return 0;
}

NetworkSlave::NetworkSlave(const QByteArray &programSocket)
    : SlaveBase("network", programSocket),
    m_kdnssd(nullptr)
{
}

NetworkSlave::~NetworkSlave()
{
    delete m_kdnssd;
}

void NetworkSlave::stat(const KUrl &url)
{
    if (!KDNSSD::isSupported()) {
        error(KIO::ERR_UNSUPPORTED_ACTION, url.prettyUrl());
        return;
    }
    const QString urlpath = url.path();
    if (urlpath.isEmpty() || urlpath == QLatin1String("/")) {
        // fake the root entry, whenever listed it will list all services
        KIO::UDSEntry kioudsentry;
        kioudsentry.insert(KIO::UDSEntry::UDS_NAME, ".");
        kioudsentry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        kioudsentry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IRWXO);
        kioudsentry.insert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory");
        statEntry(kioudsentry);
        finished();
        return;
    }
    if (!m_kdnssd) {
        m_kdnssd = new KDNSSD();
    }
    if (!m_kdnssd->startBrowse()) {
        error(KIO::ERR_SLAVE_DEFINED, m_kdnssd->errorString());
        return;
    }
    const QString urlfilename = url.fileName();
    foreach (const KDNSSDService &kdnssdservice, m_kdnssd->services()) {
        if (kdnssdservice.name == urlfilename) {
            const QString servicemimetype = mimeForService(kdnssdservice);
            const QString serviceurl = urlForService(kdnssdservice);
            KIO::UDSEntry kioudsentry;
            kioudsentry.insert(KIO::UDSEntry::UDS_NAME, kdnssdservice.name);
            kioudsentry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFLNK);
            kioudsentry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IRWXO);
            kioudsentry.insert(KIO::UDSEntry::UDS_ICON_NAME, iconForService(servicemimetype));
            kioudsentry.insert(KIO::UDSEntry::UDS_MIME_TYPE, servicemimetype);
            // NOTE: UDS_URL is set because KFileItem concats UDS_NAME with itself otherwise
            kioudsentry.insert(KIO::UDSEntry::UDS_URL, serviceurl);
            kioudsentry.insert(KIO::UDSEntry::UDS_TARGET_URL, serviceurl);
            statEntry(kioudsentry);
            finished();
            return;
        }
    }
    error(KIO::ERR_DOES_NOT_EXIST, url.prettyUrl());
}

void NetworkSlave::listDir(const KUrl &url)
{
    if (!KDNSSD::isSupported()) {
        error(KIO::ERR_UNSUPPORTED_ACTION, url.prettyUrl());
        return;
    }
    // this slave has only one directory - the root directory
    const QString urlpath = url.path();
    if (!urlpath.isEmpty() && urlpath != QLatin1String("/")) {
        error(KIO::ERR_DOES_NOT_EXIST, url.prettyUrl());
        return;
    }
    if (!m_kdnssd) {
        m_kdnssd = new KDNSSD();
    }
    if (!m_kdnssd->startBrowse()) {
        error(KIO::ERR_SLAVE_DEFINED, m_kdnssd->errorString());
        return;
    }
    KIO::UDSEntry kioudsentry;
    foreach (const KDNSSDService &kdnssdservice, m_kdnssd->services()) {
        const QString servicemimetype = mimeForService(kdnssdservice);
        const QString serviceurl = urlForService(kdnssdservice);
        kioudsentry.clear();
        kioudsentry.insert(KIO::UDSEntry::UDS_NAME, kdnssdservice.name);
        kioudsentry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFLNK);
        kioudsentry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IRWXO);
        kioudsentry.insert(KIO::UDSEntry::UDS_ICON_NAME, iconForService(servicemimetype));
        kioudsentry.insert(KIO::UDSEntry::UDS_MIME_TYPE, servicemimetype);
        kioudsentry.insert(KIO::UDSEntry::UDS_URL, serviceurl);
        kioudsentry.insert(KIO::UDSEntry::UDS_TARGET_URL, serviceurl);
        listEntry(kioudsentry, false);
    }
    kioudsentry.clear();
    listEntry(kioudsentry, true);
    finished();
}
