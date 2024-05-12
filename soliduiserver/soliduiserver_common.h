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

#ifndef SOLIDUISERVER_COMMON_H
#define SOLIDUISERVER_COMMON_H

#include <kdebug.h>
#include <kserviceaction.h>
#include <kshell.h>
#include <ktoolinvocation.h>
#include <solid/device.h>
#include <solid/block.h>
#include <solid/storageaccess.h>
#include <solid/opticaldrive.h>

// TODO: store and update Solid::StorageAccess::filePath() reference
struct SolidUiAction
{
    Solid::Device device;
    QString devicenode;
    QList<KServiceAction> actions;
    QStringList when;
};
typedef QList<SolidUiAction> SolidUiActions;

static void kSolidMountUDI(const QString &solidudi)
{
    Solid::Device soliddevice(solidudi);
    Solid::StorageAccess* solidstorageacces = soliddevice.as<Solid::StorageAccess>();
    if (!solidstorageacces) {
        kWarning() << "not storage access" << solidudi;
        return;
    }
    solidstorageacces->setup();
}

static void kSolidUnmountUDI(const QString &solidudi)
{
    Solid::Device soliddevice(solidudi);
    Solid::StorageAccess* solidstorageacces = soliddevice.as<Solid::StorageAccess>();
    if (!solidstorageacces) {
        kWarning() << "not storage access" << solidudi;
        return;
    }
    solidstorageacces->teardown();
}

static void kSolidEjectUDI(const QString &solidudi)
{
    Solid::Device soliddevice(solidudi);
    Solid::OpticalDrive *solidopticaldrive = soliddevice.as<Solid::OpticalDrive>();
    if (!solidopticaldrive) {
        kWarning() << "not optical drive" << solidudi;
        return;
    }
    solidopticaldrive->eject();
}

// simplified version of KMacroExpander specialized for solid actions
static QStringList kSolidActionCommand(const QString &command, const Solid::Device &soliddevice,
                                       const QString &solidnode, const bool mount)
{
    const Solid::StorageAccess* solidstorageacces = soliddevice.as<Solid::StorageAccess>();
    if (mount && solidstorageacces && !solidstorageacces->isAccessible()) {
        kSolidMountUDI(soliddevice.udi());
    }

    QString actioncommand = command;
    if (actioncommand.contains(QLatin1String("%f")) || actioncommand.contains(QLatin1String("%F"))) {
        if (!solidstorageacces) {
            kWarning() << "device is not storage access" << soliddevice.udi();
        } else {
            const QString devicefilepath = solidstorageacces->filePath();
            actioncommand = actioncommand.replace(QLatin1String("%f"), devicefilepath);
            actioncommand = actioncommand.replace(QLatin1String("%F"), devicefilepath);
        }
    }
    if (actioncommand.contains(QLatin1String("%d")) || actioncommand.contains(QLatin1String("%D"))) {
        actioncommand = actioncommand.replace(QLatin1String("%d"), solidnode);
        actioncommand = actioncommand.replace(QLatin1String("%D"), solidnode);
    }
    if (actioncommand.contains(QLatin1String("%i")) || actioncommand.contains(QLatin1String("%I"))) {
        const QString deviceudi = soliddevice.udi();
        actioncommand = actioncommand.replace(QLatin1String("%i"), deviceudi);
        actioncommand = actioncommand.replace(QLatin1String("%I"), deviceudi);
    }
    return KShell::splitArgs(actioncommand);
}

static void kExecuteAction(const KServiceAction &kserviceaction, const Solid::Device &soliddevice,
                           const QString &solidnode, const bool mount)
{
    QStringList actioncommand = kSolidActionCommand(kserviceaction.exec(), soliddevice, solidnode, mount);
    if (actioncommand.size() == 0) {
        kWarning() << "invalid action command" << kserviceaction.name();
        return;
    }
    const QString actionexe = actioncommand.takeFirst();
    const bool actionresult = KToolInvocation::self()->startProgram(actionexe, actioncommand);
    if (!actionresult) {
        kWarning() << "could not execute action for" << kserviceaction.name();
    }
}

#endif // SOLIDUISERVER_COMMON_H