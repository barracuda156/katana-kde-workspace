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
#include <QDBusInterface>
#include <KUser>
#include <kdbusconnectionpool.h>
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
#include <Plasma/ScrollWidget>
#include <Plasma/RunnerManager>
#include <KDebug>

// TODO: mime data for drag-n-drop

static const QString s_defaultpopupicon = QString::fromLatin1("start-here-kde");
static const QSizeF s_minimumsize = QSizeF(450, 350);
static const QString s_firsttimeaddress = QString::fromLatin1("_k_firsttime");
static const QStringList s_firsttimeservices = QStringList()
    << QString::fromLatin1("konsole")
    << QString::fromLatin1("dolphin")
    << QString::fromLatin1("systemsettings");
static const QString s_genericicon = QString::fromLatin1("applications-other");
static const QString s_favoriteicon = QString::fromLatin1("bookmarks");
static const QString s_recenticon = QString::fromLatin1("document-open-recent");
static const int s_searchdelay = 500; // ms
static const int s_leavetimeout = 5000; // ms

static QSizeF kIconSize()
{
    const int iconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    return QSizeF(iconsize, iconsize);
}

// TODO: custom widget to catch hover events from anywhere in the widget rect?
static Plasma::IconWidget* kMakeIconWidget(QGraphicsWidget *parent,
                                           const QSizeF &iconsize,
                                           const QString &text,
                                           const QString &infotext,
                                           const QString &icon,
                                           const QString &url)
{
    Plasma::IconWidget* iconwidget = new Plasma::IconWidget(parent);
    // TODO: actions are not visible when the orientation is horizontal..
    iconwidget->setOrientation(Qt::Horizontal);
    iconwidget->setMinimumIconSize(iconsize);
    iconwidget->setMaximumIconSize(iconsize);
    iconwidget->setText(text);
    iconwidget->setInfoText(infotext);
    iconwidget->setIcon(icon);
    iconwidget->setProperty("_k_url", url);
    return iconwidget;
}

static Plasma::ScrollWidget* kMakeScrollWidget(QGraphicsWidget *parent)
{
    Plasma::ScrollWidget* scrollwidget = new Plasma::ScrollWidget(parent);
    scrollwidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // TODO: this really does not work..
    scrollwidget->setOverShoot(false);
    return scrollwidget;
}

static void kRunService(const QString &entrypath, LauncherApplet* launcherapplet)
{
    Q_ASSERT(launcherapplet != nullptr);
    launcherapplet->resetState();

    KService::Ptr service = KService::serviceByDesktopPath(entrypath);
    Q_ASSERT(!service.isNull());
    if (!KRun::run(*service.data(), KUrl::List(), nullptr)) {
        kWarning() << "could not run" << entrypath;
    }
}

static void kRunUrl(const QString &urlpath, LauncherApplet* launcherapplet)
{
    Q_ASSERT(launcherapplet != nullptr);
    launcherapplet->resetState();

    (void)new KRun(KUrl(urlpath), nullptr);
}

static QString kGenericIcon(const QString &name)
{
    if (!name.isEmpty()) {
        return name;
    }
    return s_genericicon;
}

static QString kFavouriteIcon(const QString &name)
{
    if (!name.isEmpty()) {
        return name;
    }
    return s_favoriteicon;
}

static QString kRecentIcon(const QString &name)
{
    if (!name.isEmpty()) {
        return name;
    }
    return s_recenticon;
}

static void kLockScreen()
{
    QDBusInterface screensaver(
        "org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver",
        QDBusConnection::sessionBus()
    );
    screensaver.call("Lock");
}

static QStringList kAllowedRunners(KConfigGroup configgroup)
{
    QStringList result;
    foreach (KPluginInfo &plugin, Plasma::RunnerManager::listRunnerInfo()) {
        plugin.load(configgroup);
        if (plugin.isPluginEnabled()) {
            result.append(plugin.pluginName());
        }
    }
    // qDebug() << Q_FUNC_INFO << result << configgroup.name();
    return result;
}

