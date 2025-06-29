/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz <peter.penz19@gmail.com>             *
 *   Copyright (C) 2006 by Stefan Monov <logixoul@gmail.com>               *
 *   Copyright (C) 2006 by Cvetoslav Ludmiloff <ludmiloff@gmail.com>       *
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

#include "dolphinmainwindow.h"

#include "dolphinapplication.h"
#include "dolphindockwidget.h"
#include "dolphincontextmenu.h"
#include "dolphinnewfilemenu.h"
#include "dolphinrecenttabsmenu.h"
#include "dolphinviewcontainer.h"
#include "dolphintabpage.h"
#include "panels/places/placespanel.h"
#include "panels/information/informationpanel.h"
#include "settings/dolphinsettingsdialog.h"
#include "statusbar/dolphinstatusbar.h"
#include "views/dolphinviewactionhandler.h"
#include "views/dolphinremoteencoding.h"
#include "views/draganddrophelper.h"
#include "views/viewproperties.h"
#include "views/dolphinnewfilemenuobserver.h"

#include "panels/terminal/terminalpanel.h"

#include "dolphin_generalsettings.h"

#include <KAcceleratorManager>
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KConfig>
#include <KDesktopFile>
#include <kdeversion.h>
#include <kdualaction.h>
#include <KFileDialog>
#include <KGlobal>
#include <KLineEdit>
#include <KToolBar>
#include <KIcon>
#include <KIconLoader>
#include <KIO/NetAccess>
#include <KIO/JobUiDelegate>
#include <KInputDialog>
#include <KLocale>
#include <KProtocolManager>
#include <KMenu>
#include <KMessageBox>
#include <KFileItemListProperties>
#include <konqmimedata.h>
#include <KProtocolInfo>
#include <KRun>
#include <KShell>
#include <KStandardDirs>
#include <kstatusbar.h>
#include <KStandardAction>
#include <ktabbar.h>
#include <KToggleAction>
#include <KUrlNavigator>
#include <KUrl>
#include <KUrlComboBox>
#include <KToolInvocation>

#include <QtGui/qevent.h>
#include <QDesktopWidget>
#include <QDBusMessage>
#include <QClipboard>
#include <QToolButton>
#include <QMenuBar>

// Used for GeneralSettings::version() to determine whether
// an updated version of Dolphin is running.
static const int CurrentDolphinVersion = 200;

DolphinMainWindow::DolphinMainWindow() :
    KXmlGuiWindow(0),
    m_newFileMenu(0),
    m_tabBar(0),
    m_activeViewContainer(0),
    m_centralWidgetLayout(0),
    m_tabIndex(-1),
    m_viewTab(),
    m_actionHandler(0),
    m_remoteEncoding(0),
    m_settingsDialog(),
    m_controlButton(0),
    m_updateToolBarTimer(0),
    m_lastHandleUrlStatJob(0)
{
    setObjectName("Dolphin#");

    connect(&DolphinNewFileMenuObserver::instance(), SIGNAL(errorMessage(QString)),
            this, SLOT(showErrorMessage(QString)));

    KIO::FileUndoManager* undoManager = KIO::FileUndoManager::self();
    undoManager->setUiInterface(new UndoUiInterface());

    connect(undoManager, SIGNAL(undoAvailable(bool)),
            this, SLOT(slotUndoAvailable(bool)));
    connect(undoManager, SIGNAL(undoTextChanged(QString)),
            this, SLOT(slotUndoTextChanged(QString)));
    connect(undoManager, SIGNAL(jobRecordingStarted(CommandType)),
            this, SLOT(clearStatusBar()));
    connect(undoManager, SIGNAL(jobRecordingFinished(CommandType)),
            this, SLOT(showCommand(CommandType)));

    GeneralSettings* generalSettings = GeneralSettings::self();
    const bool firstRun = (generalSettings->version() < CurrentDolphinVersion);
    if (firstRun) {
        generalSettings->setViewPropsTimestamp(QDateTime::currentDateTime());
    }

    setAcceptDrops(true);

    setupActions();

    m_actionHandler = new DolphinViewActionHandler(actionCollection(), this);
    connect(m_actionHandler, SIGNAL(actionBeingHandled()), SLOT(clearStatusBar()));
    connect(m_actionHandler, SIGNAL(createDirectory()), SLOT(createDirectory()));

    m_remoteEncoding = new DolphinRemoteEncoding(this, m_actionHandler);
    connect(this, SIGNAL(urlChanged(KUrl)),
            m_remoteEncoding, SLOT(slotAboutToOpenUrl()));

    m_tabBar = new KTabBar(this);
    m_tabBar->setMovable(true);
    m_tabBar->setTabsClosable(true);
    connect(m_tabBar, SIGNAL(currentChanged(int)),
            this, SLOT(setActiveTab(int)));
    connect(m_tabBar, SIGNAL(tabCloseRequested(int)),
            this, SLOT(closeTab(int)));
    connect(m_tabBar, SIGNAL(contextMenu(int,QPoint)),
            this, SLOT(openTabContextMenu(int,QPoint)));
    connect(m_tabBar, SIGNAL(newTabRequest()),
            this, SLOT(openNewTab()));
    connect(m_tabBar, SIGNAL(testCanDecode(const QDragMoveEvent*,bool&)),
            this, SLOT(slotTestCanDecode(const QDragMoveEvent*,bool&)));
    connect(m_tabBar, SIGNAL(mouseMiddleClick(int)),
            this, SLOT(closeTab(int)));
    connect(m_tabBar, SIGNAL(tabMoved(int,int)),
            this, SLOT(slotTabMoved(int,int)));
    connect(m_tabBar, SIGNAL(receivedDropEvent(int,QDropEvent*)),
            this, SLOT(tabDropEvent(int,QDropEvent*)));

    m_tabBar->blockSignals(true);  // signals get unblocked after at least 2 tabs are open
    m_tabBar->hide();

    QWidget* centralWidget = new QWidget(this);
    m_centralWidgetLayout = new QVBoxLayout(centralWidget);
    m_centralWidgetLayout->setSpacing(0);
    m_centralWidgetLayout->setMargin(0);
    m_centralWidgetLayout->addWidget(m_tabBar);

    setCentralWidget(centralWidget);
    setupDockWidgets();

    setupGUI(Keys | Save | Create | ToolBar);
    stateChanged("new_file");

    QClipboard* clipboard = QApplication::clipboard();
    connect(clipboard, SIGNAL(dataChanged()),
            this, SLOT(updatePasteAction()));

    QAction* showFilterBarAction = actionCollection()->action("show_filter_bar");
    showFilterBarAction->setChecked(generalSettings->filterBar());

    if (firstRun) {
        menuBar()->setVisible(true);
        // Assure a proper default size if Dolphin runs the first time
        resize(750, 500);
    }

    const bool showMenu = !menuBar()->isHidden();
    QAction* showMenuBarAction = actionCollection()->action(KStandardAction::name(KStandardAction::ShowMenubar));
    showMenuBarAction->setChecked(showMenu);  // workaround for bug #171080
    if (!showMenu) {
        createControlButton();
    }
}

DolphinMainWindow::~DolphinMainWindow()
{
}

void DolphinMainWindow::openDirectories(const QList<KUrl>& dirs)
{
    const bool hasSplitView = GeneralSettings::splitView();

    // Open each directory inside a new tab. If the "split view" option has been enabled,
    // always show two directories within one tab.
    QList<KUrl>::const_iterator it = dirs.constBegin();
    while (it != dirs.constEnd()) {
        const KUrl& primaryUrl = *(it++);
        if (hasSplitView && (it != dirs.constEnd())) {
            const KUrl& secondaryUrl = *(it++);
            openNewTab(primaryUrl, secondaryUrl);
        } else {
            openNewTab(primaryUrl);
        }
    }
}

void DolphinMainWindow::openFiles(const QList<KUrl>& files)
{
    if (files.isEmpty()) {
        return;
    }

    // Get all distinct directories from 'files' and open a tab
    // for each directory. If the "split view" option is enabled, two
    // directories are shown inside one tab (see openDirectories()).
    QList<KUrl> dirs;
    foreach (const KUrl& url, files) {
        const KUrl dir(url.directory());
        if (!dirs.contains(dir)) {
            dirs.append(dir);
        }
    }

    openDirectories(dirs);

    // Select the files. Although the files can be split between several
    // tabs, there is no need to split 'files' accordingly, as
    // the DolphinView will just ignore invalid selections.
    foreach (DolphinTabPage* tabPage, m_viewTab) {
        tabPage->markUrlsAsSelected(files);
        tabPage->markUrlAsCurrent(files.first());
    }
}

