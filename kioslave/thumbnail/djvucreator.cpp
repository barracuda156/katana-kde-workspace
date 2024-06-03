/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "djvucreator.h"

#include <QImage>
#include <QCoreApplication>
#include <QThread>
#include <kdebug.h>
#include <kdemacros.h>

#include <libdjvu/ddjvuapi.h>

static const int s_eventstime = 250;
static const int s_sleeptime = 100;

extern "C"
{
    KDE_EXPORT ThumbCreator *new_creator()
    {
        return new DjVuCreator();
    }
}

DjVuCreator::DjVuCreator()
{
}

bool DjVuCreator::create(const QString &path, int width, int height, QImage &img)
{
    const QByteArray pathbytes = path.toUtf8();
    ddjvu_context_t* djvuctx = ddjvu_context_create("djvucreator");
    if (!djvuctx) {
        kWarning() << "Could not create DjVu context";
        return false;
    }
    ddjvu_document_t* djvudoc = ddjvu_document_create_by_filename_utf8(djvuctx, pathbytes.constData(), FALSE);
    if (!djvudoc) {
        kWarning() << "Could not create DjVu document";
        ddjvu_context_release(djvuctx);
        return false;
    }
    kDebug() << "Waiting for document decoding to complete";
    while (!ddjvu_document_decoding_done(djvudoc)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, s_eventstime);
        QThread::msleep(s_sleeptime);
    }
    kDebug() << "Done waiting for document decoding to complete";

    ddjvu_page_t* djvupage = ddjvu_page_create_by_pageno(djvudoc, 0);
    if (!djvupage) {
        kWarning() << "Could not create DjVu page";
        ddjvu_document_release(djvudoc);
        ddjvu_context_release(djvuctx);
        return false;
    }
    kDebug() << "Waiting for page decoding to complete";
    while (!ddjvu_page_decoding_done(djvupage)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, s_eventstime);
        QThread::msleep(s_sleeptime);
    }
    kDebug() << "Done waiting for page decoding to complete";

    uint djvumask[4] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };
    int djvumasksize = 3; // sizeof(djvumask);
    ddjvu_format_t* djvuformat = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, djvumasksize, djvumask);
    if (!djvuformat) {
        kWarning() << "Could not create DjVu format";
        ddjvu_page_release(djvupage);
        ddjvu_document_release(djvudoc);
        ddjvu_context_release(djvuctx);
        return false;
    }
    ddjvu_format_set_row_order(djvuformat, 1);
    ddjvu_format_set_y_direction(djvuformat, 1);

    QSize pagesize(ddjvu_page_get_width(djvupage), ddjvu_page_get_height(djvupage));
    pagesize.scale(width, height, Qt::KeepAspectRatio);
    ddjvu_rect_t djvupagerect;
    djvupagerect.x = 0;
    djvupagerect.y = 0;
    djvupagerect.w = pagesize.width();
    djvupagerect.h = pagesize.height();
    ddjvu_rect_t djvurenderrect;
    djvurenderrect = djvupagerect;
    img = QImage(djvurenderrect.w, djvurenderrect.h, QImage::Format_RGB32);
    const int djvustatus = ddjvu_page_render(
        djvupage,
        DDJVU_RENDER_COLOR,
        &djvupagerect, &djvurenderrect,
        djvuformat,
        img.bytesPerLine(), reinterpret_cast<char*>(img.bits())
    );
    if (djvustatus == FALSE) {
        kWarning() << "Could not render DjVu page";
        ddjvu_format_release(djvuformat);
        ddjvu_page_release(djvupage);
        ddjvu_document_release(djvudoc);
        ddjvu_context_release(djvuctx);
        img = QImage();
        return false;
    }

    ddjvu_format_release(djvuformat);
    ddjvu_page_release(djvupage);
    ddjvu_document_release(djvudoc);
    ddjvu_context_release(djvuctx);
    return true;
}

ThumbCreator::Flags DjVuCreator::flags() const
{
    return ThumbCreator::Flags(ThumbCreator::DrawFrame);
}
