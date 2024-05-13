/*  This file is part of the KDE project
    Copyright (C) 2021 Ivailo Monev <xakepa10@gmail.com>

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

#include "soliduiserver.h"
#include "soliduiserver_common.h"

#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>
#include <kdesktopfileactions.h>
#include <kpassworddialog.h>
#include <kauthorization.h>
#include <knotification.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <ktoolinvocation.h>
#include <kshell.h>
#include <solid/block.h>
#include <solid/predicate.h>
#include <solid/devicenotifier.h>
#include <solid/storagevolume.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/solidnamespace.h>

#include <QDir>


static const QString s_whenadd = QString::fromLatin1("Add");
static const QString s_whenremove = QString::fromLatin1("Remove");

static void kNotifyError(const Solid::ErrorType error, const bool unmount)
{
    const QString title = (unmount ? i18n("Unmount error") : i18n("Mount error"));
    KNotification::event("soliduiserver/mounterror", title, Solid::errorString(error));
}

static SolidUiActions kSolidDeviceActions()
{
    SolidUiActions result;
    const QList<Solid::Device> soliddevices = Solid::Device::allDevices();
    foreach (const Solid::Device &soliddevice, soliddevices) {
        const QStringList solidactions = KGlobal::dirs()->findAllResources("data", "solid/actions/");
        foreach (const QString &solidaction, solidactions) {
            KDesktopFile kdestopfile(solidaction);
            KConfigGroup kconfiggroup = kdestopfile.desktopGroup();
            const QStringList solidwhenlist = kconfiggroup.readEntry("X-KDE-Solid-When", QStringList());
            const QString solidpredicatestring = kconfiggroup.readEntry("X-KDE-Solid-Predicate");
            const Solid::Predicate solidpredicate = Solid::Predicate::fromString(solidpredicatestring);
            if (solidpredicate.matches(soliddevice)) {
                SolidUiAction solidactionstruct;
                solidactionstruct.device = soliddevice;
                solidactionstruct.actions = KDesktopFileActions::userDefinedServices(kdestopfile, true);
                solidactionstruct.when = solidwhenlist;
                const Solid::Block* solidblock = soliddevice.as<Solid::Block>();
                if (solidblock) {
                    solidactionstruct.devicenode = solidblock->device();
                }
                result.append(solidactionstruct);
            }
        }
    }
    return result;
}

K_PLUGIN_FACTORY(SolidUiServerFactory, registerPlugin<SolidUiServer>();)
K_EXPORT_PLUGIN(SolidUiServerFactory("soliduiserver"))

SolidUiServer::SolidUiServer(QObject* parent, const QList<QVariant>&)
    : KDEDModule(parent)
{
    m_solidactions = kSolidDeviceActions();
    connect(
        Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
        this, SLOT(slotDeviceAdded(QString))
    );
    connect(
        Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
        this, SLOT(slotDeviceRemoved(QString))
    );
}

SolidUiServer::~SolidUiServer()
{
    qDeleteAll(m_soliddialogs);
    m_soliddialogs.clear();
}

int SolidUiServer::mountDevice(const QString &device, const QString &mountpoint, bool readonly)
{
    QVariantMap mountarguments;
    mountarguments.insert("device", device);
    mountarguments.insert("mountpoint", mountpoint);
    mountarguments.insert("readonly", readonly);
    int mountreply = KAuthorization::execute(
        "org.kde.soliduiserver.mountunmounthelper", "mount", mountarguments
    );
    // qDebug() << "mount" << mountreply;

    if (mountreply == KAuthorization::NoError) {
        return int(Solid::ErrorType::NoError);
    }
    if (mountreply == KAuthorization::AuthorizationError) {
        kNotifyError(Solid::ErrorType::UnauthorizedOperation, false);
        return int(Solid::ErrorType::UnauthorizedOperation);
    }
    kNotifyError(Solid::ErrorType::OperationFailed, false);
    return int(Solid::ErrorType::OperationFailed);
}

int SolidUiServer::unmountDevice(const QString &mountpoint)
{
    QVariantMap unmountarguments;
    unmountarguments.insert("mountpoint", mountpoint);
    int unmountreply = KAuthorization::execute(
        "org.kde.soliduiserver.mountunmounthelper", "unmount", unmountarguments
    );
    // qDebug() << "unmount" << unmountreply;

    if (unmountreply == KAuthorization::NoError) {
        return int(Solid::ErrorType::NoError);
    }
    if (unmountreply == KAuthorization::AuthorizationError) {
        kNotifyError(Solid::ErrorType::UnauthorizedOperation, true);
        return int(Solid::ErrorType::UnauthorizedOperation);
    }
    kNotifyError(Solid::ErrorType::OperationFailed, true);
    return int(Solid::ErrorType::OperationFailed);
}

int SolidUiServer::mountUdi(const QString &udi)
{
    Solid::Device device(udi);
    Solid::StorageVolume *storagevolume = device.as<Solid::StorageVolume>();
    Solid::Block *block = device.as<Solid::Block>();
    if (!storagevolume || !block) {
        kWarning() << "invalid storage device" << udi;
        kNotifyError(Solid::ErrorType::InvalidOption, false);
        return int(Solid::ErrorType::InvalidOption);
    } else if (storagevolume->fsType() == "zfs_member") {
        // create pool on USB stick, put some bad stuff on it, set mount point to / and watch it
        // unfold when mounted
        kWarning() << "storage device is insecure" << udi;
        kNotifyError(Solid::ErrorType::Insecure, false);
        return int(Solid::ErrorType::Insecure);
    }

    QString devicenode = block->device();
    const QString deviceuuid(storagevolume->uuid());

    bool didcryptopen = false;
#ifdef Q_OS_LINUX
    const QString dmnode = QLatin1String("/dev/mapper/") + deviceuuid;
    if (storagevolume->usage() == Solid::StorageVolume::Encrypted) {
        KPasswordDialog passworddialog(0, KPasswordDialog::NoFlags);

        passworddialog.setPrompt(i18n("'%1' needs a password to be accessed. Please enter a password.", device.description()));
        passworddialog.setPixmap(KIcon(device.icon()).pixmap(64, 64));
        if (!passworddialog.exec()) {
            return int(Solid::ErrorType::UserCanceled);
        }

        QVariantMap cryptopenarguments;
        cryptopenarguments.insert("device", devicenode);
        cryptopenarguments.insert("name", deviceuuid);
        cryptopenarguments.insert("password", passworddialog.password().toLocal8Bit().toHex());
        int cryptopenreply = KAuthorization::execute(
            "org.kde.soliduiserver.mountunmounthelper", "cryptopen", cryptopenarguments
        );
        // qDebug() << "cryptopen" << cryptopenreply;

        if (cryptopenreply == KAuthorization::AuthorizationError) {
            kNotifyError(Solid::ErrorType::UnauthorizedOperation, false);
            return int(Solid::ErrorType::UnauthorizedOperation);
        } else if (cryptopenreply != KAuthorization::NoError) {
            kNotifyError(Solid::ErrorType::OperationFailed, false);
            return int(Solid::ErrorType::OperationFailed);
        }

        didcryptopen = true;
        // NOTE: keep in sync with kdelibs/solid/solid/backends/udev/udevstorageaccess.cpp
        devicenode = dmnode;
    }
#else
#warning crypto storage devices are not supported on this platform
#endif

    // permission denied on /run/mount so.. using base directory that is writable
    const QString mountbase = KGlobal::dirs()->saveLocation("tmp");
    const QString mountpoint = mountbase + QLatin1Char('/') + deviceuuid;
    QDir mountdir(mountbase);
    if (!mountdir.exists(mountpoint) && !mountdir.mkdir(mountpoint)) {
        kWarning() << "could not create" << mountpoint;
        if (didcryptopen) {
            QVariantMap cryptclosearguments;
            cryptclosearguments.insert("name", deviceuuid);
            (void)KAuthorization::execute(
                "org.kde.soliduiserver.mountunmounthelper", "cryptclose", cryptclosearguments
            );
        }
        kNotifyError(Solid::ErrorType::OperationFailed, false);
        return int(Solid::ErrorType::OperationFailed);
    }

    const int mountresult = mountDevice(devicenode, mountpoint);
    if (mountresult != int(Solid::ErrorType::NoError) && didcryptopen) {
        QVariantMap cryptclosearguments;
        cryptclosearguments.insert("name", deviceuuid);
        (void)KAuthorization::execute(
            "org.kde.soliduiserver.mountunmounthelper", "cryptclose", cryptclosearguments
        );
    }
    return mountresult;
}

int SolidUiServer::unmountUdi(const QString &udi)
{
    Solid::Device device(udi);
    Solid::StorageAccess *storageaccess = device.as<Solid::StorageAccess>();
    Solid::StorageVolume *storagevolume = device.as<Solid::StorageVolume>();
    if (!storagevolume || !storageaccess) {
        kWarning() << "invalid storage device" << udi;
        kNotifyError(Solid::ErrorType::InvalidOption, true);
        return int(Solid::ErrorType::InvalidOption);
    }

    bool isremovable = false;
    Solid::StorageDrive *storagedrive = device.as<Solid::StorageDrive>();
    if (storagedrive) {
        isremovable = (storagedrive->isHotpluggable() || storagedrive->isRemovable());
    }

    const int unmountresult = unmountDevice(storageaccess->filePath());
    if (unmountresult != int(Solid::ErrorType::NoError)) {
        return unmountresult;
    }

    if (storagevolume->usage() == Solid::StorageVolume::Encrypted) {
        QVariantMap cryptclosearguments;
        cryptclosearguments.insert("name", storagevolume->uuid());
        int cryptclosereply = KAuthorization::execute(
            "org.kde.soliduiserver.mountunmounthelper", "cryptclose", cryptclosearguments
        );
        // qDebug() << "cryptclose" << cryptclosereply;

        if (cryptclosereply == KAuthorization::AuthorizationError) {
            kNotifyError(Solid::ErrorType::UnauthorizedOperation, true);
            return int(Solid::ErrorType::UnauthorizedOperation);
        } else if (cryptclosereply != KAuthorization::NoError) {
            kNotifyError(Solid::ErrorType::OperationFailed, true);
            return int(Solid::ErrorType::OperationFailed);
        }
    }

    if (isremovable) {
        KNotification::event("soliduiserver/safetoremove");
    }
    return int(Solid::ErrorType::NoError);
}

QString SolidUiServer::errorString(const int error)
{
    return Solid::errorString(static_cast<Solid::ErrorType>(error));
}

void SolidUiServer::handleActions(const Solid::Device &soliddevice, const bool added)
{
    SolidUiAction solidaction;
    solidaction.device = soliddevice;
    foreach (const SolidUiAction &solidactionstruct, m_solidactions) {
        if (soliddevice.udi() != solidactionstruct.device.udi()) {
            continue;
        }
        const QStringList solidwhenlist = solidactionstruct.when;
        if (!solidwhenlist.contains(added ? s_whenadd : s_whenremove)) {
            continue;
        }

        solidaction.actions.append(solidactionstruct.actions);
        // if the UDI is different so should be the device node
        solidaction.devicenode = solidactionstruct.devicenode;
    }
    if (solidaction.actions.size() == 1) {
        kExecuteAction(solidaction.actions.first(), soliddevice, solidaction.devicenode, added);
    } else if (!solidaction.actions.isEmpty()) {
        SolidUiDialog* soliddialog = new SolidUiDialog(solidaction, added);
        connect(soliddialog, SIGNAL(finished(int)), this, SLOT(slotDialogFinished()));
        m_soliddialogs.append(soliddialog);
        soliddialog->show();
    }
}


void SolidUiServer::slotDeviceAdded(const QString &udi)
{
    m_solidactions = kSolidDeviceActions();
    Solid::Device soliddevice(udi);
    handleActions(soliddevice, true);
}

void SolidUiServer::slotDeviceRemoved(const QString &udi)
{
    QMutableListIterator<SolidUiAction> iter(m_solidactions);
    while (iter.hasNext()) {
        SolidUiAction solidactionstruct = iter.next();
        Solid::Device soliddevice = solidactionstruct.device;
        if (soliddevice.udi() == udi) {
            handleActions(soliddevice, false);
            iter.remove();
        }
    }
}

void SolidUiServer::slotDialogFinished()
{
    SolidUiDialog* soliddialog = qobject_cast<SolidUiDialog*>(sender());
    m_soliddialogs.removeAll(soliddialog);
    soliddialog->deleteLater();
}

#include "moc_soliduiserver.cpp"
