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

#include "solidrunner.h"
#include "soliduiserver_common.h"

#include <KIcon>
#include <KToolInvocation>
#include <KShell>
#include <KStandardDirs>
#include <KDesktopFile>
#include <kdesktopfileactions.h>
#include <KDebug>
#include <Solid/Predicate>
#include <Solid/StorageVolume>
#include <Solid/StorageDrive>
#include <Solid/StorageAccess>
#include <Solid/OpticalDrive>
#include <Solid/Block>

static const QChar s_actionidseparator = QChar::fromLatin1('#');
static const QString s_whensearch = QString::fromLatin1("Search");

static QString kSolidUDI(const QString &matchid)
{
    // I did not add that to the match ID but it is there
    if (matchid.startsWith(QLatin1String("solid_"))) {
        return matchid.mid(6, matchid.size() - 6);
    }
    return matchid;
}

SolidRunner::SolidRunner(QObject *parent, const QVariantList &args)
    : AbstractRunner(parent, args),
    // TODO: option for it?
    m_onlyremovable(true)
{
    setObjectName(QLatin1String("Solid"));

    addSyntax(Plasma::RunnerSyntax(":q:", i18n("Finds devices whose name match :q:")));

    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("Note this is a KRunner keyword", "device"),
            i18n("Lists all devices and allows them to be mounted, unmounted or ejected.")
        )
    );
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("Note this is a KRunner keyword", "mount"),
            i18n("Lists all devices which can be mounted, and allows them to be mounted.")
        )
    );
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("Note this is a KRunner keyword", "unlock"),
            i18n("Lists all encrypted devices which can be unlocked, and allows them to be unlocked.")
        )
    );
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("Note this is a KRunner keyword", "unmount"),
            i18n("Lists all devices which can be unmounted, and allows them to be unmounted.")
        )
    );
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("Note this is a KRunner keyword", "lock"),
            i18n("Lists all encrypted devices which can be locked, and allows them to be locked.")
        )
    );
    addSyntax(
        Plasma::RunnerSyntax(
            i18nc("Note this is a KRunner keyword", "eject"),
            i18n("Lists all devices which can be ejected, and allows them to be ejected.")
        )
    );
}

QList<QAction*> SolidRunner::actionsForMatch(const Plasma::QueryMatch &match)
{
    QList<QAction*> result;
    const Solid::Device soliddevice(kSolidUDI(match.id()));
    const QStringList solidactions = KGlobal::dirs()->findAllResources("data", "solid/actions/");
    foreach (const QString &solidaction, solidactions) {
        KDesktopFile kdestopfile(solidaction);
        KConfigGroup kconfiggroup = kdestopfile.desktopGroup();
        const QStringList solidwhenlist = kconfiggroup.readEntry("X-KDE-Solid-When", QStringList());
        if (!solidwhenlist.contains(s_whensearch)) {
            continue;
        }

        const QString solidpredicatestring = kconfiggroup.readEntry("X-KDE-Solid-Predicate");
        const Solid::Predicate solidpredicate = Solid::Predicate::fromString(solidpredicatestring);
        if (solidpredicate.matches(soliddevice)) {
            const QList<KServiceAction> kserviceactions = KDesktopFileActions::userDefinedServices(solidaction, true);
            foreach (const KServiceAction &kserviceaction, kserviceactions) {
                const QString actionname = kserviceaction.name();
                if (actionname.contains(s_actionidseparator)) {
                    kWarning() << "action name contains separator" << s_actionidseparator << actionname;
                    continue;
                }
                QString actionid = actionname;
                actionid.append(s_actionidseparator);
                actionid.append(solidaction);
                QAction* matchaction = addAction(actionid, KIcon(kserviceaction.icon()), kserviceaction.text());
                matchaction->setData(actionid);
                result.append(matchaction);
            }
        }
    }
    return result;
}

