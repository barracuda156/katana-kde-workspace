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

#include "luna.h"

#include <KIconLoader>
#include <Plasma/ToolTipManager>
#include <KDebug>

// for reference:
// https://en.wikipedia.org/wiki/Lunar_phase
// https://stardate.org/moon-phase-calculator
// https://www.timeanddate.com/astronomy/moon/phases.html
static const qreal s_moonmonth = 29.53059;

static int kLunaPhase()
{
    static const QDateTime firsnewmoon = QDateTime(QDate(1999, 8, 11), QTime(0, 0, 0), Qt::UTC);
    const QDateTime now = QDateTime::currentDateTime();
    const qreal gregoriandays = qreal(-now.daysTo(firsnewmoon.toLocalTime()));
    qreal moondays = 0;
    while ((moondays + s_moonmonth) < gregoriandays) {
        moondays += s_moonmonth;
    }
    const int roundmoonphase = qRound(gregoriandays - moondays);
    // qDebug() << Q_FUNC_INFO << now << roundmoonphase;
    return roundmoonphase;
}

static QString kLunaPhaseString()
{
    const int phase = kLunaPhase();
    switch (phase) {
        case 0: {
            return i18n("New Moon");
        }
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: {
            return i18n("Waxing Crescent");
        }
        case 7: {
            return i18n("First Quarter");
        }
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14: {
            return i18n("Waxing Gibbous");
        }
        case 15: {
            return i18n("Full Moon");
        }
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22: {
            return i18n("Waning Gibbous");
        }
        case 23: {
            return i18n("Last Quarter");
        }
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29: {
            return i18n("Waning Crescent");
        }
        default: {
            kWarning() << "invalid moon phase" << phase;
            break;
        }
    }
    return QString();
}

LunaApplet::LunaApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_svg(nullptr),
    m_timer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_luna");
    setAspectRatioMode(Plasma::AspectRatioMode::Square);
    setBackgroundHints(Plasma::Applet::NoBackground);

    m_svg = new Plasma::Svg(this);
    m_svg->setImagePath("widgets/luna");
    m_svg->setContainsMultipleImages(true);

    m_timer = new QTimer(this);
    // 3sec to account for localtime changes for example
    m_timer->setInterval(3000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
}

void LunaApplet::init()
{
    m_timer->start();
    Plasma::ToolTipManager::self()->registerWidget(this);
}

void LunaApplet::paintInterface(QPainter *painter,
                                    const QStyleOptionGraphicsItem *option,
                                    const QRect &contentsRect)
{
    Q_UNUSED(option);

    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::Antialiasing);

    m_svg->paint(painter, contentsRect, QString::number(kLunaPhase()));
}

void LunaApplet::constraintsEvent(Plasma::Constraints constraints)
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

void LunaApplet::slotTimeout()
{
    update();
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(kLunaPhaseString());
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltip);
}

#include "moc_luna.cpp"
