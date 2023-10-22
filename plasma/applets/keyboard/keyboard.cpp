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

#include "keyboard.h"

#include <QVBoxLayout>
#include <Plasma/Svg>
#include <Plasma/PaintUtils>
#include <Plasma/ToolTipManager>
#include <KIcon>
#include <KStandardDirs>
#include <KDebug>

KeyboardApplet::KeyboardApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_keyboardlayout(nullptr),
    m_showflag(false),
    m_showtext(true),
    m_indicatorbox(nullptr),
    m_spacer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_keyboard");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setStatus(Plasma::ItemStatus::PassiveStatus);

    m_keyboardlayout = new KKeyboardLayout(this);
    connect(
        m_keyboardlayout, SIGNAL(layoutChanged()),
        this, SLOT(slotLayoutChanged())
    );
}

void KeyboardApplet::init()
{
    KConfigGroup configgroup = config();
    m_showflag = configgroup.readEntry("showFlag", false);
    m_showtext = configgroup.readEntry("showText", true);

    setGlobalShortcut(KShortcut(Qt::ALT+Qt::CTRL+Qt::Key_K));
    connect(
        this, SIGNAL(activate()),
        this, SLOT(slotNextLayout())
    );
}

void KeyboardApplet::paintInterface(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    const QRect &contentsRect)
{
    if (m_showflag && m_flagpath.isEmpty()) {
        m_flagpath = KStandardDirs::locate(
            "locale",
            QString::fromLatin1("l10n/%1/flag.png").arg(m_layoutlayout)
        );
    }
    QFont font = KGlobalSettings::smallestReadableFont();
    font.setBold(true);
    font.setPointSize(qMax(font.pointSize(), contentsRect.height() / 2));

    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::Antialiasing);

    const bool paintflag = (m_showflag && !m_flagpath.isEmpty());
    if (paintflag) {
        painter->save();
        painter->drawPixmap(
            contentsRect,
            QPixmap(m_flagpath).scaled(contentsRect.size(), Qt::KeepAspectRatio)
        );
        painter->restore();
    }

    if (m_showtext) {
        if (paintflag) {
            painter->drawPixmap(
                contentsRect,
                Plasma::PaintUtils::shadowText(m_layoutlayout, font, Qt::black, Qt::white, QPoint(), 3)
            );
        } else {
            Plasma::Svg* svg = new Plasma::Svg(this);
            svg->setImagePath("widgets/labeltexture");
            svg->setContainsMultipleImages(true);
            painter->drawPixmap(
                contentsRect,
                Plasma::PaintUtils::texturedText(m_layoutlayout, font, svg)
            );
            delete svg;
        }
    }
}

void KeyboardApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QVBoxLayout* widgetlayout = new QVBoxLayout(widget);
    m_indicatorbox = new QComboBox(widget);
    m_indicatorbox->addItem(i18n("Show text"));
    m_indicatorbox->addItem(i18n("Show flag"));
    m_indicatorbox->addItem(i18n("Show text and flag"));
    if (m_showflag && m_showtext) {
        m_indicatorbox->setCurrentIndex(2);
    } else if (m_showflag) {
        m_indicatorbox->setCurrentIndex(1);
    } else {
        m_indicatorbox->setCurrentIndex(0);
    }
    widgetlayout->addWidget(m_indicatorbox);
    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addSpacerItem(m_spacer);
    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("Indicator"), "applications-graphics");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_indicatorbox, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
}

void KeyboardApplet::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        slotNextLayout();
        event->accept();
        return;
    }
    event->ignore();
}

void KeyboardApplet::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (event->delta() < 0) {
        slotNextLayout();
    } else {
        slotPreviousLayout();
    }
}

void KeyboardApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint) {
        int iconsize = 0;
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal:
            case Plasma::FormFactor::Vertical: {
                // panel
                iconsize = 0;
                break;
            }
            default: {
                // desktop-like
                iconsize = (KIconLoader::global()->currentSize(KIconLoader::Desktop) * 2);
                break;
            }
        }
        setMinimumSize(iconsize, iconsize);
    }
}

void KeyboardApplet::slotLayoutChanged()
{
    const QList<KKeyboardType> layouts = m_keyboardlayout->layouts();
    const KKeyboardType activelayout = layouts.first();
    m_layoutlayout = QString::fromLatin1(activelayout.layout.constData(), activelayout.layout.size());
    m_flagpath = KStandardDirs::locate(
        "locale",
        QString::fromLatin1("l10n/%1/flag.png").arg(m_layoutlayout)
    );
    QString layouttooltip;
    layouttooltip.append(i18n("Model: %1<br/>", KKeyboardLayout::modelDescription(activelayout.model)));
    layouttooltip.append(i18n("Variant: %1", KKeyboardLayout::variantDescription(activelayout.layout, activelayout.variant)));
    Plasma::ToolTipContent plasmatooltipcontent = Plasma::ToolTipContent(
        KKeyboardLayout::layoutDescription(activelayout.layout), layouttooltip,
        KIcon(m_flagpath)
    );
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltipcontent);

    if (layouts.size() > 1) {
        setStatus(Plasma::ItemStatus::ActiveStatus);
    } else {
        setStatus(Plasma::ItemStatus::PassiveStatus);
    }

    update();
}

void KeyboardApplet::slotNextLayout()
{
    QList<KKeyboardType> layouts = m_keyboardlayout->layouts();
    if (layouts.size() > 1) {
        layouts.move(0, layouts.size() - 1);
        m_keyboardlayout->setLayouts(layouts);
    }
}

void KeyboardApplet::slotPreviousLayout()
{
    QList<KKeyboardType> layouts = m_keyboardlayout->layouts();
    if (layouts.size() > 1) {
        layouts.move(layouts.size() - 1, 0);
        m_keyboardlayout->setLayouts(layouts);
    }
}

void KeyboardApplet::slotConfigAccepted()
{
    Q_ASSERT(m_indicatorbox);
    const int iconindex = m_indicatorbox->currentIndex();
    m_showflag = (iconindex == 1 || iconindex == 2);
    m_showtext = (iconindex == 0 || iconindex == 2);
    KConfigGroup configgroup = config();
    configgroup.writeEntry("showFlag", m_showflag);
    configgroup.writeEntry("showText", m_showtext);
    emit configNeedsSaving();
    update();
}

#include "moc_keyboard.cpp"
