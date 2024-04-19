/*
    This file is part of the KDE project
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#include "notificationsadaptor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <KGlobal>

static NotificationsAdaptor* kNotificationsAdaptor = nullptr;

NotificationsAdaptor::NotificationsAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent),
    m_ref(0)
{
}

NotificationsAdaptor::~NotificationsAdaptor()
{
    Q_ASSERT(m_ref == 0);
}

void NotificationsAdaptor::addNotification(const QString &name)
{
    emit notificationAdded(name);
}

void NotificationsAdaptor::updateNotification(const QString &name, const QVariantMap &data)
{
    emit notificationUpdated(name, data);
}

void NotificationsAdaptor::closeNotification(const QString &name)
{
    emit closeRequested(name);
}

void NotificationsAdaptor::invokeAction(const QString &name, const QString &action)
{
    emit actionRequested(name, action);
}

NotificationsAdaptor* NotificationsAdaptor::self()
{
    if (!kNotificationsAdaptor) {
        kNotificationsAdaptor = new NotificationsAdaptor(qApp);
    }
    return kNotificationsAdaptor;
}

void NotificationsAdaptor::registerObject()
{
    if (m_ref == 0) {
        QDBusConnection::sessionBus().registerObject("/Notifications", qApp);
    }
    m_ref.ref();
}

void NotificationsAdaptor::unregisterObject()
{
    m_ref.deref();
    if (m_ref == 0) {
        QDBusConnection::sessionBus().unregisterObject("/Notifications");
    }
}

#include "moc_notificationsadaptor.cpp"
