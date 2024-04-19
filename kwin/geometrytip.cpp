/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (c) 2003, Karol Szwed <kszwed@kde.org>

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

#include "geometrytip.h"

#include <kdialog.h>

namespace KWin
{

GeometryTip::GeometryTip(const XSizeHints* xSizeHints)
    : KHBox(nullptr),
    label(nullptr),
    sizeHints(xSizeHints)
{
    setObjectName(QLatin1String("kwingeometry"));

    setMargin(1);
    setLineWidth(1);
    setFrameStyle(QFrame::Raised | QFrame::StyledPanel);
    setWindowFlags(Qt::X11BypassWindowManagerHint);

    // TODO: the text has to be aligned properly (front-padded numbers)
    label = new QLabel(this);
    label->setAlignment(Qt::AlignCenter | Qt::AlignTop);
    label->setIndent(0);
    const int margin = KDialog::marginHint();
    label->setContentsMargins(margin, 0, margin, 0);
}

void GeometryTip::setGeometry(const QRect& geom)
{
    int w = geom.width();
    int h = geom.height();

    if (sizeHints) {
        if (sizeHints->flags & PResizeInc) {
            w = (w - sizeHints->base_width) / sizeHints->width_inc;
            h = (h - sizeHints->base_height) / sizeHints->height_inc;
        }
    }

    h = qMax(h, 0);   // in case of isShade() and PBaseSize
    QString pos;
    pos.sprintf("%+d,%+d<br>(<b>%d&nbsp;x&nbsp;%d</b>)",
                geom.x(), geom.y(), w, h);
    label->setText(pos);
    adjustSize();
    move(geom.x() + ((geom.width()  - width())  / 2),
         geom.y() + ((geom.height() - height()) / 2));
}

} // namespace

#include "moc_geometrytip.cpp"