class LauncherSearch : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherSearch(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

    void setAllowedRunners(const QStringList &runners);
    void prepare();
    void query(const QString &text);
    void finish();

private Q_SLOTS:
    void slotUpdateLayout(const QList<Plasma::QueryMatch> &matches);
    void slotActivated();
    void slotTriggered();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QList<Plasma::IconWidget*> m_iconwidgets;
    Plasma::Label* m_label;
    Plasma::RunnerManager* m_runnermanager;
};

LauncherSearch::LauncherSearch(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_layout(nullptr),
    m_label(nullptr),
    m_runnermanager(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    m_label = new Plasma::Label(this);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setText(i18n("No matches found"));
    m_layout->addItem(m_label);
}

void LauncherSearch::setAllowedRunners(const QStringList &runners)
{
    // NOTE: Plasma::RunnerManager basically never unloads, have to re-create it
    delete m_runnermanager;
    m_runnermanager = new Plasma::RunnerManager(this);
    connect(
        m_runnermanager, SIGNAL(matchesChanged(QList<Plasma::QueryMatch>)),
        this, SLOT(slotUpdateLayout(QList<Plasma::QueryMatch>))
    );
    m_runnermanager->setAllowedRunners(runners);
}

void LauncherSearch::prepare()
{
    // qDebug() << Q_FUNC_INFO;
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::IconWidget* iconwidget, m_iconwidgets) {
        m_layout->removeItem(iconwidget);
    }
    qDeleteAll(m_iconwidgets);
    m_iconwidgets.clear();

    m_label->setVisible(true);
    adjustSize();

    m_runnermanager->reset();
}

void LauncherSearch::query(const QString &text)
{
    // qDebug() << Q_FUNC_INFO << text;
    m_runnermanager->launchQuery(text);
}

void LauncherSearch::finish()
{
    // qDebug() << Q_FUNC_INFO;
    m_runnermanager->matchSessionComplete();
}

void LauncherSearch::slotUpdateLayout(const QList<Plasma::QueryMatch> &matches)
{
    // qDebug() << Q_FUNC_INFO;
    QMutexLocker locker(&m_mutex);
    m_label->setVisible(matches.isEmpty());
    adjustSize();

    const QSizeF iconsize = kIconSize();
    foreach (const Plasma::QueryMatch &match, matches) {
        // qDebug() << Q_FUNC_INFO << match.text() << match.subtext();
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, match.text(), match.subtext(), QString(), match.id()
        );
        iconwidget->setIcon(match.icon());
        if (match.type() == Plasma::QueryMatch::InformationalMatch) {
            iconwidget->setAcceptHoverEvents(false);
            iconwidget->setAcceptedMouseButtons(Qt::NoButton);
        }
        int counter = 1;
        if (match.hasConfigurationInterface()) {
            QAction* matchconfigaction = new QAction(iconwidget);
            matchconfigaction->setText(i18n("Configure"));
            matchconfigaction->setIcon(KIcon("preferences-system"));
            matchconfigaction->setProperty("_k_id", match.id());
            connect(
                matchconfigaction, SIGNAL(triggered()),
                this, SLOT(slotTriggered())
            );
            iconwidget->addIconAction(matchconfigaction);
            counter++;
            qDebug() << Q_FUNC_INFO << match.id();
        }
        foreach (QAction* action, m_runnermanager->actionsForMatch(match)) {
            iconwidget->addIconAction(action);
            counter++;
            if (counter >= 4) {
                // the limit of Plasma::IconWidget
                break;
            }
        }
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
}

void LauncherSearch::slotActivated()
{
    Plasma::IconWidget* iconwidget = qobject_cast<Plasma::IconWidget*>(sender());
    const QString iconwidgeturl = iconwidget->property("_k_url").toString();
    m_launcherapplet->resetState();
    m_runnermanager->run(iconwidgeturl);
}

