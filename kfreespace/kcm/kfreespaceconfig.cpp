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

#include "kfreespaceconfig.h"
#include "kfreespace.h"

#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <knuminput.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <solid/device.h>
#include <solid/storageaccess.h>

class KFreeSpaceBox : public QGroupBox
{
    Q_OBJECT
public:
    KFreeSpaceBox(QWidget *parent,
                  const Solid::Device &soliddevice,
                  bool watch, qulonglong checktime, qulonglong freespace);


public:
    QString udi() const;
    bool watch() const;
    qulonglong checkTime() const;
    qulonglong freeSpace() const;

    void setDefault();

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void slotWatch();
    void slotCheckTime();
    void slotFreeSpace();

private:
    Solid::Device m_soliddevice;
    QCheckBox* m_watchbox;
    KIntNumInput* m_checktimeinput;
    KIntNumInput* m_freespaceinput;
};

KFreeSpaceBox::KFreeSpaceBox(QWidget *parent,
                             const Solid::Device &soliddevice,
                             bool watch, qulonglong checktime, qulonglong freespace)
    : QGroupBox(parent),
    m_soliddevice(soliddevice),
    m_watchbox(nullptr),
    m_checktimeinput(nullptr),
    m_freespaceinput(nullptr)
{
    setTitle(m_soliddevice.description());

    QGridLayout* devicelayout = new QGridLayout(this);

    m_watchbox = new QCheckBox(i18n("Notify when the device free space is low"), this);
    m_watchbox->setChecked(watch);
    m_watchbox->setToolTip(i18n("Whether or not to periodically check the device free space"));
    connect(m_watchbox, SIGNAL(stateChanged(int)), this, SLOT(slotWatch()));
    devicelayout->addWidget(m_watchbox, 0, 0, 1, 2);

    QLabel* checktimelabel = new QLabel(i18n("Check time:"), this);
    devicelayout->addWidget(checktimelabel, 1, 0);
    m_checktimeinput = new KIntNumInput(this);
    m_checktimeinput->setRange(s_kfreespacechecktimemin, s_kfreespacechecktimemax);
    m_checktimeinput->setValue(checktime);
    m_checktimeinput->setSuffix(ki18np(" second", " seconds"));
    m_checktimeinput->setSliderEnabled(true);
    m_checktimeinput->setToolTip(i18n("When the time has elapsed the device free space will be checked"));
    connect(m_checktimeinput, SIGNAL(valueChanged(int)), this, SLOT(slotCheckTime()));
    devicelayout->addWidget(m_checktimeinput, 1, 1);

    QLabel* freespacelabel = new QLabel(i18n("Free space:"), this);
    devicelayout->addWidget(freespacelabel, 2, 0);
    m_freespaceinput = new KIntNumInput(this);
    m_freespaceinput->setRange(s_kfreespacefreespacemin, s_kfreespacefreespacemax);
    m_freespaceinput->setValue(freespace);
    m_freespaceinput->setSliderEnabled(true);
    m_freespaceinput->setToolTip(i18n("When the free space on the devices is equal to or less than that a notification will be shown"));
    // NOTE: the i18n() bellow is using translation from:
    // kdelibs/kdecore/localization/klocale.cpp
    m_freespaceinput->setSuffix(i18n("%1 MB", QLatin1String("")));
    m_freespaceinput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_freespaceinput, SIGNAL(valueChanged(int)), this, SLOT(slotFreeSpace()));
    devicelayout->addWidget(m_freespaceinput, 2, 1);

    m_checktimeinput->setEnabled(watch);
    m_freespaceinput->setEnabled(watch);
}

QString KFreeSpaceBox::udi() const
{
    return m_soliddevice.udi();
}

bool KFreeSpaceBox::watch() const
{
    return m_watchbox->isChecked();
}

qulonglong KFreeSpaceBox::checkTime() const
{
    return m_checktimeinput->value();
}

qulonglong KFreeSpaceBox::freeSpace() const
{
    return m_freespaceinput->value();
}

void KFreeSpaceBox::setDefault()
{
    m_watchbox->setChecked(s_kfreespacewatch);
    m_checktimeinput->setValue(s_kfreespacechecktime);
    m_freespaceinput->setValue(kCalculateFreeSpace(m_soliddevice, s_kfreespacefreespace));
}