void DolphinMainWindow::showCommand(CommandType command)
{
    DolphinStatusBar* statusBar = m_activeViewContainer->statusBar();
    switch (command) {
    case KIO::FileUndoManager::Copy:
        statusBar->setText(i18nc("@info:status", "Successfully copied."));
        break;
    case KIO::FileUndoManager::Move:
        statusBar->setText(i18nc("@info:status", "Successfully moved."));
        break;
    case KIO::FileUndoManager::Link:
        statusBar->setText(i18nc("@info:status", "Successfully linked."));
        break;
    case KIO::FileUndoManager::Trash:
        statusBar->setText(i18nc("@info:status", "Successfully moved to trash."));
        break;
    case KIO::FileUndoManager::Rename:
        statusBar->setText(i18nc("@info:status", "Successfully renamed."));
        break;

    case KIO::FileUndoManager::Mkdir:
        statusBar->setText(i18nc("@info:status", "Created folder."));
        break;

    default:
        break;
    }
}

void DolphinMainWindow::pasteIntoFolder()
{
    m_activeViewContainer->view()->pasteIntoFolder();
}

void DolphinMainWindow::changeUrl(const KUrl& url)
{
    if (!KProtocolManager::supportsListing(url)) {
        // The URL navigator only checks for validity, not
        // if the URL can be listed. An error message is
        // shown due to DolphinViewContainer::restoreView().
        return;
    }

    DolphinViewContainer* view = activeViewContainer();
    if (view) {
        view->setUrl(url);
        updateEditActions();
        updatePasteAction();
        updateViewActions();
        updateGoActions();
        setUrlAsCaption(url);

        m_tabBar->setTabIcon(m_tabIndex, QIcon(KIO::pixmapForUrl(url)));
        m_tabBar->setTabText(m_tabIndex, squeezedText(tabName(view->url())));

        emit urlChanged(url);
    }
}

void DolphinMainWindow::slotTerminalDirectoryChanged(const KUrl& url)
{
    m_activeViewContainer->setAutoGrabFocus(false);
    changeUrl(url);
    m_activeViewContainer->setAutoGrabFocus(true);
}

void DolphinMainWindow::slotEditableStateChanged(bool editable)
{
    KToggleAction* editableLocationAction =
        static_cast<KToggleAction*>(actionCollection()->action("editable_location"));
    editableLocationAction->setChecked(editable);
}

void DolphinMainWindow::slotSelectionChanged(const KFileItemList& selection)
{
    updateEditActions();

    emit selectionChanged(selection);
}

void DolphinMainWindow::slotRequestItemInfo(const KFileItem& item)
{
    emit requestItemInfo(item);
}

void DolphinMainWindow::updateHistory()
{
    const KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    const int index = urlNavigator->historyIndex();

    QAction* backAction = actionCollection()->action("go_back");
    if (backAction) {
        backAction->setToolTip(i18nc("@info", "Go back"));
        backAction->setEnabled(index < urlNavigator->historySize() - 1);
    }

    QAction* forwardAction = actionCollection()->action("go_forward");
    if (forwardAction) {
        forwardAction->setToolTip(i18nc("@info", "Go forward"));
        forwardAction->setEnabled(index > 0);
    }
}

void DolphinMainWindow::updateFilterBarAction(bool show)
{
    QAction* showFilterBarAction = actionCollection()->action("show_filter_bar");
    showFilterBarAction->setChecked(show);
}

void DolphinMainWindow::openNewMainWindow()
{
    KToolInvocation::self()->startServiceByStorageId("dolphin", QStringList(), this);
}

void DolphinMainWindow::openNewTab()
{
    const bool isUrlEditable =  m_activeViewContainer->urlNavigator()->isUrlEditable();

    openNewTab(m_activeViewContainer->url());
    m_tabBar->setCurrentIndex(m_viewTab.count() - 1);

    // The URL navigator of the new tab should have the same editable state
    // as the current tab
    KUrlNavigator* navigator = m_activeViewContainer->urlNavigator();
    navigator->setUrlEditable(isUrlEditable);

    if (isUrlEditable) {
        // If a new tab is opened and the URL is editable, assure that
        // the user can edit the URL without manually setting the focus
        navigator->setFocus();
    }
}

void DolphinMainWindow::openNewTab(const KUrl& primaryUrl, const KUrl& secondaryUrl)
{
    QWidget* focusWidget = QApplication::focusWidget();

    DolphinTabPage* tabPage = new DolphinTabPage(primaryUrl, secondaryUrl, this);
    m_viewTab.append(tabPage);

    connect(tabPage, SIGNAL(activeViewChanged()),
            this, SLOT(activeViewChanged()));

    // The places-selector from the URL navigator should only be shown
    // if the places dock is invisible
    QDockWidget* placesDock = findChild<QDockWidget*>("placesDock");
    const bool placesSelectorVisible = !placesDock || !placesDock->isVisible();
    tabPage->setPlacesSelectorVisible(placesSelectorVisible);

    tabPage->hide();

    m_tabBar->addTab(QIcon(KIO::pixmapForUrl(primaryUrl)), squeezedText(tabName(primaryUrl)));

    if (m_viewTab.count() > 1) {
        actionCollection()->action("close_tab")->setEnabled(true);
        actionCollection()->action("activate_prev_tab")->setEnabled(true);
        actionCollection()->action("activate_next_tab")->setEnabled(true);
        m_tabBar->show();
        m_tabBar->blockSignals(false);
    }

    if (focusWidget) {
        // The DolphinViewContainer grabbed the keyboard focus. As the tab is opened
        // in background, assure that the previous focused widget gets the focus back.
        focusWidget->setFocus();
    }
}

void DolphinMainWindow::openNewActivatedTab(const KUrl& primaryUrl, const KUrl& secondaryUrl)
{
    openNewTab(primaryUrl, secondaryUrl);
    setActiveTab(m_viewTab.count() - 1);
}

void DolphinMainWindow::activateNextTab()
{
    if (m_viewTab.count() >= 2) {
        const int tabIndex = (m_tabBar->currentIndex() + 1) % m_tabBar->count();
        setActiveTab(tabIndex);
    }
}

void DolphinMainWindow::activatePrevTab()
{
    if (m_viewTab.count() >= 2) {
        int tabIndex = m_tabBar->currentIndex() - 1;
        if (tabIndex == -1) {
            tabIndex = m_tabBar->count() - 1;
        }
        setActiveTab(tabIndex);
    }
}

void DolphinMainWindow::openInNewTab()
{
    const KFileItemList& list = m_activeViewContainer->view()->selectedItems();
    if (list.isEmpty()) {
        openNewTab(m_activeViewContainer->url());
    } else {
        foreach (const KFileItem& item, list) {
            const KUrl& url = DolphinView::openItemAsFolderUrl(item);
            if (!url.isEmpty()) {
                openNewTab(url);
            }
        }
    }
}

void DolphinMainWindow::openInNewWindow()
{
    KUrl newWindowUrl;

    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if (list.isEmpty()) {
        newWindowUrl = m_activeViewContainer->url();
    } else if (list.count() == 1) {
        const KFileItem& item = list.first();
        newWindowUrl = DolphinView::openItemAsFolderUrl(item);
    }

    if (!newWindowUrl.isEmpty()) {
        KToolInvocation::self()->startServiceByStorageId("dolphin", QStringList() << newWindowUrl.url(), this);
    }
}

void DolphinMainWindow::showEvent(QShowEvent* event)
{
    KXmlGuiWindow::showEvent(event);

    if (!m_activeViewContainer && m_viewTab.count() > 0) {
        // If we have no active view container yet, we set the primary view container
        // of the first tab as active view container.
        setActiveTab(0);
    }

    if (!event->spontaneous()) {
        m_activeViewContainer->view()->setFocus();
    }
}