void LauncherSearch::slotTriggered()
{
    QAction* matchconfigaction = qobject_cast<QAction*>(sender());
    const QString matchconfigid = matchconfigaction->property("_k_id").toString();
    Plasma::AbstractRunner* runner = m_runnermanager->runner(m_runnermanager->runnerName(matchconfigid));
    if (!runner) {
        kWarning() << "no runner for" << matchconfigid;
        return;
    }
    // TODO: implement
    qDebug() << Q_FUNC_INFO << matchconfigid;
}

class LauncherFavorites : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherFavorites(QGraphicsWidget *parent, LauncherApplet* launcherapplet);

private Q_SLOTS:
    void slotUpdateLayout();
    void slotActivated();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QList<Plasma::IconWidget*> m_iconwidgets;
    KBookmarkManager* m_bookmarkmanager;
};

// TODO: context menu to remove
LauncherFavorites::LauncherFavorites(QGraphicsWidget *parent, LauncherApplet* launcherapplet)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_layout(nullptr),
    m_bookmarkmanager(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    const QString bookmarfile = KStandardDirs::locateLocal("data", "plasma/bookmarks.xml");
    m_bookmarkmanager = KBookmarkManager::managerForFile(bookmarfile, "launcher");
    // m_bookmarkmanager->slotEditBookmarks();
    slotUpdateLayout();
    connect(
        m_bookmarkmanager, SIGNAL(changed(QString,QString)),
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
    foreach (Plasma::IconWidget* iconwidget, m_iconwidgets) {
        m_layout->removeItem(iconwidget);
    }
    qDeleteAll(m_iconwidgets);
    m_iconwidgets.clear();

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
        m_bookmarkmanager->emitChanged(bookmarkgroup);
    }

    const QSizeF iconsize = kIconSize();
    bookmark = bookmarkgroup.first();
    while (!bookmark.isNull()) {
        // qDebug() << Q_FUNC_INFO << bookmark.text() << bookmark.description() << bookmark.icon() << bookmark.url();
        if (bookmark.isSeparator()) {
            bookmark = bookmarkgroup.next(bookmark);
            continue;
        }
        const QString serviceentrypath = bookmark.url().url();
        KService::Ptr service = KService::serviceByDesktopPath(serviceentrypath);
        if (service.isNull()) {
            service = KService::serviceByDesktopName(bookmark.text());
        }
        if (service.isNull()) {
            kWarning() << "could not find service for" << serviceentrypath;
            bookmark = bookmarkgroup.next(bookmark);
            continue;
        }
        // qDebug() << Q_FUNC_INFO << service->entryPath() << service->name() << service->comment();
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, service->name(), service->genericName(), kFavouriteIcon(service->icon()), service->entryPath()
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
        bookmark = bookmarkgroup.next(bookmark);
    }
}

void LauncherFavorites::slotActivated()
{
    Plasma::IconWidget* iconwidget = qobject_cast<Plasma::IconWidget*>(sender());
    kRunService(iconwidget->property("_k_url").toString(), m_launcherapplet);
}


class LauncherServiceWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherServiceWidget(QGraphicsWidget *parent, LauncherApplet *launcherapplet,
                          Plasma::TabBar *tabbar, const int tabindex);

    void appendGroup(Plasma::IconWidget* iconwidget, LauncherServiceWidget* servicewidget);
    void appendApp(Plasma::IconWidget* iconwidget);
    int serviceCount() const;

public Q_SLOTS:
    void slotGroupActivated();
    void slotAppActivated();
    void slotFavorite();
    
private:
    LauncherApplet* m_launcherapplet;
    Plasma::TabBar* m_tabbar;
    int m_tabindex;
    QGraphicsLinearLayout* m_layout;
    QList<Plasma::IconWidget*> m_iconwidgets;
};

LauncherServiceWidget::LauncherServiceWidget(QGraphicsWidget *parent, LauncherApplet *launcherapplet,
                                             Plasma::TabBar *tabbar, const int tabindex)
    : QGraphicsWidget(parent),
    m_launcherapplet(launcherapplet),
    m_tabbar(tabbar),
    m_tabindex(tabindex),
    m_layout(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);
}

