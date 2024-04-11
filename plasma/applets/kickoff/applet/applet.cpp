/*
    Copyright 2007 Robert Knight <robertknight@gmail.com>

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

// Own
#include "applet/applet.h"

// Katie
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QGraphicsLinearLayout>

// KDE
#include <KIcon>
#include <KDebug>
#include <KConfigDialog>
#include <KPluginSelector>
#include <KToolInvocation>
#include <KCategorizedView>
#include <Plasma/IconWidget>
#include <Plasma/Containment>
#include <Plasma/View>
#include <Plasma/ToolTipManager>
#include <Plasma/RunnerManager>

// Local
#include "ui_kickoffConfig.h"
#include "ui/launcher.h"
#include "core/recentapplications.h"
#include "core/models.h"
#include "core/krunnermodel.h"

// NOTE: keep in sync with:
// kdelibs/kutils/kpluginselector_p.h
static const int s_pluginnamerole = 0x0CBBBB00;

static QString kMakeToolTip(const QString &pluginname)
{
    QString result;
    Plasma::AbstractRunner* runner = Kickoff::KRunnerModel::runnerManager()->runner(pluginname);
    if (!runner) {
        return result;
    }
    const QList<Plasma::RunnerSyntax> syntaxes = runner->syntaxes();
    if (syntaxes.isEmpty()) {
        return result;
    }
    QStringList uniqueexamples;
    foreach (const  Plasma::RunnerSyntax &syntax, syntaxes) {
        foreach (const QString &example, syntax.exampleQueriesWithTermDescription()) {
            uniqueexamples.append(example);
        }
    }
    uniqueexamples.removeDuplicates();
    if (uniqueexamples.isEmpty()) {
        return result;
    }
    result.append(i18n("<b>Examples:</b><br/>"));
    foreach (const QString &example, uniqueexamples) {
        result.append(QString::fromLatin1("<i>%1</i><br/>").arg(Qt::escape(example)));
    }
    // qDebug() << Q_FUNC_INFO << result;
    return result;
}

class LauncherApplet::Private
{
public:
    Private(LauncherApplet *lApplet) : launcher(0), switcher(0), q(lApplet) { }
    ~Private() {
        delete launcher;
    }
    void createLauncher();
    void initToolTip();

    Kickoff::Launcher *launcher;

    QList<QAction*> actions;
    QAction* switcher;
    LauncherApplet *q;
    Ui::kickoffConfig ui;
    KPluginSelector* selector;
};

void LauncherApplet::Private::createLauncher()
{
    if (launcher) {
        return;
    }

    launcher = new Kickoff::Launcher(q);
    launcher->setAttribute(Qt::WA_NoSystemBackground);
    launcher->setAutoHide(true);
    QObject::connect(launcher, SIGNAL(aboutToHide()), q, SLOT(hidePopup()));
    QObject::connect(launcher, SIGNAL(configNeedsSaving()), q, SIGNAL(configNeedsSaving()));
    //launcher->resize(launcher->sizeHint());
    //QObject::connect(launcher, SIGNAL(aboutToHide()), icon, SLOT(setUnpressed()));
}

void LauncherApplet::Private::initToolTip()
{
    Plasma::ToolTipContent data(i18n("Kickoff Application Launcher"),
                                i18n("Favorites, applications, computer places, "
                                     "recently used items and desktop sessions"),
                                q->popupIcon().pixmap(IconSize(KIconLoader::Desktop)));
    Plasma::ToolTipManager::self()->setContent(q, data);
}

LauncherApplet::LauncherApplet(QObject *parent, const QVariantList &args)
        : Plasma::PopupApplet(parent, args),
        d(new Private(this))
{
    KGlobal::locale()->insertCatalog("plasma_applet_launcher");
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
}

LauncherApplet::~LauncherApplet()
{
    delete d;
}

void LauncherApplet::init()
{
    if (KService::serviceByStorageId("kmenuedit.desktop")) {
        QAction* menueditor = new QAction(i18n("Edit Applications..."), this);
        d->actions.append(menueditor);
        connect(menueditor, SIGNAL(triggered(bool)), this, SLOT(startMenuEditor()));
    }

    Q_ASSERT(! d->switcher);
    d->switcher = new QAction(i18n("Switch to Classic Menu Style"), this);
    d->actions.append(d->switcher);
    connect(d->switcher, SIGNAL(triggered(bool)), this, SLOT(switchMenuStyle()));

    setGlobalShortcut(KShortcut(Qt::ALT+Qt::Key_F2));

    configChanged();
    Plasma::ToolTipManager::self()->registerWidget(this);
}

void LauncherApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if ((constraints & Plasma::ImmutableConstraint) && d->switcher) {
        d->switcher->setVisible(immutability() == Plasma::Mutable);
    }
}

void LauncherApplet::switchMenuStyle()
{
    if (containment()) {
        Plasma::Applet * simpleLauncher =
                            containment()->addApplet("simplelauncher", QVariantList() << true, geometry());

        //Copy all the config items to the simple launcher
        QMetaObject::invokeMethod(simpleLauncher, "saveConfigurationFromKickoff",
                                  Qt::DirectConnection, Q_ARG(KConfigGroup, config()),
                                  Q_ARG(KConfigGroup, globalConfig()));

        //Switch shortcuts with the new launcher to avoid losing it
        KShortcut currentShortcut = globalShortcut();
        setGlobalShortcut(KShortcut());
        simpleLauncher->setGlobalShortcut(currentShortcut);

        //Destroy this widget
        destroy();
    }
}

void LauncherApplet::startMenuEditor()
{
    KToolInvocation::kdeinitExec("kmenuedit");
}

void LauncherApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget *widget = new QWidget();
    d->ui.setupUi(widget);
    parent->addPage(widget, i18nc("General configuration page", "General"), icon());

    d->selector = new KPluginSelector(widget);
    d->selector->addPlugins(
        Plasma::RunnerManager::listRunnerInfo(),
        KPluginSelector::ReadConfigFile,
        i18n("Available Plugins"), QString(),
        Kickoff::componentData().config()
    );
    connect(d->selector, SIGNAL(changed(bool)), parent, SLOT(settingsModified()));
    parent->addPage(d->selector, i18n("Runners"), "preferences-plugin");

    foreach (const KPluginInfo& plugin, Plasma::RunnerManager::listRunnerInfo()) {
        Kickoff::KRunnerModel::runnerManager()->loadRunner(plugin.service());
    }
    // HACK: setup tooltips for the plugins
    KCategorizedView* selectorview = d->selector->findChild<KCategorizedView*>();
    if (selectorview) {
        QAbstractItemModel* selectormodel = selectorview->model();
        if (selectormodel) {
            for (int i = 0; i < selectormodel->rowCount(); i++) {
                QModelIndex selectorindex = selectormodel->index(i, 0);
                const QString pluginname = selectormodel->data(selectorindex, s_pluginnamerole).toString();
                selectormodel->setData(selectorindex, kMakeToolTip(pluginname), Qt::ToolTipRole);
            }
        }
    }
    // forces unload of the disabled plugins that have been loaded for the tooltip
    Kickoff::KRunnerModel::runnerManager()->reloadConfiguration();

    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));

    d->createLauncher();
    d->ui.iconButton->setIcon(popupIcon());
    d->ui.switchOnHoverCheckBox->setChecked(d->launcher->switchTabsOnHover());
    d->ui.appsByNameCheckBox->setChecked(d->launcher->showAppsByName());
    d->ui.showRecentlyInstalledCheckBox->setChecked(d->launcher->showRecentlyInstalled());
    connect(d->ui.iconButton, SIGNAL(iconChanged(QString)), parent, SLOT(settingsModified()));
    connect(d->ui.switchOnHoverCheckBox, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(d->ui.appsByNameCheckBox, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(d->ui.showRecentlyInstalledCheckBox, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
}

void LauncherApplet::popupEvent(bool show)
{
    if (show) {
        Plasma::ToolTipManager::self()->clearContent(this);
        d->createLauncher();
        d->launcher->setLauncherOrigin(popupPlacement(), location());
    }
}

void LauncherApplet::toolTipAboutToShow()
{
    if (d->launcher->isVisible()) {
        Plasma::ToolTipManager::self()->clearContent(this);
    } else {
        d->initToolTip();
    }
}

void LauncherApplet::configChanged()
{
    KConfigGroup cg = config();
    setPopupIcon(cg.readEntry("icon", "start-here-kde"));
    constraintsEvent(Plasma::ImmutableConstraint);

    if (d->launcher) {
        d->launcher->setApplet(this);
    }
}

void LauncherApplet::configAccepted()
{
    d->selector->save();
    KConfigGroup pcg = Kickoff::componentData().config()->group("Plugins");
    QStringList allowed;
    foreach (KPluginInfo plugin, Plasma::RunnerManager::listRunnerInfo()) {
        plugin.load(pcg);
        if (plugin.isPluginEnabled()) {
            allowed.append(plugin.pluginName());
        }
    }
    Kickoff::KRunnerModel::runnerManager()->setAllowedRunners(allowed);

    bool switchTabsOnHover = d->ui.switchOnHoverCheckBox->isChecked();
    bool showAppsByName = d->ui.appsByNameCheckBox->isChecked();
    bool showRecentlyInstalled = d->ui.showRecentlyInstalledCheckBox->isChecked();

    const QString iconname = d->ui.iconButton->icon();

    // TODO: should this be moved into Launcher as well? perhaps even the config itself?
    d->createLauncher();

    KConfigGroup cg = config();
    const QString oldIcon = cg.readEntry("icon", "start-here-kde");
    if (!iconname.isEmpty() && iconname != oldIcon) {
        cg.writeEntry("icon", iconname);

        if (!iconname.isEmpty()) {
            setPopupIcon(iconname);
        }

        emit configNeedsSaving();
    }

    d->launcher->setSwitchTabsOnHover(switchTabsOnHover);
    d->launcher->setShowAppsByName(showAppsByName);
    d->launcher->setShowRecentlyInstalled(showRecentlyInstalled);
}

QList<QAction*> LauncherApplet::contextualActions()
{
    return d->actions;
}

QWidget *LauncherApplet::widget()
{
    d->createLauncher();
    return d->launcher;
}

void LauncherApplet::saveConfigurationFromSimpleLauncher(const KConfigGroup & configGroup, const KConfigGroup & globalConfigGroup)
{
    //Copy configuration values
    KConfigGroup cg = config();
    configGroup.copyTo(&cg);

    KConfigGroup gcg = globalConfig();
    globalConfigGroup.copyTo(&gcg);

    configChanged();
    emit configNeedsSaving();
}

#include "moc_applet.cpp"
