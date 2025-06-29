/* This file is part of KDE
    Copyright (c) 2007 David Faure <faure@kde.org>

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

#include "konqpopupmenutest.h"
#include <kconfiggroup.h>
#include <kstandarddirs.h>
#include <kbookmarkmanager.h>
#include <assert.h>
#include "qtest_kde.h"
#include <QDir>
#include <knewfilemenu.h>
#include <kdebug.h>
#include <kfileitemlistproperties.h>

QTEST_KDEMAIN(KonqPopupMenuTest, GUI)

KonqPopupMenuTest::KonqPopupMenuTest()
    : m_actionCollection(this)
{
}

static QStringList extractActionNames(const QMenu& menu)
{
    QString lastObjectName;
    QStringList ret;
    bool lastIsSeparator = false;
    foreach (const QAction* action, menu.actions()) {
        if (action->isSeparator()) {
            if (!lastIsSeparator) // Qt gets rid of duplicate separators, so we should too
                ret.append("separator");
            lastIsSeparator = true;
        } else {
            lastIsSeparator = false;
            //qDebug() << action->objectName() << action->metaObject()->className() << action->text();
            const QString objectName = action->objectName();
            if (objectName.isEmpty()) {
                if (action->menu()) // if this fails, then we have an unnamed action somewhere...
                    ret.append("submenu");
                else {
                    ret.append("UNNAMED " + action->text());
                }
            } else {
                if (objectName == "menuaction" // a single service-menu action, or a service-menu submenu: skip; too variable.
                    || objectName == "actions_submenu") {
                } else if (objectName == "openWith_submenu") {
                    ret.append("openwith");
                } else if (objectName == "openwith_browse" && lastObjectName == "openwith") {
                    // We had "open with foo" followed by openwith_browse, all is well.
                    // The expected lists only say "openwith" so that they work in both cases
                    // -> skip the browse action.
                } else {
                    ret.append(objectName);
                }
            }
        }
        lastObjectName = action->objectName();
    }
    return ret;

}

void KonqPopupMenuTest::initTestCase()
{
    KSharedConfig::Ptr dolphin = KSharedConfig::openConfig("dolphinrc");
    KConfigGroup(dolphin, "General").writeEntry("ShowCopyMoveMenu", true);

    m_thisDirectoryItem = KFileItem(QDir::currentPath());
    m_fileItem = KFileItem(KUrl(QDir::currentPath() + "/Makefile"));
    m_linkItem = KFileItem(KUrl("http://www.kde.org/foo"));
    m_subDirItem = KFileItem(KUrl(QDir::currentPath() + "/CMakeFiles"));
    m_cut = KStandardAction::cut(0, 0, this);
    m_actionCollection.addAction("cut", m_cut);
    m_copy = KStandardAction::copy(0, 0, this);
    m_actionCollection.addAction("copy", m_copy);
    m_paste = KStandardAction::paste(0, 0, this);
    m_actionCollection.addAction("paste", m_paste);
    m_pasteTo = KStandardAction::paste(0, 0, this);
    m_actionCollection.addAction("pasteto", m_pasteTo);
    m_back = new QAction(this);
    m_actionCollection.addAction("go_back", m_back);
    m_forward = new QAction(this);
    m_actionCollection.addAction("go_forward", m_forward);
    m_up = new QAction(this);
    m_actionCollection.addAction("go_up", m_up);
    m_reload = new QAction(this);
    m_actionCollection.addAction("reload", m_reload);
    m_properties = new QAction(this);
    m_actionCollection.addAction("properties", m_properties);

    m_tabHandlingActions = new QActionGroup(this);
    m_newWindow = new QAction(m_tabHandlingActions);
    m_actionCollection.addAction("openInNewWindow", m_newWindow);
    m_newTab = new QAction(m_tabHandlingActions);
    m_actionCollection.addAction("openInNewTab", m_newTab);
    QAction* separator = new QAction(m_tabHandlingActions);
    separator->setSeparator(true);
    QCOMPARE(m_tabHandlingActions->actions().count(), 3);

    m_previewActions = new QActionGroup(this);
    m_preview1 = new QAction(m_previewActions);
    m_actionCollection.addAction("preview1", m_preview1);
    m_preview2 = new QAction(m_previewActions);
    m_actionCollection.addAction("preview2", m_preview2);

    m_fileEditActions = new QActionGroup(this);
    m_rename = new QAction(m_fileEditActions);
    m_actionCollection.addAction("rename", m_rename);
    m_trash = new QAction(m_fileEditActions);
    m_actionCollection.addAction("trash", m_trash);

    m_htmlEditActions = new QActionGroup(this);
    // TODO use m_htmlEditActions like in khtml (see khtml_popupmenu.rc)

    m_linkActions = new QActionGroup(this);
    QAction* saveLinkAs = new QAction(m_linkActions);
    m_actionCollection.addAction("savelinkas", saveLinkAs);
    QAction* copyLinkLocation = new QAction(m_linkActions);
    m_actionCollection.addAction("copylinklocation", copyLinkLocation);
    // TODO there's a whole bunch of things for frames, and for images, see khtml_popupmenu.rc

    m_partActions = new QActionGroup(this);
    separator = new QAction(m_partActions);
    separator->setSeparator(true);
    m_partActions->addAction(separator); // we better start with a separator
    QAction* viewDocumentSource = new QAction(m_partActions);
    m_actionCollection.addAction("viewDocumentSource", viewDocumentSource);

    m_newMenu = new KNewFileMenu(&m_actionCollection, "newmenu", 0);

    // Check if extractActionNames works
    QMenu popup;
    popup.addAction(m_back);
    QMenu* subMenu = new QMenu(&popup);
    popup.addMenu(subMenu);
    subMenu->addAction(m_up);
    QStringList actions = extractActionNames(popup);
    kDebug() << actions;
    QCOMPARE(actions, QStringList() << "go_back" << "submenu");
}

void KonqPopupMenuTest::testFile()
{
    KFileItemList itemList;
    itemList << m_fileItem;
    KUrl viewUrl = QDir::currentPath();
    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowProperties;
    beflags |= KonqPopupMenu::ShowReload;
    beflags |= KonqPopupMenu::ShowUrlOperations;

    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("tabhandling", m_tabHandlingActions->actions());
    actionGroups.insert("editactions", m_fileEditActions->actions());
    actionGroups.insert("preview", QList<QAction *>() << m_preview1);

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, 0 /*bookmark manager*/, actionGroups);

    QStringList actions = extractActionNames(popup);
    kDebug() << actions;
    QStringList expectedActions;
    expectedActions << "openInNewWindow" << "openInNewTab" << "separator"
                    << "cut" << "copy" << "rename" << "trash" << "separator"
                    << "openwith"
                    << "preview1";
    if (!KStandardDirs::locate("services", "ServiceMenus/ark_addtoservicemenu.desktop").isEmpty())
        expectedActions << "services_submenu";
    expectedActions << "separator";
    expectedActions << "copyTo_submenu" << "moveTo_submenu" << "separator";
    // (came from arkplugin) << "compress"
    expectedActions << "properties";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testFileInReadOnlyDirectory()
{
    static const QString notwritablefile = QString::fromLatin1("/etc/passwd");
    QFileInfo fileinfo(notwritablefile);
    if (fileinfo.permission(QFile::WriteUser)) {
        QSKIP("/etc/passwd file is writable", SkipSingle);
    }

    KFileItemList itemList;
    itemList << KFileItem(KUrl(notwritablefile));

    KFileItemListProperties capabilities(itemList);
    QVERIFY(!capabilities.supportsMoving());

    KUrl viewUrl("/etc");
    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowProperties;
    beflags |= KonqPopupMenu::ShowReload;
    beflags |= KonqPopupMenu::ShowUrlOperations;

    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("tabhandling", m_tabHandlingActions->actions());
    // DolphinPart doesn't add rename/trash when supportsMoving is false
    // Maybe we should test dolphinpart directly :)
    //actionGroups.insert("editactions", m_fileEditActions->actions());
    actionGroups.insert("preview", QList<QAction *>() << m_preview1);

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, 0 /*bookmark manager*/, actionGroups);

    QStringList actions = extractActionNames(popup);
    actions.removeAll("services_submenu");
    kDebug() << actions;
    QStringList expectedActions;
    expectedActions << "openInNewWindow" << "openInNewTab" << "separator"
                    << "copy" << "separator"
                    << "openwith"
                    << "preview1";
    expectedActions << "separator";
    expectedActions << "copyTo_submenu" << "separator";
    expectedActions << "properties";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testFilePreviewSubMenu()
{
    // Same as testFile, but this time the "preview" action group has more than one action
    KFileItemList itemList;
    itemList << m_fileItem;
    KUrl viewUrl = QDir::currentPath();
    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowProperties;
    beflags |= KonqPopupMenu::ShowReload;
    beflags |= KonqPopupMenu::ShowUrlOperations;

    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("tabhandling", m_tabHandlingActions->actions());
    actionGroups.insert("editactions", m_fileEditActions->actions());
    actionGroups.insert("preview", m_previewActions->actions());

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, 0 /*bookmark manager*/, actionGroups);

    QStringList actions = extractActionNames(popup);
    kDebug() << actions;
    QStringList expectedActions;
    expectedActions << "openInNewWindow" << "openInNewTab" << "separator"
                    << "cut" << "copy" << "rename" << "trash" << "separator"
                    << "openwith"
                    << "preview_submenu";
    if (!KStandardDirs::locate("services", "ServiceMenus/ark_addtoservicemenu.desktop").isEmpty())
        expectedActions << "services_submenu";
    expectedActions << "separator";
    expectedActions << "copyTo_submenu" << "moveTo_submenu" << "separator";
    expectedActions << "properties";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testSubDirectory()
{
    KFileItemList itemList;
    itemList << m_subDirItem;
    KUrl viewUrl = QDir::currentPath();
    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowProperties;
    beflags |= KonqPopupMenu::ShowUrlOperations;

    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("tabhandling", m_tabHandlingActions->actions());
    actionGroups.insert("editactions", m_fileEditActions->actions());
    actionGroups.insert("preview", m_previewActions->actions());

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, 0 /*bookmark manager*/, actionGroups);
    QStringList actions = extractActionNames(popup);
    actions.removeAll("services_submenu");
    kDebug() << actions;
    QStringList expectedActions;
    expectedActions << "openInNewWindow" << "openInNewTab" << "separator"
                    << "cut" << "copy" << "pasteto" << "rename" << "trash" << "separator"
                    << "openwith"
                    << "preview_submenu";
    expectedActions << "separator";
    expectedActions << "copyTo_submenu" << "moveTo_submenu" << "separator";
    expectedActions << "properties";
    expectedActions << "separator" << "share";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testViewDirectory()
{
    KFileItemList itemList;
    itemList << m_thisDirectoryItem;
    KUrl viewUrl = m_thisDirectoryItem.url();

    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowNavigationItems;
    beflags |= KonqPopupMenu::ShowUp;
    beflags |= KonqPopupMenu::ShowCreateDirectory;
    beflags |= KonqPopupMenu::ShowUrlOperations;
    beflags |= KonqPopupMenu::ShowProperties;

    // doTabHandling = !openedForViewURL && ... So we don't add tabhandling here
    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("preview", m_previewActions->actions());

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, 0 /*bookmark manager*/, actionGroups);

    QStringList actions = extractActionNames(popup);
    actions.removeAll("services_submenu");
    qDebug() << actions;
    QStringList expectedActions;
    expectedActions << "newmenu" << "separator"
                    << "go_up" << "go_back" << "go_forward" << "separator"
                    << "paste" << "separator"
                    << "openwith"
                    << "preview_submenu";
    expectedActions << "separator";
    expectedActions << "copyTo_submenu" << "moveTo_submenu" << "separator";
    expectedActions << "properties";
    expectedActions << "separator" << "share";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testViewReadOnlyDirectory()
{
    static const QString notreadabledir = QDir::rootPath();
    QFileInfo dirinfo(notreadabledir);
    if (dirinfo.permission(QFile::ReadUser)) {
        QSKIP("/root directory is readable", SkipSingle);
    }

    KFileItem rootItem(notreadabledir);
    KFileItemList itemList;
    itemList << rootItem;
    KUrl viewUrl = rootItem.url();

    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowNavigationItems;
    beflags |= KonqPopupMenu::ShowUp;
    beflags |= KonqPopupMenu::ShowCreateDirectory;
    beflags |= KonqPopupMenu::ShowUrlOperations;
    beflags |= KonqPopupMenu::ShowProperties;

    // doTabHandling = !openedForViewURL && ... So we don't add tabhandling here
    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("preview", m_previewActions->actions());

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, 0 /*bookmark manager*/, actionGroups);

    QStringList actions = extractActionNames(popup);
    qDebug() << actions;
    actions.removeAll("services_submenu");
    QStringList expectedActions;
    expectedActions << "go_up" << "go_back" << "go_forward" << "separator"
                    // << "paste" // no paste since readonly
                    << "openwith"
                    << "preview_submenu";
    expectedActions << "separator";
    expectedActions << "copyTo_submenu" << "separator"; // no moveTo_submenu, since readonly
    expectedActions << "properties";
    expectedActions << "separator" << "share";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testHtmlLink()
{
    KFileItemList itemList;
    itemList << m_linkItem;
    //KUrl viewUrl = m_fileItem.url();
    KUrl viewUrl("http://www.kde.org");

    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowBookmark;
    beflags |= KonqPopupMenu::ShowReload;
    beflags |= KonqPopupMenu::IsLink;

    KonqPopupMenu::ActionGroupMap actionGroups;
    actionGroups.insert("tabhandling", m_tabHandlingActions->actions());
    actionGroups.insert("preview", m_previewActions->actions());
    actionGroups.insert("editactions", m_htmlEditActions->actions());
    actionGroups.insert("linkactions", m_linkActions->actions());
    actionGroups.insert("partactions", m_partActions->actions());

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, KBookmarkManager::userBookmarksManager(), actionGroups);

    QStringList actions = extractActionNames(popup);
    kDebug() << actions;
    QStringList expectedActions;
    expectedActions << "openInNewWindow" << "openInNewTab" << "separator"
                    << "bookmark_add" << "savelinkas" << "copylinklocation"
                    << "separator"
                    << "openwith"
                    << "preview_submenu"
                    << "separator"
                    << "viewDocumentSource";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}

