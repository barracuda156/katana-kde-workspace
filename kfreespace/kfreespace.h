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

#ifndef KFREESPACE_H
#define KFREESPACE_H

#include <kdiskfreespaceinfo.h>
#include <solid/device.h>
#include <solid/storagevolume.h>
#include <kdebug.h>

static const bool s_kfreespacewatch = true;
static const qulonglong s_kfreespacechecktime = 60; // 1 minute
static const qulonglong s_kfreespacechecktimemin = 1;
static const qulonglong s_kfreespacechecktimemax = 60;
static const qulonglong s_kfreespacefreespace = 0; // either the value from the config or 1/10, fallback is 1024
static const qulonglong s_kfreespacefreespacemin = 10;
static const qulonglong s_kfreespacefreespacemax = 1024;

static qulonglong kCalculateFreeSpace(const Solid::Device &soliddevice, const qulonglong freespace)
{
    const Solid::StorageVolume* solidvolume = soliddevice.as<Solid::StorageVolume>();
    Q_ASSERT(solidvolume);
    const qulonglong totalsize = solidvolume->size();
    if (totalsize <= 0) {
        // if the total size of the device cannot be obtained then the space is the one passed
        // bound to min and max, unless the passed value does not come from config (freespace
        // variable is zero)
        if (freespace <= 0) {
            return 1024;
        }
        return qBound(s_kfreespacefreespacemin, freespace, s_kfreespacefreespacemax);
    }
    const qulonglong autosize = (totalsize / 1024 / 1024 / 10);
    // the case of not being specified, 1/10 of the total space bound to min and max
    if (freespace <= 0) {
        return qBound(s_kfreespacefreespacemin, autosize, s_kfreespacefreespacemax);
    }
    // else it is the one explicitly specified bound to min and max
    return qBound(s_kfreespacefreespacemin, freespace, s_kfreespacefreespacemax);
}

#endif // KFREESPACE_H
