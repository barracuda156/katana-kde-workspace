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
#include <KSystemTimeZones>
#include <KCModuleInfo>
#include <Plasma/PaintUtils>
#include <Plasma/ToolTipManager>
#include <KDebug>

// the default of KLocale
static const QLocale::FormatType s_timeformat = QLocale::ShortFormat;

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

static QSizeF kClockSize(const QRectF &contentsRect, const QString &clockstring)
{
    QFontMetricsF fontmetricsf(kClockFont(contentsRect));
    QSizeF clocksize = fontmetricsf.size(Qt::TextSingleLine, clockstring);
    if (contentsRect.isNull()) {
        clocksize.setHeight(clocksize.height() * 4);
        clocksize.setWidth(clocksize.width() * 2);
    }
    return clocksize;
}

static int kClockInterval(const QString &format)
{
    if (format.contains(QLatin1String("z"))) {
        // more often that that is overkill
        return 100;
    } else if (format.contains(QLatin1String("s"))) {
        return 1000;
    }
    return 3000;
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
    m_timer->setInterval(kClockInterval(KGlobal::locale()->timeFormat(s_timeformat)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));

    m_clockstring = kClockString();

    adjustSize();
}

void DigitalClockApplet::init()
{
    Plasma::Applet::init();

    m_timer->start();
    Plasma::ToolTipManager::self()->registerWidget(this);

    connect(
        KGlobalSettings::self(), SIGNAL(localeChanged()),
        this, SLOT(slotLocaleChanged())
    );
}

void DigitalClockApplet::paintInterface(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    const QRect &contentsRect)
{
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::Antialiasing);

    painter->drawPixmap(
        contentsRect,
        Plasma::PaintUtils::texturedText(m_clockstring, kClockFont(contentsRect), m_svg)
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

void DigitalClockApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints && Plasma::SizeConstraint || constraints & Plasma::FormFactorConstraint) {
        const QSizeF clocksize = kClockSize(contentsRect(), m_clockstring);
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
                setMinimumSize(kClockSize(QRect(), m_clockstring));
                setPreferredSize(clocksize);
                break;
            }
        }
        emit sizeHintChanged(Qt::MinimumSize);
        emit sizeHintChanged(Qt::PreferredSize);
    }
}

void DigitalClockApplet::changeEvent(QEvent *event)
{
    Plasma::Applet::changeEvent(event);
    switch (event->type()) {
        // the time format depends on the locale, update the sizes and poll timer
        case QEvent::LocaleChange:
        case QEvent::LanguageChange: {
            slotLocaleChanged();
            break;
        }
        default: {
            break;
        }
    }
}

void DigitalClockApplet::updateToolTip()
{
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(i18n("Current Time"));
    QString clocktooltip;
    if (KSystemTimeZones::local() != KTimeZone::utc()) {
        clocktooltip.append(i18n("UTC: %1<br/>", KGlobal::locale()->formatTime(QDateTime::currentDateTimeUtc().time())));
        clocktooltip.append(i18n("Local: %1", m_clockstring));
    } else {
        clocktooltip.append(QString::fromLatin1("<center>%1</center>").arg(m_clockstring));
    }
    plasmatooltip.setSubText(clocktooltip);
    plasmatooltip.setImage(KIcon("clock"));
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltip);
}

void DigitalClockApplet::slotTimeout()
{
    const QString clockstring = kClockString();
    if (clockstring != m_clockstring) {
        m_clockstring = clockstring;
        update();
        updateToolTip();
    }
}

void DigitalClockApplet::slotConfigAccepted()
{
    m_kcmclockproxy->save();
    m_kcmlanguageproxy->save();
}

void DigitalClockApplet::slotLocaleChanged()
{
    // the time format depends on the locale, update the sizes and poll timer
    m_clockstring = kClockString();
    constraintsEvent(Plasma::SizeConstraint);
    m_timer->setInterval(kClockInterval(KGlobal::locale()->timeFormat(s_timeformat)));
    updateToolTip();
}

#include "moc_digitalclock.cpp"
