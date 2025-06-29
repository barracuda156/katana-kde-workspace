/***************************************************************************
 *   Copyright (C) 2012 by Peter Penz <peter.penz19@gmail.com>             *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include "kstandarditemlistwidget.h"

#include "kfileitemlistview.h"
#include "kfileitemmodel.h"

#include <KIcon>
#include <KIconEffect>
#include <KIconLoader>
#include <KLocale>
#include <KStringHandler>
#include <KDebug>

#include "private/kfileitemclipboard.h"
#include "private/kitemlistroleeditor.h"

#include <QtGui/qfontmetrics.h>
#include <QGraphicsScene>
#include <QtGui/qgraphicssceneevent.h>
#include <QGraphicsView>
#include <QPainter>
#include <QStyleOption>
#include <QTextLayout>
#include <QtGui/qtextlayout.h>
#include <QPixmapCache>

// #define KSTANDARDITEMLISTWIDGET_DEBUG

KStaticText::KStaticText()
    : m_textwidth(0.0),
    mixguard(false)
{
}

QString KStaticText::text() const
{
    return m_text;
}

void KStaticText::setText(const QString &text)
{
    brect = QRectF();
    mixguard = false;
    m_text = text;
}

void KStaticText::setTextWidth(const qreal textwidth)
{
    brect = QRectF();
    mixguard = false;
    m_textwidth = textwidth;
}

// detailed/compact mode only getter
QSizeF KStaticText::size() const
{
#ifndef QT_NO_DEBUG
    if (mixguard) {
        kWarning() << "Mixing size overloads, discarding cache";
    }
#endif

    if (!mixguard && !brect.isNull()) {
        // qDebug() << Q_FUNC_INFO << "cache hit";
        return brect.size();
    }

    QTextLayout textlayout(m_text, m_font);
    textlayout.setTextOption(m_textoption);
    textlayout.beginLayout();
    QTextLine textline = textlayout.createLine();
    textline.setLineWidth(m_textwidth);
    textlayout.endLayout();

    brect = textlayout.boundingRect();
    return brect.size();
}

// multi-line (wrapped) text aware overload
QSizeF KStaticText::size(const QFontMetricsF &fontmetrics, const int maxlines) const
{
    mixguard = true;

    if (!brect.isNull()) {
        // qDebug() << Q_FUNC_INFO << "cache hit";
        return brect.size();
    }

    QString text = m_text;
    redo:
        QTextLayout textlayout(text, m_font);
        textlayout.setTextOption(m_textoption);
        textlayout.beginLayout();
        QTextLine textline = textlayout.createLine();
        int linecount = 1;
        const qreal fontleading = fontmetrics.leading();
        qreal lineheight = -fontleading;
        while (textline.isValid()) {
            textline.setLineWidth(m_textwidth);
            lineheight += fontleading;
            textline.setPosition(QPointF(0, lineheight));
            if (maxlines != -1 && linecount == maxlines && textline.naturalTextWidth() > m_textwidth) {
                text = fontmetrics.elidedText(text, Qt::ElideRight, m_textwidth);
                goto redo;
            }
            lineheight += textline.height();
            textline = textlayout.createLine();
            linecount++;
        }
        textlayout.endLayout();

    brect = textlayout.boundingRect();
    return brect.size();
}

QTextOption KStaticText::textOption() const
{
    return m_textoption;
}

void KStaticText::setTextOption(const QTextOption &textoption)
{
    m_textoption = textoption;
}

void KStaticText::setFont(const QFont &font)
{
    m_font = font;
}

void KStaticText::paint(QPainter *painter, const QPointF position, const QFontMetricsF &fontmetrics, const int maxlines) const
{
    QString text = m_text;
    redo:
        QTextLayout textlayout(text, m_font);
        textlayout.setTextOption(m_textoption);
        textlayout.beginLayout();
        QTextLine textline = textlayout.createLine();
        int linecount = 1;
        qreal lineheight = 0;
        while (textline.isValid()) {
            textline.setLineWidth(m_textwidth);
            textline.setPosition(QPointF(0, lineheight));
            if (maxlines != -1 && linecount == maxlines && textline.naturalTextWidth() > m_textwidth) {
                text = fontmetrics.elidedText(text, Qt::ElideRight, m_textwidth);
                goto redo;
            }
            lineheight += textline.height();
            textline = textlayout.createLine();
            linecount++;
        }
        textlayout.endLayout();

    textlayout.draw(painter, position);
}

KStandardItemListWidgetInformant::KStandardItemListWidgetInformant() :
    KItemListWidgetInformant()
{
}

KStandardItemListWidgetInformant::~KStandardItemListWidgetInformant()
{
}

void KStandardItemListWidgetInformant::calculateItemSizeHints(QVector<qreal>& logicalHeightHints, qreal& logicalWidthHint, const KItemListView* view) const
{
    switch (static_cast<const KStandardItemListView*>(view)->itemLayout()) {
    case KStandardItemListView::IconsLayout:
        calculateIconsLayoutItemSizeHints(logicalHeightHints, logicalWidthHint, view);
        break;

    case KStandardItemListView::CompactLayout:
        calculateCompactLayoutItemSizeHints(logicalHeightHints, logicalWidthHint, view);
        break;

    case KStandardItemListView::DetailsLayout:
        calculateDetailsLayoutItemSizeHints(logicalHeightHints, logicalWidthHint, view);
        break;

    default:
        Q_ASSERT(false);
        break;
    }
}

qreal KStandardItemListWidgetInformant::preferredRoleColumnWidth(const QByteArray& role,
                                                                 int index,
                                                                 const KItemListView* view) const
{
    const QHash<QByteArray, QVariant> values = view->model()->data(index);
    const KItemListStyleOption& option = view->styleOption();

    const QString text = roleText(role, values);
    qreal width = KStandardItemListWidget::columnPadding(option);

    const QFontMetrics& normalFontMetrics = option.fontMetrics;
    const QFontMetrics linkFontMetrics(customizedFontForLinks(option.font));

    // If current item is a link, we use the customized link font metrics instead of the normal font metrics.
    const QFontMetrics& fontMetrics = itemIsLink(index, view) ? linkFontMetrics : normalFontMetrics;

    width += fontMetrics.width(text);

    if (role == "text") {
        // Increase the width by the required space for the icon
        width += option.padding * 2 + option.iconSize;
    }

    return width;
}

QString KStandardItemListWidgetInformant::itemText(int index, const KItemListView* view) const
{
    return view->model()->data(index).value("text").toString();
}

bool KStandardItemListWidgetInformant::itemIsLink(int index, const KItemListView* view) const
{
    return false;
}

QString KStandardItemListWidgetInformant::roleText(const QByteArray& role,
                                                   const QHash<QByteArray, QVariant>& values) const
{
    return values.value(role).toString();
}

QFont KStandardItemListWidgetInformant::customizedFontForLinks(const QFont& baseFont) const
{
    return baseFont;
}

void KStandardItemListWidgetInformant::calculateIconsLayoutItemSizeHints(QVector<qreal>& logicalHeightHints, qreal& logicalWidthHint, const KItemListView* view) const
{
    const KItemListStyleOption& option = view->styleOption();
    const QFont& normalFont = option.font;
    const int additionalRolesCount = qMax(view->visibleRoles().count() - 1, 0);

    const qreal itemWidth = view->itemSize().width();
    const qreal maxWidth = itemWidth - 2 * option.padding;
    const qreal additionalRolesSpacing = additionalRolesCount * option.fontMetrics.lineSpacing();
    const qreal spacingAndIconHeight = option.iconSize + option.padding * 3;

    const QFont linkFont = customizedFontForLinks(normalFont);

    QTextOption textOption(Qt::AlignHCenter);
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    for (int index = 0; index < logicalHeightHints.count(); ++index) {
        if (logicalHeightHints.at(index) > 0.0) {
            continue;
        }

        // If the current item is a link, we use the customized link font instead of the normal font.
        const QFont& font = itemIsLink(index, view) ? linkFont : normalFont;

        const QString& text = KStringHandler::preProcessWrap(itemText(index, view));

        // Calculate the number of lines required for wrapping the name
        qreal textHeight = 0;
        QTextLayout layout(text, font);
        layout.setTextOption(textOption);
        layout.beginLayout();
        QTextLine line;
        int lineCount = 0;
        while ((line = layout.createLine()).isValid()) {
            line.setLineWidth(maxWidth);
            line.naturalTextWidth();
            textHeight += line.height();

            ++lineCount;
            if (lineCount == option.maxTextLines) {
                break;
            }
        }
        layout.endLayout();

        // Add one line for each additional information
        textHeight += additionalRolesSpacing;

        logicalHeightHints[index] = textHeight + spacingAndIconHeight;
    }

    logicalWidthHint = itemWidth;
}

void KStandardItemListWidgetInformant::calculateCompactLayoutItemSizeHints(QVector<qreal>& logicalHeightHints, qreal& logicalWidthHint, const KItemListView* view) const
{
    const KItemListStyleOption& option = view->styleOption();
    const QFontMetrics& normalFontMetrics = option.fontMetrics;
    const int additionalRolesCount = qMax(view->visibleRoles().count() - 1, 0);

    const QList<QByteArray>& visibleRoles = view->visibleRoles();
    const bool showOnlyTextRole = (visibleRoles.count() == 1) && (visibleRoles.first() == "text");
    const qreal maxWidth = option.maxTextWidth;
    const qreal paddingAndIconWidth = option.padding * 4 + option.iconSize;
    const qreal height = option.padding * 2 + qMax(option.iconSize, (1 + additionalRolesCount) * normalFontMetrics.lineSpacing());

    const QFontMetrics linkFontMetrics(customizedFontForLinks(option.font));

    for (int index = 0; index < logicalHeightHints.count(); ++index) {
        if (logicalHeightHints.at(index) > 0.0) {
            continue;
        }

        // If the current item is a link, we use the customized link font metrics instead of the normal font metrics.
        const QFontMetrics& fontMetrics = itemIsLink(index, view) ? linkFontMetrics : normalFontMetrics;

        // For each row exactly one role is shown. Calculate the maximum required width that is necessary
        // to show all roles without horizontal clipping.
        qreal maximumRequiredWidth = 0.0;

        if (showOnlyTextRole) {
            maximumRequiredWidth = fontMetrics.width(itemText(index, view));
        } else {
            const QHash<QByteArray, QVariant>& values = view->model()->data(index);
            foreach (const QByteArray& role, visibleRoles) {
                const QString& text = roleText(role, values);
                const qreal requiredWidth = fontMetrics.width(text);
                maximumRequiredWidth = qMax(maximumRequiredWidth, requiredWidth);
            }
        }

        qreal width = paddingAndIconWidth + maximumRequiredWidth;
        if (maxWidth > 0 && width > maxWidth) {
            width = maxWidth;
        }

        logicalHeightHints[index] = width;
    }

    logicalWidthHint = height;
}

void KStandardItemListWidgetInformant::calculateDetailsLayoutItemSizeHints(QVector<qreal>& logicalHeightHints, qreal& logicalWidthHint, const KItemListView* view) const
{
    const KItemListStyleOption& option = view->styleOption();
    const qreal height = option.padding * 2 + qMax(option.iconSize, option.fontMetrics.height());
    logicalHeightHints.fill(height);
    logicalWidthHint = -1.0;
}

KStandardItemListWidget::KStandardItemListWidget(KItemListWidgetInformant* informant, QGraphicsItem* parent) :
    KItemListWidget(informant, parent),
    m_isCut(false),
    m_isHidden(false),
    m_customizedFont(),
    m_customizedFontMetrics(m_customizedFont),
    m_dirtyLayout(true),
    m_dirtyContent(true),
    m_dirtyContentRoles(),
    m_layout(IconsLayout),
    m_pixmapPos(),
    m_pixmap(),
    m_scaledPixmapSize(),
    m_iconRect(),
    m_hoverPixmap(),
    m_textInfo(),
    m_textRect(),
    m_sortedVisibleRoles(),
    m_customTextColor(),
    m_additionalInfoTextColor(),
    m_overlay(),
    m_roleEditor(0),
    m_oldRoleEditor(0)
{
}

KStandardItemListWidget::~KStandardItemListWidget()
{
    qDeleteAll(m_textInfo);
    m_textInfo.clear();

    if (m_roleEditor) {
        m_roleEditor->deleteLater();
    }

    if (m_oldRoleEditor) {
        m_oldRoleEditor->deleteLater();
    }
}

void KStandardItemListWidget::setLayout(Layout layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        m_dirtyLayout = true;
        updateAdditionalInfoTextColor();
        update();
    }
}

KStandardItemListWidget::Layout KStandardItemListWidget::layout() const
{
    return m_layout;
}

void KStandardItemListWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    triggerCacheRefreshing();

    KItemListWidget::paint(painter, option, widget);

    const KItemListStyleOption& itemListStyleOption = styleOption();
    if (isHovered()) {
        if (hoverOpacity() < 1.0) {
            /*
             * Linear interpolation between m_pixmap and m_hoverPixmap.
             *
             * Note that this cannot be achieved by painting m_hoverPixmap over
             * m_pixmap, even if the opacities are adjusted.
             */
            // Paint pixmap1 so that pixmap1 = m_pixmap * (1.0 - hoverOpacity())
            QPixmap pixmap1(m_pixmap.size());
            pixmap1.fill(Qt::transparent);
            {
                QPainter p(&pixmap1);
                p.setOpacity(1.0 - hoverOpacity());
                p.drawPixmap(0, 0, m_pixmap);
            }

            // Paint pixmap2 so that pixmap2 = m_hoverPixmap * hoverOpacity()
            QPixmap pixmap2(pixmap1.size());
            pixmap2.fill(Qt::transparent);
            {
                QPainter p(&pixmap2);
                p.setOpacity(hoverOpacity());
                p.drawPixmap(0, 0, m_hoverPixmap);
            }

            // Paint pixmap2 on pixmap1 using CompositionMode_Plus
            // Now pixmap1 = pixmap2 + m_pixmap * (1.0 - hoverOpacity())
            //             = m_hoverPixmap * hoverOpacity() + m_pixmap * (1.0 - hoverOpacity())
            {
                QPainter p(&pixmap1);
                p.setCompositionMode(QPainter::CompositionMode_Plus);
                p.drawPixmap(0, 0, pixmap2);
            }

            // Finally paint pixmap1 on the widget
            drawPixmap(painter, pixmap1);
        } else {
            drawPixmap(painter, m_hoverPixmap);
        }
    } else {
        drawPixmap(painter, m_pixmap);
    }

    painter->setFont(m_customizedFont);
    painter->setPen(textColor());
    const TextInfo* textInfo = m_textInfo.value("text");

    if (!textInfo) {
        // It seems that we can end up here even if m_textInfo does not contain
        // the key "text", see bug 306167. According to triggerCacheRefreshing(),
        // this can only happen if the index is negative. This can happen when
        // the item is about to be removed, see KItemListView::slotItemsRemoved().
        // TODO: try to reproduce the crash and find a better fix.
        return;
    }

    textInfo->staticText.paint(painter, textInfo->pos, m_customizedFontMetrics, itemListStyleOption.maxTextLines);

    painter->setPen(m_additionalInfoTextColor);
    painter->setFont(m_customizedFont);

    for (int i = 1; i < m_sortedVisibleRoles.count(); ++i) {
        const TextInfo* textInfo = m_textInfo.value(m_sortedVisibleRoles[i]);
        textInfo->staticText.paint(painter, textInfo->pos, m_customizedFontMetrics);
    }