int LauncherServiceWidget::serviceCount() const
{
    return m_iconwidgets.size();
}

void LauncherServiceWidget::appendGroup(Plasma::IconWidget* iconwidget, LauncherServiceWidget* servicewidget)
{
    m_iconwidgets.append(iconwidget);
    m_layout->addItem(iconwidget);
    connect(
        iconwidget, SIGNAL(activated()),
        servicewidget, SLOT(slotGroupActivated())
    );
}

void LauncherServiceWidget::appendApp(Plasma::IconWidget* iconwidget)
{
    // TODO: dual-action to remove
    QAction* favoriteiconaction = new QAction(iconwidget);
    favoriteiconaction->setText(i18n("Add to Favorites"));
    favoriteiconaction->setIcon(KIcon(s_favoriteicon));
    favoriteiconaction->setProperty("_k_url", iconwidget->property("_k_url"));
    favoriteiconaction->setVisible(true);
    favoriteiconaction->setEnabled(true);
    connect(
        favoriteiconaction, SIGNAL(triggered()),
        this, SLOT(slotFavorite())
    );
    iconwidget->addIconAction(favoriteiconaction);
    m_iconwidgets.append(iconwidget);
    m_layout->addItem(iconwidget);
    connect(
        iconwidget, SIGNAL(activated()),
        this, SLOT(slotAppActivated())
    );
}

void LauncherServiceWidget::slotGroupActivated()
{
    m_tabbar->setCurrentIndex(m_tabindex);
}

void LauncherServiceWidget::slotAppActivated()
{
    Plasma::IconWidget* iconwidget = qobject_cast<Plasma::IconWidget*>(sender());
    kRunService(iconwidget->property("_k_url").toString(), m_launcherapplet);
}

void LauncherServiceWidget::slotFavorite()
{
    QAction* favoriteiconaction = qobject_cast<QAction*>(sender());
    qDebug() << Q_FUNC_INFO << favoriteiconaction->property("_k_url").toString();
    // TODO: implement
}

class LauncherApplications : public Plasma::TabBar
{
    Q_OBJECT
public:
    LauncherApplications(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

private Q_SLOTS:
    void slotUpdateLayout();

private:
    void addGroup(LauncherServiceWidget *servicewidget, KServiceGroup::Ptr group);

    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
};

LauncherApplications::LauncherApplications(QGraphicsWidget *parent, LauncherApplet *launcherapplet)
    : Plasma::TabBar(parent),
    m_launcherapplet(launcherapplet)
{
    // TODO: navigation bar instead
    // setTabBarShown(false);

    slotUpdateLayout();

    connect(
        KSycoca::self(), SIGNAL(databaseChanged(QStringList)),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherApplications::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);

    int counter = count();
    while (counter) {
        counter--;
        // NOTE: deletes items too which in this case is the scroll and service widget
        removeTab(counter);
    }

    KServiceGroup::Ptr group = KServiceGroup::root();
    if (group && group->isValid()) {
        Plasma::ScrollWidget* rootscrollwidget = kMakeScrollWidget(this);
        LauncherServiceWidget* rootwidget = new LauncherServiceWidget(rootscrollwidget, m_launcherapplet, this, 0);
        rootscrollwidget->setWidget(rootwidget);
        addTab(KIcon(group->icon()), group->caption(), rootscrollwidget);

        addGroup(rootwidget, group);
    }
}

void LauncherApplications::addGroup(LauncherServiceWidget *servicewidget, KServiceGroup::Ptr group)
{
    const QString name = group->name();
    if (name.isEmpty() || group->noDisplay()) {
        kDebug() << "hidden or invalid group" << name;
        return;
    }

    const QSizeF iconsize = kIconSize();
    foreach (const KServiceGroup::Ptr subgroup, group->groupEntries(KServiceGroup::NoOptions)) {
        Plasma::ScrollWidget* scrollwidget = kMakeScrollWidget(this);
        LauncherServiceWidget* subgroupwidget = new LauncherServiceWidget(scrollwidget, m_launcherapplet, this, count());
        addGroup(subgroupwidget, subgroup);
        if (subgroupwidget->serviceCount() < 1) {
            delete subgroupwidget;
            delete scrollwidget;
        } else {
            const QString subgroupicon = kGenericIcon(subgroup->icon());
            scrollwidget->setWidget(subgroupwidget);
            Plasma::IconWidget* subgroupiconwidget = kMakeIconWidget(
                servicewidget,
                iconsize, subgroup->caption(), subgroup->comment(), subgroupicon, QString()
            );
            servicewidget->appendGroup(subgroupiconwidget, subgroupwidget);
            addTab(KIcon(subgroupicon), subgroup->caption(), scrollwidget);
        }
    }

    foreach (const KService::Ptr app, group->serviceEntries(KServiceGroup::NoOptions)) {
        if (app->noDisplay()) {
            kDebug() << "hidden entry" << app->name();
            continue;
        }
        Plasma::IconWidget* appiconwidget = kMakeIconWidget(
            servicewidget,
            iconsize, app->name(), app->comment(), kGenericIcon(app->icon()), app->entryPath()
        );
        servicewidget->appendApp(appiconwidget);
    }
}


class LauncherRecent : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherRecent(QGraphicsWidget *parent, LauncherApplet *launcherapplet);

private Q_SLOTS:
    void slotUpdateLayout();
    void slotActivated();

private:
    QMutex m_mutex;
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QList<Plasma::IconWidget*> m_iconwidgets;
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
    slotUpdateLayout();
    connect(
        m_dirwatch, SIGNAL(dirty(QString)),
        this, SLOT(slotUpdateLayout())
    );
}