void DolphinMainWindow::closeEvent(QCloseEvent* event)
{
    // Find out if Dolphin is closed directly by the user or
    // by the session manager because the session is closed
    bool closedByUser = true;
    DolphinApplication *application = qobject_cast<DolphinApplication*>(qApp);
    if (application && application->sessionSaving()) {
        closedByUser = false;
    }

    if (m_viewTab.count() > 1 && GeneralSettings::confirmClosingMultipleTabs() && closedByUser) {
        // Ask the user if he really wants to quit and close all tabs.
        // Open a confirmation dialog with 3 buttons:
        // KDialog::Yes    -> Quit
        // KDialog::No     -> Close only the current tab
        // KDialog::Cancel -> do nothing
        KDialog *dialog = new KDialog(this, Qt::Dialog);
        dialog->setCaption(i18nc("@title:window", "Confirmation"));
        dialog->setButtons(KDialog::Yes | KDialog::No | KDialog::Cancel);
        dialog->setModal(true);
        dialog->setButtonGuiItem(KDialog::Yes, KStandardGuiItem::quit());
        dialog->setButtonGuiItem(KDialog::No, KGuiItem(i18n("C&lose Current Tab"), KIcon("tab-close")));
        dialog->setButtonGuiItem(KDialog::Cancel, KStandardGuiItem::cancel());
        dialog->setDefaultButton(KDialog::Yes);

        bool doNotAskAgainCheckboxResult = false;

        const int result = KMessageBox::createKMessageBox(dialog,
            QMessageBox::Warning,
            i18n("You have multiple tabs open in this window, are you sure you want to quit?"),
            QStringList(),
            i18n("Do not ask again"),
            &doNotAskAgainCheckboxResult,
            KMessageBox::Notify);

        if (doNotAskAgainCheckboxResult) {
            GeneralSettings::setConfirmClosingMultipleTabs(false);
        }

        switch (result) {
            case KDialog::Yes:
                // Quit
                break;
            case KDialog::No:
                // Close only the current tab
                closeTab();
            default:
                event->ignore();
                return;
        }
    }

    GeneralSettings::setVersion(CurrentDolphinVersion);
    GeneralSettings::self()->writeConfig();

    KXmlGuiWindow::closeEvent(event);
}

void DolphinMainWindow::saveProperties(KConfigGroup& group)
{
    const int tabCount = m_viewTab.count();
    group.writeEntry("Tab Count", tabCount);
    group.writeEntry("Active Tab Index", m_tabBar->currentIndex());

    for (int i = 0; i < tabCount; ++i) {
        const DolphinTabPage* tabPage = m_viewTab.at(i);
        group.writeEntry("Tab " + QString::number(i), tabPage->saveState());
    }
}

void DolphinMainWindow::readProperties(const KConfigGroup& group)
{
    const int tabCount = group.readEntry("Tab Count", 1);
    for (int i = 0; i < tabCount; ++i) {
        const QByteArray state = group.readEntry("Tab " + QString::number(i), QByteArray());
        DolphinTabPage* tabPage = m_viewTab.at(i);
        tabPage->restoreState(state);

        // openNewTab() needs to be called only tabCount - 1 times
        if (i != tabCount - 1) {
            openNewTab();
        }
    }

    const int index = group.readEntry("Active Tab Index", 0);
    m_tabBar->setCurrentIndex(index);
}

void DolphinMainWindow::updateNewMenu()
{
    m_newFileMenu->setViewShowsHiddenFiles(activeViewContainer()->view()->hiddenFilesShown());
    m_newFileMenu->checkUpToDate();
    m_newFileMenu->setPopupFiles(activeViewContainer()->url());
}

void DolphinMainWindow::createDirectory()
{
    m_newFileMenu->setViewShowsHiddenFiles(activeViewContainer()->view()->hiddenFilesShown());
    m_newFileMenu->setPopupFiles(activeViewContainer()->url());
    m_newFileMenu->createDirectory();
}

void DolphinMainWindow::quit()
{
    close();
}

void DolphinMainWindow::showErrorMessage(const QString& message)
{
    m_activeViewContainer->showMessage(message, DolphinViewContainer::Error);
}

void DolphinMainWindow::slotUndoAvailable(bool available)
{
    QAction* undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    if (undoAction) {
        undoAction->setEnabled(available);
    }
}

void DolphinMainWindow::slotUndoTextChanged(const QString& text)
{
    QAction* undoAction = actionCollection()->action(KStandardAction::name(KStandardAction::Undo));
    if (undoAction) {
        undoAction->setText(text);
    }
}

void DolphinMainWindow::undo()
{
    clearStatusBar();
    KIO::FileUndoManager::self()->uiInterface()->setParentWidget(this);
    KIO::FileUndoManager::self()->undo();
}

void DolphinMainWindow::cut()
{
    m_activeViewContainer->view()->cutSelectedItems();
}

void DolphinMainWindow::copy()
{
    m_activeViewContainer->view()->copySelectedItems();
}

void DolphinMainWindow::paste()
{
    m_activeViewContainer->view()->paste();
}

void DolphinMainWindow::find()
{
    m_activeViewContainer->setSearchModeEnabled(true);
}

void DolphinMainWindow::updatePasteAction()
{
    QAction* pasteAction = actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
    QPair<bool, QString> pasteInfo = m_activeViewContainer->view()->pasteInfo();
    pasteAction->setEnabled(pasteInfo.first);
    pasteAction->setText(pasteInfo.second);
}

void DolphinMainWindow::selectAll()
{
    clearStatusBar();

    // if the URL navigator is editable and focused, select the whole
    // URL instead of all items of the view

    KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    QLineEdit* lineEdit = urlNavigator->editor()->lineEdit(); // krazy:exclude=qclasses
    const bool selectUrl = urlNavigator->isUrlEditable() &&
                           lineEdit->hasFocus();
    if (selectUrl) {
        lineEdit->selectAll();
    } else {
        m_activeViewContainer->view()->selectAll();
    }
}

void DolphinMainWindow::invertSelection()
{
    clearStatusBar();
    m_activeViewContainer->view()->invertSelection();
}

void DolphinMainWindow::toggleSplitView()
{
    DolphinTabPage* tabPage = m_viewTab.at(m_tabIndex);
    tabPage->setSplitViewEnabled(!tabPage->splitViewEnabled());

    updateViewActions();
}

void DolphinMainWindow::reloadView()
{
    clearStatusBar();
    m_activeViewContainer->view()->reload();
}

void DolphinMainWindow::stopLoading()
{
    m_activeViewContainer->view()->stopLoading();
}

void DolphinMainWindow::enableStopAction()
{
    actionCollection()->action("stop")->setEnabled(true);
}

void DolphinMainWindow::disableStopAction()
{
    actionCollection()->action("stop")->setEnabled(false);
}

void DolphinMainWindow::showFilterBar()
{
    m_activeViewContainer->setFilterBarVisible(true);
}

void DolphinMainWindow::toggleEditLocation()
{
    clearStatusBar();

    QAction* action = actionCollection()->action("editable_location");
    KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    urlNavigator->setUrlEditable(action->isChecked());
}

void DolphinMainWindow::replaceLocation()
{
    KUrlNavigator* navigator = m_activeViewContainer->urlNavigator();
    navigator->setUrlEditable(true);
    navigator->setFocus();

    // select the whole text of the combo box editor
    QLineEdit* lineEdit = navigator->editor()->lineEdit();  // krazy:exclude=qclasses
    lineEdit->selectAll();
}

void DolphinMainWindow::togglePanelLockState()
{
    const bool newLockState = !GeneralSettings::lockPanels();
    foreach (QObject* child, children()) {
        DolphinDockWidget* dock = qobject_cast<DolphinDockWidget*>(child);
        if (dock) {
            dock->setLocked(newLockState);
        }
    }

    GeneralSettings::setLockPanels(newLockState);
}

void DolphinMainWindow::slotPlacesPanelVisibilityChanged(bool visible)
{
    foreach (DolphinTabPage* tabPage, m_viewTab) {
        // The Places selector in the location bar should be shown if and only if the Places panel is hidden.
        tabPage->setPlacesSelectorVisible(!visible);
    }
}

void DolphinMainWindow::goBack()
{
    KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    urlNavigator->goBack();

    if (urlNavigator->locationState().isEmpty()) {
        // An empty location state indicates a redirection URL,
        // which must be skipped too
        urlNavigator->goBack();
    }
}

void DolphinMainWindow::goForward()
{
    m_activeViewContainer->urlNavigator()->goForward();
}

void DolphinMainWindow::goUp()
{
    m_activeViewContainer->urlNavigator()->goUp();
}

void DolphinMainWindow::goHome()
{
    m_activeViewContainer->urlNavigator()->goHome();
}

void DolphinMainWindow::goBack(Qt::MouseButtons buttons)
{
    // The default case (left button pressed) is handled in goBack().
    if (buttons == Qt::MiddleButton) {
        KUrlNavigator* urlNavigator = activeViewContainer()->urlNavigator();
        const int index = urlNavigator->historyIndex() + 1;
        openNewTab(urlNavigator->locationUrl(index));
    }
}

