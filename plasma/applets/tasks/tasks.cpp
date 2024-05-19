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

#include "tasks.h"

#include <QTimer>
#include <QGridLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QPropertyAnimation>
#include <Plasma/Animation>
#include <Plasma/Svg>
#include <Plasma/FrameSvg>
#include <Plasma/SvgWidget>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <KWindowSystem>
#include <KIcon>
#include <KIconLoader>
#include <KIconEffect>
#include <KDebug>

// standard issue margin/spacing
static const int s_spacing = 6;
static const TasksApplet::ToolTipMode s_defaultooltipmode = TasksApplet::ToolTipPreview;
// NOTE: same as the one in:
// kdelibs/plasma/animations/animation.cpp
static const int s_animationduration = 250;
// NOTE: same as the one in:
// kdelibs/plasma/popupapplet.cpp
static const int s_attentioninterval = 500;

static bool kIsPanel(const Plasma::FormFactor formfactor)
{
    switch (formfactor) {
        case Plasma::FormFactor::Planar:
        case Plasma::FormFactor::MediaCenter:
        case Plasma::FormFactor::Application: {
            return false;
        }
        default: {
            return true;
        }
    }
    Q_UNREACHABLE();
}

static QSizeF kTaskSize(const QSizeF appletsize, const Plasma::FormFactor formfactor)
{
    qreal panelspecial = 0;
    if (!kIsPanel(formfactor)) {
        panelspecial = (s_spacing * 4);
    }
    const qreal iconsize = qMin(appletsize.width(), appletsize.height()) - panelspecial;
    return QSizeF(iconsize, iconsize);
}

static QString kElementPrefixForTask(const bool isactive, const bool demandsattention,
                                     const bool minimized)
{
    if (isactive) {
        return QString::fromLatin1("focus");
    } else if (demandsattention) {
        return QString::fromLatin1("attention");
    } else if (minimized) {
        return QString::fromLatin1("minimized");
    }
    return QString::fromLatin1("normal");
}

class TasksSvg : public Plasma::SvgWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    TasksSvg(const WId task, const TasksApplet::ToolTipMode tooltipmode, QGraphicsItem *parent = nullptr);

    qreal hover() const;
    void setHover(qreal hover);

    WId task() const;
    void animatedShow();
    void animatedRemove();

    void setup(const TasksApplet::ToolTipMode tooltipmode);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) final;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const final;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) final;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) final;

private Q_SLOTS:
    void slotClicked(const Qt::MouseButton button);
    void slotWindowPreviewActivated(const WId task);
    void slotUpdate();
    void slotUpdateSvg();
    void slotTaskChanged(const WId task);
    void slotTimeout();

private:
    void updateTaskAndToolTip();

    WId m_task;
    Plasma::FrameSvg* m_framesvg;
    Plasma::FrameSvg* m_hoversvg;
    QPropertyAnimation* m_animation;
    qreal m_hover;
    QPixmap m_pixmap;
    QString m_name;
    bool m_demandsattention;
    QTimer* m_attentiontimer;
    bool m_minimized;
    TasksApplet::ToolTipMode m_tooltipmode;

    // for updateGeometry()
    friend TasksApplet;
};

TasksSvg::TasksSvg(const WId task, const TasksApplet::ToolTipMode tooltipmode, QGraphicsItem *parent)
    : Plasma::SvgWidget(parent),
    m_task(task),
    m_framesvg(nullptr),
    m_hoversvg(nullptr),
    m_animation(nullptr),
    m_hover(0.0),
    m_demandsattention(false),
    m_attentiontimer(nullptr),
    m_minimized(false),
    m_tooltipmode(tooltipmode)
{
    updateTaskAndToolTip();
    slotUpdateSvg();
    setAcceptHoverEvents(true);
    connect(
        this, SIGNAL(clicked(Qt::MouseButton)),
        this, SLOT(slotClicked(Qt::MouseButton))
    );
    connect(
        Plasma::ToolTipManager::self(), SIGNAL(windowPreviewActivated(WId,Qt::MouseButtons,Qt::KeyboardModifiers,QPoint)),
        this, SLOT(slotWindowPreviewActivated(WId))
    );
    connect(
        KTaskManager::self(), SIGNAL(taskChanged(WId)),
        this, SLOT(slotTaskChanged(WId))
    );
    connect(
        KWindowSystem::self(), SIGNAL(activeWindowChanged(WId)),
        this, SLOT(slotUpdate())
    );
    connect(
        Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
        this, SLOT(slotUpdateSvg())
    );
}

