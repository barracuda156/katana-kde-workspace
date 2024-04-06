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

#include "notes.h"

#include <Plasma/Frame>
#include <Plasma/TextEdit>

static const QSizeF s_minimumsize = QSizeF(256, 256);

class NotesAppletWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    NotesAppletWidget(QGraphicsWidget *parent);

private:
    QGraphicsLinearLayout* m_layout;
    Plasma::Frame* m_frame;
    QGraphicsLinearLayout* m_framelayout;
    Plasma::TextEdit* m_textedit;
};

NotesAppletWidget::NotesAppletWidget(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    m_layout(nullptr),
    m_frame(nullptr),
    m_framelayout(nullptr),
    m_textedit(nullptr)
{
    m_layout = new QGraphicsLinearLayout(this);
    m_layout->setOrientation(Qt::Vertical);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_frame = new Plasma::Frame(this);
    m_frame->setFrameShadow(Plasma::Frame::Sunken);
    m_frame->setMinimumSize(s_minimumsize);
    m_frame->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_framelayout = new QGraphicsLinearLayout(m_frame);
    m_framelayout->setContentsMargins(0, 0, 0, 0);
    m_frame->setLayout(m_framelayout);

    m_textedit = new Plasma::TextEdit(m_frame);
    m_framelayout->addItem(m_textedit);
    m_layout->addItem(m_frame);

    setLayout(m_layout);

    adjustSize();
}


NotesApplet::NotesApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_noteskwidget(nullptr)
{
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setStatus(Plasma::AcceptingInputStatus);
    setPopupIcon("knotes");

    m_noteskwidget = new NotesAppletWidget(this);
}

void NotesApplet::init()
{
}

QGraphicsWidget* NotesApplet::graphicsWidget()
{
    return m_noteskwidget;
}

K_EXPORT_PLASMA_APPLET(notes, NotesApplet)

#include "notes.moc"
#include "moc_notes.cpp"
