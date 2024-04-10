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

#ifndef JOBSWIDGET_H
#define JOBSWIDGET_H

#include "jobtrackeradaptor.h"

#include <QMutex>
#include <QGraphicsWidget>
#include <QGraphicsLinearLayout>
#include <Plasma/Label>
#include <Plasma/Frame>
#include <Plasma/IconWidget>
#include <Plasma/Meter>

class NotificationsWidget;

class JobFrame : public Plasma::Frame
{
    Q_OBJECT
public:
    explicit JobFrame(const QString &name, QGraphicsWidget *parent);

private Q_SLOTS:
    void slotJobUpdated(const QString &name, const QVariantMap &data);
    void slotIcon0Activated();
    void slotIcon1Activated();

private:
    QString m_name;
    Plasma::IconWidget* m_iconwidget;
    Plasma::Label* m_label;
    Plasma::IconWidget* m_iconwidget0;
    Plasma::IconWidget* m_iconwidget1;
    Plasma::Meter* m_meter;
};


class JobsWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    JobsWidget(QGraphicsItem *parent, NotificationsWidget *notificationswidget);
    ~JobsWidget();

    int count() const;

Q_SIGNALS:
    int countChanged();

private Q_SLOTS:
    void slotJobAdded(const QString &name);
    void slotFrameDestroyed(QObject *object);

private:
    QMutex m_mutex;
    NotificationsWidget* m_notificationswidget;
    QGraphicsLinearLayout* m_layout;
    Plasma::Label* m_label;
    QList<QObject*> m_frames;
    JobTrackerAdaptor* m_adaptor;
};

#endif // JOBSWIDGET_H
