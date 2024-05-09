/*
 * mousesettings.cpp
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
 *
 * Layout management, enhancements:
 * Copyright (c) 1999 Dirk A. Mueller <dmuell@gmx.net>
 *
 * SC/DC/AutoSelect/ChangeCursor:
 * Copyright (c) 2000 David Faure <faure@kde.org>
 *
 * Double click interval, drag time & dist
 * Copyright (c) 2000 Bernd Gehrmann
 *
 * Large cursor support
 * Visual activation TODO: speed
 * Copyright (c) 2000 Rik Hemsley <rik@kde.org>
 *
 * General/Advanced tabs
 * Copyright (c) 2000 Brad Hughes <bhughes@trolltech.com>
 *
 * redesign for KDE 2.2
 * Copyright (c) 2001 Ralf Nolden <nolden@kde.org>
 *
 * Logitech mouse support
 * Copyright (C) 2004 Brad Hards <bradh@frogmouth.net>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
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

#include "mousesettings.h"

#include <QX11Info>
#include <kconfiggroup.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void MouseSettings::load(KConfig *config)
{
  int accel_num = 0;
  int accel_den = 0;
  int threshold = 0;
  double accel = 0.0;
  XGetPointerControl( QX11Info::display(),
              &accel_num, &accel_den, &threshold );
  accel = float(accel_num) / float(accel_den);

  // get settings from X server
  int h = RIGHT_HANDED;
  unsigned char map[20];
  num_buttons = XGetPointerMapping(QX11Info::display(), map, 20);

  handedEnabled = true;

  // ## keep this in sync with KGlobalSettings::mouseSettings
  if( num_buttons == 1 )
  {
      /* disable button remapping */
      handedEnabled = false;
  }
  else if( num_buttons == 2 )
  {
      if ( (int)map[0] == 1 && (int)map[1] == 2 )
        h = RIGHT_HANDED;
      else if ( (int)map[0] == 2 && (int)map[1] == 1 )
        h = LEFT_HANDED;
      else
        /* custom button setup: disable button remapping */
        handedEnabled = false;
  }
  else
  {
      middle_button = (int)map[1];
      if ( (int)map[0] == 1 && (int)map[2] == 3 )
    h = RIGHT_HANDED;
      else if ( (int)map[0] == 3 && (int)map[2] == 1 )
    h = LEFT_HANDED;
      else
    {
      /* custom button setup: disable button remapping */
      handedEnabled = false;
    }
  }

  KConfigGroup group = config->group("Mouse");
  double a = group.readEntry("Acceleration",-1.0);
  if (a == -1)
    accelRate = accel;
  else
    accelRate = a;

  int t = group.readEntry("Threshold",-1);
  if (t == -1)
    thresholdMove = threshold;
  else
    thresholdMove = t;

  QString key = group.readEntry("MouseButtonMapping");
  if (key == "RightHanded")
    handed = RIGHT_HANDED;
  else if (key == "LeftHanded")
    handed = LEFT_HANDED;
#warning was key == NULL how was this working? is key.isNull() what the coder meant?
  else if (key.isNull())
    handed = h;
  reverseScrollPolarity = group.readEntry( "ReverseScrollPolarity", false);
  m_handedNeedsApply = false;

  // SC/DC/AutoSelect/ChangeCursor
  group = config->group("KDE");
  doubleClickInterval = group.readEntry("DoubleClickInterval", 400);
  dragStartTime = group.readEntry("StartDragTime", 500);
  dragStartDist = group.readEntry("StartDragDist", 4);
  wheelScrollLines = group.readEntry("WheelScrollLines", 3);

  singleClick = group.readEntry("SingleClick", KDE_DEFAULT_SINGLECLICK);
  autoSelectDelay = group.readEntry("AutoSelectDelay", KDE_DEFAULT_AUTOSELECTDELAY);
  changeCursor = group.readEntry("ChangeCursor", KDE_DEFAULT_CHANGECURSOR);
}