#ifdef KSTANDARDITEMLISTWIDGET_DEBUG
    painter->setBrush(Qt::NoBrush);
    painter->setPen(Qt::green);
    painter->drawRect(m_iconRect);

    painter->setPen(Qt::blue);
    painter->drawRect(m_textRect);

    painter->setPen(Qt::red);
    painter->drawText(QPointF(0, m_customizedFontMetrics.height()), QString::number(index()));
    painter->drawRect(rect());
#endif
}

QRectF KStandardItemListWidget::iconRect() const
{
    const_cast<KStandardItemListWidget*>(this)->triggerCacheRefreshing();
    return m_iconRect;
}

QRectF KStandardItemListWidget::textRect() const
{
    const_cast<KStandardItemListWidget*>(this)->triggerCacheRefreshing();
    return m_textRect;
}

QRectF KStandardItemListWidget::textFocusRect() const
{
    // In the compact- and details-layout a larger textRect() is returned to be aligned
    // with the iconRect(). This is useful to have a larger selection/hover-area
    // when having a quite large icon size but only one line of text. Still the
    // focus rectangle should be shown as narrow as possible around the text.

    const_cast<KStandardItemListWidget*>(this)->triggerCacheRefreshing();

    switch (m_layout) {
    case CompactLayout: {
        QRectF rect = m_textRect;
        const TextInfo* topText    = m_textInfo.value(m_sortedVisibleRoles.first());
        const TextInfo* bottomText = m_textInfo.value(m_sortedVisibleRoles.last());
        rect.setTop(topText->pos.y());
        rect.setBottom(bottomText->pos.y() + bottomText->staticText.size().height());
        return rect;
    }

    case DetailsLayout: {
        QRectF rect = m_textRect;
        const TextInfo* textInfo = m_textInfo.value(m_sortedVisibleRoles.first());
        rect.setTop(textInfo->pos.y());
        rect.setBottom(textInfo->pos.y() + textInfo->staticText.size().height());

        const KItemListStyleOption& option = styleOption();
        if (option.extendedSelectionRegion) {
            const QString text = textInfo->staticText.text();
            rect.setWidth(m_customizedFontMetrics.width(text) + 2 * option.padding);
        }

        return rect;
    }

    default:
        break;
    }

    return m_textRect;
}