void DolphinMainWindow::goForward(Qt::MouseButtons buttons)
{
    // The default case (left button pressed) is handled in goForward().
    if (buttons == Qt::MiddleButton) {
        KUrlNavigator* urlNavigator = activeViewContainer()->urlNavigator();
        const int index = urlNavigator->historyIndex() - 1;
        openNewTab(urlNavigator->locationUrl(index));
    }
}

void DolphinMainWindow::goUp(Qt::MouseButtons buttons)
{
    // The default case (left button pressed) is handled in goUp().
    if (buttons == Qt::MiddleButton) {
        openNewTab(activeViewContainer()->url().upUrl());
    }
}

void DolphinMainWindow::goHome(Qt::MouseButtons buttons)
{
    // The default case (left button pressed) is handled in goHome().
    if (buttons == Qt::MiddleButton) {
        openNewTab(GeneralSettings::self()->homeUrl());
    }
}

void DolphinMainWindow::toggleShowMenuBar()
{
    const bool visible = menuBar()->isVisible();
    menuBar()->setVisible(!visible);
    if (visible) {
        createControlButton();
    } else {
        deleteControlButton();
    }
}

void DolphinMainWindow::openTerminal()
{
    QString dir(QDir::homePath());

    const KUrl url = m_activeViewContainer->url();
    // If the URL is local, set the directory.
    if (url.isLocalFile()) {
        dir = url.toLocalFile();
    }

    KToolInvocation::self()->invokeTerminal(QString(), dir);
}

void DolphinMainWindow::editSettings()
{
    if (!m_settingsDialog) {
        DolphinViewContainer* container = activeViewContainer();
        container->view()->writeSettings();

        const KUrl url = container->url();
        DolphinSettingsDialog* settingsDialog = new DolphinSettingsDialog(url, this);
        connect(settingsDialog, SIGNAL(settingsChanged()), this, SLOT(refreshViews()));
        settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        settingsDialog->show();
        m_settingsDialog = settingsDialog;
    } else {
        m_settingsDialog.data()->raise();
    }
}

void DolphinMainWindow::setActiveTab(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_viewTab.count());
    if (index == m_tabIndex) {
        return;
    }

    m_tabBar->setCurrentIndex(index);

    // hide current tab content
    if (m_tabIndex >= 0) {
        DolphinTabPage* hiddenTabPage = m_viewTab.at(m_tabIndex);
        hiddenTabPage->hide();
        m_centralWidgetLayout->removeWidget(hiddenTabPage);
    }

    // show active tab content
    m_tabIndex = index;

    DolphinTabPage* tabPage = m_viewTab.at(index);
    m_centralWidgetLayout->addWidget(tabPage, 1);
    tabPage->show();

    setActiveViewContainer(tabPage->activeViewContainer());
}

void DolphinMainWindow::closeTab()
{
    closeTab(m_tabBar->currentIndex());
}

void DolphinMainWindow::closeTab(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < m_viewTab.count());
    if (m_viewTab.count() == 1) {
        // the last tab may never get closed
        return;
    }

    if (index == m_tabIndex) {
        // The tab that should be closed is the active tab. Activate the
        // previous tab before closing the tab.
        m_tabBar->setCurrentIndex((index > 0) ? index - 1 : 1);
    }

    DolphinTabPage* tabPage = m_viewTab.at(index);

    if (tabPage->splitViewEnabled()) {
        emit rememberClosedTab(tabPage->primaryViewContainer()->url(),
                               tabPage->secondaryViewContainer()->url());
    } else {
        emit rememberClosedTab(tabPage->primaryViewContainer()->url(), KUrl());
    }

    // delete tab
    m_viewTab.removeAt(index);
    tabPage->deleteLater();

    m_tabBar->blockSignals(true);
    m_tabBar->removeTab(index);

    if (m_tabIndex > index) {
        m_tabIndex--;
        Q_ASSERT(m_tabIndex >= 0);
    }

    // if only one tab is left, also remove the tab entry so that
    // closing the last tab is not possible
    if (m_viewTab.count() < 2) {
        actionCollection()->action("close_tab")->setEnabled(false);
        actionCollection()->action("activate_prev_tab")->setEnabled(false);
        actionCollection()->action("activate_next_tab")->setEnabled(false);
        m_tabBar->hide();
    } else {
        m_tabBar->blockSignals(false);
    }
}

void DolphinMainWindow::openTabContextMenu(int index, const QPoint& pos)
{
    KMenu menu(this);

    QAction* newTabAction = menu.addAction(KIcon("tab-new"), i18nc("@action:inmenu", "New Tab"));
    newTabAction->setShortcut(actionCollection()->action("new_tab")->shortcut());

    QAction* detachTabAction = menu.addAction(KIcon("tab-detach"), i18nc("@action:inmenu", "Detach Tab"));

    QAction* closeOtherTabsAction = menu.addAction(KIcon("tab-close-other"), i18nc("@action:inmenu", "Close Other Tabs"));

    QAction* closeTabAction = menu.addAction(KIcon("tab-close"), i18nc("@action:inmenu", "Close Tab"));
    closeTabAction->setShortcut(actionCollection()->action("close_tab")->shortcut());
    QAction* selectedAction = menu.exec(pos);
    if (selectedAction == newTabAction) {
        const KUrl url = m_viewTab.at(index)->activeViewContainer()->url();
        openNewTab(url);
        m_tabBar->setCurrentIndex(m_viewTab.count() - 1);
    } else if (selectedAction == detachTabAction) {
        QStringList dolphinArgs;
        const DolphinTabPage* tabPage = m_viewTab.at(index);
        dolphinArgs << tabPage->primaryViewContainer()->url().url();
        if (tabPage->splitViewEnabled()) {
            dolphinArgs << tabPage->secondaryViewContainer()->url().url();
            dolphinArgs << QLatin1String("-split");
        }

        KToolInvocation::self()->startProgram("dolphin", dolphinArgs, this);

        closeTab(index);
    } else if (selectedAction == closeOtherTabsAction) {
        const int count = m_tabBar->count();
        for (int i = 0; i < index; ++i) {
            closeTab(0);
        }
        for (int i = index + 1; i < count; ++i) {
            closeTab(1);
        }
    } else if (selectedAction == closeTabAction) {
        closeTab(index);
    }
}

void DolphinMainWindow::slotTabMoved(int from, int to)
{
    m_viewTab.move(from, to);
    m_tabIndex = m_tabBar->currentIndex();
}

void DolphinMainWindow::slotTestCanDecode(const QDragMoveEvent* event, bool& canDecode)
{
    canDecode = KUrl::List::canDecode(event->mimeData());
}

void DolphinMainWindow::handleUrl(const KUrl& url)
{
    delete m_lastHandleUrlStatJob;
    m_lastHandleUrlStatJob = 0;

    if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) {
        activeViewContainer()->setUrl(url);
    } else if (KProtocolManager::supportsListing(url)) {
        // stat the URL to see if it is a dir or not
        m_lastHandleUrlStatJob = KIO::stat(url, KIO::HideProgressInfo);
        if (m_lastHandleUrlStatJob->ui()) {
            m_lastHandleUrlStatJob->ui()->setWindow(this);
        }
        connect(m_lastHandleUrlStatJob, SIGNAL(result(KJob*)),
                this, SLOT(slotHandleUrlStatFinished(KJob*)));

    } else {
        KToolInvocation::self()->startServiceForUrl(url.url(), this);
    }
}

void DolphinMainWindow::slotHandleUrlStatFinished(KJob* job)
{
    m_lastHandleUrlStatJob = 0;
    const KIO::UDSEntry entry = static_cast<KIO::StatJob*>(job)->statResult();
    const KUrl url = static_cast<KIO::StatJob*>(job)->url();
    if (entry.isDir()) {
        activeViewContainer()->setUrl(url);
    } else {
        KToolInvocation::self()->startServiceForUrl(url.url(), this);
    }
}

void DolphinMainWindow::tabDropEvent(int tab, QDropEvent* event)
{
    const KUrl::List urls = KUrl::List::fromMimeData(event->mimeData());
    if (!urls.isEmpty() && tab != -1) {
        const DolphinView* view = m_viewTab.at(tab)->activeViewContainer()->view();

        QString error;
        DragAndDropHelper::dropUrls(view->rootItem(), view->url(), event, error);
        if (!error.isEmpty()) {
            activeViewContainer()->showMessage(error, DolphinViewContainer::Error);
        }
    }
}

