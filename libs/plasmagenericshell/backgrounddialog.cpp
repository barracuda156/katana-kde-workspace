/*
  Copyright (c) 2007 Paolo Capriotti <p.capriotti@gmail.com>
  Copyright (C) 2007 Ivan Cukic <ivan.cukic+kde@gmail.com>
  Copyright (c) 2008 by Petri Damsten <damu@iki.fi>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
*/

#include "backgrounddialog.h"
#include "mouseplugins.h"

#include <QAbstractItemView>
#include <QDesktopWidget>
#include <QFile>
#include <QPainter>
#include <QStandardItemModel>

#include <KColorScheme>
#include <KDebug>
#include <KDesktopFile>
#include <KStandardDirs>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/FrameSvg>
#include <Plasma/Package>
#include <Plasma/Wallpaper>
#include <Plasma/View>

#include "kworkspace/screenpreviewwidget.h"

#include "ui_BackgroundDialog.h"

// From kcategorizeditemsviewdelegate by Ivan Cukic
#define EMBLEM_ICON_SIZE 16
#define UNIVERSAL_PADDING 6
#define FADE_LENGTH 32
#define MAIN_ICON_SIZE 48

void WallpaperWidget::settingsChanged(bool isModified)
{
    emit modified(isModified);
}

class BackgroundDialogPrivate
{
public:
    BackgroundDialogPrivate(BackgroundDialog* dialog, Plasma::Containment* c, Plasma::View* v)
     : q(dialog),
       containmentModel(0),
       wallpaperModel(0),
       wallpaper(0),
       view(v),
       containment(c),
       preview(0),
       modified(false)
    {
    }

    ~BackgroundDialogPrivate()
    {
    }

    BackgroundDialog *q;

    Ui::BackgroundDialog backgroundDialogUi;

    QStandardItemModel* containmentModel;
    QStandardItemModel* wallpaperModel;
    Plasma::Wallpaper* wallpaper;
    Plasma::View* view;
    QWeakPointer<Plasma::Containment> containment;
    ScreenPreviewWidget* preview;
    KPageWidgetItem *activityItem;
    KPageWidgetItem *appearanceItem;
    KPageWidgetItem *mouseItem;
    bool modified;
};

BackgroundDialog::BackgroundDialog(const QSize& res, Plasma::Containment *c, Plasma::View* view,
                                   QWidget* parent, const QString &id, KConfigSkeleton *s)
    : KConfigDialog(parent, id, s),
      d(new BackgroundDialogPrivate(this, c, view))
{
    setWindowIcon(KIcon("preferences-desktop-wallpaper"));
    setCaption(i18n("Desktop Settings"));
    setButtons(Ok | Cancel | Apply);

    QWidget *main= new QWidget(this);
    d->backgroundDialogUi.setupUi(main);
    d->appearanceItem = addPage(main, i18n("View"), "preferences-desktop-wallpaper");

    qreal previewRatio = (qreal)res.width() / (qreal)res.height();
    QSize monitorSize(200, int(200 * previewRatio));

    d->backgroundDialogUi.monitor->setFixedSize(200, 200);
    d->backgroundDialogUi.monitor->setText(QString());
    d->backgroundDialogUi.monitor->setWhatsThis(i18n(
        "This picture of a monitor contains a preview of "
        "what the current settings will look like on your desktop."));
    d->preview = new ScreenPreviewWidget(d->backgroundDialogUi.monitor);
    d->preview->setRatio(previewRatio);
    d->preview->resize(200, 200);

    connect(this, SIGNAL(finished(int)), this, SLOT(cleanup()));
    connect(this, SIGNAL(okClicked()), this, SLOT(saveConfig()));
    connect(this, SIGNAL(applyClicked()), this, SLOT(saveConfig()));

    if (d->containment) {
        connect(d->containment.data(), SIGNAL(destroyed()), this, SLOT(close()));
        connect(d->containment.data(), SIGNAL(immutabilityChanged(Plasma::ImmutabilityType)),
                this, SLOT(containmentImmutabilityChanged(Plasma::ImmutabilityType)));
    }

    d->containmentModel = new QStandardItemModel(this);
    d->backgroundDialogUi.containmentComboBox->setModel(d->containmentModel);

    MousePlugins *m = new MousePlugins(d->containment.data(), this);
    connect(m, SIGNAL(modified(bool)), this, SLOT(settingsModified(bool)));
    d->mouseItem = addPage(m, i18n("Mouse Actions"), "input-mouse");

    if (d->containment && d->containment.data()->hasConfigurationInterface()) {
        d->containment.data()->createConfigurationInterface(this);
        connect(this, SIGNAL(applyClicked()), d->containment.data(), SLOT(configDialogFinished()));
        connect(this, SIGNAL(okClicked()), d->containment.data(), SLOT(configDialogFinished()));
    }

    d->wallpaperModel = new QStandardItemModel(this);
    d->backgroundDialogUi.wallpaperMode->setModel(d->wallpaperModel);

    QSize dialogSize = QSize(650, 720).expandedTo(sizeHint());
    if (d->containment) {
        const int screen = d->containment.data()->screen();
        dialogSize = dialogSize.boundedTo(qApp->desktop()->availableGeometry(screen).size());
    }
    setInitialSize(dialogSize);

    KConfigGroup cg(KGlobal::config(), "BackgroundConfigDialog");
    restoreDialogSize(cg);

    reloadConfig();

    connect(d->backgroundDialogUi.containmentComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(settingsModified()));
    connect(d->backgroundDialogUi.wallpaperMode, SIGNAL(currentIndexChanged(int)), this, SLOT(settingsModified()));

    settingsModified(false);
}

