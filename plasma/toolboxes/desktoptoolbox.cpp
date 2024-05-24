/*
 *   Copyright 2007 by Aaron Seigo <aseigo@kde.org>
 *   Copyright 2008 by Marco Martin <notmart@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "desktoptoolbox.h"

#include <QAction>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QtGui/qgraphicssceneevent.h>
#include <QPainter>
#include <QGraphicsLinearLayout>
#include <QGraphicsView>
#include <QTimer>
#include <QtCore/qsharedpointer.h>

#include <KDebug>
#include <QToolButton>
#include <KIconLoader>
#include <Plasma/Animation>
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/FrameSvg>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipContent>
#include <Plasma/ToolTipManager>
#include <kworkspace/kworkspace.h>

#include <limits.h>

class EmptyGraphicsItem : public QGraphicsWidget
{
    public:
        EmptyGraphicsItem(QGraphicsItem *parent)
            : QGraphicsWidget(parent)
        {
            m_layout = new QGraphicsLinearLayout(this);
            m_layout->setContentsMargins(0, 0, 0, 0);
            m_background = new Plasma::FrameSvg(this);
            m_background->setImagePath("widgets/background");
            m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
            m_layout->setOrientation(Qt::Vertical);

            qreal left = 0.0;
            qreal top = 0.0;
            qreal right = 0.0;
            qreal bottom = 0.0;
            m_background->getMargins(left, top, right, bottom);
            setContentsMargins(left, top, right, bottom);
        }

        void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
        {
            m_background->paintFrame(p, option->rect, option->rect);
        }

        void clearLayout()
        {
            while (m_layout->count()) {
                m_layout->removeAt(0);
            }
        }

        void addToLayout(QGraphicsWidget *widget)
        {
            m_layout->addItem(widget);
        }

    protected:
        void resizeEvent(QGraphicsSceneResizeEvent *event)
        {
            QGraphicsWidget::resizeEvent(event);
            m_background->resizeFrame(size());
        }

    private:
        Plasma::FrameSvg *m_background;
        QGraphicsLinearLayout *m_layout;
};

DesktopToolBox::DesktopToolBox(Plasma::Containment *parent)
    : InternalToolBox(parent)
{
    m_containment = parent;
    init();
}

DesktopToolBox::DesktopToolBox(QObject *parent, const QVariantList &args)
   : InternalToolBox(parent, args)
{
    m_containment = qobject_cast<Plasma::Containment *>(parent);
    init();
}

void DesktopToolBox::init()
{
    m_icon = KIcon("plasma");
    m_toolBacker = 0;
    m_animHighlightFrame = 0;
    m_hovering = false;
    m_background = new Plasma::FrameSvg(this);
    m_background->setImagePath("widgets/toolbox");

    setZValue(INT_MAX);

    setIsMovable(true);
    setAcceptHoverEvents(true);
    setFlags(flags() | QGraphicsItem::ItemIsFocusable);
    updateTheming();

    connect(this, SIGNAL(toggled()), this, SLOT(toggle()));
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
            this, SLOT(updateTheming()));
    Plasma::ToolTipManager::self()->registerWidget(this);

    QAction *action = new QAction(i18n("Leave..."), this);
    action->setIcon(KIcon("system-shutdown"));
    connect(action, SIGNAL(triggered()), this, SLOT(startLogout()));
    addTool(action);
}

QSize DesktopToolBox::cornerSize() const
{
    m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;
    m_background->getMargins(left, top, right, bottom);
    adjustBackgroundBorders();

    return QSize(size() + left, size() + bottom);
}

QSize DesktopToolBox::fullWidth() const
{
    m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;
    m_background->getMargins(left, top, right, bottom);
    adjustBackgroundBorders();
    return QSize(size() + left + right, size() + bottom);
}

QSize DesktopToolBox::fullHeight() const
{
    m_background->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;
    m_background->getMargins(left, top, right, bottom);
    adjustBackgroundBorders();
    return QSize(size() + left, size() + top + bottom);
}

void DesktopToolBox::toolTipAboutToShow()
{
    if (isShowing()) {
        return;
    }

    Plasma::ToolTipContent c(
        i18n("Tool Box"),
        i18n("Click to access configuration options and controls, or to add more widgets to the %1.", containment()->name()),
        KIcon("plasma")
    );
    c.setAutohide(false);
    Plasma::ToolTipManager::self()->setContent(this, c);
}

void DesktopToolBox::toolTipHidden()
{
    Plasma::ToolTipManager::self()->clearContent(this);
}

QRectF DesktopToolBox::boundingRect() const
{
    int extraSpace = size();
    adjustBackgroundBorders();

    qreal left, top, right, bottom;
    m_background->getMargins(left, top, right, bottom);

    QRectF rect;

    //disable text at corners
    if (corner() == TopLeft || corner() == TopRight || corner() == BottomLeft || corner() == BottomRight) {
        rect = QRectF(0, 0, size()+left+right, size()+top+bottom);
    } else if (corner() == Left || corner() == Right) {
        rect = QRectF(0, 0, size()+left+right, size()+extraSpace+top+bottom);
    //top or bottom
    } else {
        rect = QRectF(0, 0, size()+extraSpace+left+right, size()+top+bottom);
    }

    return rect;
}

void DesktopToolBox::updateTheming()
{
    update();
}

void DesktopToolBox::toolTriggered(bool)
{
    QAction *action = qobject_cast<QAction *>(sender());

    if (isShowing() && (!action || !action->autoRepeat())) {
        emit toggled();
    }
}

void DesktopToolBox::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    const QRectF rect = boundingRect();
    QString cornerElement;
    switch (corner()) {
    case TopLeft:
        cornerElement = "desktop-northwest";
        break;
    case TopRight:
        cornerElement = "desktop-northeast";
        break;
    case BottomRight:
        cornerElement = "desktop-southeast";
        break;
    case BottomLeft:
        cornerElement = "desktop-southwest";
        break;
    default:
        break;
    }

    adjustBackgroundBorders();
    m_background->resizeFrame(rect.size());

    if (!cornerElement.isNull()) {
        m_background->paint(painter, rect, cornerElement);
    } else {
        m_background->paintFrame(painter, rect.topLeft());
    }

    QRect iconRect = QStyle::alignedRect(
        QApplication::layoutDirection(), Qt::AlignCenter,
        iconSize(), m_background->contentsRect().toRect()
    );
    iconRect.moveTopLeft(iconRect.topLeft() + rect.topLeft().toPoint());
    const QPoint iconPos = iconRect.topLeft();

    if (qFuzzyCompare(qreal(1.0), m_animHighlightFrame)) {
        m_icon.paint(painter, QRect(iconPos, iconSize()));
    } else if (qFuzzyCompare(qreal(1.0), 1 + m_animHighlightFrame)) {
        m_icon.paint(
            painter, QRect(iconPos, iconSize()),
            Qt::AlignCenter, QIcon::Disabled, QIcon::Off
        );
    } else {
        painter->drawPixmap(
            QRect(iconPos, iconSize()),
                Plasma::PaintUtils::transition(
                m_icon.pixmap(iconSize(), QIcon::Disabled, QIcon::Off),
                m_icon.pixmap(iconSize()), m_animHighlightFrame
            )
        );
    }
}

void DesktopToolBox::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (isShowing() || m_hovering) {
        QGraphicsItem::hoverEnterEvent(event);
        return;
    }

    highlight(true);

    QGraphicsItem::hoverEnterEvent(event);
}

void DesktopToolBox::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    //kDebug() << event->pos() << event->scenePos()
    //         << m_toolBacker->rect().contains(event->scenePos().toPoint());
    if (!m_hovering || isShowing()) {
        QGraphicsItem::hoverLeaveEvent(event);
        return;
    }

    highlight(false);

    QGraphicsItem::hoverLeaveEvent(event);
}

QPainterPath DesktopToolBox::shape() const
{
    const QRectF rect = boundingRect();
    const int w = rect.width();
    const int h = rect.height();

    QPainterPath path;
    switch (corner()) {
    case BottomLeft:
        path.moveTo(rect.bottomLeft());
        path.arcTo(QRectF(rect.left() - w, rect.top(), w * 2, h * 2), 0, 90);
        break;
    case BottomRight:
        path.moveTo(rect.bottomRight());
        path.arcTo(QRectF(rect.left(), rect.top(), w * 2, h * 2), 90, 90);
        break;
    case TopRight:
        path.moveTo(rect.topRight());
        path.arcTo(QRectF(rect.left(), rect.top() - h, w * 2, h * 2), 180, 90);
        break;
    case TopLeft:
        path.arcTo(QRectF(rect.left() - w, rect.top() - h, w * 2, h * 2), 270, 90);
        break;
    default:
        path.addRect(rect);
        break;
    }

    return path;
}

QGraphicsWidget *DesktopToolBox::toolParent()
{
    if (!m_toolBacker) {
        m_toolBacker = new EmptyGraphicsItem(this);
        m_toolBacker->hide();
    }

    return m_toolBacker;
}

void DesktopToolBox::showToolBox()
{
    if (isShowing()) {
        return;
    }

    if (!m_toolBacker) {
        m_toolBacker = new EmptyGraphicsItem(this);
    }

    m_toolBacker->setZValue(zValue() + 1);

    adjustToolBackerGeometry();

    m_toolBacker->setOpacity(0);
    m_toolBacker->show();
    Plasma::Animation *fadeAnim = Plasma::Animator::create(Plasma::Animator::FadeAnimation, m_toolBacker);
    fadeAnim->setTargetWidget(m_toolBacker);
    fadeAnim->setProperty("startOpacity", 0);
    fadeAnim->setProperty("targetOpacity", 1);
    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
    highlight(true);
    setFocus();
}

void DesktopToolBox::addTool(QAction *action)
{
    if (!action) {
        return;
    }

    if (actions().contains(action)) {
        return;
    }

    InternalToolBox::addTool(action);
    Plasma::ToolButton *tool = new Plasma::ToolButton(toolParent());

    QToolButton* nativetool = tool->nativeWidget();
    tool->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    nativetool->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    nativetool->setIconSize(QSize(KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium));
    tool->setAction(action);

    tool->hide();
    const int height = static_cast<int>(tool->boundingRect().height());
    tool->setPos(toolPosition(height));
    tool->setZValue(zValue() + 10);
    tool->setToolTip(action->text());

    // make enabled/disabled tools appear/disappear instantly
    connect(action, SIGNAL(changed()), this, SLOT(updateToolBox()));

    ToolType type = AbstractToolBox::MiscTool;
    if (!action->data().isNull() && action->data().type() == QVariant::Int) {
        int t = action->data().toInt();
        if (t >= 0 && t < AbstractToolBox::UserToolType) {
            type = static_cast<AbstractToolBox::ToolType>(t);
        }
    }

    m_tools.insert(type, tool);
    //kDebug() << "added tool" << type << action->text();
}

void DesktopToolBox::updateToolBox()
{
    Plasma::ToolButton *tool = qobject_cast<Plasma::ToolButton *>(sender());
    if (tool && !tool->action()) {
        QMutableMapIterator<ToolType, Plasma::ToolButton *> it(m_tools);
        while (it.hasNext()) {
            it.next();
            if (it.value() == tool) {
                it.remove();
                break;
            }
        }

        tool->deleteLater();
        tool = 0;
    }

    if (isShowing()) {
        showToolBox();
    } else if (tool && !tool->isEnabled()) {
        tool->hide();
    }

    adjustToolBackerGeometry();
}

void DesktopToolBox::removeTool(QAction *action)
{
    QMutableMapIterator<ToolType, Plasma::ToolButton *> it(m_tools);
    while (it.hasNext()) {
        it.next();
        Plasma::ToolButton *tool = it.value();
        //kDebug() << "checking tool" << tool
        if (tool && tool->action() == action) {
            //kDebug() << "tool found!";
            tool->deleteLater();
            it.remove();
            break;
        }
    }
}

void DesktopToolBox::adjustToolBackerGeometry()
{
    if (!m_toolBacker) {
        return;
    }

    m_toolBacker->clearLayout();
    QMapIterator<ToolType, Plasma::ToolButton *> it(m_tools);
    while (it.hasNext()) {
        it.next();
        Plasma::ToolButton *tool = it.value();
        //kDebug() << "showing off" << it.key() << tool->text();
        if (tool->isEnabled()) {
            tool->show();
            m_toolBacker->addToLayout(tool);
        } else {
            tool->hide();
        }
    }

    qreal left, top, right, bottom;
    m_toolBacker->getContentsMargins(&left, &top, &right, &bottom);
    m_toolBacker->adjustSize();

    int x = 0;
    int y = 0;
    const int iconWidth = KIconLoader::SizeMedium;
    switch (corner()) {
    case TopRight:
        x = (int)boundingRect().left() - m_toolBacker->size().width();
        y = (int)boundingRect().top();
        break;
    case Top:
        x = (int)boundingRect().center().x() - (m_toolBacker->size().width() / 2);
        y = (int)boundingRect().bottom();
        break;
    case TopLeft:
        x = (int)boundingRect().right();
        y = (int)boundingRect().top();
        break;
    case Left:
        x = (int)boundingRect().left() + iconWidth;
        y = (int)boundingRect().y();
        break;
    case Right:
        x = (int)boundingRect().right() - iconWidth - m_toolBacker->size().width();
        y = (int)boundingRect().y();
        break;
    case BottomLeft:
        x = (int)boundingRect().left() + iconWidth;
        y = (int)boundingRect().bottom();
        break;
    case Bottom:
        x = (int)boundingRect().center().x() - (m_toolBacker->size().width() / 2);
        y = (int)boundingRect().top();
        break;
    case BottomRight:
    default:
        x = (int)boundingRect().right() - iconWidth - m_toolBacker->size().width();
        y = (int)boundingRect().top();
        break;
    }

    //kDebug() << "starting at" <<  x << startY;
    m_toolBacker->setPos(x, y);
    // now check that it actually fits within the parent's boundaries
    QRectF backerRect = mapToParent(m_toolBacker->geometry()).boundingRect();
    QSizeF parentSize = parentWidget()->size();
    if (backerRect.x() < 5) {
        m_toolBacker->setPos(mapFromParent(QPointF(5, 0)).x(), y);
    } else if (backerRect.right() > parentSize.width() - 5) {
        m_toolBacker->setPos(mapFromParent(QPointF(parentSize.width() - 5 - backerRect.width(), 0)).x(), y);
    }

    if (backerRect.y() < 5) {
        m_toolBacker->setPos(x, mapFromParent(QPointF(0, 5)).y());
    } else if (backerRect.bottom() > parentSize.height() - 5) {
        m_toolBacker->setPos(x, mapFromParent(QPointF(0, parentSize.height() - 5 - backerRect.height())).y());
    }
}

void DesktopToolBox::hideToolBox()
{
    if (m_toolBacker) {
        Plasma::Animation *fadeAnim = Plasma::Animator::create(Plasma::Animator::FadeAnimation, m_toolBacker);
        connect(fadeAnim, SIGNAL(finished()), this, SLOT(hideToolBacker()));
        fadeAnim->setTargetWidget(m_toolBacker);
        fadeAnim->setProperty("startOpacity", 1);
        fadeAnim->setProperty("targetOpacity", 0);
        fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    highlight(false);
}

void DesktopToolBox::hideToolBacker()
{
    m_toolBacker->hide();
}

void DesktopToolBox::highlight(bool highlighting)
{
    if (m_hovering == highlighting) {
        return;
    }

    m_hovering = highlighting;

    QPropertyAnimation *anim = m_anim.data();
    if (m_hovering) {
        if (anim) {
            anim->stop();
            m_anim.clear();
        }
        anim = new QPropertyAnimation(this, "highlight", this);
        m_anim = anim;
    }

    if (anim->state() != QAbstractAnimation::Stopped) {
        anim->stop();
    }

    anim->setDuration(250);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);

    if (m_hovering) {
        anim->start();
    } else {
        anim->setDirection(QAbstractAnimation::Backward);
        anim->start(QAbstractAnimation::DeleteWhenStopped);

    }
}

void DesktopToolBox::setHighlight(qreal progress)
{
    m_animHighlightFrame = progress;
    update();
}

qreal DesktopToolBox::highlight()
{
    return m_animHighlightFrame;
}

void DesktopToolBox::toggle()
{
    setShowing(!isShowing());
}

void DesktopToolBox::adjustBackgroundBorders() const
{
    Plasma::FrameSvg *background = const_cast<Plasma::FrameSvg *>(m_background);

    switch (corner()) {
        case InternalToolBox::TopRight:
            background->setEnabledBorders(Plasma::FrameSvg::BottomBorder | Plasma::FrameSvg::LeftBorder);
            break;
        case InternalToolBox::Top:
            background->setEnabledBorders(Plasma::FrameSvg::BottomBorder | Plasma::FrameSvg::LeftBorder | Plasma::FrameSvg::RightBorder);
            break;
        case InternalToolBox::TopLeft:
            background->setEnabledBorders(Plasma::FrameSvg::BottomBorder | Plasma::FrameSvg::RightBorder);
            break;
        case InternalToolBox::Left:
            background->setEnabledBorders(Plasma::FrameSvg::BottomBorder | Plasma::FrameSvg::TopBorder | Plasma::FrameSvg::RightBorder);
            break;
        case InternalToolBox::Right:
            background->setEnabledBorders(Plasma::FrameSvg::BottomBorder | Plasma::FrameSvg::TopBorder | Plasma::FrameSvg::LeftBorder);
            break;
        case InternalToolBox::BottomLeft:
            background->setEnabledBorders(Plasma::FrameSvg::TopBorder | Plasma::FrameSvg::RightBorder);
            break;
        case InternalToolBox::Bottom:
            background->setEnabledBorders(Plasma::FrameSvg::TopBorder | Plasma::FrameSvg::LeftBorder | Plasma::FrameSvg::RightBorder);
            break;
        case InternalToolBox::BottomRight:
        default:
            m_background->setEnabledBorders(Plasma::FrameSvg::TopBorder | Plasma::FrameSvg::LeftBorder);
            break;
    }
}

void DesktopToolBox::startLogout()
{
    if (m_containment) {
        m_containment->closeToolBox();
    } else {
        setShowing(false);
    }

    // this short delay is due to two issues:
    // a) KWorkSpace's DBus alls are all syncronous
    // b) the destrution of the menu that this action is in is delayed
    //
    // (a) leads to the menu hanging out where everyone can see it because
    // the even loop doesn't get returned to allowing it to close.
    //
    // (b) leads to a 0ms timer not working since a 0ms timer just appends to
    // the event queue, and then the menu closing event gets appended to that.
    //
    // ergo a timer with small timeout
    QTimer::singleShot(10, this, SLOT(logout()));
}

void DesktopToolBox::logout()
{
    KWorkSpace::requestShutDown();
}

// TODO: what is the focus for if key events are not passed to tools and the toolbox is hidden right
// away? that means action shortcuts are non-operational!
void DesktopToolBox::keyPressEvent(QKeyEvent *event)
{
    m_containment->setFocus();
    //don't make the containment lose keystrokes
    if (scene()) {
        scene()->sendEvent(m_containment, event);
    }
    setShowing(false);
}

#include "moc_desktoptoolbox.cpp"