void DolphinMainWindow::slotWriteStateChanged(bool isFolderWritable)
{
    newFileMenu()->setEnabled(isFolderWritable);
}

void DolphinMainWindow::openContextMenu(const QPoint& pos,
                                        const KFileItem& item,
                                        const KUrl& url,
                                        const QList<QAction*>& customActions)
{
    QWeakPointer<DolphinContextMenu> contextMenu = new DolphinContextMenu(this, pos, item, url);
    contextMenu.data()->setCustomActions(customActions);
    const DolphinContextMenu::Command command = contextMenu.data()->open();

    switch (command) {
    case DolphinContextMenu::OpenParentFolderInNewWindow: {
        KToolInvocation::self()->startServiceByStorageId("dolphin", QStringList() << item.url().upUrl().url(), this);
        break;
    }

    case DolphinContextMenu::OpenParentFolderInNewTab:
        openNewTab(item.url().upUrl());
        break;

    case DolphinContextMenu::None:
    default:
        break;
    }

    delete contextMenu.data();
}

void DolphinMainWindow::updateControlMenu()
{
    KMenu* menu = qobject_cast<KMenu*>(sender());
    Q_ASSERT(menu);

    // All actions get cleared by KMenu::clear(). The sub-menus are deleted
    // by connecting to the aboutToHide() signal from the parent-menu.
    menu->clear();

    KActionCollection* ac = actionCollection();

    // Add "Edit" actions
    bool added = addActionToMenu(ac->action(KStandardAction::name(KStandardAction::Undo)), menu);
    added |= addActionToMenu(ac->action(KStandardAction::name(KStandardAction::Find)), menu);
    added |= addActionToMenu(ac->action("select_all"), menu);
    added |= addActionToMenu(ac->action("invert_selection"), menu);

    if (added) {
        menu->addSeparator();
    }

    // Add "View" actions
    if (!GeneralSettings::showZoomSlider()) {
        addActionToMenu(ac->action(KStandardAction::name(KStandardAction::ZoomIn)), menu);
        addActionToMenu(ac->action(KStandardAction::name(KStandardAction::ZoomOut)), menu);
        menu->addSeparator();
    }

    added = addActionToMenu(ac->action("view_mode"), menu);
    added |= addActionToMenu(ac->action("sort"), menu);
    added |= addActionToMenu(ac->action("additional_info"), menu);
    added |= addActionToMenu(ac->action("show_preview"), menu);
    added |= addActionToMenu(ac->action("show_in_groups"), menu);
    added |= addActionToMenu(ac->action("show_hidden_files"), menu);

    if (added) {
        menu->addSeparator();
    }

    added = addActionToMenu(ac->action("split_view"), menu);
    added |= addActionToMenu(ac->action("reload"), menu);
    added |= addActionToMenu(ac->action("view_properties"), menu);
    if (added) {
        menu->addSeparator();
    }

    addActionToMenu(ac->action("panels"), menu);
    KMenu* locationBarMenu = new KMenu(i18nc("@action:inmenu", "Location Bar"), menu);
    locationBarMenu->addAction(ac->action("editable_location"));
    locationBarMenu->addAction(ac->action("replace_location"));
    menu->addMenu(locationBarMenu);

    menu->addSeparator();

    // Add "Go" menu
    KMenu* goMenu = new KMenu(i18nc("@action:inmenu", "Go"), menu);
    connect(menu, SIGNAL(aboutToHide()), goMenu, SLOT(deleteLater()));
    goMenu->addAction(ac->action(KStandardAction::name(KStandardAction::Back)));
    goMenu->addAction(ac->action(KStandardAction::name(KStandardAction::Forward)));
    goMenu->addAction(ac->action(KStandardAction::name(KStandardAction::Up)));
    goMenu->addAction(ac->action(KStandardAction::name(KStandardAction::Home)));
    goMenu->addAction(ac->action("closed_tabs"));
    menu->addMenu(goMenu);

    // Add "Tool" menu
    KMenu* toolsMenu = new KMenu(i18nc("@action:inmenu", "Tools"), menu);
    connect(menu, SIGNAL(aboutToHide()), toolsMenu, SLOT(deleteLater()));
    toolsMenu->addAction(ac->action("show_filter_bar"));
    toolsMenu->addAction(ac->action("open_terminal"));
    toolsMenu->addAction(ac->action("change_remote_encoding"));
    menu->addMenu(toolsMenu);

    // Add "Settings" menu entries
    addActionToMenu(ac->action(KStandardAction::name(KStandardAction::KeyBindings)), menu);
    addActionToMenu(ac->action(KStandardAction::name(KStandardAction::ConfigureToolbars)), menu);
    addActionToMenu(ac->action(KStandardAction::name(KStandardAction::Preferences)), menu);

    // Add "Help" menu
    KMenu* helpMenu = new KMenu(i18nc("@action:inmenu", "Help"), menu);
    connect(menu, SIGNAL(aboutToHide()), helpMenu, SLOT(deleteLater()));
    helpMenu->addAction(ac->action(KStandardAction::name(KStandardAction::HelpContents)));
    helpMenu->addAction(ac->action(KStandardAction::name(KStandardAction::WhatsThis)));
    helpMenu->addSeparator();
    helpMenu->addAction(ac->action(KStandardAction::name(KStandardAction::ReportBug)));
    helpMenu->addSeparator();
    helpMenu->addAction(ac->action(KStandardAction::name(KStandardAction::SwitchApplicationLanguage)));
    helpMenu->addSeparator();
    helpMenu->addAction(ac->action(KStandardAction::name(KStandardAction::AboutApp)));
    helpMenu->addAction(ac->action(KStandardAction::name(KStandardAction::AboutKDE)));
    menu->addMenu(helpMenu);

    menu->addSeparator();
    addActionToMenu(ac->action(KStandardAction::name(KStandardAction::ShowMenubar)), menu);
}

void DolphinMainWindow::updateToolBar()
{
    if (!menuBar()->isVisible()) {
        createControlButton();
    }
}

void DolphinMainWindow::slotControlButtonDeleted()
{
    m_controlButton = 0;
    m_updateToolBarTimer->start();
}

void DolphinMainWindow::slotPanelErrorMessage(const QString& error)
{
    activeViewContainer()->showMessage(error, DolphinViewContainer::Error);
}

void DolphinMainWindow::slotPlaceActivated(const KUrl& url)
{
    DolphinViewContainer* view = activeViewContainer();

    if (view->url() == url) {
        // We can end up here if the user clicked a device in the Places Panel
        // which had been unmounted earlier, see https://bugs.kde.org/show_bug.cgi?id=161385.
        reloadView();
    } else {
        changeUrl(url);
    }
}

void DolphinMainWindow::activeViewChanged()
{
    const DolphinTabPage* tabPage = m_viewTab.at(m_tabIndex);
    setActiveViewContainer(tabPage->activeViewContainer());
}

void DolphinMainWindow::setActiveViewContainer(DolphinViewContainer* viewContainer)
{
    Q_ASSERT(viewContainer);
    Q_ASSERT((viewContainer == m_viewTab.at(m_tabIndex)->primaryViewContainer()) ||
             (viewContainer == m_viewTab.at(m_tabIndex)->secondaryViewContainer()));
    if (m_activeViewContainer == viewContainer) {
        return;
    }

    if (m_activeViewContainer) {
        // Disconnect all signals between the old view container (container,
        // view and url navigator) and main window.
        m_activeViewContainer->disconnect(this);
        m_activeViewContainer->view()->disconnect(this);
        m_activeViewContainer->urlNavigator()->disconnect(this);
    }

    m_activeViewContainer = viewContainer;
    connectViewSignals(viewContainer);

    m_actionHandler->setCurrentView(viewContainer->view());

    updateHistory();
    updateEditActions();
    updatePasteAction();
    updateViewActions();
    updateGoActions();

    const KUrl url = m_activeViewContainer->url();
    setUrlAsCaption(url);
    m_tabBar->setTabText(m_tabIndex, squeezedText(tabName(url)));
    m_tabBar->setTabIcon(m_tabIndex, QIcon(KIO::pixmapForUrl(url)));

    emit urlChanged(url);
}

