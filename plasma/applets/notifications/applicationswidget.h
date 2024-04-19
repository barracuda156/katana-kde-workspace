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

#ifndef APPLICATIONSWIDGET_H
#define APPLICATIONSWIDGET_H

#include "notificationsadaptor.h"

#include <QMutex>
#include <QGraphicsWidget>
#include <QGraphicsLinearLayout>
#include <Plasma/Label>
#include <Plasma/Frame>
#include <Plasma/IconWidget>
#include <Plasma/PushButton>

class NotificationsWidget;

class ApplicationFrame : public Plasma::Frame
{
    Q_OBJECT
public:
    explicit ApplicationFrame(const QString &name, QGraphicsWidget *parent);

private Q_SLOTS:
    void slotRemoveActivated();
    void slotConfigureActivated();
    void slotActionReleased();
    void slotNotificationUpdated(const QString &name, const QVariantMap &data);

private:
    QString m_name;
    Plasma::IconWidget* m_iconwidget;
    Plasma::Label* m_label;
    Plasma::IconWidget* m_removewidget;
    Plasma::IconWidget* m_configurewidget;
};


class ApplicationsWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    ApplicationsWidget(QGraphicsItem *parent);
    ~ApplicationsWidget();

    int count() const;

Q_SIGNALS:
    int countChanged();

private Q_SLOTS:
    void slotNotificationAdded(const QString &name);
    void slotFrameDestroyed(QObject *object);

private:
    QMutex m_mutex;
    QGraphicsLinearLayout* m_layout;
    Plasma::Label* m_label;
    QList<QObject*> m_frames;
    NotificationsAdaptor* m_adaptor;
};

#endif // APPLICATIONSWIDGET_H
