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

#include "jobtrackeradaptor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <KGlobal>

static JobTrackerAdaptor* kJobTrackerAdaptor = nullptr;

JobTrackerAdaptor::JobTrackerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent),
    m_ref(0)
{
}

JobTrackerAdaptor::~JobTrackerAdaptor()
{
    Q_ASSERT(m_ref == 0);
}

void JobTrackerAdaptor::addJob(const QString &name)
{
    emit jobAdded(name);
}

void JobTrackerAdaptor::updateJob(const QString &name, const QVariantMap &data)
{
    emit jobUpdated(name, data);
}

void JobTrackerAdaptor::stopJob(const QString &name)
{
    emit stopRequested(name);
}

JobTrackerAdaptor* JobTrackerAdaptor::self()
{
    if (!kJobTrackerAdaptor) {
        kJobTrackerAdaptor = new JobTrackerAdaptor(qApp);
    }
    return kJobTrackerAdaptor;
}

void JobTrackerAdaptor::registerObject()
{
    if (m_ref == 0) {
        QDBusConnection::sessionBus().registerObject("/JobTracker", qApp);
    }
    m_ref.ref();
}

void JobTrackerAdaptor::unregisterObject()
{
    m_ref.deref();
    if (m_ref == 0) {
        QDBusConnection::sessionBus().unregisterObject("/JobTracker");
    }
}

#include "moc_jobtrackeradaptor.cpp"