void DolphinMainWindow::setupActions()
{
    // setup 'File' menu
    m_newFileMenu = new DolphinNewFileMenu(actionCollection(), this);
    KMenu* menu = m_newFileMenu->menu();
    menu->setTitle(i18nc("@title:menu Create new folder, file, link, etc.", "Create New"));
    menu->setIcon(KIcon("document-new"));
    m_newFileMenu->setDelayed(false);
    connect(menu, SIGNAL(aboutToShow()),
            this, SLOT(updateNewMenu()));

    KAction* newWindow = actionCollection()->addAction("new_window");
    newWindow->setIcon(KIcon("window-new"));
    newWindow->setText(i18nc("@action:inmenu File", "New &Window"));
    newWindow->setShortcut(Qt::CTRL | Qt::Key_N);
    connect(newWindow, SIGNAL(triggered()), this, SLOT(openNewMainWindow()));

    KAction* newTab = actionCollection()->addAction("new_tab");
    newTab->setIcon(KIcon("tab-new"));
    newTab->setText(i18nc("@action:inmenu File", "New Tab"));
    newTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T, Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(newTab, SIGNAL(triggered()), this, SLOT(openNewTab()));

    KAction* closeTab = actionCollection()->addAction("close_tab");
    closeTab->setIcon(KIcon("tab-close"));
    closeTab->setText(i18nc("@action:inmenu File", "Close Tab"));
    closeTab->setShortcut(Qt::CTRL | Qt::Key_W);
    closeTab->setEnabled(false);
    connect(closeTab, SIGNAL(triggered()), this, SLOT(closeTab()));

    KStandardAction::quit(this, SLOT(quit()), actionCollection());

    // setup 'Edit' menu
    KStandardAction::undo(this,
                          SLOT(undo()),
                          actionCollection());

    // need to remove shift+del from cut action, else the shortcut for deletejob
    // doesn't work
    KAction* cut = KStandardAction::cut(this, SLOT(cut()), actionCollection());
    QKeySequence cutShortcut = cut->shortcut();
    static const int shiftdelete = (Qt::SHIFT | Qt::Key_Delete);
    if (cutShortcut[0] == shiftdelete) {
        cutShortcut = QKeySequence(cutShortcut[1]);
    }
    if (cutShortcut[0] == shiftdelete) {
        cutShortcut = QKeySequence();
    }
    cut->setShortcut(cutShortcut);
    KStandardAction::copy(this, SLOT(copy()), actionCollection());
    KAction* paste = KStandardAction::paste(this, SLOT(paste()), actionCollection());
    // The text of the paste-action is modified dynamically by Dolphin
    // (e. g. to "Paste One Folder"). To prevent that the size of the toolbar changes
    // due to the long text, the text "Paste" is used:
    paste->setIconText(i18nc("@action:inmenu Edit", "Paste"));

    KStandardAction::find(this, SLOT(find()), actionCollection());

    KAction* selectAll = actionCollection()->addAction("select_all");
    selectAll->setText(i18nc("@action:inmenu Edit", "Select All"));
    selectAll->setShortcut(Qt::CTRL | Qt::Key_A);
    connect(selectAll, SIGNAL(triggered()), this, SLOT(selectAll()));

    KAction* invertSelection = actionCollection()->addAction("invert_selection");
    invertSelection->setText(i18nc("@action:inmenu Edit", "Invert Selection"));
    invertSelection->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_A);
    connect(invertSelection, SIGNAL(triggered()), this, SLOT(invertSelection()));

    // setup 'View' menu
    // (note that most of it is set up in DolphinViewActionHandler)

    KAction* split = actionCollection()->addAction("split_view");
    split->setShortcut(Qt::Key_F3);
    connect(split, SIGNAL(triggered()), this, SLOT(toggleSplitView()));

    KAction* reload = actionCollection()->addAction("reload");
    reload->setText(i18nc("@action:inmenu View", "Reload"));
    reload->setShortcut(Qt::Key_F5);
    reload->setIcon(KIcon("view-refresh"));
    connect(reload, SIGNAL(triggered()), this, SLOT(reloadView()));

    KAction* stop = actionCollection()->addAction("stop");
    stop->setText(i18nc("@action:inmenu View", "Stop"));
    stop->setToolTip(i18nc("@info", "Stop loading"));
    stop->setIcon(KIcon("process-stop"));
    connect(stop, SIGNAL(triggered()), this, SLOT(stopLoading()));

    KToggleAction* editableLocation = actionCollection()->add<KToggleAction>("editable_location");
    editableLocation->setText(i18nc("@action:inmenu Navigation Bar", "Editable Location"));
    editableLocation->setShortcut(Qt::Key_F6);
    connect(editableLocation, SIGNAL(triggered()), this, SLOT(toggleEditLocation()));

    KAction* replaceLocation = actionCollection()->addAction("replace_location");
    replaceLocation->setText(i18nc("@action:inmenu Navigation Bar", "Replace Location"));
    replaceLocation->setShortcut(Qt::CTRL | Qt::Key_L);
    connect(replaceLocation, SIGNAL(triggered()), this, SLOT(replaceLocation()));

    // setup 'Go' menu
    KAction* backAction = KStandardAction::back(this, SLOT(goBack()), actionCollection());
    connect(backAction, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)), this, SLOT(goBack(Qt::MouseButtons)));
    QKeySequence backShortcut = backAction->shortcut();
    backShortcut = QKeySequence(backShortcut[0], Qt::Key_Backspace);
    backAction->setShortcut(backShortcut);

    DolphinRecentTabsMenu* recentTabsMenu = new DolphinRecentTabsMenu(this);
    actionCollection()->addAction("closed_tabs", recentTabsMenu);
    connect(this, SIGNAL(rememberClosedTab(KUrl,KUrl)),
            recentTabsMenu, SLOT(rememberClosedTab(KUrl,KUrl)));
    connect(recentTabsMenu, SIGNAL(restoreClosedTab(KUrl,KUrl)),
            this, SLOT(openNewActivatedTab(KUrl,KUrl)));

    KAction* forwardAction = KStandardAction::forward(this, SLOT(goForward()), actionCollection());
    connect(forwardAction, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)), this, SLOT(goForward(Qt::MouseButtons)));

    KAction* upAction = KStandardAction::up(this, SLOT(goUp()), actionCollection());
    connect(upAction, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)), this, SLOT(goUp(Qt::MouseButtons)));

    KAction* homeAction = KStandardAction::home(this, SLOT(goHome()), actionCollection());
    connect(homeAction, SIGNAL(triggered(Qt::MouseButtons,Qt::KeyboardModifiers)), this, SLOT(goHome(Qt::MouseButtons)));

    // setup 'Tools' menu
    KAction* showFilterBar = actionCollection()->addAction("show_filter_bar");
    showFilterBar->setText(i18nc("@action:inmenu Tools", "Show Filter Bar"));
    showFilterBar->setIcon(KIcon("view-filter"));
    showFilterBar->setShortcut(Qt::CTRL | Qt::Key_I);
    connect(showFilterBar, SIGNAL(triggered()), this, SLOT(showFilterBar()));

    KAction* openTerminal = actionCollection()->addAction("open_terminal");
    openTerminal->setText(i18nc("@action:inmenu Tools", "Open Terminal"));
    openTerminal->setIcon(KIcon("utilities-terminal"));
    openTerminal->setShortcut(Qt::SHIFT | Qt::Key_F4);
    connect(openTerminal, SIGNAL(triggered()), this, SLOT(openTerminal()));

    // setup 'Settings' menu
    KToggleAction* showMenuBar = KStandardAction::showMenubar(0, 0, actionCollection());
    connect(showMenuBar, SIGNAL(triggered(bool)),                   // Fixes #286822
            this, SLOT(toggleShowMenuBar()), Qt::QueuedConnection);
    KStandardAction::preferences(this, SLOT(editSettings()), actionCollection());

    // not in menu actions
    const QKeySequence nextTabKeys = QKeySequence(
        KStandardShortcut::tabNext()[0],
        Qt::CTRL | Qt::Key_Tab
    );

    const QKeySequence prevTabKeys = QKeySequence(
        KStandardShortcut::tabPrev()[0],
        Qt::CTRL | Qt::SHIFT | Qt::Key_Tab
    );

    KAction* activateNextTab = actionCollection()->addAction("activate_next_tab");
    activateNextTab->setIconText(i18nc("@action:inmenu", "Next Tab"));
    activateNextTab->setText(i18nc("@action:inmenu", "Activate Next Tab"));
    activateNextTab->setEnabled(false);
    connect(activateNextTab, SIGNAL(triggered()), SLOT(activateNextTab()));
    activateNextTab->setShortcut(QApplication::isRightToLeft() ? prevTabKeys : nextTabKeys);

    KAction* activatePrevTab = actionCollection()->addAction("activate_prev_tab");
    activatePrevTab->setIconText(i18nc("@action:inmenu", "Previous Tab"));
    activatePrevTab->setText(i18nc("@action:inmenu", "Activate Previous Tab"));
    activatePrevTab->setEnabled(false);
    connect(activatePrevTab, SIGNAL(triggered()), SLOT(activatePrevTab()));
    activatePrevTab->setShortcut(QApplication::isRightToLeft() ? nextTabKeys : prevTabKeys);

    // for context menu
    KAction* openInNewTab = actionCollection()->addAction("open_in_new_tab");
    openInNewTab->setText(i18nc("@action:inmenu", "Open in New Tab"));
    openInNewTab->setIcon(KIcon("tab-new"));
    connect(openInNewTab, SIGNAL(triggered()), this, SLOT(openInNewTab()));

    KAction* openInNewTabs = actionCollection()->addAction("open_in_new_tabs");
    openInNewTabs->setText(i18nc("@action:inmenu", "Open in New Tabs"));
    openInNewTabs->setIcon(KIcon("tab-new"));
    connect(openInNewTabs, SIGNAL(triggered()), this, SLOT(openInNewTab()));

    KAction* openInNewWindow = actionCollection()->addAction("open_in_new_window");
    openInNewWindow->setText(i18nc("@action:inmenu", "Open in New Window"));
    openInNewWindow->setIcon(KIcon("window-new"));
    connect(openInNewWindow, SIGNAL(triggered()), this, SLOT(openInNewWindow()));
}