void KonqPopupMenuTest::testHtmlPage()
{
    KFileItemList itemList;
    itemList << m_linkItem;
    KUrl viewUrl = m_linkItem.url();

    KonqPopupMenu::PopupFlags beflags = KonqPopupMenu::NoPlugins;
    beflags |= KonqPopupMenu::ShowBookmark;
    beflags |= KonqPopupMenu::ShowReload;
    beflags |= KonqPopupMenu::ShowNavigationItems;

    KonqPopupMenu::ActionGroupMap actionGroups;
    // doTabHandling = !openedForViewURL && ... So we don't add tabhandling here
    // TODO we could just move that logic to KonqPopupMenu...
    //actionGroups.insert("tabhandling", m_tabHandlingActions->actions());
    actionGroups.insert("preview", m_previewActions->actions());
    actionGroups.insert("editactions", m_htmlEditActions->actions());
    //actionGroups.insert("linkactions", m_linkActions->actions());
    QAction* security = new QAction(m_partActions);
    m_actionCollection.addAction("security", security);
    QAction* setEncoding = new QAction(m_partActions);
    m_actionCollection.addAction("setEncoding", setEncoding);
    actionGroups.insert("partactions", m_partActions->actions());

    KonqPopupMenu popup(itemList, viewUrl, m_actionCollection, m_newMenu, beflags,
                        0 /*parent*/, KBookmarkManager::userBookmarksManager(), actionGroups);

    QStringList actions = extractActionNames(popup);
    kDebug() << actions;
    QStringList expectedActions;
    expectedActions << "go_back" << "go_forward" << "reload" << "separator"
                    << "bookmark_add"
                    << "separator"
                    << "openwith"
                    << "preview_submenu"
                    << "separator"
                    // << TODO "stopanimations"
                    << "viewDocumentSource" << "security" << "setEncoding";
    kDebug() << "Expected:" << expectedActions;
    QCOMPARE(actions, expectedActions);
}


// TODO test ShowReload (khtml passes it, but not the file views. Maybe show it if "not a directory" or "not local")

// (because file viewers don't react on changes, and remote things don't notify) -- then get rid of ShowReload.

// TODO test ShowBookmark. Probably the same logic?
// TODO separate filemanager and webbrowser bookmark managers, too (share file bookmarks with file dialog)

// TODO test text selection actions in khtml

// TODO trash:/ tests

// TODO test NoDeletion part flag