BackgroundDialog::~BackgroundDialog()
{
    KConfigGroup cg(KGlobal::config(), "BackgroundConfigDialog");
    saveDialogSize(cg);
    cleanup();
    delete d;
}

void BackgroundDialog::cleanup()
{
    delete d->wallpaper;
    d->wallpaper = 0;
}

void BackgroundDialog::containmentImmutabilityChanged(Plasma::ImmutabilityType type)
{
    bool showWidgetsUnlocking = false;

    if (d->containment && d->containment.data()->corona()) {
        QAction *unlockAction = d->containment.data()->corona()->action("lock widgets");
        if (unlockAction) {
            connect(d->backgroundDialogUi.unlockButton, SIGNAL(clicked()), unlockAction, SLOT(trigger()), Qt::UniqueConnection);
            showWidgetsUnlocking = (type == Plasma::UserImmutable);
            kDebug() << showWidgetsUnlocking << type;
        }
    }

    d->backgroundDialogUi.widgetLocking->setVisible(showWidgetsUnlocking);

    const bool enabled = type == Plasma::Mutable;
    d->backgroundDialogUi.containmentLabel->setEnabled(enabled);
    d->backgroundDialogUi.containmentComboBox->setEnabled(enabled);
}

void BackgroundDialog::reloadConfig()
{
    disconnect(d->backgroundDialogUi.wallpaperMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changeBackgroundMode(int)));
    int containmentIndex = 0;
    int wallpaperIndex = 0;

    // Containment
    KPluginInfo::List plugins = Plasma::Containment::listContainmentsOfType("desktop");
    d->containmentModel->clear();
    int i = 0;
    foreach (const KPluginInfo& info, plugins) {
        if (info.property("NoDisplay").toBool()) {
            continue;
        }

        QStandardItem* item = new QStandardItem(KIcon(info.icon()), info.name());
        item->setData(info.comment(), BackgroundDialog::DescriptionRole);
        item->setData(info.pluginName(), BackgroundDialog::PluginNameRole);
        d->containmentModel->appendRow(item);

        if (d->containment && info.pluginName() == d->containment.data()->pluginName()) {
            containmentIndex = i;
        }

        ++i;
    }

    d->backgroundDialogUi.containmentComboBox->setCurrentIndex(containmentIndex);

    if (d->containment) {
        containmentImmutabilityChanged(d->containment.data()->immutability());
    } else {
        d->backgroundDialogUi.containmentLabel->setEnabled(false);
        d->backgroundDialogUi.containmentComboBox->setEnabled(false);
    }

    // Wallpaper
    bool doWallpaper = d->containment && d->containment.data()->drawWallpaper();
    d->backgroundDialogUi.wallpaperGroup->setVisible(doWallpaper);
    d->preview->setVisible(doWallpaper);

    //kDebug() << "do wallpapers?!" << doWallpaper;
    if (doWallpaper) {
        // Load wallpaper plugins
        QString currentPlugin;
        QString currentMode;

        Plasma::Wallpaper *currentWallpaper = d->containment ? d->containment.data()->wallpaper() : 0;
        if (currentWallpaper) {
            currentPlugin = currentWallpaper->pluginName();
            currentMode = currentWallpaper->renderingMode().name();
            KConfigGroup cg = wallpaperConfig(currentPlugin);
            currentWallpaper->save(cg);
        }

        plugins = Plasma::Wallpaper::listWallpaperInfo();

        QMap<QString, KPluginInfo> sortedPlugins;
        foreach (const KPluginInfo &info, plugins) {
            sortedPlugins.insert(info.name(), info);
        }

        d->wallpaperModel->clear();
        int i = 0;
        foreach (const KPluginInfo& info, sortedPlugins) {
            //kDebug() << "doing wallpaper" << info.pluginName() << currentPlugin;
            bool matches = info.pluginName() == currentPlugin;
            const QList<KServiceAction>& modes = info.service()->actions();
            if (modes.count() > 0) {
                if (matches) {
                    wallpaperIndex = i;
                    //kDebug() << "matches at" << wallpaperIndex;
                }

                foreach (const KServiceAction& mode, modes) {
                    QStandardItem *item = new QStandardItem(KIcon(mode.icon()), mode.text());
                    KConfig config(KGlobal::dirs()->locate("services", info.entryPath()),
                                   KConfig::SimpleConfig);
                    KConfigGroup cg(&config, "Desktop Action " + mode.name());
                    item->setData(cg.readEntry("Comment", QString()), BackgroundDialog::DescriptionRole);
                    item->setData(info.pluginName(), BackgroundDialog::PluginNameRole);
                    item->setData(mode.name(), BackgroundDialog::ModeRole);
                    d->wallpaperModel->appendRow(item);

                    //kDebug() << matches << mode.name() << currentMode;
                    if (matches && mode.name() == currentMode) {
                        wallpaperIndex = i;
                        //kDebug() << "matches at" << wallpaperIndex;
                    }
                    ++i;
                }
            } else {
                QStandardItem *item = new QStandardItem(KIcon(info.icon()), info.name());
                item->setData(info.comment(), BackgroundDialog::DescriptionRole);
                item->setData(info.pluginName(), BackgroundDialog::PluginNameRole);
                d->wallpaperModel->appendRow(item);

                if (matches) {
                    wallpaperIndex = i;
                    //kDebug() << "matches at" << wallpaperIndex;
                }

                ++i;
            }
        }

        //kDebug() << "match is said to be" << wallpaperIndex << "out of" << d->backgroundDialogUi.wallpaperMode->count();
        d->backgroundDialogUi.wallpaperMode->setCurrentIndex(wallpaperIndex);
        changeBackgroundMode(wallpaperIndex);
    }


    connect(d->backgroundDialogUi.wallpaperMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changeBackgroundMode(int)));
    settingsModified(false);
}