QRectF KStandardItemListWidget::selectionRect() const
{
    const_cast<KStandardItemListWidget*>(this)->triggerCacheRefreshing();

    switch (m_layout) {
    case IconsLayout:
        return m_textRect;

    case CompactLayout:
    case DetailsLayout: {
        const int padding = styleOption().padding;
        QRectF adjustedIconRect = iconRect().adjusted(-padding, -padding, padding, padding);
        return adjustedIconRect | m_textRect;
    }

    default:
        Q_ASSERT(false);
        break;
    }

    return m_textRect;
}

QRectF KStandardItemListWidget::selectionToggleRect() const
{
    const_cast<KStandardItemListWidget*>(this)->triggerCacheRefreshing();

    const int iconHeight = styleOption().iconSize;

    int toggleSize = KIconLoader::SizeSmall;
    if (iconHeight >= KIconLoader::SizeEnormous) {
        toggleSize = KIconLoader::SizeMedium;
    } else if (iconHeight >= KIconLoader::SizeLarge) {
        toggleSize = KIconLoader::SizeSmallMedium;
    }

    QPointF pos = iconRect().topLeft();

    // If the selection toggle has a very small distance to the
    // widget borders, the size of the selection toggle will get
    // increased to prevent an accidental clicking of the item
    // when trying to hit the toggle.
    const int widgetHeight = size().height();
    const int widgetWidth = size().width();
    const int minMargin = 2;

    if (toggleSize + minMargin * 2 >= widgetHeight) {
        pos.rx() -= (widgetHeight - toggleSize) / 2;
        toggleSize = widgetHeight;
        pos.setY(0);
    }
    if (toggleSize + minMargin * 2 >= widgetWidth) {
        pos.ry() -= (widgetWidth - toggleSize) / 2;
        toggleSize = widgetWidth;
        pos.setX(0);
    }

    return QRectF(pos, QSizeF(toggleSize, toggleSize));
}

