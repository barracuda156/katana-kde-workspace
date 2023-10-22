/*
    This is the new kwindecoration kcontrol module

    Copyright (c) 2001
        Karol Szwed <gallium@kde.org>
        http://gallium.n3.net/
    Copyright 2009, 2010 Martin Gräßlin <mgraesslin@kde.org>

    Supports new kwin configuration plugins, and titlebar button position
    modification via dnd interface.

    Based on original "kwintheme" (Window Borders)
    Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef KWINDECORATION_H
#define KWINDECORATION_H

#include "kdecoration.h"

#include <QVBoxLayout>
#include <kcmodule.h>
#include <ksharedconfig.h>

namespace KWin
{

class KWinDecorationModule : public KCModule
{
    Q_OBJECT

public:
    KWinDecorationModule(QWidget *parent, const QVariantList &args);
    ~KWinDecorationModule();

    virtual void load();
    virtual void save();
    virtual void defaults();

Q_SIGNALS:
    void pluginSave(KConfigGroup &conf);

private slots:
    void slotSelectionChanged();

private:
    KSharedConfigPtr m_kwinConfig;
    QVBoxLayout* m_layout;
    QObject* (*allocatePlugin)(KConfigGroup& conf, QWidget* parent);
    QObject* m_pluginObject;
    QWidget* m_pluginConfigWidget;
};

} //namespace

#endif 