void BackgroundDialog::changeBackgroundMode(int index)
{
    kDebug();
    QWidget* w = 0;
    const QString plugin = d->backgroundDialogUi.wallpaperMode->itemData(index, BackgroundDialog::PluginNameRole).toString();
    const QString mode = d->backgroundDialogUi.wallpaperMode->itemData(index, BackgroundDialog::ModeRole).toString();

    if (d->backgroundDialogUi.wallpaperGroup->layout()->count() > 1) {
        QLayoutItem *item = d->backgroundDialogUi.wallpaperGroup->layout()->takeAt(1);
        QWidget *widget = item->widget();
        delete item;
        delete widget;
    }

    if (d->wallpaper && d->wallpaper->pluginName() != plugin) {
        delete d->wallpaper;
        d->wallpaper = 0;
    }

    if (!d->wallpaper) {
        d->wallpaper = Plasma::Wallpaper::load(plugin);
    }

    if (d->wallpaper) {
        if (mode == "SingleImage") {
            d->wallpaper->setPreviewing(true);
            d->preview->setPreview(d->wallpaper);
        }
        d->wallpaper->setRenderingMode(mode);
        KConfigGroup cfg = wallpaperConfig(plugin);
        //kDebug() << "making a" << plugin << "in mode" << mode;
        if (d->containment) {
            d->wallpaper->setTargetSizeHint(d->containment.data()->size());
        }
        d->wallpaper->restore(cfg);

        WallpaperWidget *wallpaperWidget = new WallpaperWidget(d->backgroundDialogUi.wallpaperGroup);
        w = d->wallpaper->createConfigurationInterface(wallpaperWidget);
        connect(wallpaperWidget, SIGNAL(modified(bool)), this, SLOT(settingsModified(bool)));

        const bool needsPreviewDuringConfiguration = d->wallpaper->needsPreviewDuringConfiguration();
        d->backgroundDialogUi.monitorWidget->setVisible(needsPreviewDuringConfiguration);
        d->preview->setVisible(needsPreviewDuringConfiguration);
    }

    if (!w) {
        w = new QWidget(d->backgroundDialogUi.wallpaperGroup);
    } else if (w->layout()) {
        QGridLayout *gridLayout = qobject_cast<QGridLayout *>(w->layout());
        if (gridLayout) {
            gridLayout->setColumnMinimumWidth(0, d->backgroundDialogUi.wallpaperTypeLabel->geometry().right());
            gridLayout->setColumnStretch(0, 0);
            gridLayout->setColumnStretch(1, 10);
            gridLayout->setContentsMargins(0, 0, 0, 0);
        }
    }

    d->backgroundDialogUi.wallpaperGroup->layout()->addWidget(w);
    settingsModified(true);
}

