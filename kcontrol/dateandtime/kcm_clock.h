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

#ifndef KCM_CLOCK_H
#define KCM_CLOCK_H

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTimeEdit>
#include <QTreeWidget>
#include <QTimer>
#include <kcmodule.h>
#include <kmessagewidget.h>
#include <ktreewidgetsearchline.h>

class KCMClockSearch : public KTreeWidgetSearchLine
{
    Q_OBJECT
public:
    KCMClockSearch(QWidget *parent);

protected:
    bool itemMatches(const QTreeWidgetItem *item, const QString &pattern) const final;
};


class KCMClock : public KCModule
{
    Q_OBJECT
public:
    KCMClock(QWidget *parent, const QVariantList &args);

    // KCModule reimplementations
public Q_SLOTS:
    void load() final;
    void save() final;
    void defaults() final;

private Q_SLOTS:
    void slotUpdate();
    void slotTimeChanged();
    void slotDateChanged();
    void slotZoneChanged();

private:
    void selectTimeZone(const QString &name);

    QVBoxLayout* m_layout;
    bool m_canchangeclock;
    KMessageWidget* m_messagewidget;
    QGroupBox* m_datetimebox;
    QHBoxLayout* m_datetimelayout;
    QTimeEdit* m_timeedit;
    QDateEdit* m_dateedit;
    QGroupBox* m_timezonebox;
    QVBoxLayout* m_timezonelayout;
    KCMClockSearch* m_timezonesearch;
    QTreeWidget* m_timezonewidget;
    QTimer* m_timer;
    bool m_timechanged;
    bool m_datechanged;
    bool m_zonechanged;
};

#endif // KCM_CLOCK_H
