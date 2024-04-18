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

#include "devicenotifier.h"

#include <QMutex>
#include <QTimer>
#include <QGraphicsGridLayout>
#include <QGraphicsLinearLayout>
#include <QVBoxLayout>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Plasma/Label>
#include <Plasma/Separator>
#include <Plasma/Frame>
#include <Plasma/IconWidget>
#include <Plasma/Meter>
#include <Plasma/ToolTipManager>
#include <Solid/Device>
#include <Solid/Block>
#include <Solid/Predicate>
#include <Solid/StorageVolume>
#include <Solid/StorageDrive>
#include <Solid/StorageAccess>
#include <Solid/OpticalDrive>
#include <Solid/PowerManagement>
#include <KIcon>
#include <KRun>
#include <KDiskFreeSpaceInfo>
#include <KDebug>

static const int s_freetimeout = 3000; // 3secs
static const int s_conservativefreetimeout = 10000; // 10secs
// the minimum space for 2 items, more or less
static const QSizeF s_minimumsize = QSizeF(290, 140);

class DeviceFrame;

class DeviceNotifierWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    DeviceNotifierWidget(DeviceNotifier* devicenotifier, QGraphicsWidget *parent);

    bool onlyremovable;

public Q_SLOTS:
    void slotUpdateLayout();
    void slotIconActivated();
    void slotRemoveActivated();

private Q_SLOTS:
    void slotCheckFreeSpace();
    void slotConserveResourcesChanged(bool conserve);
    void slotCheckEmblem(const bool accessible, const QString &udi);

private:
    QMutex m_mutex;
    DeviceNotifier* m_devicenotifier;
    QGraphicsLinearLayout* m_layout;
    Plasma::Label* m_label;
    QList<DeviceFrame*> m_frames;
    QTimer* m_freetimer;
    QList<Solid::Device> m_soliddevices;
};


class DeviceFrame : public Plasma::Frame
{
    Q_OBJECT
public:
    explicit DeviceFrame(const Solid::Device &soliddevice, QGraphicsWidget *parent);

    Plasma::IconWidget* iconwidget;
    Plasma::IconWidget* removewidget;
    Plasma::Meter* meter;
    Solid::Device soliddevice;
};