void SolidRunner::match(Plasma::RunnerContext &context)
{
    if (!context.isValid()) {
        return;
    }

    const QString term = context.query();
    if (term.startsWith(i18nc("Note this is a KRunner keyword", "device"), Qt::CaseInsensitive) ||
        term.startsWith(QLatin1String("device"), Qt::CaseInsensitive)) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchDevice);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchDevice);
        }
    } else if (term.startsWith(i18nc("Note this is a KRunner keyword", "mount"), Qt::CaseInsensitive) ||
        term.startsWith(QLatin1String("mount"), Qt::CaseInsensitive)) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchMount);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
            if (!solidstorageaccess || solidstorageaccess->isAccessible()) {
                continue;
            }
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchMount);
        }
    } else if (term.startsWith(i18nc("Note this is a KRunner keyword", "unmount"), Qt::CaseInsensitive) ||
        term.startsWith(QLatin1String("unmount"), Qt::CaseInsensitive)) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchUnmount);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
            if (!solidstorageaccess || !solidstorageaccess->isAccessible()) {
                continue;
            }
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchUnmount);
        }
    } else if (term.startsWith(i18nc("Note this is a KRunner keyword", "eject"), Qt::CaseInsensitive) ||
        term.startsWith(QLatin1String("eject"), Qt::CaseInsensitive)) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchEject);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            const Solid::OpticalDrive *solidopticaldrive = soliddevice.as<Solid::OpticalDrive>();
            if (!solidopticaldrive) {
                continue;
            }
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchEject);
        }
    } else if (term.startsWith(i18nc("Note this is a KRunner keyword", "unlock"), Qt::CaseInsensitive) ||
        term.startsWith(QLatin1String("unlock"), Qt::CaseInsensitive)) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchUnlock);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
            if (!solidstorageaccess || !solidstorageaccess->isAccessible()) {
                continue;
            }
            const Solid::StorageVolume *solidstoragevolume = soliddevice.as<Solid::StorageVolume>();
            if (!solidstoragevolume || solidstoragevolume->usage() != Solid::StorageVolume::Encrypted) {
                continue;
            }
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchUnlock);
        }
    } else if (term.startsWith(i18nc("Note this is a KRunner keyword", "lock"), Qt::CaseInsensitive) ||
        term.startsWith(QLatin1String("lock"), Qt::CaseInsensitive)) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchLock);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
            if (!solidstorageaccess || solidstorageaccess->isAccessible()) {
                continue;
            }
            const Solid::StorageVolume *solidstoragevolume = soliddevice.as<Solid::StorageVolume>();
            if (!solidstoragevolume || solidstoragevolume->usage() != Solid::StorageVolume::Encrypted) {
                continue;
            }
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchLock);
        }
    } else if (term.size() >= 3) {
        const QList<Solid::Device> soliddevices = solidDevices(term, SolidRunner::MatchAny);
        foreach (const Solid::Device &soliddevice, soliddevices) {
            addDeviceMatch(term, context, soliddevice, SolidRunner::MatchAny);
        }
    }
}

void SolidRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    QAction* action = match.selectedAction();
    if (action) {
        const QString actionid = action->data().toString();
        const int actionseparatorindex = actionid.indexOf(s_actionidseparator);
        if (actionseparatorindex <= 0) {
            kWarning() << "invalid solid runner action ID" << actionid;
            return;
        }
        const QString actionname = actionid.mid(0, actionseparatorindex);
        const QString actionfilepath = actionid.mid(actionseparatorindex + 1, actionid.size() - actionseparatorindex - 1);
        const QList<KServiceAction> kserviceactions = KDesktopFileActions::userDefinedServices(actionfilepath, true);
        foreach (const KServiceAction &kserviceaction, kserviceactions) {
            if (kserviceaction.name() == actionname) {
                const Solid::Device soliddevice(kSolidUDI(match.id()));
                const Solid::Block *solidblock = soliddevice.as<Solid::Block>();
                QStringList actioncommand = kSolidActionCommand(
                    kserviceaction.exec(), soliddevice,
                    solidblock ? solidblock->device() : QString(),
                    true
                );
                if (actioncommand.size() == 0) {
                    kWarning() << "invalid action command" << actionname << "in" << actionfilepath;
                    return;
                }
                const QString actionexe = actioncommand.takeFirst();
                const int actionresult = KToolInvocation::kdeinitExec(actionexe, actioncommand);
                if (actionresult != 0) {
                    kWarning() << "could not execute action for" << actionname << "in" << actionfilepath << actionresult;
                }
                return;
            }
        }
        kWarning() << "could not find action for" << actionname << "in" << actionfilepath;
        return;
    }
    const int matchtype = static_cast<int>(match.data().toInt());
    switch (matchtype) {
        case SolidRunner::MatchAny:
        case SolidRunner::MatchDevice: {
            const QString solidudi = kSolidUDI(match.id());
            Solid::Device soliddevice(solidudi);
            const Solid::OpticalDrive *solidopticaldrive = soliddevice.as<Solid::OpticalDrive>();
            const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
            if (solidopticaldrive) {
                kSolidEjectUDI(solidudi);
            } else if (solidstorageaccess) {
                if (solidstorageaccess->isAccessible()) {
                    kSolidUnmountUDI(solidudi);
                } else {
                    kSolidMountUDI(solidudi);
                }
            } else {
                kWarning() << "not optical drive and not storage access" << solidudi;
            }
            break;
        }
        case SolidRunner::MatchMount: {
            kSolidMountUDI(kSolidUDI(match.id()));
            break;
        }
        case SolidRunner::MatchUnmount: {
            kSolidUnmountUDI(kSolidUDI(match.id()));
            break;
        }
        case SolidRunner::MatchEject: {
            kSolidEjectUDI(kSolidUDI(match.id()));
            break;
        }
        case SolidRunner::MatchUnlock: {
            // same as mounting
            kSolidMountUDI(kSolidUDI(match.id()));
            break;
        }
        case SolidRunner::MatchLock: {
            // same as unmounting
            kSolidUnmountUDI(kSolidUDI(match.id()));
            break;
        }
        default: {
            kWarning() << "invalid match type" << matchtype;
            break;
        }
    }
}