void DolphinMainWindow::setupDockWidgets()
{
    const bool lock = GeneralSettings::lockPanels();

    KDualAction* lockLayoutAction = actionCollection()->add<KDualAction>("lock_panels");
    lockLayoutAction->setActiveText(i18nc("@action:inmenu Panels", "Unlock Panels"));
    lockLayoutAction->setActiveIcon(KIcon("object-unlocked"));
    lockLayoutAction->setInactiveText(i18nc("@action:inmenu Panels", "Lock Panels"));
    lockLayoutAction->setInactiveIcon(KIcon("object-locked"));
    lockLayoutAction->setActive(lock);
    connect(lockLayoutAction, SIGNAL(triggered()), this, SLOT(togglePanelLockState()));

    // Setup "Information"
    DolphinDockWidget* infoDock = new DolphinDockWidget(i18nc("@title:window", "Information"));
    infoDock->setLocked(lock);
    infoDock->setObjectName("infoDock");
    infoDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    Panel* infoPanel = new InformationPanel(infoDock);
    infoPanel->setCustomContextMenuActions(QList<QAction*>() << lockLayoutAction);
    connect(infoPanel, SIGNAL(urlActivated(KUrl)), this, SLOT(handleUrl(KUrl)));
    infoDock->setWidget(infoPanel);

    QAction* infoAction = infoDock->toggleViewAction();
    createPanelAction(KIcon("dialog-information"), Qt::Key_F11, infoAction, "show_information_panel");

    addDockWidget(Qt::RightDockWidgetArea, infoDock);
    connect(this, SIGNAL(urlChanged(KUrl)),
            infoPanel, SLOT(setUrl(KUrl)));
    connect(this, SIGNAL(selectionChanged(KFileItemList)),
            infoPanel, SLOT(setSelection(KFileItemList)));
    connect(this, SIGNAL(requestItemInfo(KFileItem)),
            infoPanel, SLOT(requestDelayedItemInfo(KFileItem)));

    // Setup "Terminal"
    DolphinDockWidget* terminalDock = new DolphinDockWidget(i18nc("@title:window Shell terminal", "Terminal"));
    terminalDock->setLocked(lock);
    terminalDock->setObjectName("terminalDock");
    terminalDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    Panel* terminalPanel = new TerminalPanel(terminalDock);
    terminalPanel->setCustomContextMenuActions(QList<QAction*>() << lockLayoutAction);
    terminalDock->setWidget(terminalPanel);

    connect(terminalPanel, SIGNAL(hideTerminalPanel()), terminalDock, SLOT(hide()));
    connect(terminalPanel, SIGNAL(changeUrl(KUrl)), this, SLOT(slotTerminalDirectoryChanged(KUrl)));
    connect(terminalDock, SIGNAL(visibilityChanged(bool)),
            terminalPanel, SLOT(dockVisibilityChanged()));

    QAction* terminalAction = terminalDock->toggleViewAction();
    createPanelAction(KIcon("utilities-terminal"), Qt::Key_F4, terminalAction, "show_terminal_panel");

    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
    connect(this, SIGNAL(urlChanged(KUrl)),
            terminalPanel, SLOT(setUrl(KUrl)));

    if (GeneralSettings::version() < CurrentDolphinVersion) {
        infoDock->hide();
        terminalDock->hide();
    }

    // Setup "Places"
    DolphinDockWidget* placesDock = new DolphinDockWidget(i18nc("@title:window", "Places"));
    placesDock->setLocked(lock);
    placesDock->setObjectName("placesDock");
    placesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    PlacesPanel* placesPanel = new PlacesPanel(placesDock);
    placesPanel->setCustomContextMenuActions(QList<QAction*>() << lockLayoutAction);
    placesDock->setWidget(placesPanel);

    QAction* placesAction = placesDock->toggleViewAction();
    createPanelAction(KIcon("bookmarks"), Qt::Key_F9, placesAction, "show_places_panel");

    addDockWidget(Qt::LeftDockWidgetArea, placesDock);
    connect(placesPanel, SIGNAL(placeActivated(KUrl)),
            this, SLOT(slotPlaceActivated(KUrl)));
    connect(placesPanel, SIGNAL(placeMiddleClicked(KUrl)),
            this, SLOT(openNewTab(KUrl)));
    connect(placesPanel, SIGNAL(errorMessage(QString)),
            this, SLOT(slotPanelErrorMessage(QString)));
    connect(this, SIGNAL(urlChanged(KUrl)),
            placesPanel, SLOT(setUrl(KUrl)));
    connect(placesDock, SIGNAL(visibilityChanged(bool)),
            this, SLOT(slotPlacesPanelVisibilityChanged(bool)));
    connect(this, SIGNAL(settingsChanged()),
	    placesPanel, SLOT(readSettings()));

    // Add actions into the "Panels" menu
    KActionMenu* panelsMenu = new KActionMenu(i18nc("@action:inmenu View", "Panels"), this);
    actionCollection()->addAction("panels", panelsMenu);
    panelsMenu->setDelayed(false);
    const KActionCollection* ac = actionCollection();
    panelsMenu->addAction(ac->action("show_places_panel"));
    panelsMenu->addAction(ac->action("show_information_panel"));
    panelsMenu->addAction(ac->action("show_terminal_panel"));
    panelsMenu->addSeparator();
    panelsMenu->addAction(lockLayoutAction);
}

void DolphinMainWindow::updateEditActions()
{
    const KFileItemList list = m_activeViewContainer->view()->selectedItems();
    if (list.isEmpty()) {
        stateChanged("has_no_selection");
    } else {
        stateChanged("has_selection");

        KActionCollection* col = actionCollection();
        QAction* renameAction      = col->action("rename");
        QAction* moveToTrashAction = col->action("move_to_trash");
        QAction* deleteAction      = col->action("delete");
        QAction* cutAction         = col->action(KStandardAction::name(KStandardAction::Cut));
        QAction* deleteWithTrashShortcut = col->action("delete_shortcut"); // see DolphinViewActionHandler

        KFileItemListProperties capabilities(list);
        const bool enableMoveToTrash = capabilities.isLocal() && capabilities.supportsMoving();

        renameAction->setEnabled(capabilities.supportsMoving());
        moveToTrashAction->setEnabled(enableMoveToTrash);
        deleteAction->setEnabled(capabilities.supportsDeleting());
        deleteWithTrashShortcut->setEnabled(capabilities.supportsDeleting() && !enableMoveToTrash);
        cutAction->setEnabled(capabilities.supportsMoving());
    }
}

void DolphinMainWindow::updateViewActions()
{
    m_actionHandler->updateViewActions();

    QAction* showFilterBarAction = actionCollection()->action("show_filter_bar");
    showFilterBarAction->setChecked(m_activeViewContainer->isFilterBarVisible());

    updateSplitAction();

    QAction* editableLocactionAction = actionCollection()->action("editable_location");
    const KUrlNavigator* urlNavigator = m_activeViewContainer->urlNavigator();
    editableLocactionAction->setChecked(urlNavigator->isUrlEditable());
}

