/*
    This file is part of the network kioslave, part of the KDE project.

    Copyright 2009 Friedrich W. H. Kossebau <kossebau@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "networkslave.h"

#include <KMimeType>
#include <KDebug>

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
        kioudsentry.insert(KIO::UDSEntry::UDS_NAME, "root");
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
    foreach (const KDNSSDService &kdnssdservice, m_kdnssd->services()) {
        // qDebug() << Q_FUNC_INFO << kdnssdservice.url << url.prettyUrl();
        if (kdnssdservice.url == url.prettyUrl()) {
            const QString servicemimetype = mimeForService(kdnssdservice);
            KIO::UDSEntry kioudsentry;
            kioudsentry.insert(KIO::UDSEntry::UDS_NAME, kdnssdservice.name);
            kioudsentry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFLNK);
            kioudsentry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IRWXO);
            kioudsentry.insert(KIO::UDSEntry::UDS_ICON_NAME, iconForService(servicemimetype));
            kioudsentry.insert(KIO::UDSEntry::UDS_MIME_TYPE, servicemimetype);
            kioudsentry.insert(KIO::UDSEntry::UDS_TARGET_URL, urlForService(kdnssdservice));
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
        kioudsentry.clear();
        kioudsentry.insert(KIO::UDSEntry::UDS_NAME, kdnssdservice.name);
        kioudsentry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFLNK);
        kioudsentry.insert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU | S_IRWXG | S_IRWXO);
        kioudsentry.insert(KIO::UDSEntry::UDS_ICON_NAME, iconForService(servicemimetype));
        kioudsentry.insert(KIO::UDSEntry::UDS_MIME_TYPE, servicemimetype);
        kioudsentry.insert(KIO::UDSEntry::UDS_TARGET_URL, urlForService(kdnssdservice));
        listEntry(kioudsentry, false);
    }
    kioudsentry.clear();
    listEntry(kioudsentry, true);
    finished();
}
