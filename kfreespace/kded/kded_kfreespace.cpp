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

#include "kded_kfreespace.h"
#include "kfreespace.h"

#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>
#include <knotification.h>
#include <kpluginfactory.h>
#include <kdebug.h>
#include <solid/device.h>
#include <solid/storageaccess.h>
#include <solid/devicenotifier.h>

K_PLUGIN_FACTORY(KFreeSpaceModuleFactory, registerPlugin<KFreeSpaceModule>();)
K_EXPORT_PLUGIN(KFreeSpaceModuleFactory("kfreespace"))

KFreeSpaceModule::KFreeSpaceModule(QObject *parent, const QList<QVariant> &args)
    : KDEDModule(parent),
    m_dirwatch(nullptr)
{
    Q_UNUSED(args);

    slotInit();

    m_dirwatch = new KDirWatch(this);
    m_dirwatch->setInterval(5000);
    const QString kfreespacercfile = KStandardDirs::locateLocal("config", "kfreespacerc");
    m_dirwatch->addFile(kfreespacercfile);
    connect(m_dirwatch, SIGNAL(dirty(QString)), this, SLOT(slotInit()));

    Solid::DeviceNotifier* solidnotifier = Solid::DeviceNotifier::instance();
    connect(
        solidnotifier, SIGNAL(deviceAdded(QString)),
        this, SLOT(slotDeviceAdded(QString))
    );
    connect(
        solidnotifier, SIGNAL(deviceRemoved(QString)),
        this, SLOT(slotDeviceRemoved(QString))
    );
}

KFreeSpaceModule::~KFreeSpaceModule()
{
    qDeleteAll(m_freespaces);
    m_freespaces.clear();
}

void KFreeSpaceModule::slotInit()
{
    kDebug() << "Initializing";

    qDeleteAll(m_freespaces);
    m_freespaces.clear();

    KConfig kfreespaceconfig("kfreespacerc", KConfig::SimpleConfig);
    bool watcherror = false;
    const QList<Solid::Device> storagedevices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    foreach (const Solid::Device &soliddevice, storagedevices) {
        const Solid::StorageAccess* solidaccess = soliddevice.as<Solid::StorageAccess>();
        if (!solidaccess) {
            continue;
        } else if (solidaccess->isIgnored()) {
            kDebug() << "Ignored" << soliddevice.udi();
            continue;
        }

        // qDebug() << Q_FUNC_INFO << soliddevice.udi();
        KConfigGroup kfreespacegroup = kfreespaceconfig.group(soliddevice.udi());
        const bool kfreespacewatch = kfreespacegroup.readEntry("watch", s_kfreespacewatch);
        if (!kfreespacewatch) {
            kDebug() << "Not watching" << soliddevice.udi();
            continue;
        }

        const qulonglong kfreespacechecktime = kfreespacegroup.readEntry("checktime", s_kfreespacechecktime);
        const qulonglong kfreespacefreespace = kfreespacegroup.readEntry("freespace", s_kfreespacefreespace);
        KFreeSpaceImpl* kfreespaceimpl = new KFreeSpaceImpl(this);
        const bool kfreespacestatus = kfreespaceimpl->watch(
            soliddevice,
            kfreespacechecktime, kCalculateFreeSpace(soliddevice, kfreespacefreespace)
        );
        if (!kfreespacestatus) {
            delete kfreespaceimpl;
            watcherror = true;
        } else {
            m_freespaces.append(kfreespaceimpl);
        }
    }
    if (watcherror) {
        KNotification::event("kfreespace/WatchError");
    }
}

void KFreeSpaceModule::slotDeviceAdded(const QString &udi)
{
    Solid::Device soliddevice(udi);
    const Solid::StorageAccess* solidaccess = soliddevice.as<Solid::StorageAccess>();
    if (solidaccess) {
        kDebug() << "Storage access added" << udi;
        slotInit();
    }
}

void KFreeSpaceModule::slotDeviceRemoved(const QString &udi)
{
    // NOTE: at that point obtaining valid device is impossible
    kDebug() << "Solid device removed" << udi;
    slotInit();
}
