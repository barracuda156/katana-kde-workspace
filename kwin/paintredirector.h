/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2009 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#ifndef PAINTREDIRECTOR_H
#define PAINTREDIRECTOR_H

#include <qregion.h>
#include <qtimer.h>
#include <qwidget.h>
#include <qbasictimer.h>
// xcb
#include <xcb/render.h>

namespace KWin
{

// forward declarations
class Client;
class Deleted;
class XRenderPicture;

// This class redirects all painting of a given widget (including its children)
// into a paint device (QImage).
class PaintRedirector
    : public QObject
{
    Q_OBJECT
public:
    enum DecorationPixmap {
        TopPixmap,
        RightPixmap,
        BottomPixmap,
        LeftPixmap,
        PixmapCount
    };
    virtual ~PaintRedirector();
    virtual bool eventFilter(QObject* o, QEvent* e);
    QRegion pendingRegion() const;
    QRegion scheduledRepaintRegion();

    void markAsRepainted() {
        m_requiresRepaint = false;
    }
    bool requiresRepaint() const {
        return m_requiresRepaint;
    }
    void resizePixmaps();

    template <typename T>
    T topDecoPixmap() const;
    template <typename T>
    T leftDecoPixmap() const;
    template <typename T>
    T bottomDecoPixmap() const;
    template <typename T>
    T rightDecoPixmap() const;

    /**
     * Used by Deleted::copyToDeleted() to move the PaintRedirector to the Deleted.
     * The actual redirecting ends as the decoration gets destroyed after the Deleted
     * is created.
     **/
    void reparent(Deleted *d);
    static PaintRedirector *create(Client *c, QWidget* widget);

public slots:
    void ensurePixmapsPainted();
protected:
    PaintRedirector(Client *c, QWidget* widget);
    virtual xcb_render_picture_t picture(DecorationPixmap border) const;
    virtual void resizePixmaps(const QRect *rects);
    virtual void resize(DecorationPixmap border, const QSize &size);
    virtual void updatePixmaps(const QRect *rects, const QRegion &region);
    virtual void paint(DecorationPixmap border, const QRect& r, const QRect &b, const QRegion &reg);
    virtual QPaintDevice *scratch() = 0;
    virtual QPaintDevice *recreateScratch(const QSize &size) = 0;
    virtual void fillScratch(Qt::GlobalColor color) = 0;
    virtual void discardScratch() = 0;

private:
    void added(QWidget* widget);
    void removed(QWidget* widget);
    bool isToolTip(QWidget* widget) const;
    void timerEvent(QTimerEvent* event);

    void performPendingPaint();
    void repaintPixmap(DecorationPixmap border, const QRect& r, QRegion reg);
    QWidget* widget;
    QRegion pending;
    QRegion scheduled;
    bool recursionCheck;
    QBasicTimer cleanupTimer;

    Client *m_client;
    bool m_requiresRepaint;
};

class RasterXRenderPaintRedirector : public PaintRedirector
{
    Q_OBJECT
public:
    RasterXRenderPaintRedirector(Client *c, QWidget *widget);
    virtual ~RasterXRenderPaintRedirector();

protected:
    virtual xcb_render_picture_t picture(DecorationPixmap border) const;
    virtual void resize(DecorationPixmap border, const QSize &size);
    virtual void paint(DecorationPixmap border, const QRect &r, const QRect &b, const QRegion &reg);

    virtual QPaintDevice *recreateScratch(const QSize &size);
    virtual QPaintDevice *scratch();
    virtual void fillScratch(Qt::GlobalColor color);
    virtual void discardScratch();
private:
    QSize m_sizes[PixmapCount];
    xcb_pixmap_t m_pixmaps[PixmapCount];
    xcb_gcontext_t m_gc;
    XRenderPicture* m_pictures[PixmapCount];
    QImage m_scratchImage;
};

template <>
inline
xcb_render_picture_t PaintRedirector::bottomDecoPixmap() const
{
    return picture(BottomPixmap);
}

template <>
inline
xcb_render_picture_t PaintRedirector::leftDecoPixmap() const
{
    return picture(LeftPixmap);
}

template <>
inline
xcb_render_picture_t PaintRedirector::rightDecoPixmap() const
{
    return picture(RightPixmap);
}

template <>
inline
xcb_render_picture_t PaintRedirector::topDecoPixmap() const
{
    return picture(TopPixmap);
}

} // namespace

#endif
