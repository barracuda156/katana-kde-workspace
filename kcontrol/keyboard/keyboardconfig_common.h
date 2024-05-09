/*  This file is part of the KDE project
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

#ifndef KEYBOARDCONFIG_COMMON_H
#define KEYBOARDCONFIG_COMMON_H

#include <QX11Info>
#include <qmath.h>
#include <kkeyboardlayout.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

static const float s_defaultrepeatdelay = 660.0;
static const float s_defaultrepeatrate = 25.0;

static QList<KKeyboardType> kLayoutsFromConfig()
{
    KConfig kconfig("kcminputrc", KConfig::NoGlobals);
    KConfigGroup kconfiggroup(&kconfig, "Keyboard");
    const KKeyboardType defaultlayout = KKeyboardLayout::defaultLayout();
    const QByteArray layoutsmodel = kconfiggroup.readEntry("LayoutsModel", defaultlayout.model);
    const QByteArray layoutsoptions = kconfiggroup.readEntry("LayoutsOptions", defaultlayout.option);
    const QStringList layoutslayouts = kconfiggroup.readEntry(
        "LayoutsLayouts",
        QStringList() << QString::fromLatin1(defaultlayout.layout.constData(), defaultlayout.layout.size())
    );
    const QStringList layoutsvariants = kconfiggroup.readEntry(
        "LayoutsVariants",
        QStringList() << QString::fromLatin1(defaultlayout.variant.constData(), defaultlayout.variant.size())
    );
    QList<KKeyboardType> result;
    for (int i = 0; i < layoutslayouts.size(); i++) {
        KKeyboardType kkeyboardtype;
        kkeyboardtype.model = layoutsmodel;
        kkeyboardtype.layout = layoutslayouts.at(i).toLatin1();
        if (i < layoutsvariants.size()) {
            kkeyboardtype.variant = layoutsvariants.at(i).toLatin1();
        }
        kkeyboardtype.option = layoutsoptions;
        result.append(kkeyboardtype);
    }
    return result;
}

static void kApplyKeyboardConfig()
{
    const QList<KKeyboardType> layouts = kLayoutsFromConfig();
    if (layouts.size() > 0) {
        KKeyboardLayout().setLayouts(layouts);
    }

    KConfig kconfig("kcminputrc", KConfig::NoGlobals);
    KConfigGroup kconfiggroup(&kconfig, "Keyboard");
    const float repeatdelay = kconfiggroup.readEntry("RepeatDelay", s_defaultrepeatdelay);
    const float repeatrate = kconfiggroup.readEntry("RepeatRate", s_defaultrepeatrate);

    XkbDescPtr xkbkeyboard = XkbAllocKeyboard();
    if (!xkbkeyboard) {
        kError() << "Failed to allocate keyboard";
        return;
    }
    Status xkbgetresult = XkbGetControls(QX11Info::display(), XkbRepeatKeysMask, xkbkeyboard);
    if (xkbgetresult != Success) {
        kError() << "Failed to get keyboard repeat controls";
        XkbFreeKeyboard(xkbkeyboard, 0, true);
        return;
    }
    xkbkeyboard->ctrls->repeat_delay = repeatdelay;
    xkbkeyboard->ctrls->repeat_interval = qFloor(1000 / repeatrate + 0.5);
    const Bool xkbsetresult = XkbSetControls(QX11Info::display(), XkbRepeatKeysMask, xkbkeyboard);
    if (xkbsetresult != True) {
        kError() << "Failed to set keyboard repeat controls";
    }
    XkbFreeKeyboard(xkbkeyboard, 0, true);
}


#endif // KEYBOARDCONFIG_COMMON_H