qreal TasksSvg::hover() const
{
    return m_hover;
}

void TasksSvg::setHover(qreal hover)
{
    m_hover = hover;
    update();
}

WId TasksSvg::task() const
{
    return m_task;
}

void TasksSvg::animatedShow()
{
    setOpacity(0.0);
    show();
    Plasma::Animation *animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
    Q_ASSERT(animation != nullptr);
    animation->setTargetWidget(this);
    animation->setProperty("startOpacity", 0.0);
    animation->setProperty("targetOpacity", 1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void TasksSvg::animatedRemove()
{
    Plasma::Animation *animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
    Q_ASSERT(animation != nullptr);
    connect(animation, SIGNAL(finished()), this, SLOT(deleteLater()));
    animation->setTargetWidget(this);
    animation->setProperty("startOpacity", 1.0);
    animation->setProperty("targetOpacity", 0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void TasksSvg::setup(const TasksApplet::ToolTipMode tooltipmode)
{
    m_tooltipmode = tooltipmode;
    updateTaskAndToolTip();
}

void TasksSvg::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    const bool isactive = KTaskManager::isActive(m_task);
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF brect = boundingRect();
    const QSizeF brectsize = brect.size();
    m_framesvg->setElementPrefix(kElementPrefixForTask(isactive, m_demandsattention, m_minimized));
    m_framesvg->resizeFrame(brectsize);
    m_framesvg->paintFrame(painter, brect);
    const qreal oldopacity = painter->opacity();
    painter->setOpacity(m_hover);
    m_hoversvg->resizeFrame(brectsize);
    m_hoversvg->paintFrame(painter, brect);
    painter->setOpacity(oldopacity);
    const int spacingx2 = (s_spacing * 2);
    const int iconsize = qRound(qMin(brectsize.width(), brectsize.height()));
    QPixmap iconpixmap = m_pixmap;
    if (!iconpixmap.isNull()) {
        iconpixmap = iconpixmap.scaled(
            iconsize - spacingx2,
            iconsize - spacingx2,
            Qt::KeepAspectRatio, Qt::SmoothTransformation
        );
    }
    // gray-out unless the task is active or demands attention
    if (!iconpixmap.isNull() && !isactive && !m_demandsattention) {
        iconpixmap = KIconEffect::apply(iconpixmap, KIconEffect::ToGray, 0.5, QColor(), QColor(), true);
    }
    if (!iconpixmap.isNull()) {
        painter->drawPixmap(QPoint(s_spacing, s_spacing), iconpixmap);
    }
}

QSizeF TasksSvg::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    const TasksApplet* tasksapplet = qobject_cast<TasksApplet*>(parentObject());
    Q_ASSERT(tasksapplet != nullptr);
    // the panel containment completely ignores minimum size so no hint for that
    if (kIsPanel(tasksapplet->formFactor()) && which == Qt::MinimumSize) {
        return Plasma::SvgWidget::sizeHint(which, constraint);
    }
    if (which != Qt::MaximumSize) {
        const int paneliconsize = KIconLoader::global()->currentSize(KIconLoader::Panel);
        return QSizeF(paneliconsize, paneliconsize);
    }
    return Plasma::SvgWidget::sizeHint(which, constraint);
}

void TasksSvg::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_animation) {
        m_animation->stop();
    } else {
        m_animation = new QPropertyAnimation(this, "hover", this);
        m_animation->setDuration(s_animationduration);
    }
    m_animation->setStartValue(m_hover);
    m_animation->setEndValue(1.0);
    m_animation->start(QAbstractAnimation::KeepWhenStopped);
}

void TasksSvg::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_animation) {
        m_animation->stop();
    } else {
        m_animation = new QPropertyAnimation(this, "hover", this);
        m_animation->setDuration(s_animationduration);
    }
    m_animation->setStartValue(m_hover);
    m_animation->setEndValue(0.0);
    m_animation->start(QAbstractAnimation::KeepWhenStopped);
}