void KFreeSpaceBox::slotWatch()
{
    const bool watch = m_watchbox->isChecked();
    m_checktimeinput->setEnabled(watch);
    m_freespaceinput->setEnabled(watch);
    emit changed();
}

void KFreeSpaceBox::slotCheckTime()
{
    emit changed();
}

void KFreeSpaceBox::slotFreeSpace()
{
    emit changed();
}


K_PLUGIN_FACTORY(KCMFreeSpaceFactory, registerPlugin<KCMFreeSpace>();)
K_EXPORT_PLUGIN(KCMFreeSpaceFactory("kcmfreespaceconfig", "kcm_freespaceconfig"))

KCMFreeSpace::KCMFreeSpace(QWidget *parent, const QVariantList &args)
    : KCModule(KCMFreeSpaceFactory::componentData(), parent),
    m_layout(nullptr),
    m_spacer(nullptr),
    m_message(nullptr)
{
    Q_UNUSED(args);

    setButtons(KCModule::Default | KCModule::Apply);
    setQuickHelp(i18n("<h1>Free Space Notifier</h1> This module allows you to change KDE free space notifier options."));

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmfreespaceconfig"), 0,
        ki18n("KDE Free Space Notifier Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2023, Ivailo Monev <email>xakepa10@gmail.com</email>")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    m_layout = new QVBoxLayout(this);
    setLayout(m_layout);
}

KCMFreeSpace::~KCMFreeSpace()
{
}

void KCMFreeSpace::load()
{
    qDeleteAll(m_deviceboxes);
    m_deviceboxes.clear();
    for (int i = 0; i < m_layout->count(); i++) {
        const QLayoutItem* layoutitem = m_layout->itemAt(i);
        if (layoutitem == m_spacer) {
            delete m_layout->takeAt(i);
            m_spacer = nullptr;
            break;
        }
    }
    Q_ASSERT(m_spacer == nullptr);
    if (m_message) {
        delete m_message;
        m_message = nullptr;
    }

    bool hasdevices = false;
    KConfig kfreespaceconfig("kfreespacerc", KConfig::SimpleConfig);
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
        hasdevices = true;
        KConfigGroup kfreespacegroup = kfreespaceconfig.group(soliddevice.udi());
        const bool kfreespacewatch = kfreespacegroup.readEntry("watch", s_kfreespacewatch);
        const qulonglong kfreespacechecktime = kfreespacegroup.readEntry("checktime", s_kfreespacechecktime);
        const qulonglong kfreespacefreespace = kfreespacegroup.readEntry("freespace", s_kfreespacefreespace);

        KFreeSpaceBox* devicebox = new KFreeSpaceBox(
            this,
            soliddevice,
            kfreespacewatch, kfreespacechecktime, kCalculateFreeSpace(soliddevice, kfreespacefreespace)
        );
        m_deviceboxes.append(devicebox);
        connect(devicebox, SIGNAL(changed()), this, SLOT(slotDeviceChanged()));
        m_layout->addWidget(devicebox);
    }

    if (!hasdevices) {
        m_message = new KMessageWidget(this);
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setCloseButtonVisible(false);
        m_message->setText(
            i18n("No storage device found on this computer.")
        );
        m_layout->addWidget(m_message);
    }

    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addSpacerItem(m_spacer);

    emit changed(false);
}

void KCMFreeSpace::save()
{
    KConfig kfreespaceconfig("kfreespacerc", KConfig::SimpleConfig);
    foreach (const KFreeSpaceBox* devicebox, m_deviceboxes) {
        KConfigGroup kfreespacegroup = kfreespaceconfig.group(devicebox->udi());
        kfreespacegroup.writeEntry("watch", devicebox->watch());
        kfreespacegroup.writeEntry("checktime", devicebox->checkTime());
        kfreespacegroup.writeEntry("freespace", devicebox->freeSpace());
    }
    emit changed(false);
}

void KCMFreeSpace::defaults()
{
    foreach (KFreeSpaceBox* devicebox, m_deviceboxes) {
        devicebox->setDefault();
    }
    emit changed(true);
}

void KCMFreeSpace::slotDeviceChanged()
{
    emit changed(true);
}

#include "moc_kfreespaceconfig.cpp"
#include "kfreespaceconfig.moc"
