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

#ifndef NOTES_H
#define NOTES_H

#include <QSpacerItem>
#include <KConfigDialog>
#include <KFontRequester>
#include <Plasma/PopupApplet>

class NotesAppletWidget;

class NotesApplet : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    NotesApplet(QObject *parent, const QVariantList &args);

    // Plasma::Applet reimplementations
    void init() final;
    void createConfigurationInterface(KConfigDialog *parent) final;
    // Plasma::PopupApplet reimplementation
    QGraphicsWidget* graphicsWidget() final;

    // Plasma::Applet reimplementations
public Q_SLOTS:
    void configChanged();
    void slotConfigAccepted();

protected:
    void saveState(KConfigGroup &group) const final;

private:
    friend NotesAppletWidget;
    NotesAppletWidget *m_noteswidget;
    KFontRequester* m_fontrequester;
    QSpacerItem* m_spacer;
};

#endif // NOTES_H