QPixmap KStandardItemListWidget::createDragPixmap(const QStyleOptionGraphicsItem* option,
                                                  QWidget* widget)
{
    QPixmap pixmap = KItemListWidget::createDragPixmap(option, widget);
    if (m_layout != DetailsLayout) {
        return pixmap;
    }

    // Only return the content of the text-column as pixmap
    const int leftClip = m_pixmapPos.x();

    const TextInfo* textInfo = m_textInfo.value("text");
    const int rightClip = textInfo->pos.x() +
                          textInfo->staticText.size().width() +
                          2 * styleOption().padding;

    QPixmap clippedPixmap(rightClip - leftClip + 1, pixmap.height());
    clippedPixmap.fill(Qt::transparent);

    QPainter painter(&clippedPixmap);
    painter.drawPixmap(-leftClip, 0, pixmap);

    return clippedPixmap;
}


KItemListWidgetInformant* KStandardItemListWidget::createInformant()
{
    return new KStandardItemListWidgetInformant();
}

void KStandardItemListWidget::invalidateCache()
{
    m_dirtyLayout = true;
    m_dirtyContent = true;
}

void KStandardItemListWidget::refreshCache()
{
}

bool KStandardItemListWidget::isRoleRightAligned(const QByteArray& role) const
{
    Q_UNUSED(role);
    return false;
}

bool KStandardItemListWidget::isHidden() const
{
    return false;
}

QFont KStandardItemListWidget::customizedFont(const QFont& baseFont) const
{
    return baseFont;
}

QPalette::ColorRole KStandardItemListWidget::normalTextColorRole() const
{
    return QPalette::Text;
}

void KStandardItemListWidget::setTextColor(const QColor& color)
{
    if (color != m_customTextColor) {
        m_customTextColor = color;
        updateAdditionalInfoTextColor();
        update();
    }
}

QColor KStandardItemListWidget::textColor() const
{
    if (!isSelected()) {
        if (m_isHidden) {
            return m_additionalInfoTextColor;
        } else if (m_customTextColor.isValid()) {
            return m_customTextColor;
        }
    }

    const QPalette::ColorGroup group = isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    const QPalette::ColorRole role = isSelected() ? QPalette::HighlightedText : normalTextColorRole();
    return styleOption().palette.color(group, role);
}

void KStandardItemListWidget::setOverlay(const QPixmap& overlay)
{
    m_overlay = overlay;
    m_dirtyContent = true;
    update();
}

QPixmap KStandardItemListWidget::overlay() const
{
    return m_overlay;
}


QString KStandardItemListWidget::roleText(const QByteArray& role,
                                          const QHash<QByteArray, QVariant>& values) const
{
    return static_cast<const KStandardItemListWidgetInformant*>(informant())->roleText(role, values);
}

void KStandardItemListWidget::dataChanged(const QHash<QByteArray, QVariant>& current,
                                          const QSet<QByteArray>& roles)
{
    Q_UNUSED(current);

    m_dirtyContent = true;

    QSet<QByteArray> dirtyRoles;
    if (roles.isEmpty()) {
        dirtyRoles = visibleRoles().toSet();
    } else {
        dirtyRoles = roles;
    }

    // The URL might have changed (i.e., if the sort order of the items has
    // been changed). Therefore, the "is cut" state must be updated.
    KFileItemClipboard* clipboard = KFileItemClipboard::instance();
    const KUrl itemUrl = data().value("url").value<KUrl>();
    m_isCut = clipboard->isCut(itemUrl);

    // The icon-state might depend from other roles and hence is
    // marked as dirty whenever a role has been changed
    dirtyRoles.insert("iconPixmap");
    dirtyRoles.insert("iconName");

    QSetIterator<QByteArray> it(dirtyRoles);
    while (it.hasNext()) {
        const QByteArray& role = it.next();
        m_dirtyContentRoles.insert(role);
    }
}

void KStandardItemListWidget::visibleRolesChanged(const QList<QByteArray>& current,
                                              const QList<QByteArray>& previous)
{
    Q_UNUSED(previous);
    m_sortedVisibleRoles = current;
    m_dirtyLayout = true;
}

void KStandardItemListWidget::columnWidthChanged(const QByteArray& role,
                                             qreal current,
                                             qreal previous)
{
    Q_UNUSED(role);
    Q_UNUSED(current);
    Q_UNUSED(previous);
    m_dirtyLayout = true;
}

void KStandardItemListWidget::styleOptionChanged(const KItemListStyleOption& current,
                                             const KItemListStyleOption& previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    updateAdditionalInfoTextColor();
    m_dirtyLayout = true;
}

void KStandardItemListWidget::hoveredChanged(bool hovered)
{
    Q_UNUSED(hovered);
    m_dirtyLayout = true;
}

void KStandardItemListWidget::selectedChanged(bool selected)
{
    Q_UNUSED(selected);
    updateAdditionalInfoTextColor();
    m_dirtyContent = true;
}

void KStandardItemListWidget::siblingsInformationChanged(const QBitArray& current, const QBitArray& previous)
{
    Q_UNUSED(current);
    Q_UNUSED(previous);
    m_dirtyLayout = true;
}

int KStandardItemListWidget::selectionLength(const QString& text) const
{
    return text.length();
}

