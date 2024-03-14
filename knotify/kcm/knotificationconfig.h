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

#ifndef KNOTIFICATIONCONFIG_H
#define KNOTIFICATIONCONFIG_H

#include <QGridLayout>
#include <QLabel>
#include <kcmodule.h>
#include <kcombobox.h>
#include <knotificationconfigwidget.h>

/**
 * Control KDE notifications
 *
 * @author Ivailo Monev (xakepa10@gmail.com)
 */
class KCMNotification : public KCModule
{
    Q_OBJECT
public:
    KCMNotification(QWidget *parent, const QVariantList &args);

    // KCModule reimplementations
public Q_SLOTS:
    void load() final;
    void save() final;

private Q_SLOTS:
    void slotSourceIndexChanged(int index);
    void slotNotificationChanged(bool state);

private:
    QGridLayout* m_layout;
    QLabel* m_notificationslabel;
    KComboBox* m_notificationsbox;
    KNotificationConfigWidget* m_notificationswidget;
    bool m_firstload;
};

#endif // KNOTIFICATIONCONFIG_H
