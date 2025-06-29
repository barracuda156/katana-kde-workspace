/*  This file is part of the KDE libraries
    Copyright (C) 2000,2002 Carsten Pfeiffer <pfeiffer@kde.org>
                  2000 Malte Starostik <malte@kde.org>

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

#include "textcreator.h"

#include <QFile>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QTextCodec>

#include <kstandarddirs.h>
#include <kglobalsettings.h>
#include <klocale.h>
#include <kdemacros.h>

extern "C"
{
    KDE_EXPORT ThumbCreator *new_creator()
    {
        return new TextCreator();
    }
}

TextCreator::TextCreator()
{
}

static QTextCodec *codecFromContent(const char *data, int dataSize)
{
    QByteArray ba = QByteArray::fromRawData(data, dataSize);
    // try to detect text encoding, fall back to locale (which is usually UTF-8)
    return QTextCodec::codecForText(ba, QTextCodec::codecForLocale());
}

bool TextCreator::create(const QString &path, int width, int height, QImage &img)
{
    bool ok = false;

    // determine some sizes...
    // example: width: 60, height: 64
    QSize pixmapSize( width, height );
    if (height * 3 > width * 4)
        pixmapSize.setHeight( width * 4 / 3 );
    else
        pixmapSize.setWidth( height * 3 / 4 );

    QPixmap pixmap( pixmapSize );

    // one pixel for the rectangle, the rest. whitespace
    int xborder = 1 + pixmapSize.width()/16;  // minimum x-border
    int yborder = 1 + pixmapSize.height()/16; // minimum y-border

    // this font is supposed to look good at small sizes
    QFont font = KGlobalSettings::smallestReadableFont();
    font.setPixelSize( qMax(7, qMin( 10, ( pixmapSize.height() - 2 * yborder ) / 16 ) ) );
    QFontMetrics fm( font );

    // calculate a better border so that the text is centered
    int canvasWidth  = pixmapSize.width()  - 2*xborder;
    int canvasHeight = pixmapSize.height() -  2*yborder;
    int numLines = (int) ( canvasHeight / fm.height() );

    // assumes an average line length of <= 120 chars
    const int bytesToRead = 120 * numLines;

    // create text-preview
    QByteArray data(bytesToRead + 1, '\0');
    QFile file( path );
    if ( file.open( QIODevice::ReadOnly ))
    {
        int read = file.read( data.data(), bytesToRead );
        if ( read > 0 )
        {
            ok = true;
            data[read] = '\0';
            QString text = codecFromContent( data, read )->toUnicode( data, read ).trimmed();
            // FIXME: maybe strip whitespace and read more?

            // If the text contains tabs or consecutive spaces, it is probably
            // formatted using white space. Use a fixed pitch font in this case.
            QStringList textLines = text.split( '\n' );
            foreach ( const QString &line, textLines ) {
                QString trimmedLine = line.trimmed();
                if ( trimmedLine.contains( '\t' ) || trimmedLine.contains( "  " ) ) {
                    font.setFamily( KGlobalSettings::fixedFont().family() );
                    break;
                }
            }

#if 0
            QPalette palette;
            QColor bgColor = palette.color( QPalette::Base );
            QColor fgColor = palette.color( QPalette::Text );
            if ( qGray( bgColor.rgb() ) > qGray( fgColor.rgb() ) ) {
                bgColor = bgColor.darker( 103 );
            } else {
                bgColor = bgColor.lighter( 103 );
            }
#else
            QColor bgColor = QColor ( 245, 245, 245 ); // light-grey background
            QColor fgColor = Qt::black;
#endif
            pixmap.fill( bgColor );

            QPainter painter( &pixmap );
            painter.setFont( font );
            painter.setPen( fgColor );

            QTextOption textOption( Qt::AlignTop | Qt::AlignLeft );
            textOption.setTabStop( 8 * painter.fontMetrics().width( ' ' ) );
            textOption.setWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
            painter.drawText( QRect( xborder, yborder, canvasWidth, canvasHeight ), text, textOption );
            painter.end();

            img = pixmap.toImage();
        }

        file.close();
    }
    return ok;
}

ThumbCreator::Flags TextCreator::flags() const
{
    return ThumbCreator::Flags(ThumbCreator::DrawFrame);
}

