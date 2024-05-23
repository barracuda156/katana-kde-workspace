/*
    This file is part of the KDE project
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#include "launcher.h"
#include "kworkspace/kworkspace.h"
#include "kworkspace/kdisplaymanager.h"

#include <QMutex>
#include <QGraphicsLinearLayout>
#include <QGraphicsGridLayout>
#include <QHostInfo>
#include <QFile>
#include <QTimer>
#include <QDBusInterface>
#include <QGraphicsSceneMouseEvent>
#include <KUser>
#include <KIcon>
#include <KIconLoader>
#include <KStandardDirs>
#include <KSycoca>
#include <KToolInvocation>
#include <KRun>
#include <KBookmarkManager>
#include <KServiceGroup>
#include <KDirWatch>
#include <KDesktopFile>
#include <KRecentDocument>
#include <Solid/PowerManagement>
#include <Plasma/IconWidget>
#include <Plasma/Separator>
#include <Plasma/Label>
#include <Plasma/LineEdit>
#include <Plasma/TabBar>
#include <Plasma/Frame>
#include <Plasma/SvgWidget>
#include <Plasma/ToolButton>
#include <Plasma/ScrollWidget>
#include <Plasma/BusyWidget>
#include <Plasma/Animation>
#include <Plasma/ToolTipManager>
#include <KDebug>

// TODO: maybe favorites moving, animated layout updates

static const QString s_defaultpopupicon = QString::fromLatin1("start-here-kde");
static const QSizeF s_minimumsize = QSizeF(450, 350);
static const QString s_firsttimeaddress = QString::fromLatin1("_k_firsttime");
static const QStringList s_firsttimeservices = QStringList()
    << QString::fromLatin1("konsole")
    << QString::fromLatin1("dolphin")
    << QString::fromLatin1("systemsettings");
static const QString s_usericon = QString::fromLatin1("user-identity");
static const QString s_genericicon = QString::fromLatin1("applications-other");
static const QString s_favoriteicon = QString::fromLatin1("bookmarks");
static const QString s_recenticon = QString::fromLatin1("document-open-recent");
static const int s_searchdelay = 500; // ms
static const int s_polltimeout = 5000; // ms
// enough time for the animation to finish
static const int s_launcherdelay = 500; // ms
// NOTE: same as the one in:
// kdelibs/plasma/animations/animation.cpp
static const int s_animationduration = 250;
static const int s_iconmargin = 2;
static const int s_actionmargin = 1;

static QSizeF kIconSize()
{
    const int iconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    return QSizeF(iconsize, iconsize);
}

static QGraphicsWidget* kMakeSpacer(QGraphicsWidget *parent)
{
    QGraphicsWidget* widget = new QGraphicsWidget(parent);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    widget->setMinimumSize(0, 0);
    widget->setPreferredSize(0, 0);
    widget->setFlag(QGraphicsItem::ItemHasNoContents);
    return widget;
}

static Plasma::ScrollWidget* kMakeScrollWidget(QGraphicsWidget *parent)
{
    Plasma::ScrollWidget* scrollwidget = new Plasma::ScrollWidget(parent);
    scrollwidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return scrollwidget;
}

static void kRunService(const QString &entrypath, LauncherApplet* launcherapplet)
{
    Q_ASSERT(launcherapplet != nullptr);
    launcherapplet->resetState();
    KToolInvocation::self()->startServiceByStorageId(entrypath);
}

static void kRunUrl(const QString &urlpath, LauncherApplet* launcherapplet)
{
    Q_ASSERT(launcherapplet != nullptr);
    launcherapplet->resetState();
    KToolInvocation::self()->startServiceForUrl(urlpath);
}

static KIcon kGenericIcon(const QString &name)
{
    if (!name.isEmpty()) {
        return KIcon(name);
    }
    return KIcon(s_genericicon);
}

static KIcon kFavoriteIcon(const QString &name)
{
    if (!name.isEmpty()) {
        return KIcon(name);
    }
    return KIcon(s_favoriteicon);
}

static KIcon kRecentIcon(const QString &name)
{
    if (!name.isEmpty()) {
        return KIcon(name);
    }
    return KIcon(s_recenticon);
}

static QStringList kAllowedRunners(KConfigGroup configgroup)
{
    QStringList result;
    foreach (KPluginInfo plugin, Plasma::RunnerManager::listRunnerInfo()) {
        plugin.load(configgroup);
        if (plugin.isPluginEnabled()) {
            result.append(plugin.pluginName());
        }
    }
    return result;
}

static QMimeData* kMakeMimeData(const QString &url)
{
    Q_ASSERT(!url.isEmpty());
    QMimeData* mimedata = new QMimeData();
    mimedata->setUrls(QList<QUrl>() << url);
    return mimedata;
}

class LauncherWidget : public Plasma::SvgWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    LauncherWidget(QGraphicsWidget *parent);
    ~LauncherWidget();

    qreal hover() const;
    void setHover(qreal hover);

    void setup(const QSizeF &iconsize, const QIcon &icon, const QString &text, const QString &subtext);
    void disableActivation();

    QString data() const;
    void setData(const QString &data);

    void setMimeData(QMimeData *mimedata);

    void addAction(QAction *action);
    void removeAction(const int action);

Q_SIGNALS:
    void activated();

private Q_SLOTS:
    void slotClicked(const Qt::MouseButton button);
    void slotUpdateFonts();
    void slotTimeout();

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) final;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) final;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) final;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) final;
    bool sceneEventFilter(QGraphicsItem *item, QEvent *event) final;
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) final;

private:
    QPropertyAnimation* animateHover(QPropertyAnimation *animation, const bool fadeout);
    Plasma::Animation* animateButton(Plasma::Animation *animation, Plasma::ToolButton *toolbutton,
                                     const bool fadeout);
    bool handleMouseEvent(QGraphicsSceneMouseEvent *event);

    QGraphicsLinearLayout* m_layout;
    Plasma::FrameSvg* m_framesvg;
    qreal m_hover;
    QPropertyAnimation* m_hoveranimation;
    Plasma::IconWidget* m_iconwidget;
    QGraphicsLinearLayout* m_textlayout;
    Plasma::Label* m_textwidget;
    Plasma::Label* m_subtextwidget;
    QGraphicsGridLayout* m_actionslayout;
    Plasma::ToolButton* m_action1widget;
    Plasma::ToolButton* m_action2widget;
    Plasma::ToolButton* m_action3widget;
    Plasma::ToolButton* m_action4widget;
    Plasma::Animation* m_action1animation;
    Plasma::Animation* m_action2animation;
    Plasma::Animation* m_action3animation;
    Plasma::Animation* m_action4animation;
    QString m_data;
    int m_actioncounter;
    QPointer<QMimeData> m_mimedata;
    QTimer* m_undermousetimer;
};

LauncherWidget::LauncherWidget(QGraphicsWidget *parent)
    : Plasma::SvgWidget(parent),
    m_layout(nullptr),
    m_framesvg(nullptr),
    m_hover(0.0),
    m_hoveranimation(nullptr),
    m_iconwidget(nullptr),
    m_textlayout(nullptr),
    m_textwidget(nullptr),
    m_subtextwidget(nullptr),
    m_actionslayout(nullptr),
    m_action1widget(nullptr),
    m_action2widget(nullptr),
    m_action3widget(nullptr),
    m_action4widget(nullptr),
    m_action1animation(nullptr),
    m_action2animation(nullptr),
    m_action3animation(nullptr),
    m_action4animation(nullptr),
    m_actioncounter(0),
    m_mimedata(nullptr),
    m_undermousetimer(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    setLayout(m_layout);

    setAcceptHoverEvents(true);
    m_framesvg = new Plasma::FrameSvg(this);
    m_framesvg->setImagePath("widgets/viewitem");
    m_framesvg->setElementPrefix("hover");
    setSvg(m_framesvg);
    connect(
        this, SIGNAL(clicked(Qt::MouseButton)),
        this, SLOT(slotClicked(Qt::MouseButton))
    );

    m_iconwidget = new Plasma::IconWidget(this);
    m_iconwidget->setAcceptHoverEvents(false);
    m_iconwidget->setContentsMargins(s_iconmargin, s_iconmargin, s_iconmargin, s_iconmargin);
    m_layout->addItem(m_iconwidget);
    connect(
        m_iconwidget, SIGNAL(activated()),
        this, SIGNAL(activated())
    );

    m_textlayout = new QGraphicsLinearLayout(Qt::Vertical, m_layout);
    m_textlayout->setContentsMargins(6, 6, 6, 6);
    m_layout->addItem(m_textlayout);
    m_layout->setStretchFactor(m_textlayout, 100);
    m_textwidget = new Plasma::Label(this);
    m_textwidget->setWordWrap(false);
    m_textlayout->addItem(m_textwidget);
    m_subtextwidget = new Plasma::Label(this);
    m_subtextwidget->setWordWrap(false);
    m_textlayout->addItem(m_subtextwidget);

    m_actionslayout = new QGraphicsGridLayout(m_layout);
    m_actionslayout->setContentsMargins(s_actionmargin, s_actionmargin, s_actionmargin, s_actionmargin);
    m_layout->addItem(m_actionslayout);
    m_action1widget = new Plasma::ToolButton(this);
    m_action1widget->setVisible(false);
    m_actionslayout->addItem(m_action1widget, 0, 0);
    m_action2widget = new Plasma::ToolButton(this);
    m_action2widget->setVisible(false);
    m_actionslayout->addItem(m_action2widget, 1, 0);
    m_action3widget = new Plasma::ToolButton(this);
    m_action3widget->setVisible(false);
    m_actionslayout->addItem(m_action3widget, 0, 1);
    m_action4widget = new Plasma::ToolButton(this);
    m_action4widget->setVisible(false);
    m_actionslayout->addItem(m_action4widget, 1, 1);

    slotUpdateFonts();
    connect(
        KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()),
        this, SLOT(slotUpdateFonts())
    );

    m_undermousetimer = new QTimer(this);
    // this could be done on scroll event (Plasma::ScrollWidget::scrollStateChanged()) but that
    // means locking for thread-safety
    m_undermousetimer->setInterval(s_animationduration * 4);
    connect(
        m_undermousetimer, SIGNAL(timeout()),
        this, SLOT(slotTimeout())
    );
}

LauncherWidget::~LauncherWidget()
{
    if (m_mimedata) {
        m_mimedata->deleteLater();
    }
}

qreal LauncherWidget::hover() const
{
    return m_hover;
}

void LauncherWidget::setHover(qreal hover)
{
    m_hover = hover;
    update();
}

void LauncherWidget::setup(const QSizeF &iconsize, const QIcon &icon, const QString &text, const QString &subtext)
{
    m_iconwidget->setMinimumIconSize(iconsize);
    m_iconwidget->setIcon(icon);
    m_textwidget->setText(text);
    m_subtextwidget->setText(subtext);
    m_subtextwidget->setVisible(!subtext.isEmpty());
}

void LauncherWidget::disableActivation()
{
    setAcceptedMouseButtons(Qt::NoButton);
    m_iconwidget->setAcceptedMouseButtons(Qt::NoButton);
}

QString LauncherWidget::data() const
{
    return m_data;
}

void LauncherWidget::setData(const QString &data)
{
    m_data = data;
}

void LauncherWidget::setMimeData(QMimeData *mimedata)
{
    if (mimedata) {
        m_mimedata = mimedata;
        setAcceptedMouseButtons(Qt::LeftButton);
        // Plasma::IconWidget is kinda special, event filter is a must for DnD
        m_iconwidget->setAcceptedMouseButtons(Qt::LeftButton);
        m_iconwidget->installSceneEventFilter(this);
    }
}

void LauncherWidget::addAction(QAction *action)
{
    // 4 actions are packed into a small area, the text is getting in the way. however if there is
    // no tooltip use the text as tooltip
    if (action->toolTip().isEmpty() && !action->text().isEmpty()) {
        action->setToolTip(action->text());
    }
    action->setText(QString());
    if (action->icon().isNull()) {
        kWarning() << "action does not have icon" << action;
        return;
    }
    switch (m_actioncounter) {
        case 0: {
            m_action1widget->setVisible(true);
            m_action1widget->setOpacity(0.0);
            m_action1widget->setAction(action);
            break;
        }
        case 1: {
            m_action2widget->setVisible(true);
            m_action2widget->setOpacity(0.0);
            m_action2widget->setAction(action);
            break;
        }
        case 2: {
            m_action3widget->setVisible(true);
            m_action3widget->setOpacity(0.0);
            m_action3widget->setAction(action);
            break;
        }
        case 3: {
            m_action4widget->setVisible(true);
            m_action4widget->setOpacity(0.0);
            m_action4widget->setAction(action);
            break;
        }
        default: {
            Q_ASSERT(false);
            kWarning() << "invalid action counter" << m_actioncounter;
            return;
        }
    }
    m_actioncounter++;
}

void LauncherWidget::removeAction(const int actionnumber)
{
    QAction* action = nullptr;
    switch (actionnumber) {
        case 0: {
            m_action1widget->setVisible(false);
            action = m_action1widget->action();
            break;
        }
        case 1: {
            m_action2widget->setVisible(false);
            action = m_action2widget->action();
            break;
        }
        case 2: {
            m_action3widget->setVisible(false);
            action = m_action3widget->action();
            break;
        }
        case 3: {
            m_action4widget->setVisible(false);
            action = m_action4widget->action();
            break;
        }
        default: {
            Q_ASSERT(false);
            kWarning() << "invalid action number" << action;
            return;
        }
    }
    if (action) {
        action->deleteLater();
        if (m_actioncounter > 0) {
            m_actioncounter--;
        }
    }
}

void LauncherWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (m_hover > 0.0) {
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF brect = boundingRect();
        const qreal oldopacity = painter->opacity();
        m_framesvg->resizeFrame(brect.size());
        painter->setOpacity(m_hover);
        m_framesvg->paintFrame(painter, brect);
        painter->setOpacity(oldopacity);
    }
}

void LauncherWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    m_hoveranimation = animateHover(m_hoveranimation, false);
    m_action1animation = animateButton(m_action1animation, m_action1widget, false);
    m_action2animation = animateButton(m_action2animation, m_action2widget, false);
    m_action3animation = animateButton(m_action3animation, m_action3widget, false);
    m_action4animation = animateButton(m_action4animation, m_action4widget, false);
}

void LauncherWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    m_hoveranimation = animateHover(m_hoveranimation, true);
    m_action1animation = animateButton(m_action1animation, m_action1widget, true);
    m_action2animation = animateButton(m_action2animation, m_action2widget, true);
    m_action3animation = animateButton(m_action3animation, m_action3widget, true);
    m_action4animation = animateButton(m_action4animation, m_action4widget, true);
}

void LauncherWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_mimedata && handleMouseEvent(event)) {
        return;
    }
    Plasma::SvgWidget::mouseMoveEvent(event);
}

bool LauncherWidget::sceneEventFilter(QGraphicsItem *item, QEvent *event)
{
    if (item == m_iconwidget && event->type() == QEvent::GraphicsSceneMouseMove) {
        QGraphicsSceneMouseEvent* mouseevent = static_cast<QGraphicsSceneMouseEvent*>(event);
        if (m_mimedata && handleMouseEvent(mouseevent)) {
            return true;
        }
    }
    return false;
}

QVariant LauncherWidget::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value)
{
    const QVariant result = Plasma::SvgWidget::itemChange(change, value);
    switch (change) {
        case QGraphicsItem::ItemVisibleHasChanged: {
            if (value.toBool()) {
                m_undermousetimer->start();
            } else {
                m_undermousetimer->stop();
            }
            break;
        }
        default: {
            break;
        }
    }
    return result;
}

Plasma::Animation* LauncherWidget::animateButton(Plasma::Animation *animation,
                                                 Plasma::ToolButton *toolbutton,
                                                 const bool fadeout)
{
    if (!toolbutton->isVisible()) {
        return nullptr;
    }
    if (animation) {
        animation->stop();
    } else {
        animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
        Q_ASSERT(animation != nullptr);
        animation->setTargetWidget(toolbutton);
    }
    animation->setProperty("startOpacity", toolbutton->opacity());
    animation->setProperty("targetOpacity", fadeout ? 0.0 : 1.0);
    animation->start(QAbstractAnimation::KeepWhenStopped);
    return animation;
}

QPropertyAnimation* LauncherWidget::animateHover(QPropertyAnimation *animation, const bool fadeout)
{
    if (animation) {
        animation->stop();
    } else {
        animation = new QPropertyAnimation(this, "hover", this);
        animation->setDuration(s_animationduration);
    }
    animation->setStartValue(m_hover);
    animation->setEndValue(fadeout ? 0.0 : 1.0);
    animation->start(QAbstractAnimation::KeepWhenStopped);
    return animation;
}

bool LauncherWidget::handleMouseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton &&
        (event->pos() - event->buttonDownPos(Qt::LeftButton)).manhattanLength() > KGlobalSettings::dndEventDelay())
    {
        // NOTE QDrag takes ownership of QMimeData, have to copy
        QMimeData* dragmimedata = new QMimeData();
        dragmimedata->setText(m_mimedata->text());
        dragmimedata->setUrls(m_mimedata->urls());
        foreach (const QString &mimeformat, m_mimedata->formats()) {
            dragmimedata->setData(mimeformat, m_mimedata->data(mimeformat));
        }
        event->accept();
        QDrag* drag = new QDrag(event->widget());
        drag->setMimeData(dragmimedata);
        const QPixmap iconpixmap = m_iconwidget->icon().pixmap(kIconSize().toSize());
        if (!iconpixmap.isNull()) {
            drag->setPixmap(iconpixmap);
            // same as the one in KColorMimeData and KPixmapWidget
            drag->setHotSpot(QPoint(-5,-7));
        }
        drag->start();
        return true;
    }
    return false;
}

void LauncherWidget::slotClicked(const Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        emit activated();
    }
}

void LauncherWidget::slotUpdateFonts()
{
    QFont textfont = KGlobalSettings::generalFont();
    textfont.setBold(true);
    m_textwidget->setFont(textfont);
    QFont subtextfont = KGlobalSettings::smallestReadableFont();
    subtextfont.setItalic(true);
    m_subtextwidget->setFont(subtextfont);
}

void LauncherWidget::slotTimeout()
{
    m_hoveranimation = animateHover(m_hoveranimation, !isUnderMouse());
}


class LauncherSearch : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherSearch(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

    void prepare();
    void query(const QString &text);

Q_SIGNALS:
    void queryFinished();

private Q_SLOTS:
    void slotUpdateLayout();
    void slotTriggered();
    void slotActivated();
    void slotDelayedRun();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QList<LauncherWidget*> m_launcherwidgets;
    Plasma::Label* m_label;
    Plasma::BusyWidget* m_busywidget;
    Plasma::RunnerManager* m_runnermanager;
    Plasma::QueryMatch m_match;
};

LauncherSearch::LauncherSearch(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_layout(nullptr),
    m_label(nullptr),
    m_busywidget(nullptr),
    m_runnermanager(launcherapplet->runnerManager()),
    m_match(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    m_label = new Plasma::Label(this);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_label->setAlignment(Qt::AlignCenter);
    m_layout->addItem(m_label);

    m_busywidget = new Plasma::BusyWidget(this);
    m_busywidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addItem(m_busywidget);

    connect(
        m_runnermanager, SIGNAL(queryFinished()),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherSearch::prepare()
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherWidget* launcherwidget, m_launcherwidgets) {
        m_layout->removeItem(launcherwidget);
    }
    qDeleteAll(m_launcherwidgets);
    m_launcherwidgets.clear();

    m_label->setText(i18n("Searching.."));
    m_label->setVisible(true);
    m_busywidget->setVisible(true);
    m_busywidget->setRunning(true);
    adjustSize();

    m_runnermanager->reset();
}

void LauncherSearch::query(const QString &text)
{
    m_runnermanager->launchQuery(text);
}

void LauncherSearch::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    const QList<Plasma::QueryMatch> matches = m_runnermanager->matches();
    m_busywidget->setRunning(false);
    m_busywidget->setVisible(false);
    if (matches.isEmpty()) {
        m_label->setText(i18n("No matches found"));
        m_label->setVisible(true);
    } else {
        m_label->setVisible(false);
    }
    adjustSize();

    const QSizeF iconsize = kIconSize();
    foreach (const Plasma::QueryMatch &match, matches) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, match.icon(), match.text(), match.subtext()
        );
        launcherwidget->setData(match.id());
        if (!match.isEnabled()) {
            launcherwidget->disableActivation();
        }
        int counter = 0;
        const QList<QAction*> matchactions = m_runnermanager->actionsForMatch(match);
        foreach (QAction* action, matchactions) {
            action->setProperty("_k_id", match.id());
            launcherwidget->addAction(action);
            connect(
                action, SIGNAL(triggered()),
                this, SLOT(slotTriggered())
            );
            counter++;
            if (counter >= 4) {
                kWarning() << "the limit of LauncherWidget actions has been reached" << matchactions.size();
                break;
            }
        }
        launcherwidget->setMimeData(m_runnermanager->mimeDataForMatch(match));
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    emit queryFinished();
}

void LauncherSearch::slotTriggered()
{
    QAction* matchaction = qobject_cast<QAction*>(sender());
    const QString matchid = matchaction->property("_k_id").toString();
    m_launcherapplet->resetState();
    foreach (const Plasma::QueryMatch &match, m_runnermanager->matches()) {
        if (match.id() == matchid) {
            m_match = match;
            m_match.setSelectedAction(matchaction);
            QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedRun()));
            return;
        }
    }
    kWarning() << "could not find match for" << matchid;
}

void LauncherSearch::slotActivated()
{
    LauncherWidget* launcherwidget = qobject_cast<LauncherWidget*>(sender());
    m_launcherapplet->resetState();
    const QString matchid = launcherwidget->data();
    foreach (const Plasma::QueryMatch &match, m_runnermanager->matches()) {
        if (match.id() == matchid) {
            m_match = match;
            QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedRun()));
            return;
        }
    }
    kWarning() << "could not find match for" << matchid;
}

void LauncherSearch::slotDelayedRun()
{
    m_match.run();
}


class LauncherSearchHelp : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherSearchHelp(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

private:
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    Plasma::RunnerManager* m_runnermanager;
};

LauncherSearchHelp::LauncherSearchHelp(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_layout(nullptr),
    m_runnermanager(launcherapplet->runnerManager())
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    // TODO: fill with text about the runners
}


class LauncherFavorites : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherFavorites(QGraphicsWidget *parent, LauncherApplet* launcherapplet);

public Q_SLOTS:
    void slotUpdateLayout();

private Q_SLOTS:
    void slotActivated();
    void slotTriggered();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    KBookmarkManager* m_bookmarkmanager;
    QGraphicsLinearLayout* m_layout;
    QList<LauncherWidget*> m_launcherwidgets;
};

LauncherFavorites::LauncherFavorites(QGraphicsWidget *parent, LauncherApplet* launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_bookmarkmanager(launcherapplet->bookmarkManager()),
    m_layout(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    connect(
        m_bookmarkmanager, SIGNAL(changed(QString,QString)),
        this, SLOT(slotUpdateLayout())
    );
    connect(
        m_bookmarkmanager, SIGNAL(bookmarksChanged(QString)),
        this, SLOT(slotUpdateLayout())
    );
    connect(
        KSycoca::self(), SIGNAL(databaseChanged(QStringList)),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherFavorites::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherWidget* launcherwidget, m_launcherwidgets) {
        m_layout->removeItem(launcherwidget);
    }
    qDeleteAll(m_launcherwidgets);
    m_launcherwidgets.clear();

    adjustSize();

    bool isfirsttime = true;
    KBookmarkGroup bookmarkgroup = m_bookmarkmanager->root();
    // first time gets a special treatment
    KBookmark bookmark = bookmarkgroup.first();
    while (!bookmark.isNull()) {
        if (bookmark.url().url() == s_firsttimeaddress) {
            isfirsttime = false;
            break;
        }
        bookmark = bookmarkgroup.next(bookmark);
    }
    if (isfirsttime) {
        bookmark = bookmarkgroup.createNewSeparator();
        bookmark.setUrl(s_firsttimeaddress);
        bookmark.setDescription("internal bookmark");
        foreach (const QString &name, s_firsttimeservices) {
            KService::Ptr service = KService::serviceByDesktopName(name);
            if (!service.isNull()) {
                bookmarkgroup.addBookmark(service->desktopEntryName(), KUrl(service->entryPath()), service->icon());
            } else {
                kWarning() << "invalid first-time serivce" << name;
            }
        }
        disconnect(
            m_bookmarkmanager, SIGNAL(changed(QString,QString)),
            this, SLOT(slotUpdateLayout())
        );
        disconnect(
            m_bookmarkmanager, SIGNAL(bookmarksChanged(QString)),
            this, SLOT(slotUpdateLayout())
        );
        m_bookmarkmanager->emitChanged(bookmarkgroup);
    }

    const QSizeF iconsize = kIconSize();
    bookmark = bookmarkgroup.first();
    while (!bookmark.isNull()) {
        if (bookmark.isSeparator()) {
            bookmark = bookmarkgroup.next(bookmark);
            continue;
        }
        const QString bookmarkentrypath = bookmark.url().url();
        KService::Ptr service = KService::serviceByDesktopPath(bookmarkentrypath);
        if (service.isNull()) {
            service = KService::serviceByDesktopName(bookmark.text());
        }
        if (service.isNull()) {
            kWarning() << "could not find service for" << bookmarkentrypath;
            bookmark = bookmarkgroup.next(bookmark);
            continue;
        }
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, kFavoriteIcon(service->icon()), service->name(), service->genericName()
        );
        const QString entrypath = service->entryPath();
        launcherwidget->setData(entrypath);
        QAction* favoriteaction = new QAction(launcherwidget);
        favoriteaction->setIcon(KIcon("edit-delete"));
        favoriteaction->setToolTip(i18n("Remove"));
        favoriteaction->setProperty("_k_id", bookmarkentrypath);
        connect(
            favoriteaction, SIGNAL(triggered()),
            this, SLOT(slotTriggered()),
            Qt::QueuedConnection
        );
        launcherwidget->addAction(favoriteaction);
        launcherwidget->setMimeData(kMakeMimeData(entrypath));
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
        bookmark = bookmarkgroup.next(bookmark);
    }

    if (isfirsttime) {
        locker.unlock();
        connect(
            m_bookmarkmanager, SIGNAL(changed(QString,QString)),
            this, SLOT(slotUpdateLayout())
        );
        connect(
            m_bookmarkmanager, SIGNAL(bookmarksChanged(QString)),
            this, SLOT(slotUpdateLayout())
        );
    }
}

void LauncherFavorites::slotActivated()
{
    LauncherWidget* launcherwidget = qobject_cast<LauncherWidget*>(sender());
    kRunService(launcherwidget->data(), m_launcherapplet);
}

void LauncherFavorites::slotTriggered()
{
    QAction* favoriteaction = qobject_cast<QAction*>(sender());
    const QString favoriteid = favoriteaction->property("_k_id").toString();
    KBookmarkGroup bookmarkgroup = m_bookmarkmanager->root();
    KBookmark bookmark = bookmarkgroup.first();
    while (!bookmark.isNull()) {
        if (bookmark.url().url() == favoriteid) {
            bookmarkgroup.deleteBookmark(bookmark);
            m_bookmarkmanager->emitChanged(bookmarkgroup);
            return;
        }
        bookmark = bookmarkgroup.next(bookmark);
    }
    kWarning() << "invalid bookmark" << favoriteid;
}


class LauncherNavigatorStruct
{
public:
    LauncherNavigatorStruct(Plasma::SvgWidget *_svgwidget, Plasma::ToolButton *_toolbutton)
        : svgwidget(_svgwidget), toolbutton(_toolbutton)
    {
    }

    Plasma::SvgWidget* svgwidget;
    Plasma::ToolButton* toolbutton;
};

class LauncherNavigator : public Plasma::Frame
{
    Q_OBJECT
public:
    LauncherNavigator(QGraphicsWidget *parent);

    void reset();
    void addNavigation(const QString &id, const QString &text);
    void finish();

Q_SIGNALS:
    void navigate(const QString &id);

private Q_SLOTS:
    void slotReleased();

private:
    QMutex m_mutex;
    QGraphicsLinearLayout* m_layout;
    Plasma::Svg* m_svg;
    QList<LauncherNavigatorStruct*> m_navigations;
    QGraphicsWidget* m_spacer;
};

LauncherNavigator::LauncherNavigator(QGraphicsWidget *parent)
    : Plasma::Frame(parent),
    m_layout(nullptr),
    m_svg(nullptr),
    m_spacer(nullptr)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    setFrameShadow(Plasma::Frame::Sunken);
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    setLayout(m_layout);

    m_svg = new Plasma::Svg(this);
    m_svg->setImagePath("widgets/arrows");
}

void LauncherNavigator::reset()
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherNavigatorStruct *navigation, m_navigations) {
        m_layout->removeItem(navigation->toolbutton);
        delete navigation->toolbutton;
        m_layout->removeItem(navigation->svgwidget);
        delete navigation->svgwidget;
    }
    qDeleteAll(m_navigations);
    m_navigations.clear();
    if (m_spacer) {
        m_layout->removeItem(m_spacer);
        delete m_spacer;
        m_spacer = nullptr;
    }
    m_spacer = kMakeSpacer(this);
    m_layout->addItem(m_spacer);
}

void LauncherNavigator::addNavigation(const QString &id, const QString &text)
{
    QMutexLocker locker(&m_mutex);
    Plasma::SvgWidget* svgwidget = new Plasma::SvgWidget(this);
    svgwidget->setElementID("right-arrow");
    svgwidget->setSvg(m_svg);
    m_layout->addItem(svgwidget);
    Plasma::ToolButton* toolbutton = new Plasma::ToolButton(this);
    toolbutton->setText(text);
    toolbutton->setProperty("_k_id", id);
    m_layout->addItem(toolbutton);
    connect(
        toolbutton, SIGNAL(released()),
        this, SLOT(slotReleased())
    );
    if (m_spacer) {
        m_layout->removeItem(m_spacer);
        delete m_spacer;
        m_spacer = nullptr;
    }
    m_spacer = kMakeSpacer(this);
    m_layout->addItem(m_spacer);
    m_navigations.append(new LauncherNavigatorStruct(svgwidget, toolbutton));
}

void LauncherNavigator::finish()
{
    QMutexLocker locker(&m_mutex);
    if (m_navigations.size() > 0) {
        // make the last navigator button match the tabbar
        LauncherNavigatorStruct *navigation =  m_navigations.last();
        navigation->toolbutton->setAutoRaise(false);
        navigation->toolbutton->setAcceptedMouseButtons(Qt::NoButton);
    }
}

void LauncherNavigator::slotReleased()
{
    Plasma::ToolButton* toolbutton = qobject_cast<Plasma::ToolButton*>(sender());
    emit navigate(toolbutton->property("_k_id").toString());
}

class LauncherServiceWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherServiceWidget(QGraphicsWidget *parent, LauncherApplet *launcherapplet, const QString &serviceid);

    QString serviceID() const;

Q_SIGNALS:
    void navigate(const QString &serviceid);

private Q_SLOTS:
    void slotGroupActivated();
    void slotAppActivated();
    void slotCheckBookmarks();
    void slotTriggered();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    KBookmarkManager* m_bookmarkmanager;
    QGraphicsLinearLayout* m_layout;
    QList<LauncherWidget*> m_launcherwidgets;
    QString m_serviceid;
};

LauncherServiceWidget::LauncherServiceWidget(QGraphicsWidget *parent, LauncherApplet *launcherapplet, const QString &serviceid)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_bookmarkmanager(launcherapplet->bookmarkManager()),
    m_layout(nullptr),
    m_serviceid(serviceid)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    KServiceGroup::Ptr servicegroup = KServiceGroup::group(serviceid);
    if (!servicegroup.isNull() && servicegroup->isValid()) {
        const QSizeF iconsize = kIconSize();
        foreach (const KServiceGroup::Ptr subgroup, servicegroup->groupEntries(KServiceGroup::NoOptions)) {
            if (subgroup->noDisplay() || subgroup->childCount() < 1) {
                continue;
            }
            LauncherWidget* launcherwidget = new LauncherWidget(this);
            launcherwidget->setup(
                iconsize, kGenericIcon(subgroup->icon()), subgroup->caption(), subgroup->comment()
            );
            launcherwidget->setData(subgroup->relPath());
            m_launcherwidgets.append(launcherwidget);
            m_layout->addItem(launcherwidget);
            connect(
                launcherwidget, SIGNAL(activated()),
                this, SLOT(slotGroupActivated())
            );
        }
        foreach (const KService::Ptr appservice, servicegroup->serviceEntries(KServiceGroup::NoOptions)) {
            if (appservice->noDisplay()) {
                continue;
            }
            const QString entrypath = appservice->entryPath();
            LauncherWidget* launcherwidget = new LauncherWidget(this);
            launcherwidget->setup(
                iconsize, kGenericIcon(appservice->icon()), appservice->name(), appservice->comment()
            );
            launcherwidget->setData(entrypath);
            launcherwidget->setMimeData(kMakeMimeData(entrypath));
            m_launcherwidgets.append(launcherwidget);
            m_layout->addItem(launcherwidget);
            connect(
                launcherwidget, SIGNAL(activated()),
                this, SLOT(slotAppActivated())
            );
        }
    } else {
        kWarning() << "invalid serivce group" << serviceid;
    }

    slotCheckBookmarks();
    connect(
        m_bookmarkmanager, SIGNAL(changed(QString,QString)),
        this, SLOT(slotCheckBookmarks())
    );
    connect(
        m_bookmarkmanager, SIGNAL(bookmarksChanged(QString)),
        this, SLOT(slotCheckBookmarks())
    );
}

QString LauncherServiceWidget::serviceID() const
{
    return m_serviceid;
}

void LauncherServiceWidget::slotGroupActivated()
{
    LauncherWidget* launcherwidget = qobject_cast<LauncherWidget*>(sender());
    emit navigate(launcherwidget->data());
}

void LauncherServiceWidget::slotAppActivated()
{
    LauncherWidget* launcherwidget = qobject_cast<LauncherWidget*>(sender());
    kRunService(launcherwidget->data(), m_launcherapplet);
}

void LauncherServiceWidget::slotCheckBookmarks()
{
    QStringList bookmarkurls;
    KBookmarkGroup bookmarkgroup = m_bookmarkmanager->root();
    KBookmark bookmark = bookmarkgroup.first();
    while (!bookmark.isNull()) {
        bookmarkurls.append(bookmark.url().url());
        bookmark = bookmarkgroup.next(bookmark);
    }

    QMutexLocker locker(&m_mutex);
    foreach (LauncherWidget* launcherwidget, m_launcherwidgets) {
        const QString launcherdata = launcherwidget->data();
        const bool isinfavorites = bookmarkurls.contains(launcherdata);
        // there is only one action, it is known which one is that
        launcherwidget->removeAction(0);
        if (!isinfavorites) {
            KService::Ptr service = KService::serviceByDesktopPath(launcherdata);
            if (!service.isNull() && service->isValid()) {
                QAction* favoriteaction = new QAction(launcherwidget);
                favoriteaction->setIcon(KIcon(s_favoriteicon));
                favoriteaction->setToolTip(i18n("Add to Favorites"));
                favoriteaction->setProperty("_k_id", launcherdata);
                launcherwidget->addAction(favoriteaction);
                connect(
                    favoriteaction, SIGNAL(triggered()),
                    this, SLOT(slotTriggered())
                );
            }
        }
    }
}

void LauncherServiceWidget::slotTriggered()
{
    QAction* favoriteaction = qobject_cast<QAction*>(sender());
    const QString favoriteid = favoriteaction->property("_k_id").toString();
    KService::Ptr service = KService::serviceByDesktopPath(favoriteid);
    if (!service.isNull()) {
        KBookmarkGroup bookmarkgroup = m_bookmarkmanager->root();
        bookmarkgroup.addBookmark(service->desktopEntryName(), KUrl(service->entryPath()), service->icon());
        m_bookmarkmanager->emitChanged(bookmarkgroup);
        favoriteaction->setVisible(false);
    } else {
        kWarning() << "invalid favorite serivce" << favoriteid;
    }
}

class LauncherApplications : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherApplications(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

public Q_SLOTS:
    void slotUpdateLayout();

private Q_SLOTS:
    void slotNavigate(const QString &id);

private:
    void addGroup(KServiceGroup::Ptr servicegroup);

    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    LauncherNavigator* m_launchernavigator;
    Plasma::ScrollWidget* m_scrollwidget;
    QList<LauncherServiceWidget*> m_servicewidgets;
};

LauncherApplications::LauncherApplications(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_layout(nullptr),
    m_launchernavigator(nullptr),
    m_scrollwidget(nullptr)
    
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    m_launchernavigator = new LauncherNavigator(this);
    m_layout->addItem(m_launchernavigator);

    m_scrollwidget = kMakeScrollWidget(this);
    m_layout->addItem(m_scrollwidget);

    connect(
        m_launchernavigator, SIGNAL(navigate(QString)),
        this, SLOT(slotNavigate(QString)),
        Qt::QueuedConnection
    );
    connect(
        KSycoca::self(), SIGNAL(databaseChanged(QStringList)),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherApplications::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherServiceWidget* servicewidget, m_servicewidgets) {
        m_layout->removeItem(servicewidget);
    }
    qDeleteAll(m_servicewidgets);
    m_servicewidgets.clear();

    adjustSize();

    QString rootid;
    KServiceGroup::Ptr rootgroup = KServiceGroup::root();
    if (!rootgroup.isNull() && rootgroup->isValid()) {
        rootid = rootgroup->relPath();
        addGroup(rootgroup);
    }

    locker.unlock();
    slotNavigate(rootid);
}

void LauncherApplications::addGroup(KServiceGroup::Ptr servicegroup)
{
    const QSizeF iconsize = kIconSize();
    const QString serviceid = servicegroup->relPath();
    if (!servicegroup->noDisplay() && servicegroup->childCount() > 0) {
        LauncherServiceWidget* servicewidget = new LauncherServiceWidget(m_scrollwidget, m_launcherapplet, serviceid);
        servicewidget->setVisible(false);
        m_servicewidgets.append(servicewidget);
        connect(
            servicewidget, SIGNAL(navigate(QString)),
            this, SLOT(slotNavigate(QString)),
            Qt::QueuedConnection
        );
    }
    foreach (const KServiceGroup::Ptr subgroup, servicegroup->groupEntries(KServiceGroup::NoOptions)) {
        if (subgroup->noDisplay() || subgroup->childCount() < 1) {
            continue;
        }
        addGroup(subgroup);
    }
    if (serviceid.isEmpty() || serviceid == QLatin1String("/")) {
        // hide the navigator when the root group is empty
        m_launchernavigator->setVisible(m_servicewidgets.size() > 0);
    }
}

void LauncherApplications::slotNavigate(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherServiceWidget* servicewidget, m_servicewidgets) {
        if (servicewidget->serviceID() == id) {
            m_scrollwidget->setWidget(servicewidget);
            servicewidget->setVisible(true);
        } else {
            servicewidget->setVisible(false);
        }
    }

    m_launchernavigator->reset();

    adjustSize();

    KServiceGroup::Ptr servicegroup = KServiceGroup::group(id);
    if (!servicegroup.isNull() && servicegroup->isValid()) {
        KServiceGroup::Ptr rootgroup = KServiceGroup::root();
        if (!rootgroup.isNull() && rootgroup->isValid()) {
            m_launchernavigator->addNavigation(rootgroup->relPath(), rootgroup->caption());
        } else {
            kWarning() << "root group is not valid";
        }

        QString groupid;
        foreach (const QString &subname, id.split(QLatin1Char('/'), QString::SkipEmptyParts)) {
            groupid.append(subname);
            groupid.append(QLatin1Char('/'));
            KServiceGroup::Ptr subgroup = KServiceGroup::group(groupid);
            if (subgroup.isNull() || !subgroup->isValid()) {
                kWarning() << "invalid subgroup" << subname;
                continue;
            }
            m_launchernavigator->addNavigation(groupid, subgroup->caption());
        }
    } else {
        kWarning() << "invalid group" << id;
    }
    m_launchernavigator->finish();
}


class LauncherRecent : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherRecent(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

public Q_SLOTS:
    void slotUpdateLayout();

private Q_SLOTS:
    void slotActivated();
    void slotTriggered();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QList<LauncherWidget*> m_launcherwidgets;
    KDirWatch* m_dirwatch;
};

LauncherRecent::LauncherRecent(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_layout(nullptr),
    m_dirwatch(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    m_dirwatch = new KDirWatch(this);
    m_dirwatch->addDir(KRecentDocument::recentDocumentDirectory());
    connect(
        m_dirwatch, SIGNAL(dirty(QString)),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherRecent::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherWidget* launcherwidget, m_launcherwidgets) {
        m_layout->removeItem(launcherwidget);
    }
    qDeleteAll(m_launcherwidgets);
    m_launcherwidgets.clear();

    adjustSize();

    const QSizeF iconsize = kIconSize();
    foreach (const QString &recent, KRecentDocument::recentDocuments()) {
        KDesktopFile recentfile(recent);
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, kRecentIcon(recentfile.readIcon()), recentfile.readName(), recentfile.readComment()
        );
        launcherwidget->setData(recentfile.readUrl());
        QAction* recenteaction = new QAction(launcherwidget);
        recenteaction->setIcon(KIcon("edit-delete"));
        recenteaction->setToolTip(i18n("Remove"));
        recenteaction->setProperty("_k_id", recent);
        connect(
            recenteaction, SIGNAL(triggered()),
            this, SLOT(slotTriggered()),
            Qt::QueuedConnection
        );
        launcherwidget->addAction(recenteaction);
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
}

void LauncherRecent::slotActivated()
{
    LauncherWidget* launcherwidget = qobject_cast<LauncherWidget*>(sender());
    kRunUrl(launcherwidget->data(), m_launcherapplet);
}

void LauncherRecent::slotTriggered()
{
    QAction* recenteaction = qobject_cast<QAction*>(sender());
    const QString recenteid = recenteaction->property("_k_id").toString();
    if (!QFile::remove(recenteid)) {
        kWarning() << "invalid recent" << recenteid;
    }
}


class LauncherLeave : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherLeave(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

public Q_SLOTS:
    void slotUpdateLayout();
    void slotTimeout();

private Q_SLOTS:
    void slotActivated();
    void slotDelayedSwitch();
    void slotDelayedSleep();
    void slotDelayedShutdown();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QList<LauncherWidget*> m_launcherwidgets;
    Plasma::Separator* m_systemseparator;
    QTimer* m_timer;
    bool m_canswitch;
    bool m_canreboot;
    bool m_canshutdown;
    KDisplayManager m_displaymanager;
    KWorkSpace::ShutdownType m_shutdowntype;
    Solid::PowerManagement::SleepState m_sleepstate;
};

LauncherLeave::LauncherLeave(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_systemseparator(nullptr),
    m_timer(nullptr),
    m_canswitch(false),
    m_canreboot(false),
    m_canshutdown(false),
    m_shutdowntype(KWorkSpace::ShutdownTypeNone),
    m_sleepstate(Solid::PowerManagement::SuspendState)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    m_timer = new QTimer(this);
    m_timer->setInterval(s_polltimeout);
    connect(
        m_timer, SIGNAL(timeout()),
        this, SLOT(slotTimeout())
    );
    m_timer->start();

    connect(
        Solid::PowerManagement::notifier(), SIGNAL(supportedSleepStatesChanged()),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherLeave::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (LauncherWidget* launcherwidget, m_launcherwidgets) {
        m_layout->removeItem(launcherwidget);
    }
    qDeleteAll(m_launcherwidgets);
    m_launcherwidgets.clear();

    if (m_systemseparator) {
        m_layout->removeItem(m_systemseparator);
        delete m_systemseparator;
        m_systemseparator = nullptr;
    }

    adjustSize();

    const QSizeF iconsize = kIconSize();
    bool hassessionicon = false;
    if (m_canswitch) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, KIcon("system-switch-user"), i18n("Switch user"), i18n("Start a parallel session as a different user")
        );
        launcherwidget->setData("switch");
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
        hassessionicon = true;
    }
    if (hassessionicon) {
        m_systemseparator = new Plasma::Separator(this);
        m_systemseparator->setOrientation(Qt::Horizontal);
        m_layout->addItem(m_systemseparator);
    }

    const QSet<Solid::PowerManagement::SleepState> sleepsates = Solid::PowerManagement::supportedSleepStates();
    if (sleepsates.contains(Solid::PowerManagement::SuspendState)) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, KIcon("system-suspend"), i18n("Sleep"), i18n("Suspend to RAM")
        );
        launcherwidget->setData("suspendram");
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    if (sleepsates.contains(Solid::PowerManagement::HibernateState)) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, KIcon("system-suspend-hibernate"), i18n("Hibernate"), i18n("Suspend to disk")
        );
        launcherwidget->setData("suspenddisk");
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    if (sleepsates.contains(Solid::PowerManagement::HybridSuspendState)) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, KIcon("system-suspend"), i18n("Hybrid Suspend"), i18n("Hybrid Suspend")
        );
        launcherwidget->setData("suspendhybrid");
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }

    if (m_canreboot) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, KIcon("system-reboot"), i18nc("Restart computer", "Restart"), i18n("Restart computer")
        );
        launcherwidget->setData("restart");
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    if (m_canshutdown) {
        LauncherWidget* launcherwidget = new LauncherWidget(this);
        launcherwidget->setup(
            iconsize, KIcon("system-shutdown"), i18n("Shut down"), i18n("Turn off computer")
        );
        launcherwidget->setData("shutdown");
        m_launcherwidgets.append(launcherwidget);
        m_layout->addItem(launcherwidget);
        connect(
            launcherwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    LauncherWidget* launcherwidget = new LauncherWidget(this);
    launcherwidget->setup(
        iconsize, KIcon("system-log-out"), i18n("Log out"), i18n("End session")
    );
    launcherwidget->setData("logout");
    m_launcherwidgets.append(launcherwidget);
    m_layout->addItem(launcherwidget);
    connect(
        launcherwidget, SIGNAL(activated()),
        this, SLOT(slotActivated())
    );
}

void LauncherLeave::slotActivated()
{
    LauncherWidget* launcherwidget = qobject_cast<LauncherWidget*>(sender());
    const QString launcherwidgetdata = launcherwidget->data();
    m_launcherapplet->resetState();
    if (launcherwidgetdata == QLatin1String("switch")) {
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedSwitch()));
    } else if (launcherwidgetdata == QLatin1String("suspendram")) {
        m_sleepstate = Solid::PowerManagement::SuspendState;
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedSleep()));
    } else if (launcherwidgetdata == QLatin1String("suspenddisk")) {
        m_sleepstate = Solid::PowerManagement::HibernateState;
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedSleep()));
    } else if (launcherwidgetdata == QLatin1String("suspendhybrid")) {
        m_sleepstate = Solid::PowerManagement::HybridSuspendState;
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedSleep()));
    } else if (launcherwidgetdata == QLatin1String("restart")) {
        m_shutdowntype = KWorkSpace::ShutdownTypeReboot;
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedShutdown()));
    } else if (launcherwidgetdata == QLatin1String("shutdown")) {
        m_shutdowntype = KWorkSpace::ShutdownTypeHalt;
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedShutdown()));
    } else if (launcherwidgetdata == QLatin1String("logout")) {
        m_shutdowntype = KWorkSpace::ShutdownTypeNone;
        QTimer::singleShot(s_launcherdelay, this, SLOT(slotDelayedShutdown()));
    } else {
        Q_ASSERT(false);
        kWarning() << "invalid url" << launcherwidgetdata;
    }
}

void LauncherLeave::slotTimeout()
{
    const bool oldcanswitch = m_canswitch;
    const bool oldcanreboot = m_canreboot;
    const bool oldcanshutdown = m_canshutdown;
    m_canswitch = m_displaymanager.isSwitchable();
    m_canreboot = KWorkSpace::canShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeReboot);
    m_canshutdown = KWorkSpace::canShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeHalt);
    if (oldcanswitch != m_canswitch || oldcanreboot != m_canreboot || oldcanshutdown != m_canshutdown) {
        slotUpdateLayout();
    }
}

void LauncherLeave::slotDelayedSwitch()
{
    m_displaymanager.newSession();
}

void LauncherLeave::slotDelayedSleep()
{
    Solid::PowerManagement::requestSleep(m_sleepstate);
}

void LauncherLeave::slotDelayedShutdown()
{
    KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmDefault, m_shutdowntype);
}


class LauncherAppletWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherAppletWidget(LauncherApplet* auncherapplet);
    ~LauncherAppletWidget();

    void resetSearch();

protected:
    void focusInEvent(QFocusEvent *event) final;

public Q_SLOTS:
    void slotUpdateLayout();

private Q_SLOTS:
    void slotSearch(const QString &text);
    void slotActivated();
    void slotUserTimeout();
    void slotSearchTimeout();
    void slotQueryFinished();

private:
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QGraphicsLinearLayout* m_toplayout;
    Plasma::IconWidget* m_iconwidget;
    Plasma::Label* m_label;
    Plasma::LineEdit* m_lineedit;
    Plasma::IconWidget* m_helpiconwidget;
    Plasma::TabBar* m_tabbar;
    Plasma::ScrollWidget* m_favoritesscrollwidget;
    LauncherFavorites* m_favoriteswidget;
    LauncherApplications* m_applicationswidget;
    Plasma::ScrollWidget* m_recentscrollwidget;
    LauncherRecent* m_recentwidget;
    Plasma::ScrollWidget* m_leavecrollwidget;
    LauncherLeave* m_leavewidget;
    Plasma::ScrollWidget* m_searchscrollwidget;
    LauncherSearch* m_searchwidget;
    Plasma::ScrollWidget* m_searchhelpscrollwidget;
    LauncherSearchHelp* m_searchhelpwidget;
    KUser* m_user;
    QTimer* m_usertimer;
    QTimer* m_searchtimer;
};

LauncherAppletWidget::LauncherAppletWidget(LauncherApplet* auncherapplet)
    : QGraphicsWidget(auncherapplet),
    m_launcherapplet(auncherapplet),
    m_layout(nullptr),
    m_toplayout(nullptr),
    m_iconwidget(nullptr),
    m_label(nullptr),
    m_lineedit(nullptr),
    m_helpiconwidget(nullptr),
    m_tabbar(nullptr),
    m_favoritesscrollwidget(nullptr),
    m_favoriteswidget(nullptr),
    m_applicationswidget(nullptr),
    m_recentscrollwidget(nullptr),
    m_recentwidget(nullptr),
    m_leavecrollwidget(nullptr),
    m_leavewidget(nullptr),
    m_searchscrollwidget(nullptr),
    m_searchwidget(nullptr),
    m_searchhelpscrollwidget(nullptr),
    m_searchhelpwidget(nullptr),
    m_user(nullptr),
    m_usertimer(nullptr),
    m_searchtimer(nullptr)
{
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_toplayout = new QGraphicsLinearLayout(Qt::Horizontal, m_layout);
    m_toplayout->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_layout->addItem(m_toplayout);

    m_user = new KUser(KUser::UseEffectiveUID);

    m_iconwidget = new Plasma::IconWidget(this);
    m_iconwidget->setAcceptHoverEvents(false);
    m_iconwidget->setFocusPolicy(Qt::NoFocus);
    m_iconwidget->setIcon(s_usericon);
    m_toplayout->addItem(m_iconwidget);

    m_label = new Plasma::Label(this);
    m_label->setWordWrap(false);
    m_label->nativeWidget()->setTextInteractionFlags(Qt::NoTextInteraction);
    m_label->setFocusPolicy(Qt::NoFocus);
    m_toplayout->addItem(m_label);
    m_toplayout->setAlignment(m_label, Qt::AlignCenter);

    m_lineedit = new Plasma::LineEdit(this);
    m_lineedit->setClickMessage(i18n("Search"));
    m_lineedit->setClearButtonShown(true);
    m_lineedit->setFocusPolicy(Qt::StrongFocus);
    m_toplayout->addItem(m_lineedit);
    m_toplayout->setAlignment(m_lineedit, Qt::AlignCenter);
    setFocusProxy(m_lineedit);

    m_helpiconwidget = new Plasma::IconWidget(this);
    m_helpiconwidget->setAcceptHoverEvents(false);
    m_helpiconwidget->setAcceptedMouseButtons(Qt::NoButton);
    m_helpiconwidget->setIcon("help-contextual");
    // TODO: enable once implemented
    m_helpiconwidget->setVisible(false);
    connect(
        m_helpiconwidget, SIGNAL(activated()),
        this, SLOT(slotActivated())
    );
    m_toplayout->addItem(m_helpiconwidget);

    m_tabbar = new Plasma::TabBar(this);
    m_favoritesscrollwidget = kMakeScrollWidget(m_tabbar);
    m_favoritesscrollwidget->setMinimumSize(s_minimumsize);
    m_favoriteswidget = new LauncherFavorites(m_favoritesscrollwidget, m_launcherapplet);
    m_favoritesscrollwidget->setWidget(m_favoriteswidget);
    m_tabbar->addTab(KIcon(s_favoriteicon), i18n("Favorites"), m_favoritesscrollwidget);
    m_applicationswidget = new LauncherApplications(m_tabbar, m_launcherapplet);
    m_applicationswidget->setMinimumSize(s_minimumsize);
    m_tabbar->addTab(KIcon(s_genericicon), i18n("Applications"), m_applicationswidget);
    m_recentscrollwidget = kMakeScrollWidget(m_tabbar);
    m_recentscrollwidget->setMinimumSize(s_minimumsize);
    m_recentwidget = new LauncherRecent(m_recentscrollwidget, m_launcherapplet);
    m_recentscrollwidget->setWidget(m_recentwidget);
    m_tabbar->addTab(KIcon(s_recenticon), i18n("Recently Used"), m_recentscrollwidget);
    m_leavecrollwidget = kMakeScrollWidget(m_tabbar);
    m_leavecrollwidget->setMinimumSize(s_minimumsize);
    m_leavewidget = new LauncherLeave(m_leavecrollwidget, m_launcherapplet);
    m_leavecrollwidget->setWidget(m_leavewidget);
    m_tabbar->addTab(KIcon("system-shutdown"), i18n("Leave"), m_leavecrollwidget);
    m_layout->addItem(m_tabbar);
    // squeeze the icon
    m_layout->setStretchFactor(m_tabbar, 100);

    m_searchscrollwidget = kMakeScrollWidget(this);
    m_searchscrollwidget->setMinimumSize(s_minimumsize);
    m_searchscrollwidget->setVisible(false);
    m_searchwidget = new LauncherSearch(m_searchscrollwidget, m_launcherapplet);
    connect(
        m_searchwidget, SIGNAL(queryFinished()),
        this, SLOT(slotQueryFinished())
    );
    m_searchscrollwidget->setWidget(m_searchwidget);
    m_layout->addItem(m_searchscrollwidget);
    connect(
        m_lineedit, SIGNAL(textChanged(QString)),
        this, SLOT(slotSearch(QString))
    );

    m_searchhelpscrollwidget = kMakeScrollWidget(this);
    m_searchhelpscrollwidget->setMinimumSize(s_minimumsize);
    m_searchhelpscrollwidget->setVisible(false);
    m_searchhelpwidget = new LauncherSearchHelp(m_searchhelpscrollwidget, m_launcherapplet);
    m_searchhelpscrollwidget->setWidget(m_searchhelpwidget);
    m_layout->addItem(m_searchhelpscrollwidget);

    m_usertimer = new QTimer(this);
    m_usertimer->setInterval(s_polltimeout);
    connect(
        m_usertimer, SIGNAL(timeout()),
        this, SLOT(slotUserTimeout())
    );
    m_usertimer->start();

    m_searchtimer = new QTimer(this);
    m_searchtimer->setSingleShot(true);
    m_searchtimer->setInterval(s_searchdelay);
    connect(
        m_searchtimer, SIGNAL(timeout()),
        this, SLOT(slotSearchTimeout())
    );

    setLayout(m_layout);
}

LauncherAppletWidget::~LauncherAppletWidget()
{
    delete m_user;
}

void LauncherAppletWidget::resetSearch()
{
    m_lineedit->setText(QString());
}

void LauncherAppletWidget::focusInEvent(QFocusEvent *event)
{
    QGraphicsWidget::focusInEvent(event);
    if (!m_lineedit->hasFocus()) {
        m_lineedit->setFocus(event->reason());
    }
}

void LauncherAppletWidget::slotUpdateLayout()
{
    slotUserTimeout();
    m_favoriteswidget->slotUpdateLayout();
    m_applicationswidget->slotUpdateLayout();
    m_recentwidget->slotUpdateLayout();
    m_leavewidget->slotTimeout();
    m_leavewidget->slotUpdateLayout();
}

void LauncherAppletWidget::slotSearch(const QString &text)
{
    const QString query = text.trimmed();
    if (query.isEmpty()) {
        m_searchtimer->stop();
        m_searchscrollwidget->setVisible(false);
        m_tabbar->setVisible(true);
        return;
    }
    if (!m_searchtimer->isActive()) {
        m_searchwidget->prepare();
    }
    m_searchtimer->start();
}

void LauncherAppletWidget::slotActivated()
{
    m_searchhelpscrollwidget->setVisible(!m_searchhelpscrollwidget->isVisible());
}

void LauncherAppletWidget::slotUserTimeout()
{
    const QString hostname = QHostInfo::localHostName();
    QString usericon = m_user->faceIconPath();
    if (usericon.isEmpty()) {
        // from theme
        m_iconwidget->setIcon(KIcon(s_usericon));
    } else {
        // full path with modification time detection
        m_iconwidget->setIcon(QIcon(usericon));
    }

    QString usertext;
    QString fullusername = m_user->property(KUser::FullName);
    if (fullusername.isEmpty()) {
        usertext = i18nc("login name, hostname", "User <b>%1</b> on <b>%2</b>", m_user->loginName(), hostname);
    } else {
        usertext = i18nc("full name, login name, hostname", "<b>%1 (%2)</b> on <b>%3</b>", fullusername, m_user->loginName(), hostname);
    }
    m_label->setText(usertext);
}

void LauncherAppletWidget::slotSearchTimeout()
{
    m_lineedit->setEnabled(false);
    m_searchwidget->query(m_lineedit->text());
    m_tabbar->setVisible(false);
    m_searchscrollwidget->setVisible(true);
}

void LauncherAppletWidget::slotQueryFinished()
{
    m_lineedit->setEnabled(true);
}


LauncherApplet::LauncherApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_launcherwidget(nullptr),
    m_bookmarkmanager(nullptr),
    m_runnermanager(nullptr),
    m_editmenuaction(nullptr),
    m_selector(nullptr),
    m_shareconfig(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_launcher");
    setPopupIcon(s_defaultpopupicon);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);

    const QString bookmarfile = KStandardDirs::locateLocal("data", "plasma/bookmarks.xml");
    m_bookmarkmanager = KBookmarkManager::managerForFile(bookmarfile, "launcher");
    // m_bookmarkmanager->slotEditBookmarks();
    m_runnermanager = new Plasma::RunnerManager(this);
}

void LauncherApplet::init()
{
    Plasma::PopupApplet::init();

    setGlobalShortcut(QKeySequence(Qt::ALT+Qt::Key_F2));
    m_shareconfig = KSharedConfig::openConfig(globalConfig().config()->name());
    m_configgroup = m_shareconfig->group("Plugins");

    m_launcherwidget = new LauncherAppletWidget(this);
    setFocusProxy(m_launcherwidget);
    m_runnermanager->setAllowedRunners(kAllowedRunners(m_configgroup));
    QTimer::singleShot(1000, m_launcherwidget, SLOT(slotUpdateLayout()));
    slotUpdateToolTip();
    connect(
        KGlobalSettings::self(), SIGNAL(localeChanged()),
        this, SLOT(slotUpdateToolTip())
    );
}

QGraphicsWidget* LauncherApplet::graphicsWidget()
{
    return m_launcherwidget;
}

QList<QAction*> LauncherApplet::contextualActions()
{
    QList<QAction*> result;
    const KService::Ptr service = KService::serviceByStorageId("kmenuedit");
    if (!service.isNull()) {
        if (!m_editmenuaction) {
            m_editmenuaction = new QAction(this);
            m_editmenuaction->setText(i18n("Edit Applications..."));
            connect(
                m_editmenuaction, SIGNAL(triggered()),
                this, SLOT(slotEditMenu())
            );
        }
        result.append(m_editmenuaction);
    }
    return result;
}

void LauncherApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget *widget = new QWidget();
    m_selector = new KPluginSelector(widget);
    m_selector->addPlugins(
        Plasma::RunnerManager::listRunnerInfo(),
        KPluginSelector::ReadConfigFile,
        i18n("Available Plugins"), QString(),
        m_shareconfig
    );
    connect(m_selector, SIGNAL(changed(bool)), parent, SLOT(settingsModified()));
    parent->addPage(m_selector, i18n("Runners"), "preferences-plugin");

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
}

void LauncherApplet::resetState()
{
    hidePopup();
    m_launcherwidget->resetSearch();
}

KBookmarkManager* LauncherApplet::bookmarkManager() const
{
    return m_bookmarkmanager;
}

Plasma::RunnerManager* LauncherApplet::runnerManager() const
{
    return m_runnermanager;
}

void LauncherApplet::slotEditMenu()
{
    hidePopup();
    KToolInvocation::self()->startServiceByStorageId("kmenuedit");
}

void LauncherApplet::slotConfigAccepted()
{
    Q_ASSERT(m_selector != nullptr);
    Q_ASSERT(m_shareconfig != nullptr);
    m_selector->save();
    m_configgroup.sync();
    m_shareconfig->sync();
    m_runnermanager->reloadConfiguration();
    m_runnermanager->setAllowedRunners(kAllowedRunners(m_configgroup));
    m_launcherwidget->resetSearch();
    emit configNeedsSaving();
}

void LauncherApplet::slotUpdateToolTip()
{
    Plasma::ToolTipContent plasmatooltipcontent = Plasma::ToolTipContent(
        i18n("Application Launcher"), i18n("Favorites, applications, recently used, session and search"),
        KIcon(s_defaultpopupicon)
    );
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltipcontent);
}

#include "launcher.moc"
#include "moc_launcher.cpp"