void DolphinMainWindow::updateGoActions()
{
    QAction* goUpAction = actionCollection()->action(KStandardAction::name(KStandardAction::Up));
    const KUrl currentUrl = m_activeViewContainer->url();
    goUpAction->setEnabled(currentUrl.upUrl() != currentUrl);
}

void DolphinMainWindow::createControlButton()
{
    if (m_controlButton) {
        return;
    }
    Q_ASSERT(!m_controlButton);

    m_controlButton = new QToolButton(this);
    m_controlButton->setIcon(KIcon("applications-system"));
    m_controlButton->setText(i18nc("@action", "Control"));
    m_controlButton->setPopupMode(QToolButton::InstantPopup);
    m_controlButton->setToolButtonStyle(toolBar()->toolButtonStyle());

    KMenu* controlMenu = new KMenu(m_controlButton);
    connect(controlMenu, SIGNAL(aboutToShow()), this, SLOT(updateControlMenu()));

    m_controlButton->setMenu(controlMenu);

    toolBar()->addWidget(m_controlButton);
    connect(toolBar(), SIGNAL(iconSizeChanged(QSize)),
            m_controlButton, SLOT(setIconSize(QSize)));
    connect(toolBar(), SIGNAL(toolButtonStyleChanged(Qt::ToolButtonStyle)),
            m_controlButton, SLOT(setToolButtonStyle(Qt::ToolButtonStyle)));

    // The added widgets are owned by the toolbar and may get deleted when e.g. the toolbar
    // gets edited. In this case we must add them again. The adding is done asynchronously by
    // m_updateToolBarTimer.
    connect(m_controlButton, SIGNAL(destroyed()), this, SLOT(slotControlButtonDeleted()));
    m_updateToolBarTimer = new QTimer(this);
    m_updateToolBarTimer->setInterval(500);
    connect(m_updateToolBarTimer, SIGNAL(timeout()), this, SLOT(updateToolBar()));
}

void DolphinMainWindow::deleteControlButton()
{
    delete m_controlButton;
    m_controlButton = 0;

    delete m_updateToolBarTimer;
    m_updateToolBarTimer = 0;
}

bool DolphinMainWindow::addActionToMenu(QAction* action, KMenu* menu)
{
    Q_ASSERT(action);
    Q_ASSERT(menu);

    const KToolBar* toolBarWidget = toolBar();
    foreach (const QWidget* widget, action->associatedWidgets()) {
        if (widget == toolBarWidget) {
            return false;
        }
    }

    menu->addAction(action);
    return true;
}

void DolphinMainWindow::refreshViews()
{
    foreach (DolphinTabPage* tabPage, m_viewTab) {
        tabPage->refreshViews();
    }

    if (GeneralSettings::modifiedStartupSettings()) {
        // The startup settings have been changed by the user (see bug #254947).
        // Synchronize the split-view setting with the active view:
        const bool splitView = GeneralSettings::splitView();
        m_viewTab.at(m_tabIndex)->setSplitViewEnabled(splitView);
        updateSplitAction();
    }

    emit settingsChanged();
}

void DolphinMainWindow::clearStatusBar()
{
    m_activeViewContainer->statusBar()->resetToDefaultText();
}

void DolphinMainWindow::connectViewSignals(DolphinViewContainer* container)
{
    connect(container, SIGNAL(showFilterBarChanged(bool)),
            this, SLOT(updateFilterBarAction(bool)));
    connect(container, SIGNAL(writeStateChanged(bool)),
            this, SLOT(slotWriteStateChanged(bool)));

    const DolphinView* view = container->view();
    connect(view, SIGNAL(selectionChanged(KFileItemList)),
            this, SLOT(slotSelectionChanged(KFileItemList)));
    connect(view, SIGNAL(requestItemInfo(KFileItem)),
            this, SLOT(slotRequestItemInfo(KFileItem)));
    connect(view, SIGNAL(tabRequested(KUrl)),
            this, SLOT(openNewTab(KUrl)));
    connect(view, SIGNAL(requestContextMenu(QPoint,KFileItem,KUrl,QList<QAction*>)),
            this, SLOT(openContextMenu(QPoint,KFileItem,KUrl,QList<QAction*>)));
    connect(view, SIGNAL(directoryLoadingStarted()),
            this, SLOT(enableStopAction()));
    connect(view, SIGNAL(directoryLoadingCompleted()),
            this, SLOT(disableStopAction()));

    const KUrlNavigator* navigator = container->urlNavigator();
    connect(navigator, SIGNAL(urlChanged(KUrl)),
            this, SLOT(changeUrl(KUrl)));
    connect(navigator, SIGNAL(historyChanged()),
            this, SLOT(updateHistory()));
    connect(navigator, SIGNAL(editableStateChanged(bool)),
            this, SLOT(slotEditableStateChanged(bool)));
    connect(navigator, SIGNAL(tabRequested(KUrl)),
            this, SLOT(openNewTab(KUrl)));
}

void DolphinMainWindow::updateSplitAction()
{
    QAction* splitAction = actionCollection()->action("split_view");
    const DolphinTabPage* tabPage = m_viewTab.at(m_tabIndex);
    if (tabPage->splitViewEnabled()) {
        if (tabPage->primaryViewActive()) {
            splitAction->setText(i18nc("@action:intoolbar Close left view", "Close"));
            splitAction->setToolTip(i18nc("@info", "Close left view"));
            splitAction->setIcon(KIcon("view-left-close"));
        } else {
            splitAction->setText(i18nc("@action:intoolbar Close right view", "Close"));
            splitAction->setToolTip(i18nc("@info", "Close right view"));
            splitAction->setIcon(KIcon("view-right-close"));
        }
    } else {
        splitAction->setText(i18nc("@action:intoolbar Split view", "Split"));
        splitAction->setToolTip(i18nc("@info", "Split view"));
        splitAction->setIcon(KIcon("view-right-new"));
    }
}

QString DolphinMainWindow::tabName(const KUrl& url) const
{
    QString name;
    if (url.equals(KUrl("file:///"))) {
        name = '/';
    } else {
        name = url.fileName();
        if (name.isEmpty()) {
            name = url.protocol();
        } else {
            // Make sure that a '&' inside the directory name is displayed correctly
            // and not misinterpreted as a keyboard shortcut in QTabBar::setTabText()
            name.replace('&', "&&");
        }
    }
    return name;
}

void DolphinMainWindow::setUrlAsCaption(const KUrl& url)
{
    QString caption;
    if (!url.isLocalFile()) {
        caption.append(url.protocol() + " - ");
        if (url.hasHost()) {
            caption.append(url.host() + " - ");
        }
    }

    const QString fileName = url.fileName().isEmpty() ? "/" : url.fileName();
    caption.append(fileName);

    setCaption(caption);
}

QString DolphinMainWindow::squeezedText(const QString& text) const
{
    const QFontMetrics fm = fontMetrics();
    return fm.elidedText(text, Qt::ElideMiddle, fm.maxWidth() * 10);
}

void DolphinMainWindow::createPanelAction(const KIcon& icon,
                                          const QKeySequence& shortcut,
                                          QAction* dockAction,
                                          const QString& actionName)
{
    KAction* panelAction = actionCollection()->addAction(actionName);
    panelAction->setCheckable(true);
    panelAction->setChecked(dockAction->isChecked());
    panelAction->setText(dockAction->text());
    panelAction->setIcon(icon);
    panelAction->setShortcut(shortcut);

    connect(panelAction, SIGNAL(triggered()), dockAction, SLOT(trigger()));
    connect(dockAction, SIGNAL(toggled(bool)), panelAction, SLOT(setChecked(bool)));
}

DolphinMainWindow::UndoUiInterface::UndoUiInterface() :
    KIO::FileUndoManager::UiInterface()
{
}

DolphinMainWindow::UndoUiInterface::~UndoUiInterface()
{
}

void DolphinMainWindow::UndoUiInterface::jobError(KIO::Job* job)
{
    DolphinMainWindow* mainWin= qobject_cast<DolphinMainWindow *>(parentWidget());
    if (mainWin) {
        DolphinViewContainer* container = mainWin->activeViewContainer();
        container->showMessage(job->errorString(), DolphinViewContainer::Error);
    } else {
        KIO::FileUndoManager::UiInterface::jobError(job);
    }
}

#include "moc_dolphinmainwindow.cpp"