void MouseSettings::apply(bool force)
{
  XChangePointerControl( QX11Info::display(),
                         true, true, int(qRound(accelRate*10)), 10, thresholdMove);

  // 256 might seems extreme, but X has already been known to return 32, 
  // and we don't want to truncate things. Xlib limits the table to 256 bytes,
  // so it's a good uper bound..
  unsigned char map[256];
  ::memset(map, 0, 256 * sizeof(unsigned char));
  num_buttons = XGetPointerMapping(QX11Info::display(), map, 256);

  int remap=(num_buttons>=1);
  if (handedEnabled && (m_handedNeedsApply || force)) {
      if( num_buttons == 1 )
      {
          map[0] = (unsigned char) 1;
      }
      else if( num_buttons == 2 )
      {
          if (handed == RIGHT_HANDED)
          {
              map[0] = (unsigned char) 1;
              map[1] = (unsigned char) 3;
          }
          else
          {
              map[0] = (unsigned char) 3;
              map[1] = (unsigned char) 1;
          }
      }
      else // 3 buttons and more
      {
          if (handed == RIGHT_HANDED)
          {
              map[0] = (unsigned char) 1;
              map[1] = (unsigned char) middle_button;
              map[2] = (unsigned char) 3;
          }
          else
          {
              map[0] = (unsigned char) 3;
              map[1] = (unsigned char) middle_button;
              map[2] = (unsigned char) 1;
          }
          if( num_buttons >= 5 )
          {
          // Apps seem to expect logical buttons 4,5 are the vertical wheel.
          // With mice with more than 3 buttons (not including wheel) the physical
          // buttons mapped to logical 4,5 may not be physical 4,5 , so keep
          // this mapping, only possibly reversing them.
              int pos;
              for( pos = 0; pos < num_buttons; ++pos )
                  if( map[pos] == 4 || map[pos] == 5 )
                      break;
              if( pos < num_buttons - 1 )
              {
                  map[pos] = reverseScrollPolarity ? (unsigned char) 5 : (unsigned char) 4;
                  map[pos+1] = reverseScrollPolarity ? (unsigned char) 4 : (unsigned char) 5;
              }
          }
      }
      int retval;
      if (remap)
          while ((retval=XSetPointerMapping(QX11Info::display(), map,
                                            num_buttons)) == MappingBusy)
              /* keep trying until the pointer is free */
          { };
      m_handedNeedsApply = false;
  }

  // This iterates through the various Logitech mice, if we have support.
#ifdef HAVE_LIBUSB
  Q_FOREACH( LogitechMouse *logitechMouse, logitechMouseList ) {
      logitechMouse->applyChanges();
  }
#endif
}

void MouseSettings::save(KConfig *config)
{
  KConfigGroup group = config->group("Mouse");
  group.writeEntry("Acceleration",accelRate);
  group.writeEntry("Threshold",thresholdMove);
  if (handed == RIGHT_HANDED)
      group.writeEntry("MouseButtonMapping",QString("RightHanded"));
  else
      group.writeEntry("MouseButtonMapping",QString("LeftHanded"));
  group.writeEntry( "ReverseScrollPolarity", reverseScrollPolarity );

  group = config->group("KDE");
  group.writeEntry("DoubleClickInterval", doubleClickInterval, KConfig::Persistent|KConfig::Global);
  group.writeEntry("StartDragTime", dragStartTime, KConfig::Persistent|KConfig::Global);
  group.writeEntry("StartDragDist", dragStartDist, KConfig::Persistent|KConfig::Global);
  group.writeEntry("WheelScrollLines", wheelScrollLines, KConfig::Persistent|KConfig::Global);
  group.writeEntry("SingleClick", singleClick, KConfig::Persistent|KConfig::Global);
  group.writeEntry("AutoSelectDelay", autoSelectDelay, KConfig::Persistent|KConfig::Global);
  group.writeEntry("ChangeCursor", changeCursor,KConfig::Persistent|KConfig::Global);
  // This iterates through the various Logitech mice, if we have support.
#ifdef HAVE_LIBUSB
  Q_FOREACH( LogitechMouse *logitechMouse, logitechMouseList ) {
      logitechMouse->save(config);
  }
#endif
  config->sync();
  KGlobalSettings::self()->emitChange(KGlobalSettings::MouseChanged);
}