void LauncherRecent::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::IconWidget* iconwidget, m_iconwidgets) {
        m_layout->removeItem(iconwidget);
    }
    qDeleteAll(m_iconwidgets);
    m_iconwidgets.clear();

    adjustSize();

    const QSizeF iconsize = kIconSize();
    foreach (const QString &recent, KRecentDocument::recentDocuments()) {
        KDesktopFile recentfile(recent);
        // qDebug() << Q_FUNC_INFO << recentfile.readName() << recentfile.readComment();
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, recentfile.readName(), recentfile.readComment(), kRecentIcon(recentfile.readIcon()), recentfile.readUrl()
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
}

void LauncherRecent::slotActivated()
{
    Plasma::IconWidget* iconwidget = qobject_cast<Plasma::IconWidget*>(sender());
    kRunUrl(iconwidget->property("_k_url").toString(), m_launcherapplet);
}


class LauncherLeave : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherLeave(QGraphicsWidget *parent);

private Q_SLOTS:
    void slotUpdateLayout();
    void slotActivated();
    void slotTimeout();

private:
    QMutex m_mutex;
    QGraphicsLinearLayout* m_layout;
    QList<Plasma::IconWidget*> m_iconwidgets;
    Plasma::Separator* m_systemseparator;
    QTimer* m_timer;
    bool m_canlock;
    bool m_canswitch;
    bool m_canreboot;
    bool m_canshutdown;
};

