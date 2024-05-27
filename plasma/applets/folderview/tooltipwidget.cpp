/*
 *   Copyright © 2009 Fredrik Höglund <fredrik@kde.org>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public License
 *   along with this library; see the file COPYING.LIB.  If not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "tooltipwidget.h"
#include "abstractitemview.h"
#include "proxymodel.h"

#include <QApplication>
#include <QtGui/qgraphicssceneevent.h>
#include <QtCore/qabstractitemmodel.h>

#include <KFileMetaInfo>
#include <KDesktopFile>
#include <KDirModel>
#include <KLocale>
#include <KIO/PreviewJob>
#include <Plasma/ToolTipManager>


ToolTipWidget::ToolTipWidget(AbstractItemView *parent)
    : QGraphicsWidget(parent), m_view(parent), m_previewJob(0)
{
    Plasma::ToolTipManager::self()->registerWidget(this);
}

void ToolTipWidget::updateToolTip(const QModelIndex &index, const QRectF &rect)
{
    if (!index.isValid()) {
        // Send a fake hover leave event to the widget to trick the tooltip
        // manager into doing a delayed hide.
        QGraphicsSceneHoverEvent event(QEvent::GraphicsSceneHoverLeave);
        QApplication::sendEvent(this, &event);

        m_preview = QPixmap();
        m_item = KFileItem();
        m_index = QModelIndex();
        return;
    }

    setGeometry(rect);
    m_item = static_cast<ProxyModel*>(m_view->model())->itemForIndex(index);
    m_index = index;
    m_preview = QPixmap();

    // If a preview job is still running (from a previously hovered item),
    // wait 200 ms before starting a new one. This is done to throttle
    // the number of preview jobs that are started when the user moves
    // the cursor over the icon view.
    if (m_previewJob) {
        m_previewTimer.start(200, this);
    } else {
        if (m_previewTimer.isActive()) {
            m_previewTimer.stop();
        }
        startPreviewJob();
    }

    Plasma::ToolTipManager::self()->show(this);
}

QString ToolTipWidget::metaInfo() const
{
    const QString mimetype = m_item.mimetype();
    if (!mimetype.startsWith(QLatin1String("audio/")) &&
        !mimetype.startsWith(QLatin1String("video/")) &&
        !mimetype.startsWith(QLatin1String("image/")) &&
        !m_item.mimeTypePtr()->is("application/vnd.oasis.opendocument.text"))
    {
        return QString();
    }

    const KFileMetaInfo info(m_item.url());
    const QStringList preferredinfo = info.preferredKeys();
    QString text = "<p><table border='0' cellspacing='0' cellpadding='0'>";
    foreach (const KFileMetaInfoItem &it, info.items()) {
        const QString itvalue = it.value();
        if (itvalue.isEmpty() || !preferredinfo.contains(it.key())) {
            continue;
        }
        text += QString("<tr><td>") + it.name() + QString(": </td><td>") + itvalue + QString("</td></tr>");
    }
    text += "</table>";

    return text;
}

void ToolTipWidget::setContent()
{
    Plasma::ToolTipContent content;
    content.setMainText(m_index.data(Qt::DisplayRole).toString());

    if (m_preview.isNull()) {
        content.setImage(qvariant_cast<QIcon>(m_index.data(Qt::DecorationRole)));
    } else {
        content.setImage(m_preview);
    }

    QString subText;

    if (m_item.isDesktopFile()) {
        // Add the comment in the .desktop file to the subtext.
        // Note that we don't include the mime type for .desktop files,
        // since users will likely be confused about what will happen when
        // they click a "Desktop configuration file" on their desktop.
        KDesktopFile file(m_item.localPath());
        subText = file.readComment();
    } else {
        if (!m_item.mimeTypePtr().isNull()) {
            subText = m_item.mimeComment();
        }

        if (m_item.isDir()) {
            // Include information about the number of files and folders in the directory.
            const QVariant value = m_index.data(KDirModel::ChildCountRole);
            const int count = value.type() == QVariant::Int ? value.toInt() : KDirModel::ChildCountUnknown;

            if (count != KDirModel::ChildCountUnknown) {
                subText += QString("<br>") + i18ncp("Items in a folder", "1 item", "%1 items", count);
            }
        } else {
            // File size
            if (m_item.isFile()) {
                subText += QString("<br>") + KGlobal::locale()->formatByteSize(m_item.size());
            }

            // Add meta info from the strigi analyzers
            subText += metaInfo();
        }
    }

    content.setSubText(subText);
    content.setAutohide(false);

    Plasma::ToolTipManager::self()->setContent(this, content);
}

void ToolTipWidget::startPreviewJob()
{
    m_previewJob = KIO::filePreview(KFileItemList() << m_item, QSize(256, 256));
    connect(m_previewJob, SIGNAL(gotPreview(KFileItem,QPixmap)), SLOT(gotPreview(KFileItem,QPixmap)));
    connect(m_previewJob, SIGNAL(finished(KJob*)), SLOT(previewJobFinished(KJob*)));
}

void ToolTipWidget::gotPreview(const KFileItem &item, const QPixmap &pixmap)
{
    if (item == m_item) {
        m_preview = pixmap;
        setContent();
    } else if (m_item.isNull()) {
        m_preview = QPixmap();
    }
}

void ToolTipWidget::previewJobFinished(KJob *job)
{
    if (job == m_previewJob) {
        m_previewJob = 0;
    }
}

void ToolTipWidget::toolTipAboutToShow()
{
    if (m_index.isValid()) {
        setContent();
        m_hideTimer.start(10000, this);
    } else {
        Plasma::ToolTipManager::self()->clearContent(this);
    }
}

void ToolTipWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();

        if (m_index.isValid()) {
            startPreviewJob();
        }
    }

    if (event->timerId() == m_hideTimer.timerId()) {
        m_hideTimer.stop();
        Plasma::ToolTipManager::self()->hide(this);
    }
}

#include "moc_tooltipwidget.cpp"
