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

#ifndef MIXER_H
#define MIXER_H

#include <QGraphicsSceneWheelEvent>
#include <QCheckBox>
#include <QLabel>
#include <QSpacerItem>
#include <KIntNumInput>
#include <KConfigDialog>
#include <KColorButton>
#include <Plasma/PopupApplet>

class MixerWidget;

class MixerApplet : public Plasma::PopupApplet
{
    Q_OBJECT
public:
    MixerApplet(QObject *parent, const QVariantList &args);
    ~MixerApplet();

    // Plasma::Applet reimplementations
    void init() final;
    void createConfigurationInterface(KConfigDialog *parent) final;
    // Plasma::PopupApplet reimplementation
    QGraphicsWidget* graphicsWidget() final;
    // QGraphicsWidget reimplementation
    void wheelEvent(QGraphicsSceneWheelEvent *event) final;

private Q_SLOTS:
    void slotVisualizerToggled(bool toggled);
    void slotConfigAccepted();
    void slotThemeChanged();

private:
    friend MixerWidget;
    MixerWidget *m_mixerwidget;
    bool m_showvisualizer;
    uint m_visualizerscale;
    QColor m_visualizercolor;
    bool m_visualizericon;
    QCheckBox* m_visualizerbox;
    QLabel* m_visualizerscalelabel;
    KIntNumInput* m_visualizerscalebox;
    KColorButton* m_visualizerbutton;
    QCheckBox* m_visualizericonbox;
    QSpacerItem* m_spacer;
};

#endif // MIXER_H