LauncherLeave::LauncherLeave(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    m_systemseparator(nullptr),
    m_timer(nullptr),
    m_canlock(false),
    m_canswitch(false),
    m_canreboot(false),
    m_canshutdown(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    setLayout(m_layout);

    m_timer = new QTimer(this);
    m_timer->setInterval(s_leavetimeout);

    slotTimeout();
    slotUpdateLayout();
    connect(
        Solid::PowerManagement::notifier(), SIGNAL(supportedSleepStatesChanged()),
        this, SLOT(slotUpdateLayout())
    );
    connect(
        m_timer, SIGNAL(timeout()),
        this, SLOT(slotTimeout())
    );
    m_timer->start();
}

void LauncherLeave::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::IconWidget* iconwidget, m_iconwidgets) {
        m_layout->removeItem(iconwidget);
    }
    qDeleteAll(m_iconwidgets);
    m_iconwidgets.clear();

    if (m_systemseparator) {
        m_layout->removeItem(m_systemseparator);
        delete m_systemseparator;
        m_systemseparator = nullptr;
    }

    adjustSize();

    const QSizeF iconsize = kIconSize();
    bool hassessionicon = false;
    if (m_canlock) {
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18n("Lock"), i18n("Lock screen"), "system-lock-screen", "lock"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
        hassessionicon = true;
    }
    if (m_canswitch) {
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18n("Switch user"), i18n("Start a parallel session as a different user"), "system-switch-user", "switch"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
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
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18n("Sleep"), i18n("Suspend to RAM"), "system-suspend", "suspendram"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    if (sleepsates.contains(Solid::PowerManagement::HibernateState)) {
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18n("Hibernate"), i18n("Suspend to disk"), "system-suspend-hibernate", "suspenddisk"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    if (sleepsates.contains(Solid::PowerManagement::HybridSuspendState)) {
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18n("Hybrid Suspend"), i18n("Hybrid Suspend"), "system-suspend", "suspendhybrid"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }

    if (m_canreboot) {
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18nc("Restart computer", "Restart"), i18n("Restart computer"), "system-reboot", "restart"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    if (m_canshutdown) {
        Plasma::IconWidget* iconwidget = kMakeIconWidget(
            this,
            iconsize, i18n("Shut down"), i18n("Turn off computer"), "system-shutdown", "shutdown"
        );
        m_iconwidgets.append(iconwidget);
        m_layout->addItem(iconwidget);
        connect(
            iconwidget, SIGNAL(activated()),
            this, SLOT(slotActivated())
        );
    }
    Plasma::IconWidget* iconwidget = kMakeIconWidget(
        this,
        iconsize, i18n("Log out"), i18n("End session"), "system-log-out", "logout"
    );
    m_iconwidgets.append(iconwidget);
    m_layout->addItem(iconwidget);
    connect(
        iconwidget, SIGNAL(activated()),
        this, SLOT(slotActivated())
    );
}

void LauncherLeave::slotActivated()
{
    Plasma::IconWidget* iconwidget = qobject_cast<Plasma::IconWidget*>(sender());
    const QString iconwidgeturl = iconwidget->property("_k_url").toString();
    if (iconwidgeturl == QLatin1String("lock")) {
        kLockScreen();
    } else if (iconwidgeturl == QLatin1String("switch")) {
        kLockScreen();
        KDisplayManager().newSession();
    } else if (iconwidgeturl == QLatin1String("suspendram")) {
        Solid::PowerManagement::requestSleep(Solid::PowerManagement::SuspendState);
    } else if (iconwidgeturl == QLatin1String("suspenddisk")) {
        Solid::PowerManagement::requestSleep(Solid::PowerManagement::HibernateState);
    } else if (iconwidgeturl == QLatin1String("suspendhybrid")) {
        Solid::PowerManagement::requestSleep(Solid::PowerManagement::HybridSuspendState);
    } else if (iconwidgeturl == QLatin1String("restart")) {
        KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeReboot);
    } else if (iconwidgeturl == QLatin1String("shutdown")) {
        KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeHalt);
    } else if (iconwidgeturl == QLatin1String("logout")) {
        KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeNone);
    } else {
        Q_ASSERT(false);
        kWarning() << "invalid url" << iconwidgeturl;
    }
}

void LauncherLeave::slotTimeout()
{
    const bool oldcanlock = m_canlock;
    const bool oldcanswitch = m_canswitch;
    const bool oldcanreboot = m_canreboot;
    const bool oldcanshutdown = m_canshutdown;
    m_canlock = KDBusConnectionPool::isServiceRegistered("org.freedesktop.ScreenSaver", QDBusConnection::sessionBus());
    m_canswitch = KDisplayManager().isSwitchable();
    m_canreboot = KWorkSpace::canShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeReboot);
    m_canshutdown = KWorkSpace::canShutDown(KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeHalt);
    if (oldcanlock != m_canlock || oldcanswitch != m_canswitch ||
        oldcanreboot != m_canreboot || oldcanshutdown != m_canshutdown) {
        slotUpdateLayout();
    }
}


class LauncherAppletWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    LauncherAppletWidget(LauncherApplet* auncherapplet);

    void resetSearch();
    void setAllowedRunners(const QStringList &runners);

private Q_SLOTS:
    void slotSearch(const QString &text);
    void slotTimeout();

private:
    LauncherApplet* m_launcherapplet;
    QGraphicsLinearLayout* m_layout;
    QGraphicsLinearLayout* m_toplayout;
    Plasma::IconWidget* m_iconwidget;
    Plasma::Label* m_label;
    Plasma::LineEdit* m_lineedit;
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
    QTimer* m_timer;
};

LauncherAppletWidget::LauncherAppletWidget(LauncherApplet* auncherapplet)
    : QGraphicsWidget(auncherapplet),
    m_launcherapplet(auncherapplet),
    m_layout(nullptr),
    m_toplayout(nullptr),
    m_iconwidget(nullptr),
    m_label(nullptr),
    m_lineedit(nullptr),
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
    m_timer(nullptr)
{
    m_layout = new QGraphicsLinearLayout(this);
    m_layout->setOrientation(Qt::Vertical);
    m_toplayout = new QGraphicsLinearLayout(m_layout);
    m_toplayout->setOrientation(Qt::Horizontal);
    m_toplayout->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_layout->addItem(m_toplayout);

    const QString hostname = QHostInfo::localHostName();
    KUser user(KUser::UseEffectiveUID);
    QString usericon = user.faceIconPath();
    if (usericon.isEmpty()) {
        usericon = QLatin1String("system-search");
    }
    m_iconwidget = new Plasma::IconWidget(this);
    m_iconwidget->setAcceptHoverEvents(false);
    m_iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    m_iconwidget->setIcon(usericon);
    m_toplayout->addItem(m_iconwidget);

    QString usertext;
    QString fullusername = user.property(KUser::FullName);
    if (fullusername.isEmpty()) {
        usertext = i18nc("login name, hostname", "User <b>%1</b> on <b>%2</b>", user.loginName(), hostname);
    } else {
        usertext = i18nc("full name, login name, hostname", "<b>%1 (%2)</b> on <b>%3</b>", fullusername, user.loginName(), hostname);
    }
    m_label = new Plasma::Label(this);
    m_label->setWordWrap(false);
    m_label->setText(usertext);
    m_toplayout->addItem(m_label);
    m_toplayout->setAlignment(m_label, Qt::AlignCenter);

    m_lineedit = new Plasma::LineEdit(this);
    m_lineedit->setClickMessage(i18n("Search"));
    m_lineedit->setClearButtonShown(true);
    m_toplayout->addItem(m_lineedit);
    m_toplayout->setAlignment(m_lineedit, Qt::AlignCenter);
    setFocusProxy(m_lineedit);

    m_tabbar = new Plasma::TabBar(this);
    // has not effect..
    // m_tabbar->nativeWidget()->setShape(QTabBar::RoundedSouth);
    m_favoritesscrollwidget = kMakeScrollWidget(m_tabbar);
    m_favoritesscrollwidget->setMinimumSize(s_minimumsize);
    m_favoriteswidget = new LauncherFavorites(m_favoritesscrollwidget, m_launcherapplet);
    m_favoritesscrollwidget->setWidget(m_favoriteswidget);
    m_tabbar->addTab(KIcon(s_favoriteicon), i18n("Favorites"), m_favoritesscrollwidget);
    m_applicationswidget = new LauncherApplications(m_tabbar, m_launcherapplet);
    m_applicationswidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_applicationswidget->setMinimumSize(s_minimumsize);
    m_tabbar->addTab(KIcon(s_genericicon), i18n("Applications"), m_applicationswidget);
    m_recentscrollwidget = kMakeScrollWidget(m_tabbar);
    m_recentscrollwidget->setMinimumSize(s_minimumsize);
    m_recentwidget = new LauncherRecent(m_recentscrollwidget, m_launcherapplet);
    m_recentscrollwidget->setWidget(m_recentwidget);
    m_tabbar->addTab(KIcon(s_recenticon), i18n("Recently Used"), m_recentscrollwidget);
    m_leavecrollwidget = kMakeScrollWidget(m_tabbar);
    m_leavecrollwidget->setMinimumSize(s_minimumsize);
    m_leavewidget = new LauncherLeave(m_leavecrollwidget);
    m_leavecrollwidget->setWidget(m_leavewidget);
    m_tabbar->addTab(KIcon("system-shutdown"), i18n("Leave"), m_leavecrollwidget);
    m_layout->addItem(m_tabbar);
    // squeeze the icon
    m_layout->setStretchFactor(m_tabbar, 100);

    m_searchscrollwidget = kMakeScrollWidget(this);
    m_searchscrollwidget->setMinimumSize(s_minimumsize);
    m_searchscrollwidget->setVisible(false);
    m_searchwidget = new LauncherSearch(m_searchscrollwidget, m_launcherapplet);
    m_searchscrollwidget->setWidget(m_searchwidget);
    m_layout->addItem(m_searchscrollwidget);
    connect(
        m_lineedit, SIGNAL(textChanged(QString)),
        this, SLOT(slotSearch(QString))
    );

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(s_searchdelay);
    connect(
        m_timer, SIGNAL(timeout()),
        this, SLOT(slotTimeout())
    );

    setLayout(m_layout);
}