void KStandardItemListWidget::editedRoleChanged(const QByteArray& current, const QByteArray& previous)
{
    Q_UNUSED(previous);

    QGraphicsView* parent = scene()->views()[0];
    if (current.isEmpty() || !parent || current != "text") {
        if (m_roleEditor) {
            emit roleEditingCanceled(index(), current, data().value(current));

            disconnect(m_roleEditor, SIGNAL(roleEditingCanceled(QByteArray,QVariant)),
                       this, SLOT(slotRoleEditingCanceled(QByteArray,QVariant)));
            disconnect(m_roleEditor, SIGNAL(roleEditingFinished(QByteArray,QVariant)),
                       this, SLOT(slotRoleEditingFinished(QByteArray,QVariant)));

            if (m_oldRoleEditor) {
                m_oldRoleEditor->deleteLater();
            }
            m_oldRoleEditor = m_roleEditor;
            m_roleEditor->hide();
            m_roleEditor = 0;
        }
        return;
    }

    Q_ASSERT(!m_roleEditor);

    const TextInfo* textInfo = m_textInfo.value("text");

    m_roleEditor = new KItemListRoleEditor(parent);
    m_roleEditor->setRole(current);
    m_roleEditor->setFont(styleOption().font);

    const QString text = data().value(current).toString();
    m_roleEditor->setPlainText(text);

    QTextOption textOption = textInfo->staticText.textOption();
    m_roleEditor->document()->setDefaultTextOption(textOption);

    const int textSelectionLength = selectionLength(text);

    if (textSelectionLength > 0) {
        QTextCursor cursor = m_roleEditor->textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, textSelectionLength);
        m_roleEditor->setTextCursor(cursor);
    }

    connect(m_roleEditor, SIGNAL(roleEditingCanceled(QByteArray,QVariant)),
            this, SLOT(slotRoleEditingCanceled(QByteArray,QVariant)));
    connect(m_roleEditor, SIGNAL(roleEditingFinished(QByteArray,QVariant)),
            this, SLOT(slotRoleEditingFinished(QByteArray,QVariant)));

    // Adjust the geometry of the editor
    QRectF rect = roleEditingRect(current);
    const int frameWidth = m_roleEditor->frameWidth();
    rect.adjust(-frameWidth, -frameWidth, frameWidth, frameWidth);
    rect.translate(pos());
    if (rect.right() > parent->width()) {
        rect.setWidth(parent->width() - rect.left());
    }
    m_roleEditor->setGeometry(rect.toRect());
    m_roleEditor->show();
    m_roleEditor->setFocus();
}

void KStandardItemListWidget::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    if (m_roleEditor) {
        setEditedRole(QByteArray());
        Q_ASSERT(!m_roleEditor);
    }

    KItemListWidget::resizeEvent(event);

    m_dirtyLayout = true;
}

void KStandardItemListWidget::showEvent(QShowEvent* event)
{
    KItemListWidget::showEvent(event);

    // Listen to changes of the clipboard to mark the item as cut/uncut
    KFileItemClipboard* clipboard = KFileItemClipboard::instance();

    const KUrl itemUrl = data().value("url").value<KUrl>();
    m_isCut = clipboard->isCut(itemUrl);

    connect(clipboard, SIGNAL(cutItemsChanged()),
            this, SLOT(slotCutItemsChanged()));
}

void KStandardItemListWidget::hideEvent(QHideEvent* event)
{
    disconnect(KFileItemClipboard::instance(), SIGNAL(cutItemsChanged()),
               this, SLOT(slotCutItemsChanged()));

    KItemListWidget::hideEvent(event);
}

void KStandardItemListWidget::slotCutItemsChanged()
{
    const KUrl itemUrl = data().value("url").value<KUrl>();
    const bool isCut = KFileItemClipboard::instance()->isCut(itemUrl);
    if (m_isCut != isCut) {
        m_isCut = isCut;
        m_pixmap = QPixmap();
        m_dirtyContent = true;
        update();
    }
}

void KStandardItemListWidget::slotRoleEditingCanceled(const QByteArray& role,
                                                      const QVariant& value)
{
    closeRoleEditor();
    emit roleEditingCanceled(index(), role, value);
    setEditedRole(QByteArray());
}

void KStandardItemListWidget::slotRoleEditingFinished(const QByteArray& role,
                                                      const QVariant& value)
{
    closeRoleEditor();
    emit roleEditingFinished(index(), role, value);
    setEditedRole(QByteArray());
}

void KStandardItemListWidget::triggerCacheRefreshing()
{
    if ((!m_dirtyContent && !m_dirtyLayout) || index() < 0) {
        return;
    }

    refreshCache();

    const QHash<QByteArray, QVariant> values = data();
    m_isHidden = isHidden();
    m_customizedFont = customizedFont(styleOption().font);
    m_customizedFontMetrics = QFontMetrics(m_customizedFont);

    updateTextsCache();
    updatePixmapCache();

    m_dirtyLayout = false;
    m_dirtyContent = false;
    m_dirtyContentRoles.clear();
}