KConfigGroup BackgroundDialog::wallpaperConfig(const QString &plugin)
{
    //FIXME: we have details about the structure of the containment config duplicated here!
    KConfigGroup cfg = d->containment ? d->containment.data()->config() : KConfigGroup(KGlobal::config(), "Wallpaper");
    cfg = KConfigGroup(&cfg, "Wallpaper");
    return KConfigGroup(&cfg, plugin);
}

void BackgroundDialog::setLayoutChangeable(bool changeable)
{
    d->backgroundDialogUi.containmentLabel->setVisible(changeable);
    d->backgroundDialogUi.containmentComboBox->setVisible(changeable);
}

bool BackgroundDialog::isLayoutChangeable() const
{
    return d->backgroundDialogUi.containmentComboBox->isVisible();
}

void BackgroundDialog::saveConfig()
{
    if (!isButtonEnabled(Apply)) {
        return;
    }

    const int wallpaperIndex = d->backgroundDialogUi.wallpaperMode->currentIndex();
    const QString wallpaperPlugin = d->backgroundDialogUi.wallpaperMode->itemData(wallpaperIndex, BackgroundDialog::PluginNameRole).toString();
    const QString wallpaperMode = d->backgroundDialogUi.wallpaperMode->itemData(wallpaperIndex, BackgroundDialog::ModeRole).toString();
    const QString containmentPlugin = d->backgroundDialogUi.containmentComboBox->itemData(d->backgroundDialogUi.containmentComboBox->currentIndex(),
                                                          BackgroundDialog::PluginNameRole).toString();

    // Containment
    if (isLayoutChangeable()) {
        if (!d->containment || d->containment.data()->pluginName() != containmentPlugin) {
            if (d->containment) {
                disconnect(d->containment.data(), SIGNAL(destroyed()), this, SLOT(close()));
                disconnect(this, 0, d->containment.data(), 0);
            }

            Plasma::Containment *containment = d->view->swapContainment(d->containment.data(), containmentPlugin);
            if (containment != d->containment.data()) {
                d->containment = containment;
            emit containmentPluginChanged(d->containment.data());

            //remove all pages but our own
            KPageWidgetModel *m = qobject_cast<KPageWidgetModel *>(pageWidget()->model());
            if (m) {
                int rows = m->rowCount();
                QList<KPageWidgetItem *> itemsToRemove;
                for (int i = 0; i < rows; ++i) {
                    QModelIndex idx = m->index(i, 0);

                    if (!idx.isValid()) {
                        continue;
                    }

                    KPageWidgetItem *item = m->item(idx);

                    if (item && item != d->appearanceItem &&
                        item != d->mouseItem && item != d->activityItem) {
                        itemsToRemove.append(item);
                    }
                }

                foreach (KPageWidgetItem *item, itemsToRemove) {
                    removePage(item);
                }
            }

            //add the new containment's config
            if (d->containment.data()->hasConfigurationInterface()) {
                d->containment.data()->createConfigurationInterface(this);
                connect(this, SIGNAL(applyClicked()), d->containment.data(), SLOT(configDialogFinished()));
                connect(this, SIGNAL(okClicked()), d->containment.data(), SLOT(configDialogFinished()));
            }
            connect(d->containment.data(), SIGNAL(destroyed()), this, SLOT(close()));
            }
        }

        // Wallpaper
        Plasma::Wallpaper *currentWallpaper = d->containment.data()->wallpaper();
        if (currentWallpaper) {
            KConfigGroup cfg = wallpaperConfig(currentWallpaper->pluginName());
            currentWallpaper->save(cfg);
        }
    }

    if (d->wallpaper) {
        KConfigGroup cfg = wallpaperConfig(d->wallpaper->pluginName());
        d->wallpaper->save(cfg);
    }

    if (d->containment) {
        d->containment.data()->setWallpaper(wallpaperPlugin, wallpaperMode);
    }

    settingsModified(false);
}

void BackgroundDialog::settingsModified(bool modified)
{
    d->modified = modified;
    updateButtons();
}

bool BackgroundDialog::hasChanged()
{
    return d->modified;
}
