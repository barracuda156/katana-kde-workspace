/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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

#include "powerdevilprofilegenerator.h"

#include <PowerDevilSettings.h>

#include <QtCore/QFile>

#include <Solid/Device>
#include <Solid/Battery>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KNotification>
#include <KIcon>
#include <KStandardDirs>

namespace PowerDevil {

ProfileGenerator::GeneratorResult ProfileGenerator::generateProfiles(bool toRam, bool toDisk)
{
    // Let's change some defaults
    if (!toRam) {
        if (!toDisk) {
            PowerDevilSettings::setBatteryCriticalAction(0);
        } else {
            PowerDevilSettings::setBatteryCriticalAction(2);
        }
    }

    // Ok, let's get our config file.
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig);

    // And clear it
    foreach (const QString &group, profilesConfig->groupList()) {
        // Don't delete activity-specific settings
        if (group != "Activities") {
            profilesConfig->deleteGroup(group);
        }
    }

    // Let's start: AC profile before anything else
    KConfigGroup acProfile(profilesConfig, "AC");
    acProfile.writeEntry("icon", "battery-charging");

    // We want to dim the screen after a while, definitely
    {
        KConfigGroup dimDisplay(&acProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 300000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&acProfile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", LogoutDialogMode);
        if (toRam) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    }

    // And we also want to turn off the screen after another while
    {
        KConfigGroup dpmsControl(&acProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 600);
    }

    // Easy part done. Now, any batteries?
    bool hasBattery = false;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->isPowerSupply() && (b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery)) {
            hasBattery = true;
            break;
        }
    }

    if (hasBattery) {
        // Then we want to handle brightness in performance.
        {
            KConfigGroup brightnessControl(&acProfile, "BrightnessControl");
            brightnessControl.writeEntry< int >("value", 100);
        }
    }

    // Powersave
    KConfigGroup batteryProfile(profilesConfig, "Battery");
    batteryProfile.writeEntry("icon", "battery-060");
    // Less brightness.
    {
        KConfigGroup brightnessControl(&batteryProfile, "BrightnessControl");
        brightnessControl.writeEntry< int >("value", 60);
    }
    // We want to dim the screen after a while, definitely
    {
        KConfigGroup dimDisplay(&batteryProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 120000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&batteryProfile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", LogoutDialogMode);
        if (toRam) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    }
    // We want to turn off the screen after another while
    {
        KConfigGroup dpmsControl(&batteryProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 300);
    }
    // Last but not least, we want to suspend after a rather long period of inactivity
    if (toRam) {
        KConfigGroup suspendSession(&batteryProfile, "SuspendSession");
        suspendSession.writeEntry< uint >("idleTime", 600000);
        suspendSession.writeEntry< uint >("suspendType", ToRamMode);
    }


    // Ok, now for aggressive powersave
    KConfigGroup lowBatteryProfile(profilesConfig, "LowBattery");
    lowBatteryProfile.writeEntry("icon", "battery-low");
    // Less brightness.
    {
        KConfigGroup brightnessControl(&lowBatteryProfile, "BrightnessControl");
        brightnessControl.writeEntry< int >("value", 30);
    }
    // We want to dim the screen after a while, definitely
    {
        KConfigGroup dimDisplay(&lowBatteryProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 60000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&lowBatteryProfile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", LogoutDialogMode);
        if (toRam) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    }
    // We want to turn off the screen after another while
    {
        KConfigGroup dpmsControl(&lowBatteryProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 120);
    }
    // Last but not least, we want to suspend after a rather long period of inactivity
    if (toRam) {
        KConfigGroup suspendSession(&lowBatteryProfile, "SuspendSession");
        suspendSession.writeEntry< uint >("idleTime", 300000);
        suspendSession.writeEntry< uint >("suspendType", ToRamMode);
    }

    // Save and be happy
    profilesConfig->sync();

    return ResultGenerated;
}

}