DeviceFrame::DeviceFrame(const Solid::Device &_soliddevice, QGraphicsWidget *parent)
    : Plasma::Frame(parent),
    iconwidget(nullptr),
    removewidget(nullptr),
    meter(nullptr),
    soliddevice(_soliddevice)
{
    DeviceNotifierWidget* devicenotifierwidget = qobject_cast<DeviceNotifierWidget*>(parent);
    const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
    const Solid::OpticalDrive *solidopticaldrive = soliddevice.as<Solid::OpticalDrive>();

    setFrameShadow(Plasma::Frame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QGraphicsGridLayout* framelayout = new QGraphicsGridLayout(this);

    iconwidget = new Plasma::IconWidget(this);
    iconwidget->setOrientation(Qt::Horizontal);
    iconwidget->setIcon(KIcon(soliddevice.icon(), KIconLoader::global(), soliddevice.emblems()));
    iconwidget->setText(soliddevice.description());
    iconwidget->setToolTip(i18n("Click to access this device from other applications."));
    iconwidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    connect(
        iconwidget, SIGNAL(activated()),
        devicenotifierwidget, SLOT(slotIconActivated())
    );
    framelayout->addItem(iconwidget, 0, 0, 1, 1);

    removewidget = new Plasma::IconWidget(this);
    const int smalliconsize = KIconLoader::global()->currentSize(KIconLoader::Small);
    removewidget->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    removewidget->setIcon(KIcon("media-eject"));
    removewidget->setToolTip(
        solidopticaldrive ? i18n("Click to eject this disc.") : i18n("Click to safely remove this device.")
    );
    removewidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    if (solidopticaldrive) {
        removewidget->setVisible(true);
    } else {
        removewidget->setVisible(solidstorageaccess ? solidstorageaccess->isAccessible() : false);
    }
    connect(
        removewidget, SIGNAL(activated()),
        devicenotifierwidget, SLOT(slotRemoveActivated())
    );
    framelayout->addItem(removewidget, 0, 1, 1, 1);

    meter = new Plasma::Meter(this);
    meter->setMeterType(Plasma::Meter::BarMeterHorizontal);
    meter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    framelayout->addItem(meter, 1, 0, 1, 2);

    setLayout(framelayout);
}


DeviceNotifierWidget::DeviceNotifierWidget(DeviceNotifier* devicenotifier, QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    onlyremovable(true),
    m_devicenotifier(devicenotifier),
    m_layout(nullptr),
    m_label(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_label = new Plasma::Label(this);
    m_label->setText(i18n("No devices available"));
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addItem(m_label);
    setLayout(m_layout);

    m_freetimer = new QTimer(this);
    m_freetimer->setInterval(s_freetimeout);
    connect(
        m_freetimer, SIGNAL(timeout()),
        this, SLOT(slotCheckFreeSpace())
    );
    connect(
        Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
        this, SLOT(slotUpdateLayout())
    );
    connect(
        Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
        this, SLOT(slotUpdateLayout())
    );
    connect(
        Solid::DeviceNotifier::instance(), SIGNAL(contentChanged(QString,bool)),
        this, SLOT(slotUpdateLayout())
    );
    connect(
        Solid::PowerManagement::notifier(), SIGNAL(appShouldConserveResourcesChanged(bool)),
        this, SLOT(slotConserveResourcesChanged(bool))
    );
}

void DeviceNotifierWidget::slotUpdateLayout()
{
    Solid::Predicate solidpredicate(Solid::DeviceInterface::StorageVolume);
    QList<Solid::Device> soliddevices = Solid::Device::listFromQuery(solidpredicate);
    QMutableListIterator<Solid::Device> soliditer(soliddevices);
    while (soliditer.hasNext()) {
        const Solid::Device soliddevice = soliditer.next();
        const Solid::StorageVolume *solidstoragevolume = soliddevice.as<Solid::StorageVolume>();
        if (!solidstoragevolume || solidstoragevolume->isIgnored()) {
            soliditer.remove();
            continue;
        }
        if (onlyremovable) {
            const Solid::StorageDrive *solidstoragedrive = soliddevice.as<Solid::StorageDrive>();
            if (!solidstoragedrive || !solidstoragedrive->isRemovable()) {
                soliditer.remove();
                continue;
            }
        }
        // qDebug() << Q_FUNC_INFO << soliddevice.udi() << solidstoragevolume->usage();
    }

    m_freetimer->stop();
    QMutexLocker locker(&m_mutex);
    foreach (DeviceFrame* frame, m_frames) {
        m_layout->removeItem(frame);
    }
    qDeleteAll(m_frames);
    m_frames.clear();
    // ensures that the widget does not expand after items removal
    adjustSize();

    m_soliddevices = soliddevices;
    if (m_soliddevices.isEmpty()) {
        m_label->show();
        m_devicenotifier->setStatus(Plasma::ItemStatus::PassiveStatus);
        return;
    }

    m_label->hide();
    m_devicenotifier->setStatus(Plasma::ItemStatus::ActiveStatus);
    foreach (const Solid::Device &soliddevice, m_soliddevices) {
        DeviceFrame* frame = new DeviceFrame(soliddevice, this);
        m_layout->addItem(frame);
        m_frames.append(frame);
        const Solid::StorageAccess *solidstorageaccess = soliddevice.as<Solid::StorageAccess>();
        if (solidstorageaccess) {
            connect(
                solidstorageaccess, SIGNAL(accessibilityChanged(bool,QString)),
                this, SLOT(slotCheckEmblem(bool,QString))
            );
        }
    }

    locker.unlock();
    slotCheckFreeSpace();
    m_freetimer->start();
}

void DeviceNotifierWidget::slotCheckFreeSpace()
{
    QMutexLocker locker(&m_mutex);
    foreach (const DeviceFrame* frame, m_frames) {
        const Solid::StorageAccess *solidstorageaccess = frame->soliddevice.as<Solid::StorageAccess>();
        QString devicepath;
        // using the mountpoint is slightly more reliable
        if (solidstorageaccess) {
            devicepath = solidstorageaccess->filePath();
        }
        if (devicepath.isEmpty()) {
            const Solid::Block *solidblock = frame->soliddevice.as<Solid::Block>();
            if (solidblock) {
                devicepath = solidblock->device();
            }
        }
        if (!devicepath.isEmpty()) {
            const KDiskFreeSpaceInfo kfreespaceinfo = KDiskFreeSpaceInfo::freeSpaceInfo(devicepath);
            frame->meter->setMaximum(qMax(kfreespaceinfo.size() / 1024, KIO::filesize_t(100)));
            frame->meter->setValue(kfreespaceinfo.used() / 1024);
            // qDebug() << Q_FUNC_INFO << frame->soliddevice.udi() << kfreespaceinfo.size() << kfreespaceinfo.used();
        } else {
            kWarning() << "no mountpoint and no device path for" << frame->soliddevice.udi();
        }
    }
}

void DeviceNotifierWidget::slotConserveResourcesChanged(const bool conserve)
{
    m_freetimer->setInterval(conserve ? s_conservativefreetimeout : s_freetimeout);
}

void DeviceNotifierWidget::slotCheckEmblem(const bool accessible, const QString &udi)
{
    QMutexLocker locker(&m_mutex);
    foreach (const DeviceFrame* frame, m_frames) {
        if (frame->soliddevice.udi() != udi) {
            continue;
        }
        const Solid::OpticalDrive *solidopticaldrive = frame->soliddevice.as<Solid::OpticalDrive>();

        frame->iconwidget->setIcon(KIcon(frame->soliddevice.icon(), KIconLoader::global(), frame->soliddevice.emblems()));
        frame->removewidget->setVisible(accessible || solidopticaldrive);
        break;
    }
}

void DeviceNotifierWidget::slotIconActivated()
{
    const Plasma::IconWidget* iconwidget = qobject_cast<Plasma::IconWidget*>(sender());
    DeviceFrame* deviceframe = qobject_cast<DeviceFrame*>(iconwidget->parentObject());
    Solid::StorageAccess* solidstorageacces = deviceframe->soliddevice.as<Solid::StorageAccess>();
    if (!solidstorageacces) {
        kWarning() << "not storage access" << deviceframe->soliddevice.udi();
        return;
    }
    QString mountpoint = solidstorageacces->filePath();
    if (mountpoint.isEmpty()) {
        solidstorageacces->setup();
        mountpoint = solidstorageacces->filePath();
    }
    if (!mountpoint.isEmpty()) {
        KRun::runUrl(KUrl(mountpoint), "inode/directory", nullptr);
    }
}

void DeviceNotifierWidget::slotRemoveActivated()
{
    const Plasma::IconWidget* removewidget = qobject_cast<Plasma::IconWidget*>(sender());
    DeviceFrame* deviceframe = qobject_cast<DeviceFrame*>(removewidget->parentObject());
    Solid::OpticalDrive *solidopticaldrive = deviceframe->soliddevice.as<Solid::OpticalDrive>();
    if (solidopticaldrive) {
        solidopticaldrive->eject();
        return;
    }
    Solid::StorageAccess* solidstorageacces = deviceframe->soliddevice.as<Solid::StorageAccess>();
    if (!solidstorageacces) {
        kWarning() << "not storage access" << deviceframe->soliddevice.udi();
        return;
    }
    solidstorageacces->teardown();
}

DeviceNotifier::DeviceNotifier(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_plasmascrollwidget(nullptr),
    m_devicenotifierwidget(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_devicenotifier");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setStatus(Plasma::ItemStatus::PassiveStatus);
    setPopupIcon("device-notifier");

    m_plasmascrollwidget = new Plasma::ScrollWidget(this);
    m_plasmascrollwidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_plasmascrollwidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // ensures the scroll area does not shrink bellow the preferred size
    m_plasmascrollwidget->setMinimumSize(s_minimumsize);
    m_devicenotifierwidget = new DeviceNotifierWidget(this, m_plasmascrollwidget);
    m_plasmascrollwidget->setWidget(m_devicenotifierwidget);
}

DeviceNotifier::~DeviceNotifier()
{
    delete m_devicenotifierwidget;
}

void DeviceNotifier::init()
{
    Plasma::PopupApplet::init();

    KConfigGroup configgroup = config();
    m_devicenotifierwidget->onlyremovable = configgroup.readEntry("showOnlyRemovable", true);
    QTimer::singleShot(500, m_devicenotifierwidget, SLOT(slotUpdateLayout()));
}

QGraphicsWidget* DeviceNotifier::graphicsWidget()
{
    return m_plasmascrollwidget;
}

void DeviceNotifier::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QVBoxLayout* widgetlayout = new QVBoxLayout(widget);
    m_removablebox = new QCheckBox(widget);
    m_removablebox->setText(i18n("Removable devices only"));
    m_removablebox->setChecked(m_devicenotifierwidget->onlyremovable);
    widgetlayout->addWidget(m_removablebox);
    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addSpacerItem(m_spacer);
    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("Devices"), "device-notifier");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_removablebox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
}

void DeviceNotifier::slotConfigAccepted()
{
    Q_ASSERT(m_removablebox);
    m_devicenotifierwidget->onlyremovable = m_removablebox->isChecked();
    KConfigGroup configgroup = config();
    configgroup.writeEntry("showOnlyRemovable", m_devicenotifierwidget->onlyremovable);
    emit configNeedsSaving();
    QTimer::singleShot(500, m_devicenotifierwidget, SLOT(slotUpdateLayout()));
}

#include "moc_devicenotifier.cpp"
#include "devicenotifier.moc"
