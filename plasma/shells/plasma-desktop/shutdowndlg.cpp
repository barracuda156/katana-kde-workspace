/*
    This file is part of the KDE project
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

#include "shutdowndlg.h"
#include "kworkspace/kdisplaymanager.h"

#include <QX11Info>
#include <klocale.h>
#include <kwindowsystem.h>
#include <kdialog.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

static const int s_timeout = 10; // 10secs
static const QSizeF s_iconsize = QSizeF(64, 64);
static const QSizeF s_chooseminumumsize = QSizeF(280, 130);
static const QSizeF s_nochooseminumumsize = QSizeF(280, 80);

static bool kSwitchTitleEvent(QEvent *event)
{
    switch (event->type()) {
        case QEvent::HoverEnter:
        case QEvent::GraphicsSceneHoverEnter: {
            return true;
        }
        default: {
            return false;
        }
    }
    Q_UNREACHABLE();
}

KSMShutdownDlg::KSMShutdownDlg(QWidget* parent,
                               bool maysd, bool choose, KWorkSpace::ShutdownType sdtype)
    : Plasma::Dialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint),
    m_scene(nullptr),
    m_widget(nullptr),
    m_layout(nullptr),
    m_titlelabel(nullptr),
    m_separator(nullptr),
    m_logoutwidget(nullptr),
    m_rebootwidget(nullptr),
    m_haltwidget(nullptr),
    m_okbutton(nullptr),
    m_cancelbutton(nullptr),
    m_eventloop(nullptr),
    m_timer(nullptr),
    m_second(s_timeout),
    m_shutdownType(sdtype)
{
    if (KWindowSystem::compositingActive()) {
        // Announce that the user MAY be logging out (Intended for the compositor)
        Display* dpy = QX11Info::display();
        Atom announce = XInternAtom(dpy, "_KDE_LOGGING_OUT", False);
        unsigned char dummy = 0;
        XChangeProperty(dpy, QX11Info::appRootWindow(), announce, announce, 8, PropModeReplace, &dummy, 1);
    }

    m_scene = new QGraphicsScene(this);
    m_widget = new QGraphicsWidget();

    m_layout = new QGraphicsGridLayout(m_widget);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);

    if (!choose) {
        m_widget->setMinimumSize(s_nochooseminumumsize);

        m_titlelabel = new Plasma::Label(m_widget);
        m_titlelabel->setAlignment(Qt::AlignCenter);
        m_titlelabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_layout->addItem(m_titlelabel, 0, 0, 1, 2);

        switch (m_shutdownType) {
            case KWorkSpace::ShutdownTypeReboot: {
                m_titlelabel->setText(
                    i18np("Restarting computer in 1 second.", "Restarting computer in %1 seconds.", m_second)
                );
                break;
            }
            case KWorkSpace::ShutdownTypeHalt: {
                m_titlelabel->setText(
                    i18np("Turning off computer in 1 second.", "Turning off computer in %1 seconds.", m_second)
                );
                break;
            }
            default: {
                m_titlelabel->setText(
                    i18np("Logging out in 1 second.", "Logging out in %1 seconds.", m_second)
                );
                break;
            }
        }

        m_okbutton = new Plasma::PushButton(m_widget);
        m_okbutton->setText(i18n("&OK"));
        m_okbutton->setIcon(KIcon("dialog-ok"));
        m_okbutton->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        m_okbutton->installEventFilter(this);
        connect(
            m_okbutton, SIGNAL(released()),
            this, SLOT(slotOk())
        );
        m_layout->addItem(m_okbutton, 1, 0, 1, 1);

        m_cancelbutton = new Plasma::PushButton(m_widget);
        m_cancelbutton->setText(i18n("&Cancel"));
        m_cancelbutton->setIcon(KIcon("dialog-cancel"));
        m_cancelbutton->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        m_cancelbutton->installEventFilter(this);
        connect(
            m_cancelbutton, SIGNAL(released()),
            this, SLOT(slotCancel())
        );
        m_layout->addItem(m_cancelbutton, 1, 1, 1, 1);
        m_layout->setRowMaximumHeight(1, m_cancelbutton->preferredSize().height() + m_layout->verticalSpacing());

        slotTimeout();
        m_timer = new QTimer(this);
        m_timer->setInterval(1000);
        connect(
            m_timer, SIGNAL(timeout()),
            this, SLOT(slotTimeout())
        );
        m_timer->start();
    } else {
        m_widget->setMinimumSize(s_chooseminumumsize);

        int buttonscount = 0;

        m_titlelabel = new Plasma::Label(m_widget);
        m_titlelabel->setAlignment(Qt::AlignCenter);
        m_titlelabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_titlelabel->setText(i18n("Choose"));
        m_separator = new Plasma::Separator(m_widget);

        m_logoutwidget = new Plasma::IconWidget(m_widget);
        m_logoutwidget->setPreferredIconSize(s_iconsize);
        m_logoutwidget->setIcon(KIcon("system-log-out"));
        m_logoutwidget->installEventFilter(this);
        connect(
            m_logoutwidget, SIGNAL(clicked()),
            this, SLOT(slotLogout())
        );
        m_layout->addItem(m_logoutwidget, 2, buttonscount);
        buttonscount++;

        if (maysd) {
            m_rebootwidget = new Plasma::IconWidget(m_widget);
            m_rebootwidget->setPreferredIconSize(s_iconsize);
            m_rebootwidget->setIcon(KIcon("system-reboot"));
            m_rebootwidget->installEventFilter(this);
            connect(
                m_rebootwidget, SIGNAL(clicked()),
                this, SLOT(slotReboot())
            );
            m_layout->addItem(m_rebootwidget, 2, buttonscount);
            buttonscount++;

            m_haltwidget = new Plasma::IconWidget(m_widget);
            m_haltwidget->setPreferredIconSize(s_iconsize);
            m_haltwidget->setIcon(KIcon("system-shutdown"));
            m_haltwidget->installEventFilter(this);
            connect(
                m_haltwidget, SIGNAL(clicked()),
                this, SLOT(slotHalt())
            );
            m_layout->addItem(m_haltwidget, 2, buttonscount);
            buttonscount++;
        }

        m_cancelbutton = new Plasma::PushButton(m_widget);
        m_cancelbutton->setText(i18n("&Cancel"));
        m_cancelbutton->setIcon(KIcon("dialog-cancel"));
        m_cancelbutton->installEventFilter(this);
        connect(
            m_cancelbutton, SIGNAL(released()),
            this, SLOT(slotCancel())
        );
        m_layout->addItem(m_cancelbutton, 3, 0, 1, buttonscount);
        m_layout->setRowMaximumHeight(3, m_cancelbutton->preferredSize().height() + m_layout->horizontalSpacing());

        m_layout->addItem(m_titlelabel, 0, 0, 1, buttonscount);
        m_layout->addItem(m_separator, 1, 0, 1, buttonscount);
    }

    m_widget->setLayout(m_layout);

    m_scene->addItem(m_widget);
    setGraphicsWidget(m_widget);

    setFocus(Qt::ActiveWindowFocusReason);
    m_scene->installEventFilter(this);

    if (!choose) {
        m_cancelbutton->setFocus();
    } else if (sdtype == KWorkSpace::ShutdownTypeNone && m_logoutwidget) {
        m_logoutwidget->setFocus();
    } else if (sdtype == KWorkSpace::ShutdownTypeReboot && m_rebootwidget) {
        m_rebootwidget->setFocus();
    } else if (sdtype == KWorkSpace::ShutdownTypeHalt && m_haltwidget) {
        m_haltwidget->setFocus();
    }

    adjustSize();
    KDialog::centerOnScreen(this, -3);
}

KSMShutdownDlg::~KSMShutdownDlg()
{
    if (KWindowSystem::compositingActive()) {
        // No longer logging out, announce (Intended for the compositor)
        Display* dpy = QX11Info::display();
        Atom announce = XInternAtom(dpy, "_KDE_LOGGING_OUT", False);
        XDeleteProperty(QX11Info::display(), QX11Info::appRootWindow(), announce);
    }
    // delete m_widget;
}

void KSMShutdownDlg::hideEvent(QHideEvent *event)
{
    interrupt();
    Plasma::Dialog::hideEvent(event);
}

bool KSMShutdownDlg::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scene && event->type() == QEvent::WindowDeactivate) {
        interrupt();
    } else if (!m_timer && watched == m_logoutwidget && kSwitchTitleEvent(event)) {
        m_titlelabel->setText(i18n("Logout"));
    } else if (!m_timer && watched == m_rebootwidget && kSwitchTitleEvent(event)) {
        m_titlelabel->setText(i18n("Restart Computer"));
    } else if (!m_timer && watched == m_haltwidget && kSwitchTitleEvent(event)) {
        m_titlelabel->setText(i18n("Halt Computer"));
    } else if (!m_timer && watched == m_okbutton && kSwitchTitleEvent(event)) {
        m_titlelabel->setText(i18n("OK"));
    } else if (!m_timer && watched == m_cancelbutton && kSwitchTitleEvent(event)) {
        m_titlelabel->setText(i18n("Cancel"));
    }
    return Plasma::Dialog::eventFilter(watched, event);
}

void KSMShutdownDlg::slotLogout()
{
    m_shutdownType = KWorkSpace::ShutdownTypeNone;
    m_eventloop->exit(0);
}

void KSMShutdownDlg::slotReboot()
{
    m_shutdownType = KWorkSpace::ShutdownTypeReboot;
    m_eventloop->exit(0);
}

void KSMShutdownDlg::slotHalt()
{
    m_shutdownType = KWorkSpace::ShutdownTypeHalt;
    m_eventloop->exit(0);
}

void KSMShutdownDlg::slotOk()
{
    m_second = 0;
}

void KSMShutdownDlg::slotCancel()
{
    interrupt();
}

void KSMShutdownDlg::slotTimeout()
{
    switch (m_shutdownType) {
        case KWorkSpace::ShutdownTypeReboot: {
            m_titlelabel->setText(
                i18np("Restarting computer in 1 second.", "Restarting computer in %1 seconds.", m_second)
            );
            m_second--;
            if (m_second <= -1) {
                slotReboot();
            }
            break;
        }
        case KWorkSpace::ShutdownTypeHalt: {
            m_titlelabel->setText(
                i18np("Turning off computer in 1 second.", "Turning off computer in %1 seconds.", m_second)
            );
            m_second--;
            if (m_second <= -1) {
                slotHalt();
            }
            break;
        }
        default: {
            m_titlelabel->setText(
                i18np("Logging out in 1 second.", "Logging out in %1 seconds.", m_second)
            );
            m_second--;
            if (m_second <= -1) {
                slotLogout();
            }
            break;
        }
    }
}

bool KSMShutdownDlg::execDialog()
{
    KWindowSystem::setState(winId(), NET::SkipPager | NET::SkipTaskbar);
    animatedShow(Plasma::locationToDirection(Plasma::Location::Desktop));
    Q_ASSERT(!m_eventloop);
    m_eventloop = new QEventLoop(this);
    return (m_eventloop->exec() == 0);
}

void KSMShutdownDlg::interrupt()
{
    if (!m_eventloop) {
        return;
    }
    m_eventloop->exit(1);
    m_eventloop->deleteLater();
    m_eventloop = nullptr;
}

bool KSMShutdownDlg::confirmShutdown(bool maysd, bool choose, KWorkSpace::ShutdownType &sdtype)
{
    KSMShutdownDlg* dialog = new KSMShutdownDlg(nullptr, maysd, choose, sdtype);

    // NOTE: KWin logout effect expects class hint values to be plasma-desktop
    XClassHint classHint;
    classHint.res_name = const_cast<char*>("plasma-desktop");
    classHint.res_class = const_cast<char*>("plasma-desktop");
    XSetClassHint(QX11Info::display(), dialog->winId(), &classHint);

    dialog->setWindowRole("logoutdialog");

    bool result = dialog->execDialog();
    sdtype = dialog->m_shutdownType;

    delete dialog;

    return result;
}

#include "moc_shutdowndlg.cpp"