void LauncherAppletWidget::resetSearch()
{
    m_lineedit->setText(QString());
}


void LauncherAppletWidget::setAllowedRunners(const QStringList &runners)
{
    m_searchwidget->setAllowedRunners(runners);
}

void LauncherAppletWidget::slotSearch(const QString &text)
{
    const QString query = text.trimmed();
    if (query.isEmpty()) {
        m_timer->stop();
        m_searchwidget->finish();
        m_searchscrollwidget->setVisible(false);
        m_tabbar->setVisible(true);
        return;
    }
    if (!m_timer->isActive()) {
        m_searchwidget->prepare();
    }
    m_timer->start();
}

void LauncherAppletWidget::slotTimeout()
{
    m_searchwidget->query(m_lineedit->text());
    m_tabbar->setVisible(false);
    m_searchscrollwidget->setVisible(true);
}


LauncherApplet::LauncherApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_launcherwidget(nullptr),
    m_editmenuaction(nullptr),
    m_selector(nullptr),
    m_shareconfig(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_launcher");
    setPopupIcon(s_defaultpopupicon);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);

    m_launcherwidget = new LauncherAppletWidget(this);
    setFocusProxy(m_launcherwidget);
}

void LauncherApplet::init()
{
    setGlobalShortcut(KShortcut(Qt::ALT+Qt::Key_F2));
    m_shareconfig = KSharedConfig::openConfig(globalConfig().config()->name());
    m_configgroup = m_shareconfig->group("Plugins");
    m_launcherwidget->setAllowedRunners(kAllowedRunners(m_configgroup));
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

void LauncherApplet::slotEditMenu()
{
    if (KToolInvocation::kdeinitExec("kmenuedit") == 0) {
        hidePopup();
    } else {
        showMessage(KIcon("dialog-error"), i18n("Failed to launch menu editor"), Plasma::MessageButton::ButtonOk);
    }
}

void LauncherApplet::slotConfigAccepted()
{
    Q_ASSERT(m_selector != nullptr);
    Q_ASSERT(m_shareconfig != nullptr);
    m_selector->save();
    m_configgroup.sync();
    m_shareconfig->sync();
    m_launcherwidget->setAllowedRunners(kAllowedRunners(m_configgroup));
    m_launcherwidget->resetSearch();
    emit configNeedsSaving();
}

#include "launcher.moc"
#include "moc_launcher.cpp"