void TasksSvg::updateTaskAndToolTip()
{
    m_demandsattention = KTaskManager::demandsAttention(m_task);
    // during task removal (when the window is no more) getting icon will not work
    const QPixmap windowicon = KWindowSystem::icon(
        m_task, -1, -1, false,
        KWindowSystem::NETWM | KWindowSystem::WMHints | KWindowSystem::ClassHint
    );
    if (!windowicon.isNull()) {
        m_pixmap = windowicon;
    }
    // the fallback pixmap is same as the one of KWindowSystem::icon() except that when even that
    // is not found the "unknown" icon is used instead
    if (m_pixmap.isNull()) {
        m_pixmap = KIconLoader::global()->loadIcon("xorg", KIconLoader::Small);
    }
    const KWindowInfo kwindowinfo = KWindowSystem::windowInfo(
        m_task,
        NET::WMVisibleName | NET::WMDesktop | NET::WMState | NET::XAWMState
    );
    const QString windowname = kwindowinfo.visibleName();
    if (!windowname.isEmpty()) {
        m_name = windowname;
    }
    m_minimized = kwindowinfo.isMinimized();
    const bool oncurrentdesktop = kwindowinfo.isOnDesktop(KWindowSystem::currentDesktop());
    const bool isvisible = isVisible();
    if (!isvisible && oncurrentdesktop) {
        animatedShow();
    } else if (isvisible && !oncurrentdesktop) {
        hide();
    }
    Plasma::ToolTipContent plasmatooltip;
    if (!m_name.isEmpty()) {
        plasmatooltip.setMainText(QString::fromLatin1("<center>%1</center>").arg(m_name));
    }
    switch (m_tooltipmode) {
        case TasksApplet::ToolTipNone: {
            break;
        }
        case TasksApplet::ToolTipPreview: {
            plasmatooltip.setWindowToPreview(m_task);
            plasmatooltip.setClickable(true);
            break;
        }
        case TasksApplet::ToolTipHighlight: {
            plasmatooltip.setWindowToPreview(m_task);
            plasmatooltip.setHighlightWindows(true);
            plasmatooltip.setClickable(true);
            break;
        }
    }
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltip);
    if (m_demandsattention) {
        if (!m_attentiontimer) {
            m_attentiontimer = new QTimer(this);
            m_attentiontimer->setInterval(s_attentioninterval);
            connect(m_attentiontimer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
            m_attentiontimer->start();
        }
    } else if (m_attentiontimer) {
        m_attentiontimer->deleteLater();
        m_attentiontimer = nullptr;
    }
}

void TasksSvg::slotClicked(const Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        KTaskManager::activateRaiseOrIconify(m_task);
    }
}

void TasksSvg::slotWindowPreviewActivated(const WId window)
{
    // manually hide the tooltip first
    Plasma::ToolTipManager::self()->hide(this);
    KWindowSystem::activateWindow(window);
    KWindowSystem::raiseWindow(window);
}

void TasksSvg::slotUpdate()
{
    update();
}

void TasksSvg::slotUpdateSvg()
{
    delete m_framesvg;
    delete m_hoversvg;
    m_framesvg = new Plasma::FrameSvg(this);
    m_framesvg->setImagePath("widgets/tasks");
    setSvg(m_framesvg);
    m_hoversvg = new Plasma::FrameSvg(this);
    m_hoversvg->setImagePath("widgets/tasks");
    m_hoversvg->setElementPrefix(QString::fromLatin1("hover"));
}

void TasksSvg::slotTaskChanged(const WId task)
{
    if (task == m_task) {
        updateTaskAndToolTip();
        update();
    }
}

void TasksSvg::slotTimeout()
{
    m_demandsattention = !m_demandsattention;
    update();
}


TasksApplet::TasksApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_layout(nullptr),
    m_spacer(nullptr),
    m_tooltipmode(s_defaultooltipmode),
    m_tooltipmodebox(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_tasks");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_spacer = new QGraphicsWidget(this);
    m_spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_spacer->setMinimumSize(0, 0);
    m_spacer->setFlag(QGraphicsItem::ItemHasNoContents);
    m_layout->addItem(m_spacer);
}

void TasksApplet::init()
{
    Plasma::Applet::init();

    KConfigGroup configgroup = config();
    m_tooltipmode = static_cast<TasksApplet::ToolTipMode>(configgroup.readEntry("tooltipMode", static_cast<int>(s_defaultooltipmode)));

    foreach (const WId task, KTaskManager::self()->tasks()) {
        slotTaskAdded(task);
    }

    connect(
        KTaskManager::self(), SIGNAL(taskAdded(WId)),
        this, SLOT(slotTaskAdded(WId))
    );
    connect(
        KTaskManager::self(), SIGNAL(taskRemoved(WId)),
        this, SLOT(slotTaskRemoved(WId))
    );
    connect(
        KWindowSystem::self(), SIGNAL(currentDesktopChanged(int)),
        this, SLOT(slotCurrentDesktopChanged(int))
    );
}


void TasksApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QGridLayout* widgetlayout = new QGridLayout(widget);
    QLabel* tooltipmodelabel = new QLabel(widget);
    tooltipmodelabel->setText(i18n("Tooltip:"));
    widgetlayout->addWidget(tooltipmodelabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_tooltipmodebox = new QComboBox(widget);
    m_tooltipmodebox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_tooltipmodebox->addItem(i18n("Task name"), static_cast<int>(TasksApplet::ToolTipPreview));
    m_tooltipmodebox->addItem(i18n("Task name and preview"), static_cast<int>(TasksApplet::ToolTipPreview));
    m_tooltipmodebox->addItem(i18n("Task name, preview and highlight window"), static_cast<int>(TasksApplet::ToolTipHighlight));
    m_tooltipmodebox->setCurrentIndex(static_cast<int>(m_tooltipmode));
    widgetlayout->addWidget(m_tooltipmodebox, 0, 1);
    QSpacerItem* spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addItem(spacer, 1, 0, 1, 2);
    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("General"), "preferences-system-windows");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_tooltipmodebox, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
}

void TasksApplet::constraintsEvent(Plasma::Constraints constraints)
{
    // update once
    bool update = false;
    if (constraints & Plasma::SizeConstraint) {
        update = true;
    }
    if (constraints & Plasma::FormFactorConstraint) {
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal: {
                m_layout->setOrientation(Qt::Horizontal);
                break;
            }
            case Plasma::FormFactor::Vertical: {
                m_layout->setOrientation(Qt::Vertical);
                break;
            }
            default: {
                m_layout->setOrientation(Qt::Horizontal);
                break;
            }
        }
        update = true;
    }
    if (update) {
        QMutexLocker locker(&m_mutex);
        const QSizeF taskssize = kTaskSize(size(), formFactor());
        foreach (TasksSvg* taskssvg, m_taskssvgs) {
            taskssvg->setPreferredSize(taskssize);
            taskssvg->updateGeometry();
        }
    }
}

void TasksApplet::slotTaskAdded(const WId task)
{
    QMutexLocker locker(&m_mutex);
    m_layout->removeItem(m_spacer);
    TasksSvg* taskssvg = new TasksSvg(task, m_tooltipmode, this);
    m_layout->addItem(taskssvg);
    m_taskssvgs.append(taskssvg);
    taskssvg->setPreferredSize(kTaskSize(size(), formFactor()));
    taskssvg->updateGeometry();
    const KWindowInfo kwindowinfo = KWindowSystem::windowInfo(taskssvg->task(), NET::WMDesktop);
    if (kwindowinfo.isOnDesktop(KWindowSystem::currentDesktop())) {
        taskssvg->animatedShow();
    } else {
        taskssvg->hide();
    }
    // TODO: once the taskbar is nearly full show arrow for items that shall be available via
    // menu-like widget
    m_layout->addItem(m_spacer);
}

void TasksApplet::slotTaskRemoved(const WId task)
{
    QMutexLocker locker(&m_mutex);
    QMutableListIterator<TasksSvg*> iter(m_taskssvgs);
    while (iter.hasNext()) {
        TasksSvg* taskssvg = iter.next();
        if (taskssvg->task() == task) {
            iter.remove();
            taskssvg->animatedRemove();
            break;
        }
    }
}

void TasksApplet::slotCurrentDesktopChanged(const int desktop)
{
    QMutexLocker locker(&m_mutex);
    foreach (TasksSvg* taskssvg, m_taskssvgs) {
        const KWindowInfo kwindowinfo = KWindowSystem::windowInfo(taskssvg->task(), NET::WMDesktop);
        if (!kwindowinfo.isOnDesktop(desktop)) {
            taskssvg->hide();
        } else {
            taskssvg->animatedShow();
        }
    }
}

void TasksApplet::slotConfigAccepted()
{
    Q_ASSERT(m_tooltipmodebox != nullptr);
    const int tooltipmodeindex = m_tooltipmodebox->currentIndex();
    m_tooltipmode = static_cast<TasksApplet::ToolTipMode>(tooltipmodeindex);
    KConfigGroup configgroup = config();
    configgroup.writeEntry("tooltipMode", tooltipmodeindex);
    QMutexLocker locker(&m_mutex);
    foreach (TasksSvg* taskssvg, m_taskssvgs) {
        taskssvg->setup(m_tooltipmode);
    }
    emit configNeedsSaving();
}

#include "moc_tasks.cpp"
#include "tasks.moc"
