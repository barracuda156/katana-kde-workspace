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

#ifndef JOBTRACKERADAPTOR_H
#define JOBTRACKERADAPTOR_H

#include <QDBusAbstractAdaptor>
#include <QVariantMap>

// Adaptor class for interface org.kde.JobTracker
class JobTrackerAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.JobTracker")
public:
    JobTrackerAdaptor(QObject *parent);

public Q_SLOTS:
    void addJob(const QString &name);
    void updateJob(const QString &name, const QVariantMap &data);
    void stopJob(const QString &name);

Q_SIGNALS:
    void jobAdded(const QString &name);
    void jobUpdated(const QString &name, const QVariantMap &data);

    void stopRequested(const QString &name);
};

#endif // JOBTRACKERADAPTOR_H
