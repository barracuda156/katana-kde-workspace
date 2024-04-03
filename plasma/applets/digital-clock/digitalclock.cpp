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

#include "digitalclock.h"

#include <QApplication>
#include <QClipboard>
#include <KGlobal>
#include <KLocale>
#include <KCModuleInfo>
#include <Plasma/Svg>
#include <Plasma/PaintUtils>
#include <KDebug>

static QFont kClockFont(const QRectF &contentsRect)
{
    QFont font = KGlobalSettings::smallestReadableFont();
    font.setBold(true);
    if (!contentsRect.isNull()) {
        font.setPointSize(qMax(qreal(font.pointSize()), contentsRect.height() / 2));
    }
    return font;
}

static QString kClockString()
{
    return KGlobal::locale()->formatTime(QTime::currentTime());
}

static QSizeF kClockSize(const QRectF &contentsRect)
{
    QFontMetricsF fontmetricsf(kClockFont(contentsRect));
    QSizeF clocksize = fontmetricsf.size(Qt::TextSingleLine, kClockString());
    if (contentsRect.isNull()) {
        clocksize.setHeight(clocksize.height() * 4);
        clocksize.setWidth(clocksize.width() * 2);
    }
    return clocksize;
}

DigitalClockApplet::DigitalClockApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_svg(nullptr),
    m_timer(nullptr),
    m_kcmclockproxy(nullptr),
    m_kcmlanguageproxy(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_dig_clock");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);

    m_svg = new Plasma::Svg(this);
    m_svg->setImagePath("widgets/labeltexture");
    m_svg->setContainsMultipleImages(true);

    m_timer = new QTimer(this);
    // even if the time format contains ms polling and repainting more often that 1sec is overkill
    m_timer->setInterval(1000);
    connect(
        m_timer, SIGNAL(timeout()),
        this, SLOT(slotTimeout())
    );

    m_menu = new KMenu(i18n("C&opy to Clipboard"));
    m_menu->setIcon(KIcon("edit-copy"));
    connect(m_menu, SIGNAL(triggered(QAction*)), this, SLOT(slotCopyToClipboard(QAction*)));
}

void DigitalClockApplet::init()
{
    m_timer->start();
}

void DigitalClockApplet::paintInterface(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    const QRect &contentsRect)
{
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::Antialiasing);

    painter->drawPixmap(
        contentsRect,
        Plasma::PaintUtils::texturedText(kClockString(), kClockFont(contentsRect), m_svg)
    );
}

void DigitalClockApplet::createConfigurationInterface(KConfigDialog *parent)
{
    m_kcmclockproxy = new KCModuleProxy("clock");
    parent->addPage(
        m_kcmclockproxy, m_kcmclockproxy->moduleInfo().moduleName(),
        m_kcmclockproxy->moduleInfo().icon()
    );

    m_kcmlanguageproxy = new KCModuleProxy("language");
    parent->addPage(
        m_kcmlanguageproxy, m_kcmlanguageproxy->moduleInfo().moduleName(),
        m_kcmlanguageproxy->moduleInfo().icon()
    );

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_kcmclockproxy, SIGNAL(changed(bool)), parent, SLOT(settingsModified()));
    connect(m_kcmlanguageproxy, SIGNAL(changed(bool)), parent, SLOT(settingsModified()));
}

QList<QAction*> DigitalClockApplet::contextualActions()
{
    const QDateTime datetime = QDateTime::currentDateTime();
    const QDate date = datetime.date();
    const QTime time = datetime.time();

    m_menu->clear();
    m_menu->addAction(KGlobal::locale()->formatDate(date, QLocale::LongFormat));
    m_menu->addAction(KGlobal::locale()->formatDate(date, QLocale::ShortFormat));

    QAction* separator0 = new QAction(this);
    separator0->setSeparator(true);
    m_menu->addAction(separator0);

    m_menu->addAction(KGlobal::locale()->formatTime(time, QLocale::LongFormat));
    m_menu->addAction(KGlobal::locale()->formatTime(time, QLocale::ShortFormat));

    QAction* separator1 = new QAction(this);
    separator1->setSeparator(true);
    m_menu->addAction(separator1);

    m_menu->addAction(KGlobal::locale()->formatDateTime(datetime, QLocale::LongFormat));
    m_menu->addAction(KGlobal::locale()->formatDateTime(datetime, QLocale::ShortFormat));
    m_menu->addAction(KGlobal::locale()->formatDateTime(datetime, QLocale::NarrowFormat));

    QList<QAction*> actions;
    actions.append(m_menu->menuAction());
    return actions;
}

void DigitalClockApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints && Plasma::SizeConstraint || constraints & Plasma::FormFactorConstraint) {
        const QSizeF clocksize = kClockSize(contentsRect());
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal:
            case Plasma::FormFactor::Vertical: {
                // panel
                setMinimumSize(0, 0);
                setPreferredSize(clocksize);
                break;
            }
            default: {
                // desktop-like
                setMinimumSize(kClockSize(QRect()));
                setPreferredSize(clocksize);
                break;
            }
        }
    }
}

void DigitalClockApplet::changeEvent(QEvent *event)
{
    Plasma::Applet::changeEvent(event);
    switch (event->type()) {
        // the time format depends on the locale, update the sizes
        case QEvent::LocaleChange:
        case QEvent::LanguageChange: {
            constraintsEvent(Plasma::SizeConstraint);
            break;
        }
        default: {
            break;
        }
    }
}

void DigitalClockApplet::slotTimeout()
{
    update();
}

void DigitalClockApplet::slotConfigAccepted()
{
    m_kcmclockproxy->save();
    m_kcmlanguageproxy->save();
}

void DigitalClockApplet::slotCopyToClipboard(QAction *action)
{
    QString actiontext = action->text();
    actiontext.remove(QChar('&'));
    QApplication::clipboard()->setText(actiontext);
}

#include "moc_digitalclock.cpp"
