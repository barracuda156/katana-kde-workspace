/*
 * Copyright (c) 2008      Harry Bock <hbock@providence.edu>
 * Copyright (c) 2007      Gustavo Pichorim Boiko <gustavo.boiko@kdemail.net>
 * Copyright (c) 2002,2003 Hamish Rodda <rodda@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <config-X11.h>

#include <KLocale>
#include <KConfigGroup>
#include <KDebug>
#include <QApplication>
#include <QDesktopWidget>
#include <QtGui/qx11info_x11.h>

#include "randrdisplay.h"
#include "randrscreen.h"

RandRDisplay::RandRDisplay()
    : m_valid(true),
    m_numScreens(0),
    m_currentScreenIndex(0),
    m_eventBase(0),
    m_errorBase(0)
{
    m_dpy = QX11Info::display();
    
    // Check extension
    if (XRRQueryExtension(m_dpy, &m_eventBase, &m_errorBase) == False) {
        m_valid = false;
        return;
    }

    int major_version = 0;
    int minor_version = 0;
    XRRQueryVersion(m_dpy, &major_version, &minor_version);

    m_version = i18n("X Resize and Rotate extension version %1.%2", major_version, minor_version);
    
    kDebug() << "Using XRANDR extension 1.3 or greater.";
    
    kDebug() << "XRANDR error base: " << m_errorBase;
    m_numScreens = ScreenCount(m_dpy);
    m_currentScreenIndex = 0;

    // set the timestamp to 0
    RandR::timestamp = 0;

    // This assumption is WRONG with Xinerama
    // Q_ASSERT(QApplication::desktop()->numScreens() == ScreenCount(QX11Info::display()));

    for (int i = 0; i < m_numScreens; i++) {
        m_screens.append(new RandRScreen(i));
    }
    setCurrentScreen(DefaultScreen(QX11Info::display()));
}

RandRDisplay::~RandRDisplay()
{
    qDeleteAll(m_screens);
}

bool RandRDisplay::isValid() const
{
    return m_valid;
}

const QString& RandRDisplay::errorCode() const
{
    return m_errorCode;
}

int RandRDisplay::eventBase() const
{
    return m_eventBase;
}

int RandRDisplay::errorBase() const
{
    return m_errorBase;
}

const QString& RandRDisplay::version() const
{
    return m_version;
}

void RandRDisplay::setCurrentScreen(int index)
{
    Q_ASSERT(index < ScreenCount(m_dpy));
    m_currentScreenIndex = index;
}

int RandRDisplay::screenIndexOfWidget(QWidget* widget)
{
    // int ret = QApplication::desktop()->screenNumber(widget);
    // return ret != -1 ? ret : QApplication::desktop()->primaryScreen();
    
    // get info from Qt's X11 info directly; QDesktopWidget seems to use
    // Xinerama by default, which doesn't work properly with randr.
    // It will return more screens than exist for the display, causing
    // a crash in the screen/currentScreen methods.
    if (widget) {
        return widget->x11Info().screen();
    }

    return -1;
}

int RandRDisplay::currentScreenIndex() const
{
    return m_currentScreenIndex;
}

bool RandRDisplay::needsRefresh() const
{
    Time time, config_timestamp;
    time = XRRTimes(m_dpy, m_currentScreenIndex, &config_timestamp);
    
    kDebug() << "Cache:" << RandR::timestamp << "Server:" << time << "Config:" << config_timestamp;
    return (RandR::timestamp < time);
}

void RandRDisplay::refresh()
{
    for (int i = 0; i < m_screens.count(); ++i) {
        RandRScreen* s = m_screens.at(i);
        s->loadSettings();
    }
}

bool RandRDisplay::canHandle(const XEvent *e) const 
{
    if (e->type == m_eventBase + RRScreenChangeNotify) {
        return true;
    } else if (e->type == m_eventBase + RRNotify) {
        return true;
    }
    return false;
}


void RandRDisplay::handleEvent(XEvent *e)
{
    if (e->type == m_eventBase + RRScreenChangeNotify) {
        XRRScreenChangeNotifyEvent *event = (XRRScreenChangeNotifyEvent*)(e);
        for (int i=0; i < m_screens.count(); ++i) {
            RandRScreen *screen = m_screens.at(i);
            if (screen->rootWindow() == event->root) {
                screen->handleEvent(event);
            }
        }
    } else if (e->type == m_eventBase + RRNotify) {
        // forward the event to the right screen
        XRRNotifyEvent *event = (XRRNotifyEvent*)e;
        for (int i=0; i < m_screens.count(); ++i) {
            RandRScreen *screen = m_screens.at(i);
            if (screen->rootWindow() == event->window ) {
                screen->handleRandREvent(event);
            }
        }
    }
}

int RandRDisplay::numScreens() const
{
    Q_ASSERT(ScreenCount(QX11Info::display()) == m_numScreens);
    return m_numScreens;
}

RandRScreen* RandRDisplay::screen(int index)
{
    return m_screens.at(index);
}

RandRScreen* RandRDisplay::currentScreen()
{
    return m_screens.at(m_currentScreenIndex);
}

bool RandRDisplay::applyOnStartup(KConfig& config)
{
    return config.group("Display").readEntry("ApplyOnStartup", false);
}

bool RandRDisplay::syncTrayApp(KConfig& config)
{
    return config.group("Display").readEntry("SyncTrayApp", false);
}

// to be used during desktop startup, make all screens provide the shell commands
// (using xrandr cli tool), save them here and the KCM will execute these commands
// early during desktop startup
void RandRDisplay::saveStartup(KConfig& config)
{
    KConfigGroup group = config.group("Display");
    group.writeEntry("ApplyOnStartup", true);
    QStringList commands;
    foreach(const RandRScreen *s, m_screens) {
        commands += s->startupCommands();
    }
    group.writeEntry("StartupCommands", commands.join( "\n"));
}

void RandRDisplay::disableStartup(KConfig& config)
{
    KConfigGroup group = config.group("Display");
    group.writeEntry("ApplyOnStartup", false);
    group.deleteEntry("StartupCommands");
}

void RandRDisplay::applyProposed(bool confirm)
{

    foreach(RandRScreen *s, m_screens) {
        s->applyProposed(confirm);
    }
}

// vim:noet:sts=8:sw=8:
