/* This file is part of the KDE libraries
    Copyright (C) 1997 Matthias Kalle Dalheimer (kalle@kde.org)

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

#ifndef KWORKSPACE_H
#define KWORKSPACE_H

#include "kworkspace_export.h"

namespace KWorkSpace
{
 
  /**
   * The possible values for the @p confirm parameter of requestShutDown().
   */
  enum ShutdownConfirm {
    /**
     * Obey the user's confirmation setting.
     */
    ShutdownConfirmDefault = -1,
    /**
     * Don't confirm, shutdown without asking.
     */
    ShutdownConfirmNo = 0,
    /**
     * Always confirm, ask even if the user turned it off.
     */
    ShutdownConfirmYes = 1
  };

  /**
   * The possible values for the @p sdtype parameter of requestShutDown().
   */
  enum ShutdownType {
    /**
     * Select previous action or the default if it's the first time.
     */
    ShutdownTypeDefault = -1,
    /**
     * Only log out
     */
    ShutdownTypeNone = 0,
    /**
     * Log out and reboot the machine.
     */
    ShutdownTypeReboot = 1,
    /**
     * Log out and halt the machine.
     */
    ShutdownTypeHalt = 2
  };

  /**
   * Asks the session manager to shut the session down.
   *
   * Using @p confirm == ShutdownConfirmYes or @p sdtype != ShutdownTypeDefault
   * causes the use of plasma's D-Bus interface. The remaining two
   * combinations use the standard XSMP and will work with any session manager
   * compliant with it.
   *
   * @param confirm Whether to ask the user if he really wants to log out.
   * ShutdownConfirm
   * @param sdtype The action to take after logging out. ShutdownType
   */
  KWORKSPACE_EXPORT void requestShutDown(ShutdownConfirm confirm = ShutdownConfirmDefault,
                                         ShutdownType sdtype = ShutdownTypeDefault);

  /**
   * Used to check whether a requestShutDown call with the same arguments
   * has any chance of succeeding.
   *
   * For example, if KDE's own session manager cannot be contacted, we can't
   * demand that the computer be shutdown, or force a confirmation dialog.
   *
   * Even if we can access the KDE session manager, the system or user
   * configuration may prevent the user from requesting a shutdown or
   * reboot.
   */
  KWORKSPACE_EXPORT bool canShutDown(ShutdownConfirm confirm = ShutdownConfirmDefault,
                                     ShutdownType sdtype = ShutdownTypeDefault);
}

#endif
