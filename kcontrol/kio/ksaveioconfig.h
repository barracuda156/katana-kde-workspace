/*
   Copyright (C) 2001 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KSAVEIO_CONFIG_H_
#define KSAVEIO_CONFIG_H_

#include <kprotocolmanager.h>

#include <QWidget>

namespace KSaveIOConfig
{
/* Reload config file (kioslaverc) */
void reparseConfiguration();

/** Timeout Settings */
void setConnectTimeout (int);

void setResponseTimeout (int);

/** Miscellaneous Settings */
void setMarkPartial (bool);

void setMinimumKeepSize (int);

void setAutoResume (bool);

/** Update all running io-slaves */
void updateRunningIOSlaves (QWidget* parent = 0L);

}

#endif
