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

#ifndef NOTIFICATIONSADAPTOR_H
#define NOTIFICATIONSADAPTOR_H

#include <QDBusAbstractAdaptor>
#include <QVariantMap>

// Adaptor class for interface org.kde.Notifications
class NotificationsAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Notifications")
public:
    NotificationsAdaptor(QObject *parent);

public Q_SLOTS:
    void addNotification(const QString &name);
    void updateNotification(const QString &name, const QVariantMap &data);
    void closeNotification(const QString &name);
    void invokeAction(const QString &name, const QString &action);

Q_SIGNALS:
    void notificationAdded(const QString &name);
    void notificationUpdated(const QString &name, const QVariantMap &data);

    void closeRequested(const QString &name);
    void actionRequested(const QString &name, const QString &action);
};

#endif // NOTIFICATIONSADAPTOR_H
