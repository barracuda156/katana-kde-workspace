/*
 *   Copyright (C) 2007 Christopher Blauvelt <cblauvelt@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "soliddeviceengine.h"
#include "soliddeviceservice.h"

#include <QtCore/qmetaobject.h>
#include <QDateTime>

#include <KDebug>
#include <KDiskFreeSpaceInfo>
#include <KLocale>

#include <Plasma/DataContainer>

SolidDeviceEngine::SolidDeviceEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args),
        m_notifier(0)
{
    Q_UNUSED(args)
    m_signalmanager = new DeviceSignalMapManager(this);

    listenForNewDevices();
    setMinimumPollingInterval(1000);
    connect(this, SIGNAL(sourceRemoved(QString)),
            this, SLOT(sourceWasRemoved(QString)));
}

SolidDeviceEngine::~SolidDeviceEngine()
{
}

Plasma::Service* SolidDeviceEngine::serviceForSource(const QString& source)
{
    return new SolidDeviceService (this, source);
}

void SolidDeviceEngine::listenForNewDevices()
{
    if (m_notifier) {
        return;
    }

    //detect when new devices are added
    m_notifier = Solid::DeviceNotifier::instance();
    connect(m_notifier, SIGNAL(deviceAdded(QString)),
            this, SLOT(deviceAdded(QString)));
    connect(m_notifier, SIGNAL(deviceRemoved(QString)),
            this, SLOT(deviceRemoved(QString)));
    connect(m_notifier, SIGNAL(contentChanged(QString,bool)),
            this, SLOT(deviceContentChanged(QString,bool)));
}

bool SolidDeviceEngine::sourceRequestEvent(const QString &name)
{
    if (name.startsWith('/')) {
        Solid::Device device = Solid::Device(name);
        if (device.isValid()) {
            if (m_devicemap.contains(name) ) {
                return true;
            } else {
                m_devicemap[name] = device;
                return populateDeviceData(name);
            }
        }
    } else {
        Solid::Predicate predicate = Solid::Predicate::fromString(name);
        if (predicate.isValid()  && !m_predicatemap.contains(name)) {
            foreach (const Solid::Device &device, Solid::Device::listFromQuery(predicate)) {
                m_predicatemap[name] << device.udi();
            }

            setData(name, m_predicatemap[name]);
            return true;
        }
    }

    kDebug() << "Source is not a predicate or a device.";
    return false;
}

void SolidDeviceEngine::sourceWasRemoved(const QString &source)
{
    m_devicemap.remove(source);
    m_predicatemap.remove(source);
}

bool SolidDeviceEngine::populateDeviceData(const QString &name)
{
    Solid::Device device = m_devicemap.value(name);
    if (!device.isValid()) {
        return false;
    }

    QStringList devicetypes;
    setData(name, I18N_NOOP("Parent UDI"), device.parentUdi());
    setData(name, I18N_NOOP("Vendor"), device.vendor());
    setData(name, I18N_NOOP("Product"), device.product());
    setData(name, I18N_NOOP("Description"), device.description());
    setData(name, I18N_NOOP("Icon"), device.icon());
    setData(name, I18N_NOOP("Emblems"), device.emblems());
    setData(name, I18N_NOOP("State"), Idle);
    setData(name, I18N_NOOP("Operation result"), Working);
    setData(name, I18N_NOOP("Timestamp"), QDateTime::currentDateTime());

    if (device.is<Solid::Processor>()) {
        Solid::Processor *processor = device.as<Solid::Processor>();
        if (!processor) {
            return false;
        }

        devicetypes << I18N_NOOP("Processor");
        setData(name, I18N_NOOP("Number"), processor->number());
        setData(name, I18N_NOOP("Min Speed"), processor->minSpeed());
        setData(name, I18N_NOOP("Max Speed"), processor->maxSpeed());
        setData(name, I18N_NOOP("Can Change Frequency"), processor->canChangeFrequency());
    }
    if (device.is<Solid::Block>()) {
        Solid::Block *block = device.as<Solid::Block>();
        if (!block) {
            return false;
        }

        devicetypes << I18N_NOOP("Block");
        setData(name, I18N_NOOP("Major"), block->deviceMajor());
        setData(name, I18N_NOOP("Minor"), block->deviceMajor());
        setData(name, I18N_NOOP("Device"), block->device());
    }
    if (device.is<Solid::StorageAccess>()) {
        Solid::StorageAccess *storageaccess = device.as<Solid::StorageAccess>();
        if (!storageaccess) {
            return false;
        }

        devicetypes << I18N_NOOP("Storage Access");
        setData(name, I18N_NOOP("Accessible"), storageaccess->isAccessible());
        setData(name, I18N_NOOP("File Path"), storageaccess->filePath());

        if (storageaccess->isAccessible()) {
            QVariant freeDiskVar;
            qulonglong freeDisk = freeDiskSpace(storageaccess->filePath());
            if ( freeDisk != (qulonglong)-1 ) {
                freeDiskVar.setValue( freeDisk );
            }
            if (!device.is<Solid::OpticalDisc>()) {
                setData(name, I18N_NOOP("Free Space"), freeDiskVar );
                setData(name, I18N_NOOP("Free Space Text"), KGlobal::locale()->formatByteSize(freeDisk));
            }
        }

        m_signalmanager->mapDevice(storageaccess, device.udi());
    }

    if (device.is<Solid::StorageDrive>()) {
        Solid::StorageDrive *storagedrive = device.as<Solid::StorageDrive>();
        if (!storagedrive) {
            return false;
        }

        devicetypes << I18N_NOOP("Storage Drive");

        QStringList bus;
        bus << I18N_NOOP("Ide") << I18N_NOOP("Usb") << I18N_NOOP("Ieee1394")
                 << I18N_NOOP("Scsi") << I18N_NOOP("Sata") << I18N_NOOP("Platform");
        QStringList drivetype;
        drivetype << I18N_NOOP("Hard Disk") <<  I18N_NOOP("Cdrom Drive") <<  I18N_NOOP("Floppy")
                 << I18N_NOOP("Tape") << I18N_NOOP("Compact Flash") << I18N_NOOP("Memory Stick")
                 << I18N_NOOP("Smart Media") <<  I18N_NOOP("SdMmc");

        setData(name, I18N_NOOP("Bus"), bus.at((int)storagedrive->bus()));
        setData(name, I18N_NOOP("Drive Type"), drivetype.at((int)storagedrive->driveType()));
        setData(name, I18N_NOOP("Removable"), storagedrive->isRemovable());
        setData(name, I18N_NOOP("Hotpluggable"), storagedrive->isHotpluggable());
    }
    else {
        bool isRemovable = false;
        bool isHotpluggable = false;
        Solid::StorageDrive *drive = device.as<Solid::StorageDrive>();
        if (drive) {
            //remove check for isHotpluggable() when plasmoids are changed to check for both properties
            isRemovable = (drive->isRemovable() || drive->isHotpluggable());
            isHotpluggable = drive->isHotpluggable();
        }
        setData(name, I18N_NOOP("Removable"), isRemovable);
        setData(name, I18N_NOOP("Hotpluggable"), isHotpluggable);
    }


    if (device.is<Solid::OpticalDrive>()) {
        Solid::OpticalDrive *opticaldrive = device.as<Solid::OpticalDrive>();
        if (!opticaldrive) {
            return false;
        }

        devicetypes << I18N_NOOP("Optical Drive");

        QStringList supportedtypes;
        Solid::OpticalDrive::MediumTypes mediatypes = opticaldrive->supportedMedia();
        if (mediatypes & Solid::OpticalDrive::Cdr) {
            supportedtypes << I18N_NOOP("CD-R");
        }
        if (mediatypes & Solid::OpticalDrive::Cdrw) {
            supportedtypes << I18N_NOOP("CD-RW");
        }
        if (mediatypes & Solid::OpticalDrive::Dvd) {
            supportedtypes << I18N_NOOP("DVD");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdr) {
            supportedtypes << I18N_NOOP("DVD-R");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdrw) {
            supportedtypes << I18N_NOOP("DVD-RW");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdram) {
            supportedtypes << I18N_NOOP("DVD-RAM");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdplusr) {
            supportedtypes << I18N_NOOP("DVD+R");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdplusrw) {
            supportedtypes << I18N_NOOP("DVD+RW");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdplusdl) {
            supportedtypes << I18N_NOOP("DVD+DL");
        }
        if (mediatypes & Solid::OpticalDrive::Dvdplusdlrw) {
            supportedtypes << I18N_NOOP("DVD+DLRW");
        }
        if (mediatypes & Solid::OpticalDrive::Bd) {
            supportedtypes << I18N_NOOP("BD");
        }
        if (mediatypes & Solid::OpticalDrive::Bdr) {
            supportedtypes << I18N_NOOP("BD-R");
        }
        if (mediatypes & Solid::OpticalDrive::Bdre) {
            supportedtypes << I18N_NOOP("BD-RE");
        }
        if (mediatypes & Solid::OpticalDrive::HdDvd) {
            supportedtypes << I18N_NOOP("HDDVD");
        }
        if (mediatypes & Solid::OpticalDrive::HdDvdr) {
            supportedtypes << I18N_NOOP("HDDVD-R");
        }
        if (mediatypes & Solid::OpticalDrive::HdDvdrw) {
            supportedtypes << I18N_NOOP("HDDVD-RW");
        }
        setData(name, I18N_NOOP("Supported Media"), supportedtypes);

        setData(name, I18N_NOOP("Read Speed"), opticaldrive->readSpeed());
        setData(name, I18N_NOOP("Write Speed"), opticaldrive->writeSpeed());

        //the following method return QList<int> so we need to convert it to QList<QVariant>
        QList<int> writespeeds = opticaldrive->writeSpeeds();
        QList<QVariant> variantlist = QList<QVariant>();
        foreach(int num, writespeeds) {
            variantlist << QVariant(num);
        }
        setData(name, I18N_NOOP("Write Speeds"), variantlist);

    }
    if (device.is<Solid::StorageVolume>()) {
        Solid::StorageVolume *storagevolume = device.as<Solid::StorageVolume>();
        if (!storagevolume) {
            return false;
        }

        devicetypes << I18N_NOOP("Storage Volume");

        QStringList usagetypes;
        usagetypes << i18n("Other") << i18n("Unused") << i18n("File System")
                   << i18n("Partition Table") << i18n("Raid") << i18n("Encrypted");

        if (usagetypes.count() > storagevolume->usage()) {
            setData(name, I18N_NOOP("Usage"), usagetypes.at((int)storagevolume->usage()));
        } else {
            setData(name, I18N_NOOP("Usage"), i18n("Unknown"));
        }

        setData(name, I18N_NOOP("Ignored"), storagevolume->isIgnored());
        setData(name, I18N_NOOP("File System Type"), storagevolume->fsType());
        setData(name, I18N_NOOP("Label"), storagevolume->label());
        setData(name, I18N_NOOP("UUID"), storagevolume->uuid());
        setData(name, I18N_NOOP("Size"), storagevolume->size());
        updateInUse(name);

        //Check if the volume is part of an encrypted container
        //This needs to trigger an update for the encrypted container volume since
        //libsolid cannot notify us when the accessibility of the container changes
        Solid::Device encryptedContainer = storagevolume->encryptedContainer();
        if (encryptedContainer.isValid()) {
            QString containerUdi = encryptedContainer.udi();
            setData(name, I18N_NOOP("Encrypted Container"), containerUdi);
            m_encryptedContainerMap[name] = containerUdi;
            //TODO: compress the calls?
            forceUpdateAccessibility(containerUdi);
        }
    }
    if (device.is<Solid::OpticalDisc>()) {
        Solid::OpticalDisc *opticaldisc = device.as<Solid::OpticalDisc>();
        if (!opticaldisc) {
            return false;
        }

        devicetypes << I18N_NOOP("OpticalDisc");

        //get the content types
        QStringList contenttypelist;
        Solid::OpticalDisc::ContentTypes contenttypes = opticaldisc->availableContent();
        if (contenttypes & Solid::OpticalDisc::Audio) {
            contenttypelist << I18N_NOOP("Audio");
        }
        if (contenttypes & Solid::OpticalDisc::Data) {
            contenttypelist << I18N_NOOP("Data");
        }
        if (contenttypes & Solid::OpticalDisc::VideoCd) {
            contenttypelist << I18N_NOOP("Video CD");
        }
        if (contenttypes & Solid::OpticalDisc::SuperVideoCd) {
            contenttypelist << I18N_NOOP("Super Video CD");
        }
        if (contenttypes & Solid::OpticalDisc::VideoDvd) {
            contenttypelist << I18N_NOOP("Video DVD");
        }
        if (contenttypes & Solid::OpticalDisc::VideoBluRay) {
            contenttypelist << I18N_NOOP("Video Blu Ray");
        }
        setData(name, I18N_NOOP("Available Content"), contenttypelist);

        QStringList disctypes;
        disctypes << I18N_NOOP("Unknown Disc Type") << I18N_NOOP("CD Rom") << I18N_NOOP("CD Recordable")
                << I18N_NOOP("CD Rewritable") << I18N_NOOP("DVD Rom") << I18N_NOOP("DVD Ram")
                << I18N_NOOP("DVD Recordable") << I18N_NOOP("DVD Rewritable") << I18N_NOOP("DVD Plus Recordable")
                << I18N_NOOP("DVD Plus Rewritable") << I18N_NOOP("DVD Plus Recordable Duallayer")
                << I18N_NOOP("DVD Plus Rewritable Duallayer") << I18N_NOOP("Blu Ray Rom") << I18N_NOOP("Blu Ray Recordable")
                << I18N_NOOP("Blu Ray Rewritable") << I18N_NOOP("HD DVD Rom") <<  I18N_NOOP("HD DVD Recordable")
                << I18N_NOOP("HD DVD Rewritable");

        setData(name, I18N_NOOP("Disc Type"), disctypes.at((int)opticaldisc->discType()));
        setData(name, I18N_NOOP("Appendable"), opticaldisc->isAppendable());
        setData(name, I18N_NOOP("Blank"), opticaldisc->isBlank());
        setData(name, I18N_NOOP("Rewritable"), opticaldisc->isRewritable());
        setData(name, I18N_NOOP("Capacity"), opticaldisc->capacity());
    }
    if (device.is<Solid::Camera>()) {
        Solid::Camera *camera = device.as<Solid::Camera>();
        if (!camera) {
            return false;
        }

        devicetypes << I18N_NOOP("Camera");

        setData(name, I18N_NOOP("Supported Protocols"), camera->supportedProtocols());
        setData(name, I18N_NOOP("Supported Drivers"), camera->supportedDrivers());
        // Cameras are necessarily Removable and Hotpluggable
        setData(name, I18N_NOOP("Removable"), true);
        setData(name, I18N_NOOP("Hotpluggable"), true);

    }
    if (device.is<Solid::PortableMediaPlayer>()) {
        Solid::PortableMediaPlayer *mediaplayer = device.as<Solid::PortableMediaPlayer>();
        if (!mediaplayer) {
            return false;
        }

        devicetypes << I18N_NOOP("Portable Media Player");

        setData(name, I18N_NOOP("Supported Protocols"), mediaplayer->supportedProtocols());
        setData(name, I18N_NOOP("Supported Drivers"), mediaplayer->supportedDrivers());
        // Portable Media Players are necessarily Removable and Hotpluggable
        setData(name, I18N_NOOP("Removable"), true);
        setData(name, I18N_NOOP("Hotpluggable"), true);

    }
    if (device.is<Solid::NetworkInterface>()) {
        Solid::NetworkInterface *networkinterface = device.as<Solid::NetworkInterface>();
        if (!networkinterface) {
            return false;
        }

        devicetypes << I18N_NOOP("Network Interface");

        setData(name, I18N_NOOP("Interface Name"), networkinterface->ifaceName());
        setData(name, I18N_NOOP("Wireless"), networkinterface->isWireless());
        setData(name, I18N_NOOP("Loopback"), networkinterface->isLoopback());
        setData(name, I18N_NOOP("Hardware Address"), networkinterface->hwAddress());
        setData(name, I18N_NOOP("MAC Address"), networkinterface->macAddress());
    }
    if (device.is<Solid::AcAdapter>()) {
        Solid::AcAdapter *ac = device.as<Solid::AcAdapter>();
        if (!ac) {
            return false;
        }

        devicetypes << I18N_NOOP("AC Adapter");

        setData(name, I18N_NOOP("Plugged In"), ac->isPlugged());
        m_signalmanager->mapDevice(ac, device.udi());
    }
    if (device.is<Solid::Battery>()) {
        Solid::Battery *battery = device.as<Solid::Battery>();
        if (!battery) {
            return false;
        }

        devicetypes << I18N_NOOP("Battery");

        QStringList batterytype;
        batterytype << I18N_NOOP("Unknown Battery") << I18N_NOOP("Primary Battery") << I18N_NOOP("UPS Battery")
                 << I18N_NOOP("USB Battery");

        QStringList chargestate;
        chargestate << I18N_NOOP("Unknown Charge") << I18N_NOOP("Charging") << I18N_NOOP("Discharging")
                 << I18N_NOOP("Fully Charged");

        setData(name, I18N_NOOP("Plugged In"), battery->isPlugged());
        setData(name, I18N_NOOP("Type"), batterytype.at((int)battery->type()));
        setData(name, I18N_NOOP("Is Power Supply"), battery->isPowerSupply());
        setData(name, I18N_NOOP("Capacity"), battery->capacity());
        setData(name, I18N_NOOP("Charge Percent"), battery->chargePercent());
        setData(name, I18N_NOOP("Rechargeable"), battery->isRechargeable());
        setData(name, I18N_NOOP("Charge State"), chargestate.at((int)battery->chargeState()));

        m_signalmanager->mapDevice(battery, device.udi());
    }
    if (device.is<Solid::AudioInterface>()) {
        Solid::AudioInterface *audiointerface = device.as<Solid::AudioInterface>();
        if (!audiointerface) {
            return false;
        }

        devicetypes << I18N_NOOP("Audio Interface");

        QStringList audiodriver;
        audiodriver << I18N_NOOP("ALSA") << I18N_NOOP("Open Sound System") << I18N_NOOP("Unknown Audio Driver");

        setData(name, I18N_NOOP("Driver"), audiodriver.at((int)audiointerface->driver()));
        setData(name, I18N_NOOP("Driver Handle"), audiointerface->driverHandle());
        setData(name, I18N_NOOP("Name"), audiointerface->name());

        //get AudioInterfaceTypes
        QStringList audiointerfacetypes;
        Solid::AudioInterface::AudioInterfaceTypes devicetypes = audiointerface->deviceType();
        if (devicetypes & Solid::AudioInterface::UnknownAudioInterfaceType) {
            audiointerfacetypes << I18N_NOOP("Unknown Audio Interface Type");
        }
        if (devicetypes & Solid::AudioInterface::AudioControl) {
            audiointerfacetypes << I18N_NOOP("Audio Control");
        }
        if (devicetypes & Solid::AudioInterface::AudioInput) {
            audiointerfacetypes << I18N_NOOP("Audio Input");
        }
        if (devicetypes & Solid::AudioInterface::AudioOutput) {
            audiointerfacetypes << I18N_NOOP("Audio Output");
        }
        setData(name, I18N_NOOP("Audio Device Type"), audiointerfacetypes);

        //get SoundCardTypes
        QStringList soundcardtype;
        soundcardtype << I18N_NOOP("Internal Soundcard") << I18N_NOOP("USB Soundcard") << I18N_NOOP("Firewire Soundcard")
                << I18N_NOOP("Headset") << I18N_NOOP("Modem");
        setData(name, I18N_NOOP("Soundcard Type"), soundcardtype.at((int)audiointerface->soundcardType()));
    }
    if (device.is<Solid::Video>()) {
        Solid::Video *video = device.as<Solid::Video>();
        if (!video) {
            return false;
        }

        devicetypes << I18N_NOOP("Video");

        setData(name, I18N_NOOP("Supported Protocols"), video->supportedProtocols());
        setData(name, I18N_NOOP("Supported Drivers"), video->supportedDrivers());

        QStringList handles;
        foreach (const QString &driver, video->supportedDrivers()) {
            handles << video->driverHandle(driver).toString();
        }
        setData(name, I18N_NOOP("Driver Handles"), handles);
    }
    if (device.is<Solid::Graphic>()) {
        Solid::Graphic *graphic = device.as<Solid::Graphic>();
        if (!graphic) {
            return false;
        }

        devicetypes << I18N_NOOP("Graphic");

        setData(name, I18N_NOOP("Driver"), graphic->driver());
    }

    if (device.is<Solid::Input>()) {
        Solid::Input *input = device.as<Solid::Input>();
        if (!input) {
            return false;
        }

        devicetypes << I18N_NOOP("Input");

        QStringList inputtype;
        inputtype << I18N_NOOP("Unknown Input") << I18N_NOOP("Mouse") << I18N_NOOP("Keyboard")
                << I18N_NOOP("Joystick");

        setData(name, I18N_NOOP("Input Type"), inputtype.at((int)input->inputType()));
        setData(name, I18N_NOOP("Driver"), input->driver());
    }

    int index = Solid::DeviceInterface::staticMetaObject.indexOfEnumerator("Type");
    QMetaEnum typeEnum = Solid::DeviceInterface::staticMetaObject.enumerator(index);
    for (int i = typeEnum.keyCount() - 1 ; i > 0; i--) {
        Solid::DeviceInterface::Type type = (Solid::DeviceInterface::Type)typeEnum.value(i);
        const Solid::DeviceInterface *interface = device.asDeviceInterface(type);
        if (interface) {
            setData(name, I18N_NOOP("Type Description"), Solid::DeviceInterface::typeDescription(type));
            break;
        }
    }

    setData(name, I18N_NOOP("Device Types"), devicetypes);
    return true;
}

void SolidDeviceEngine::deviceAdded(const QString& udi)
{
    Solid::Device device(udi);

    foreach (const QString &query, m_predicatemap.keys()) {
        Solid::Predicate predicate = Solid::Predicate::fromString(query);
        if (predicate.matches(device)) {
            m_predicatemap[query] << udi;
            setData(query, m_predicatemap[query]);
        }
    }

    if (device.is<Solid::OpticalDisc>()) {
        Solid::OpticalDrive *drive = device.as<Solid::OpticalDrive>();
        if (drive) {
            connect(drive, SIGNAL(ejectRequested(QString)),
                    this, SLOT(setUnmountingState(QString)));
            connect(drive, SIGNAL(ejectDone(Solid::ErrorType,QString,QString)),
                    this, SLOT(setIdleState(Solid::ErrorType,QString,QString)));
        }
    } else if (device.is<Solid::StorageVolume>()) {
        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
        if (access) {
            connect(access, SIGNAL(setupRequested(QString)),
                    this, SLOT(setMountingState(QString)));
            connect(access, SIGNAL(setupDone(Solid::ErrorType,QString,QString)),
                    this, SLOT(setIdleState(Solid::ErrorType,QString,QString)));
            connect(access, SIGNAL(teardownRequested(QString)),
                    this, SLOT(setUnmountingState(QString)));
            connect(access, SIGNAL(teardownDone(Solid::ErrorType,QString,QString)),
                    this, SLOT(setIdleState(Solid::ErrorType,QString,QString)));
        }
    }
}

void SolidDeviceEngine::setMountingState(const QString &udi)
{
    setData(udi, I18N_NOOP("State"), Mounting);
    setData(udi, I18N_NOOP("Operation result"), Working);
}

void SolidDeviceEngine::setUnmountingState(const QString &udi)
{
    setData(udi, I18N_NOOP("State"), Unmounting);
    setData(udi, I18N_NOOP("Operation result"), Working);
}

void SolidDeviceEngine::setIdleState(Solid::ErrorType error, const QString &errorData, const QString &udi)
{
    Q_UNUSED(errorData)

    if (error == Solid::NoError) {
        setData(udi, I18N_NOOP("Operation result"), Successful);
    } else {
        setData(udi, I18N_NOOP("Operation result"), Unsuccessful);
    }
    setData(udi, I18N_NOOP("State"), Idle);

    Solid::Device device = m_devicemap.value(udi);
    if (!device.isValid()) {
        return;
    }

    Solid::StorageAccess *storageaccess = device.as<Solid::StorageAccess>();
    if (!storageaccess) {
        return;
    }

    setData(udi, I18N_NOOP("Accessible"), storageaccess->isAccessible());
    setData(udi, I18N_NOOP("File Path"), storageaccess->filePath());
}

void SolidDeviceEngine::deviceContentChanged(const QString &udi, const bool hascontent)
{
    if (hascontent) {
        if (populateDeviceData(udi)) {
            forceImmediateUpdateOfAllVisualizations();
        }
    }
}

qulonglong SolidDeviceEngine::freeDiskSpace(const QString &mountPoint)
{
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint);
    if (info.isValid()) {
        return info.available();
    }
    return (qulonglong)-1;
}

bool SolidDeviceEngine::updateFreeSpace(const QString &udi)
{
    Solid::Device device = m_devicemap.value(udi);
    if (!device.is<Solid::StorageAccess>() || device.is<Solid::OpticalDisc>()) {
        return false;
    } else if (!device.as<Solid::StorageAccess>()->isAccessible()) {
        removeData(udi, I18N_NOOP("Free Space"));
        removeData(udi, I18N_NOOP("Free Space Text"));
    }

    Solid::StorageAccess *storageaccess = device.as<Solid::StorageAccess>();
    if (!storageaccess) {
        return false;
    }

    QVariant freeSpaceVar;
    qulonglong freeSpace = freeDiskSpace(storageaccess->filePath());
    if (freeSpace != (qulonglong)-1) {
        freeSpaceVar.setValue( freeSpace );
    }
    setData(udi, I18N_NOOP("Free Space"), freeSpaceVar );
    setData(udi, I18N_NOOP("Free Space Text"), KGlobal::locale()->formatByteSize(freeSpace));
    return true;
}

bool SolidDeviceEngine::updateEmblems(const QString &udi)
{
    Solid::Device device = m_devicemap.value(udi);

    setData(udi, I18N_NOOP("Emblems"), device.emblems() );
    return true;
}

bool SolidDeviceEngine::forceUpdateAccessibility(const QString &udi)
{
    Solid::Device device = m_devicemap.value(udi);
    if (!device.isValid()) {
        return false;
    }

    updateEmblems(udi);
    Solid::StorageAccess *storageaccess = device.as<Solid::StorageAccess>();
    if (storageaccess) {
        setData(udi, I18N_NOOP("Accessible"), storageaccess->isAccessible());
    }

    return true;
}

bool SolidDeviceEngine::updateInUse(const QString &udi)
{
    Solid::Device device = m_devicemap.value(udi);
    if (!device.isValid()) {
        return false;
    }

    Solid::StorageAccess *storageaccess = device.as<Solid::StorageAccess>();
    if (!storageaccess) {
        return false;
    }

    if (storageaccess->isAccessible()) {
        setData(udi, I18N_NOOP("In Use"), true);
    } else {
        Solid::Device device = Solid::Device(udi);
        Solid::StorageDrive *drive = device.as<Solid::StorageDrive>();
        if (drive) {
            setData(udi, I18N_NOOP("In Use"), drive->isInUse());
        }
    }

    return true;
}

bool SolidDeviceEngine::updateSourceEvent(const QString& source)
{
    bool update1 = updateFreeSpace(source);
    bool update2 = updateEmblems(source);
    bool update3 = updateInUse(source);
    // TODO: update battery state?

    return (update1 || update2 || update3);
}

void SolidDeviceEngine::deviceRemoved(const QString& udi)
{
    //libsolid cannot notify us when an encrypted container is closed,
    //hence we trigger an update when a device contained in an encrypted container device dies
    QString containerUdi = m_encryptedContainerMap.value(udi, QString());

    if (!containerUdi.isEmpty()) {
        forceUpdateAccessibility(containerUdi);
        m_encryptedContainerMap.remove(udi);
    }

    foreach (const QString &query, m_predicatemap.keys()) {
        m_predicatemap[query].removeAll(udi);
        setData(query, m_predicatemap[query]);
    }

    Solid::Device device(udi);
    if (device.is<Solid::StorageVolume>()) {
        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
        if (access) {
            disconnect(access, 0, this, 0);
        }
    }
    else if (device.is<Solid::OpticalDisc>()) {
        Solid::OpticalDrive *drive = device.as<Solid::OpticalDrive>();
        if (drive) {
            disconnect(drive, 0, this, 0);
        }
    }

    m_devicemap.remove(udi);
    removeSource(udi);
}

void SolidDeviceEngine::deviceChanged(const QString& udi, const QString &property, const QVariant &value)
{
    setData(udi, property, value);
    updateSourceEvent(udi);
}

K_EXPORT_PLASMA_DATAENGINE(soliddevice, SolidDeviceEngine)

#include "moc_soliddeviceengine.cpp"