void KStandardItemListWidget::updatePixmapCache()
{
    // Precondition: Requires already updated m_textPos values to calculate
    // the remaining height when the alignment is vertical.

    const QSizeF widgetSize = size();
    const bool iconOnTop = (m_layout == IconsLayout);
    const KItemListStyleOption& option = styleOption();
    const qreal padding = option.padding;

    const int maxIconWidth = iconOnTop ? widgetSize.width() - 2 * padding : option.iconSize;
    const int maxIconHeight = option.iconSize;

    const QHash<QByteArray, QVariant> values = data();

    bool updatePixmap = (m_pixmap.width() != maxIconWidth || m_pixmap.height() != maxIconHeight);
    if (!updatePixmap && m_dirtyContent) {
        updatePixmap = m_dirtyContentRoles.isEmpty()
                       || m_dirtyContentRoles.contains("iconPixmap")
                       || m_dirtyContentRoles.contains("iconName")
                       || m_dirtyContentRoles.contains("iconOverlays");
    }

    if (updatePixmap) {
        m_pixmap = values["iconPixmap"].value<QPixmap>();
        if (m_pixmap.isNull()) {
            // Use the icon that fits to the MIME-type
            QString iconName = values["iconName"].toString();
            if (iconName.isEmpty()) {
                // The icon-name has not been not resolved by KFileItemModelRolesUpdater,
                // use a generic icon as fallback
                iconName = QLatin1String("unknown");
            }
            const QStringList overlays = values["iconOverlays"].toStringList();
            m_pixmap = pixmapForIcon(iconName, overlays, maxIconHeight);
        } else if (m_pixmap.width() != maxIconWidth || m_pixmap.height() != maxIconHeight) {
            // A custom pixmap has been applied. Assure that the pixmap
            // is scaled to the maximum available size.
            m_pixmap = m_pixmap.scaled(QSize(maxIconWidth, maxIconHeight), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        if (m_isCut) {
            KIconEffect* effect = KIconLoader::global()->iconEffect();
            m_pixmap = effect->apply(m_pixmap, KIconLoader::Desktop, KIconLoader::DisabledState);
        }

        if (m_isHidden) {
            KIconEffect::semiTransparent(m_pixmap);
        }

        if (m_layout == IconsLayout && isSelected()) {
            const QColor color = palette().brush(QPalette::Normal, QPalette::Highlight).color();
            QImage image = m_pixmap.toImage();
            KIconEffect::colorize(image, color, 0.8f);
            m_pixmap = QPixmap::fromImage(image);
        }
    }

    if (!m_overlay.isNull()) {
        QPainter painter(&m_pixmap);
        painter.drawPixmap(0, m_pixmap.height() - m_overlay.height(), m_overlay);
    }

    int scaledIconSize = 0;
    if (iconOnTop) {
        const TextInfo* textInfo = m_textInfo.value("text");
        scaledIconSize = static_cast<int>(textInfo->pos.y() - 2 * padding);
    } else {
        const int textRowsCount = (m_layout == CompactLayout) ? visibleRoles().count() : 1;
        const qreal requiredTextHeight = textRowsCount * m_customizedFontMetrics.height();
        scaledIconSize = (requiredTextHeight < maxIconHeight) ?
                           widgetSize.height() - 2 * padding : maxIconHeight;
    }

    const int maxScaledIconWidth = iconOnTop ? widgetSize.width() - 2 * padding : scaledIconSize;
    const int maxScaledIconHeight = scaledIconSize;

    m_scaledPixmapSize = m_pixmap.size();
    m_scaledPixmapSize.scale(maxScaledIconWidth, maxScaledIconHeight, Qt::KeepAspectRatio);

    if (iconOnTop) {
        // Center horizontally and align on bottom within the icon-area
        m_pixmapPos.setX((widgetSize.width() - m_scaledPixmapSize.width()) / 2);
        m_pixmapPos.setY(padding + scaledIconSize - m_scaledPixmapSize.height());
    } else {
        // Center horizontally and vertically within the icon-area
        const TextInfo* textInfo = m_textInfo.value("text");
        m_pixmapPos.setX(textInfo->pos.x() - 2 * padding
                         - (scaledIconSize + m_scaledPixmapSize.width()) / 2);
        m_pixmapPos.setY(padding
                         + (scaledIconSize - m_scaledPixmapSize.height()) / 2);
    }

    m_iconRect = QRectF(m_pixmapPos, QSizeF(m_scaledPixmapSize));

    // Prepare the pixmap that is used when the item gets hovered
    if (isHovered()) {
        m_hoverPixmap = m_pixmap;
        KIconEffect* effect = KIconLoader::global()->iconEffect();
        // In the KIconLoader terminology, active = hover.
        if (effect->hasEffect(KIconLoader::Desktop, KIconLoader::ActiveState)) {
            m_hoverPixmap = effect->apply(m_pixmap, KIconLoader::Desktop, KIconLoader::ActiveState);
        } else {
            m_hoverPixmap = m_pixmap;
        }
    } else if (hoverOpacity() <= 0.0) {
        // No hover animation is ongoing. Clear m_hoverPixmap to save memory.
        m_hoverPixmap = QPixmap();
    }
}

void KStandardItemListWidget::updateTextsCache()
{
    QTextOption textOption;
    switch (m_layout) {
    case IconsLayout:
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textOption.setAlignment(Qt::AlignHCenter);
        break;
    case CompactLayout:
    case DetailsLayout:
        textOption.setAlignment(Qt::AlignLeft);
        textOption.setWrapMode(QTextOption::NoWrap);
        break;
    default:
        Q_ASSERT(false);
        break;
    }

    qDeleteAll(m_textInfo);
    m_textInfo.clear();
    for (int i = 0; i < m_sortedVisibleRoles.count(); ++i) {
        TextInfo* textInfo = new TextInfo();
        textInfo->staticText.setTextOption(textOption);
        textInfo->staticText.setFont(m_customizedFont);
        m_textInfo.insert(m_sortedVisibleRoles[i], textInfo);
    }

    switch (m_layout) {
    case IconsLayout:   updateIconsLayoutTextCache(); break;
    case CompactLayout: updateCompactLayoutTextCache(); break;
    case DetailsLayout: updateDetailsLayoutTextCache(); break;
    default: Q_ASSERT(false); break;
    }
}

void KStandardItemListWidget::updateIconsLayoutTextCache()
{
    //      +------+
    //      | Icon |
    //      +------+
    //
    //    Name role that
    // might get wrapped above
    //    several lines.
    //  Additional role 1
    //  Additional role 2

    const QHash<QByteArray, QVariant> values = data();

    const KItemListStyleOption& option = styleOption();
    const qreal padding = option.padding;
    const qreal maxWidth = size().width() - 2 * padding;
    const qreal widgetHeight = size().height();
    const qreal lineSpacing = m_customizedFontMetrics.lineSpacing();

    // Initialize properties for the "text" role. It will be used as anchor
    // for initializing the position of the other roles.
    TextInfo* nameTextInfo = m_textInfo.value("text");
    const QString nameText = KStringHandler::preProcessWrap(values["text"].toString());
    nameTextInfo->staticText.setText(nameText);

    // Calculate the number of lines required for the name and the required width
    qreal nameWidth = 0;
    qreal nameHeight = 0;
    QTextLine line;

    QTextLayout layout(nameTextInfo->staticText.text(), m_customizedFont);
    layout.setTextOption(nameTextInfo->staticText.textOption());
    layout.beginLayout();
    int nameLineIndex = 0;
    while ((line = layout.createLine()).isValid()) {
        line.setLineWidth(maxWidth);
        nameWidth = qMax(nameWidth, line.naturalTextWidth());
        nameHeight += line.height();

        ++nameLineIndex;
        if (nameLineIndex == option.maxTextLines) {
            // The maximum number of textlines has been reached. If this is
            // the case provide an elided text if necessary.
            const int textLength = line.textStart() + line.textLength();
            if (textLength < nameText.length()) {
                // Elide the last line of the text
                qreal elidingWidth = maxWidth;
                qreal lastLineWidth;
                do {
                    QString lastTextLine = nameText.mid(line.textStart());
                    lastTextLine = m_customizedFontMetrics.elidedText(lastTextLine,
                                                                      Qt::ElideRight,
                                                                      elidingWidth);
                    const QString elidedText = nameText.left(line.textStart()) + lastTextLine;
                    nameTextInfo->staticText.setText(elidedText);

                    lastLineWidth = m_customizedFontMetrics.boundingRect(lastTextLine).width();

                    // We do the text eliding in a loop with decreasing width (1 px / iteration)
                    // to avoid problems related to different width calculation code paths
                    // within Qt. (see bug 337104)
                    elidingWidth -= 1.0;
                } while (lastLineWidth > maxWidth);

                nameWidth = qMax(nameWidth, lastLineWidth);
            }
            break;
        }
    }
    layout.endLayout();

    // Use one line for each additional information
    const int additionalRolesCount = qMax(visibleRoles().count() - 1, 0);
    nameTextInfo->staticText.setTextWidth(maxWidth);
    nameTextInfo->pos = QPointF(padding, widgetHeight -
                                         nameHeight -
                                         additionalRolesCount * lineSpacing -
                                         padding);
    m_textRect = QRectF(padding + (maxWidth - nameWidth) / 2,
                        nameTextInfo->pos.y(),
                        nameWidth,
                        nameHeight);

    // Calculate the position for each additional information
    qreal y = nameTextInfo->pos.y() + nameHeight;
    foreach (const QByteArray& role, m_sortedVisibleRoles) {
        if (role == "text") {
            continue;
        }

        const QString text = roleText(role, values);
        TextInfo* textInfo = m_textInfo.value(role);
        textInfo->staticText.setText(text);

        qreal requiredWidth = 0;

        QTextLayout layout(text, m_customizedFont);
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::NoWrap);
        layout.setTextOption(textOption);

        layout.beginLayout();
        QTextLine textLine = layout.createLine();
        if (textLine.isValid()) {
            textLine.setLineWidth(maxWidth);
            requiredWidth = textLine.naturalTextWidth();
            if (requiredWidth > maxWidth) {
                const QString elidedText = m_customizedFontMetrics.elidedText(text, Qt::ElideRight, maxWidth);
                textInfo->staticText.setText(elidedText);
                requiredWidth = m_customizedFontMetrics.width(elidedText);;
            }
        }
        layout.endLayout();

        textInfo->pos = QPointF(padding, y);
        textInfo->staticText.setTextWidth(maxWidth);

        const QRectF textRect(padding + (maxWidth - requiredWidth) / 2, y, requiredWidth, lineSpacing);
        m_textRect |= textRect;

        y += lineSpacing;
    }

    // Add a padding to the text rectangle
    m_textRect.adjust(-padding, -padding, padding, padding);
}

void KStandardItemListWidget::updateCompactLayoutTextCache()
{
    // +------+  Name role
    // | Icon |  Additional role 1
    // +------+  Additional role 2

    const QHash<QByteArray, QVariant> values = data();

    const KItemListStyleOption& option = styleOption();
    const qreal widgetHeight = size().height();
    const qreal lineSpacing = m_customizedFontMetrics.lineSpacing();
    const qreal textLinesHeight = qMax(visibleRoles().count(), 1) * lineSpacing;
    const int scaledIconSize = (textLinesHeight < option.iconSize) ? widgetHeight - 2 * option.padding : option.iconSize;

    qreal maximumRequiredTextWidth = 0;
    const qreal x = option.padding * 3 + scaledIconSize;
    qreal y = qRound((widgetHeight - textLinesHeight) / 2);
    const qreal maxWidth = size().width() - x - option.padding;
    foreach (const QByteArray& role, m_sortedVisibleRoles) {
        const QString text = roleText(role, values);
        TextInfo* textInfo = m_textInfo.value(role);
        textInfo->staticText.setText(text);
        textInfo->staticText.setFont(m_customizedFont);

        qreal requiredWidth = m_customizedFontMetrics.width(text);
        if (requiredWidth > maxWidth) {
            requiredWidth = maxWidth;
            const QString elidedText = m_customizedFontMetrics.elidedText(text, Qt::ElideRight, maxWidth);
            textInfo->staticText.setText(elidedText);
        }

        textInfo->pos = QPointF(x, y);
        textInfo->staticText.setTextWidth(maxWidth);

        maximumRequiredTextWidth = qMax(maximumRequiredTextWidth, requiredWidth);

        y += lineSpacing;
    }

    m_textRect = QRectF(x - 2 * option.padding, 0, maximumRequiredTextWidth + 3 * option.padding, widgetHeight);
}

void KStandardItemListWidget::updateDetailsLayoutTextCache()
{
    // +------+
    // | Icon |  Name role   Additional role 1   Additional role 2
    // +------+
    m_textRect = QRectF();

    const KItemListStyleOption& option = styleOption();
    const QHash<QByteArray, QVariant> values = data();

    const qreal widgetHeight = size().height();
    const int scaledIconSize = widgetHeight - 2 * option.padding;
    const int fontHeight = m_customizedFontMetrics.height();

    const qreal columnWidthInc = columnPadding(option);
    qreal firstColumnInc = (scaledIconSize + option.padding);

    qreal x = firstColumnInc;
    const qreal y = qMax(qreal(option.padding), (widgetHeight - fontHeight) / 2);

    foreach (const QByteArray& role, m_sortedVisibleRoles) {
        QString text = roleText(role, values);

        // Elide the text in case it does not fit into the available column-width
        qreal requiredWidth = m_customizedFontMetrics.width(text);
        const qreal roleWidth = columnWidth(role);
        qreal availableTextWidth = roleWidth - columnWidthInc;

        const bool isTextRole = (role == "text");
        if (isTextRole) {
            availableTextWidth -= firstColumnInc;
        }

        if (requiredWidth > availableTextWidth) {
            text = m_customizedFontMetrics.elidedText(text, Qt::ElideRight, availableTextWidth);
            requiredWidth = m_customizedFontMetrics.width(text);
        }

        TextInfo* textInfo = m_textInfo.value(role);
        textInfo->staticText.setText(text);
        textInfo->pos = QPointF(x + columnWidthInc / 2, y);
        x += roleWidth;

        if (isTextRole) {
            const qreal textWidth = option.extendedSelectionRegion
                                    ? size().width() - textInfo->pos.x()
                                    : requiredWidth + 2 * option.padding;
            m_textRect = QRectF(textInfo->pos.x() - 2 * option.padding, 0,
                                textWidth + option.padding, size().height());

            // The column after the name should always be aligned on the same x-position independent
            // from the expansion-level shown in the name column
            x -= firstColumnInc;
        } else if (isRoleRightAligned(role)) {
            textInfo->pos.rx() += roleWidth - requiredWidth - columnWidthInc;
        }
    }
}

void KStandardItemListWidget::updateAdditionalInfoTextColor()
{
    QColor c1;
    if (m_customTextColor.isValid()) {
        c1 = m_customTextColor;
    } else if (isSelected() && m_layout != DetailsLayout) {
        c1 = styleOption().palette.highlightedText().color();
    } else {
        c1 = styleOption().palette.text().color();
    }

    // For the color of the additional info the inactive text color
    // is not used as this might lead to unreadable text for some color schemes. Instead
    // the text color c1 is slightly mixed with the background color.
    const QColor c2 = styleOption().palette.base().color();
    const int p1 = 70;
    const int p2 = 100 - p1;
    m_additionalInfoTextColor = QColor((c1.red()   * p1 + c2.red()   * p2) / 100,
                                       (c1.green() * p1 + c2.green() * p2) / 100,
                                       (c1.blue()  * p1 + c2.blue()  * p2) / 100);
}

void KStandardItemListWidget::drawPixmap(QPainter* painter, const QPixmap& pixmap)
{
    if (m_scaledPixmapSize != pixmap.size()) {
        QPixmap scaledPixmap = pixmap.scaled(m_scaledPixmapSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);;
        painter->drawPixmap(m_pixmapPos, scaledPixmap);

#ifdef KSTANDARDITEMLISTWIDGET_DEBUG
        painter->setPen(Qt::blue);
        painter->drawRect(QRectF(m_pixmapPos, QSizeF(m_scaledPixmapSize)));
#endif
    } else {
        painter->drawPixmap(m_pixmapPos, pixmap);
    }
}

QRectF KStandardItemListWidget::roleEditingRect(const QByteArray& role) const
{
    const TextInfo* textInfo = m_textInfo.value(role);
    if (!textInfo) {
        return QRectF();
    }

    QRectF rect;
    if (m_layout == IconsLayout) {
        const KItemListStyleOption& option = styleOption();
        rect = QRectF(textInfo->pos, textInfo->staticText.size(m_customizedFontMetrics, option.maxTextLines));
    } else {
        rect = QRectF(textInfo->pos, textInfo->staticText.size());
    }

    if (m_layout == DetailsLayout) {
        rect.setWidth(columnWidth(role) - rect.x());
    }

    return rect;
}

void KStandardItemListWidget::closeRoleEditor()
{
    disconnect(m_roleEditor, SIGNAL(roleEditingCanceled(QByteArray,QVariant)),
               this, SLOT(slotRoleEditingCanceled(QByteArray,QVariant)));
    disconnect(m_roleEditor, SIGNAL(roleEditingFinished(QByteArray,QVariant)),
               this, SLOT(slotRoleEditingFinished(QByteArray,QVariant)));

    if (m_roleEditor->hasFocus()) {
        // If the editing was not ended by a FocusOut event, we have
        // to transfer the keyboard focus back to the KItemListContainer.
        scene()->views()[0]->parentWidget()->setFocus();
    }

    if (m_oldRoleEditor) {
        m_oldRoleEditor->deleteLater();
    }
    m_oldRoleEditor = m_roleEditor;
    m_roleEditor->hide();
    m_roleEditor = 0;
}

QPixmap KStandardItemListWidget::pixmapForIcon(const QString& name, const QStringList& overlays, int size)
{
    QByteArray key = "KStandardItemListWidget:" + name.toLatin1() + ":" + overlays.join(":").toLatin1() + ":" + QByteArray::number(size);
    QPixmap pixmap;

    if (!QPixmapCache::find(key, pixmap)) {
        const KIcon icon(name);

        int requestedSize;
        if (size <= KIconLoader::SizeSmall) {
            requestedSize = KIconLoader::SizeSmall;
        } else if (size <= KIconLoader::SizeSmallMedium) {
            requestedSize = KIconLoader::SizeSmallMedium;
        } else if (size <= KIconLoader::SizeMedium) {
            requestedSize = KIconLoader::SizeMedium;
        } else if (size <= KIconLoader::SizeLarge) {
            requestedSize = KIconLoader::SizeLarge;
        } else if (size <= KIconLoader::SizeHuge) {
            requestedSize = KIconLoader::SizeHuge;
        } else if (size <= KIconLoader::SizeEnormous) {
            requestedSize = KIconLoader::SizeEnormous;
        } else if (size <= KIconLoader::SizeEnormous * 2) {
            requestedSize = KIconLoader::SizeEnormous * 2;
        } else {
            requestedSize = size;
        }

        pixmap = icon.pixmap(requestedSize, requestedSize);
        if (requestedSize != size) {
            pixmap = pixmap.scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation);;
        }

        // Strangely KFileItem::overlays() returns empty string-values, so
        // we need to check first whether an overlay must be drawn at all.
        // It is more efficient to do it here, as KIconLoader::drawOverlays()
        // assumes that an overlay will be drawn and has some additional
        // setup time.
        foreach (const QString& overlay, overlays) {
            if (!overlay.isEmpty()) {
                // There is at least one overlay, draw all overlays above m_pixmap
                // and cancel the check
                KIconLoader::global()->drawOverlays(overlays, pixmap, KIconLoader::Desktop);
                break;
            }
        }

        QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
}

qreal KStandardItemListWidget::columnPadding(const KItemListStyleOption& option)
{
    return option.padding * 6;
}

#include "moc_kstandarditemlistwidget.cpp"
