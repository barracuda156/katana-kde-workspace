/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Bernhard Loos <nhuh.put@web.de>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "diminactive/diminactive_config.h"
#include "dimscreen/dimscreen_config.h"
#include "presentwindows/presentwindows_config.h"
#include "resize/resize_config.h"
#include "showfps/showfps_config.h"
#include "thumbnailaside/thumbnailaside_config.h"
#include "windowgeometry/windowgeometry_config.h"
#include "zoom/zoom_config.h"

#include "magnifier/magnifier_config.h"
#include "mousemark/mousemark_config.h"
#include "trackmouse/trackmouse_config.h"

#include <kwineffects.h>

#include <KPluginLoader>

namespace KWin
{

KWIN_EFFECT_CONFIG_MULTIPLE(builtins,
    KWIN_EFFECT_CONFIG_SINGLE(diminactive, DimInactiveEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(dimscreen, DimScreenEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(presentwindows, PresentWindowsEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(resize, ResizeEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(showfps, ShowFpsEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(thumbnailaside, ThumbnailAsideEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(windowgeometry, WindowGeometryConfig)
    KWIN_EFFECT_CONFIG_SINGLE(zoom, ZoomEffectConfig)

    KWIN_EFFECT_CONFIG_SINGLE(magnifier, MagnifierEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(mousemark, MouseMarkEffectConfig)
    KWIN_EFFECT_CONFIG_SINGLE(trackmouse, TrackMouseEffectConfig)
)

} // namespace
