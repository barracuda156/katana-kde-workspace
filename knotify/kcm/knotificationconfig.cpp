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

#include "knotificationconfig.h"

#include <QFileInfo>
#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfiggroup.h>
#include <kicon.h>
#include <kaboutdata.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

K_PLUGIN_FACTORY(KCMNotificationFactory, registerPlugin<KCMNotification>();)
K_EXPORT_PLUGIN(KCMNotificationFactory("kcmknotificationconfig", "kcm_knotificationconfig"))

static const QString s_kdenotification = QString::fromLatin1("kde");

KCMNotification::KCMNotification(QWidget *parent, const QVariantList &args)
    : KCModule(KCMNotificationFactory::componentData(), parent),
    m_layout(nullptr),
    m_notificationslabel(nullptr),
    m_notificationsbox(nullptr),
    m_notificationswidget(nullptr),
    m_firstload(true)
{
    Q_UNUSED(args);

    setButtons(KCModule::Apply);
    setQuickHelp(i18n("<h1>Notifications Configuration</h1> This module allows you to change KDE notification options."));

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmknotificationconfig"), 0,
        ki18n("KDE Notifications Configuration Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2023, Ivailo Monev <email>xakepa10@gmail.com</email>")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    m_layout = new QGridLayout(this);
    setLayout(m_layout);

    m_notificationslabel = new QLabel(this);
    m_notificationslabel->setText(i18n("Event source:"));
    m_layout->addWidget(m_notificationslabel, 0, 0);
    m_notificationsbox = new KComboBox(this);
    const QStringList notifyconfigs = KGlobal::dirs()->findAllResources("config", "notifications/*.notifyrc");
    foreach (const QString &notifyconfig, notifyconfigs) {
        KConfig eventsconfig(notifyconfig, KConfig::NoGlobals);
        foreach (const QString &eventgroup, eventsconfig.groupList()) {
            if (eventgroup.contains(QLatin1Char('/'))) {
                continue;
            }
            KConfigGroup eventgroupconfig(&eventsconfig, eventgroup);
            m_notificationsbox->addItem(
                KIcon(eventgroupconfig.readEntry("IconName")),
                eventgroupconfig.readEntry("Comment"),
                QFileInfo(notifyconfig).baseName()
            );
        }
    }
    m_notificationsbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_notificationsbox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotSourceIndexChanged(int)));
    m_layout->addWidget(m_notificationsbox, 0, 1);

    m_notificationswidget = new KNotificationConfigWidget(s_kdenotification, this);
    connect(m_notificationswidget, SIGNAL(changed(bool)), this, SLOT(slotNotificationChanged(bool)));
    m_layout->addWidget(m_notificationswidget, 1, 0, 1, 2);
}

void KCMNotification::load()
{
    if (m_firstload) {
        m_firstload = false;

        const int kdeindex = m_notificationsbox->findData(s_kdenotification);
        if (kdeindex >= 0) {
            m_notificationsbox->setCurrentIndex(kdeindex);
        } else {
            kWarning() << "could not find the index of" << s_kdenotification;
        }

        m_notificationswidget->setNotification(s_kdenotification);
        emit changed(false);
        return;
    }

    const QString notification = m_notificationsbox->itemData(m_notificationsbox->currentIndex()).toString();
    m_notificationswidget->setNotification(notification);
    emit changed(false);
}

void KCMNotification::save()
{
    m_notificationswidget->save();
    emit changed(false);
}

void KCMNotification::slotSourceIndexChanged(int index)
{
    const QString notification = m_notificationsbox->itemData(index).toString();
    m_notificationswidget->setNotification(notification);
    emit changed(false);
}

void KCMNotification::slotNotificationChanged(bool state)
{
    emit changed(state);
}

#include "moc_knotificationconfig.cpp"
