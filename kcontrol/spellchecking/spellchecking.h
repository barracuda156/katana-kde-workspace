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

#ifndef SPELLCHECKING_H
#define SPELLCHECKING_H

#include <kcmodule.h>
#include <kspellconfigwidget.h>
#include <kconfig.h>

#include <QBoxLayout>

class SpellCheckingModule : public KCModule
{
    Q_OBJECT

public:
    SpellCheckingModule(QWidget *parent, const QVariantList &args);
    ~SpellCheckingModule();

    // KCModule reimplementations
public Q_SLOTS:
    void load() final;
    void save() final;
    void defaults() final;

private Q_SLOTS:
    void slotConfigChanged();

private:
    QVBoxLayout* m_layout;
    KSpellConfigWidget *m_configWidget;
    KConfig *m_config;
};

#endif // SPELLCHECKING_H