QList<Solid::Device> SolidRunner::solidDevices(const QString &term, const SolidMatchType solidmatchtype) const
{
    QList<Solid::Device> result;
    // filter duplicates that are Solid::StorageVolume-castable (e.g. Solid::OpticalDrive)
    QStringList uniqueudis;
    Solid::Predicate solidpredicate(Solid::DeviceInterface::StorageVolume);
    solidpredicate |= Solid::Predicate(Solid::DeviceInterface::OpticalDrive);
    const QList<Solid::Device> soliddevices = Solid::Device::listFromQuery(solidpredicate);
    foreach (const Solid::Device &soliddevice, soliddevices) {
        if (uniqueudis.contains(soliddevice.udi())) {
            continue;
        }
        uniqueudis.append(soliddevice.udi());
        const Solid::StorageVolume *solidstoragevolume = soliddevice.as<Solid::StorageVolume>();
        if (!solidstoragevolume || solidstoragevolume->isIgnored()) {
            continue;
        }
        const Solid::OpticalDrive *solidopticaldrive = soliddevice.as<Solid::OpticalDrive>();
        // optical drives are not removable storage
        if (!solidopticaldrive && m_onlyremovable) {
            const Solid::StorageDrive *solidstoragedrive = soliddevice.as<Solid::StorageDrive>();
            if (!solidstoragedrive || !solidstoragedrive->isRemovable()) {
                continue;
            }
        }
        if (solidmatchtype == SolidRunner::MatchAny) {
            if (!soliddevice.description().contains(term, Qt::CaseInsensitive)) {
                continue;
            }
        } else {
            const int indexofspace = term.indexOf(QLatin1Char(' '));
            if (indexofspace > 0) {
                const QString termsearch = term.mid(indexofspace + 1, term.size() - indexofspace - 1);
                if (!soliddevice.description().contains(termsearch, Qt::CaseInsensitive)) {
                    continue;
                }
            }
        }
        result.append(soliddevice);
    }
    return result;
}

void SolidRunner::addDeviceMatch(const QString &term, Plasma::RunnerContext &context,
                                 const Solid::Device &soliddevice, const SolidMatchType solidmatchtype)
{
    Plasma::QueryMatch match(this);
    match.setType(Plasma::QueryMatch::PossibleMatch);
    match.setId(soliddevice.udi());
    match.setData(static_cast<int>(solidmatchtype));
    match.setIcon(KIcon(soliddevice.icon()));
    match.setText(soliddevice.description());
    const Solid::OpticalDrive *solidopticaldrive = soliddevice.as<Solid::OpticalDrive>();
    const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
    if (solidopticaldrive) {
        match.setSubtext(i18n("Eject medium"));
    } else if (solidstorageaccess) {
        const Solid::StorageVolume *solidstoragevolume = soliddevice.as<Solid::StorageVolume>();
        const bool encrypted = (solidstoragevolume && solidstoragevolume->usage() == Solid::StorageVolume::Encrypted);
        if (solidstorageaccess->isAccessible()) {
            if (encrypted) {
                match.setSubtext(
                    i18nc("Close the encrypted container; partitions inside will disappear as they had been unplugged", "Lock the container")
                );
            } else {
                match.setSubtext(i18n("Unmount the device"));
            }
        } else {
            if (encrypted) {
                match.setSubtext(
                    i18nc("Unlock the encrypted container; will ask for a password; partitions inside will appear as they had been plugged in","Unlock the container")
                );
            } else {
                match.setSubtext(i18n("Mount the device"));
            }
        }
    }
    context.addMatch(term, match);
}

#include "moc_solidrunner.cpp"
