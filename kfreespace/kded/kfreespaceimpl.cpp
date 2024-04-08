/*  This file is part of the KDE project
    Copyright (C) 2023 Ivailo Monev <xakepa10@gmail.com>

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

#include "kfreespaceimpl.h"
#include "kfreespace.h"

#include <QDir>
#include <klocale.h>
#include <knotification.h>
#include <kdebug.h>
#include <solid/storageaccess.h>

KFreeSpaceImpl::KFreeSpaceImpl(QObject *parent)
    : QObject(parent),
    m_checktime(s_kfreespacechecktime),
    m_freespace(s_kfreespacefreespace),
    m_timerid(0)
{
}

KFreeSpaceImpl::~KFreeSpaceImpl()
{
    if (m_timerid > 0) {
        killTimer(m_timerid);
        m_timerid = 0;
    }
}

bool KFreeSpaceImpl::watch(const Solid::Device &soliddevice,
                           const qulonglong checktime, const qulonglong freespace)
{
    // qDebug() << Q_FUNC_INFO << soliddevice.udi() << checktime << freespace;
    m_soliddevice = soliddevice;
    // NOTE: time from config is in seconds, has to be in ms here
    m_checktime = (qBound(s_kfreespacechecktimemin, checktime, s_kfreespacechecktimemax) * 1000);
    // NOTE: size from config is in MB, has to be in bytes here
    m_freespace = (kCalculateFreeSpace(soliddevice, freespace) * 1024 * 1024);
    m_timerid = startTimer(m_checktime);
    kDebug() << "Checking" << m_soliddevice.udi()
             << "every" << (m_checktime / 1000)
             << "if space is less or equal to" << KGlobal::locale()->formatByteSize(m_freespace);
    return true;
}

void KFreeSpaceImpl::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerid) {
        event->accept();

        const Solid::StorageAccess* solidaccess = m_soliddevice.as<Solid::StorageAccess>();
        Q_ASSERT(solidaccess);
        const QString mountpoint = solidaccess->filePath();
        if (mountpoint.isEmpty()) {
            kDebug() << "Device not mounted" << m_soliddevice.udi();
            return;
        }

        const KDiskFreeSpaceInfo kdiskinfo = KDiskFreeSpaceInfo::freeSpaceInfo(mountpoint);
        if (!kdiskinfo.isValid()) {
            kDebug() << "Disk info is not valid for" << mountpoint;
            return;
        }

        const qulonglong freespace = kdiskinfo.available();
        const QString freespacestring = KGlobal::locale()->formatByteSize(freespace);
        kDebug() << "Current" << m_soliddevice.udi()
                 << "space is" << freespacestring;
        if (freespace <= m_freespace) {
            KNotification::event(
                "kfreespace/WatchLow",
                QString(),
                i18n("%1 has %2 free space", m_soliddevice.description(), freespacestring)
            );
        }
    } else {
        event->ignore();
    }
}

#include "moc_kfreespaceimpl.cpp"
