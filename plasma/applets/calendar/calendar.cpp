/***************************************************************************
 *   Copyright 2008 by Davide Bettio <davide.bettio@kdemail.net>           *
 *   Copyright 2009 by John Layt <john@layt.net>                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "calendar.h"

#include <QGraphicsLinearLayout>
#include <QPainter>
#include <KCalendarWidget>
#include <KSystemTimeZones>
#include <KIcon>
#include <Plasma/Theme>
#include <Plasma/CalendarWidget>
#include <Plasma/ToolTipManager>
#include <KDebug>

static int kGetDay()
{
    return QDate::currentDate().day();
}

class CalendarWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    CalendarWidget(QGraphicsWidget *parent);

    void showToday();

private:
    QGraphicsLinearLayout* m_layout;
    Plasma::CalendarWidget* m_plasmacalendar;
    KCalendarWidget* m_nativewidget;
};

CalendarWidget::CalendarWidget(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    m_layout(nullptr),
    m_plasmacalendar(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_plasmacalendar = new Plasma::CalendarWidget(this);
    m_plasmacalendar->setMinimumSize(QSize(300, 250));
    m_plasmacalendar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_nativewidget = m_plasmacalendar->nativeWidget();
    // changing the date on the calendar does not make sense
    m_nativewidget->setSelectionMode(QCalendarWidget::NoSelection);
    m_nativewidget->setDateEditEnabled(false);
    m_layout->addItem(m_plasmacalendar);
    setLayout(m_layout);
}

void CalendarWidget::showToday()
{
    m_nativewidget->showToday();
}


CalendarApplet::CalendarApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_calendarwidget(nullptr),
    m_svg(nullptr),
    m_timer(nullptr),
    m_day(-1)
{
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setPopupIcon("view-pim-calendar");

    m_calendarwidget = new CalendarWidget(this);

    m_svg = new Plasma::Svg(this);
    m_svg->setImagePath("calendar/mini-calendar");
    m_svg->setContainsMultipleImages(true);

    m_timer = new QTimer(this);
    // 3sec to account for localtime changes for example
    m_timer->setInterval(3000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slotCheckDate()));
}

void CalendarApplet::init()
{
    slotCheckDate();
    m_timer->start();
    Plasma::ToolTipManager::self()->registerWidget(this);
}

void CalendarApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint || constraints & Plasma::SizeConstraint) {
        paintIcon();
    }
}

QGraphicsWidget *CalendarApplet::graphicsWidget()
{
    return m_calendarwidget;
}

void CalendarApplet::popupEvent(const bool show)
{
    if (show) {
        m_calendarwidget->showToday();
    }
}

void CalendarApplet::slotCheckDate()
{
    const int today = kGetDay();
    if (today != m_day) {
        m_day = today;
        kDebug() << "updating calendar icon" << m_day << today;
        paintIcon();
    }

    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(i18n("Current Date"));
    const QString calendarstring = KGlobal::locale()->formatDate(QDateTime::currentDateTime().date());
    QString calendartooltip;
    if (KSystemTimeZones::local() != KTimeZone::utc()) {
        calendartooltip.append(i18n("UTC: %1<br/>", KGlobal::locale()->formatDate(QDateTime::currentDateTimeUtc().date())));
        calendartooltip.append(i18n("Local: %1", calendarstring));
    } else {
        calendartooltip.append(i18n("<center>%1</center>", calendarstring));
    }
    plasmatooltip.setSubText(calendartooltip);
    plasmatooltip.setImage(KIcon("office-calendar"));
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltip);
}

void CalendarApplet::paintIcon()
{
    const int iconSize = qMin(size().width(), size().height());

    if (iconSize <= 0) {
        return;
    }

    QPixmap icon(iconSize, iconSize);
    icon.fill(Qt::transparent);
    QPainter p(&icon);

    m_svg->paint(&p, icon.rect(), "mini-calendar");

    QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);
    p.setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::ButtonTextColor));
    font.setPixelSize(icon.height() / 2);
    p.setFont(font);
    p.drawText(
        icon.rect().adjusted(0, icon.height() / 4, 0, 0), Qt::AlignCenter,
        QString::number(kGetDay())
    );
    m_svg->resize();
    p.end();
    setPopupIcon(icon);
}

#include "moc_calendar.cpp"
#include "calendar.moc"
