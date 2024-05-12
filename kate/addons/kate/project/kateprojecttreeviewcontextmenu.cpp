/*  This file is part of the Kate project.
 *
 *  Copyright (C) 2013 Dominik Haumann <dhaumann.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kateprojecttreeviewcontextmenu.h"

#include <KMimeType>
#include <KMimeTypeTrader>
#include <KStandardDirs>
#include <QMenu>
#include <QFileInfo>
#include <QDir>
#include <KToolInvocation>
#include <KIcon>
#include <QProcess>
#include <QApplication>
#include <QClipboard>

KateProjectTreeViewContextMenu::KateProjectTreeViewContextMenu ()
{
}

KateProjectTreeViewContextMenu::~KateProjectTreeViewContextMenu ()
{
}

static inline bool isGit(const QString& filename)
{
  QFileInfo fi(filename);
  QDir dir (fi.absoluteDir());
  QProcess git;
  git.setWorkingDirectory (dir.absolutePath());
  QStringList args;
  args << "ls-files" << fi.fileName();
  git.start("git", args);
  bool isGit = false;
  if (git.waitForStarted() && git.waitForFinished()) {
    QStringList files = QString::fromLocal8Bit (git.readAllStandardOutput ()).split (QRegExp("[\n\r]"), QString::SkipEmptyParts);
    isGit = files.contains(fi.fileName());
  }
  return isGit;
}

void KateProjectTreeViewContextMenu::exec(const QString& filename, const QPoint& pos, QWidget* parent)
{
  /**
   * create context menu
   */
  QMenu menu;

  QAction *copyAction=menu.addAction(KIcon("edit-copy"),i18n("Copy Filename"));

  /**
   * handle "open with"
   * find correct mimetype to query for possible applications
   */
  QMenu *openWithMenu = menu.addMenu(i18n("Open With"));
  KMimeType::Ptr mimeType = KMimeType::findByUrl(KUrl(filename));
  KService::List offers = KMimeTypeTrader::self()->query(mimeType->name(), "Application");

  /**
   * for each one, insert a menu item...
   */
  for(KService::List::Iterator it = offers.begin(); it != offers.end(); ++it)
  {
    KService::Ptr service = *it;
    if (service->name() == "Kate") continue; // omit Kate
    QAction *action = openWithMenu->addAction(KIcon(service->icon()), service->name());
    action->setData(service->entryPath());
  }

  /**
   * handle "open directory with"
   * the mimetype to query for possible applications is known
   */
  // QFileInfo dirname(filename);
  QMenu *openDirectoryWithMenu = menu.addMenu(i18n("Open Directory With"));
  KService::List dirOffers = KMimeTypeTrader::self()->query("inode/directory", "Application");

  /**
   * for each one, insert a menu item...
   */
  for(KService::List::Iterator it = dirOffers.begin(); it != dirOffers.end(); ++it)
  {
    KService::Ptr service = *it;
    QAction *action = openDirectoryWithMenu->addAction(KIcon(service->icon()), service->name());
    action->setData(service->entryPath());
  }

  /**
   * perhaps disable menu, if no entries!
   */
  openWithMenu->setEnabled(!openWithMenu->isEmpty());
  openDirectoryWithMenu->setEnabled(!openDirectoryWithMenu->isEmpty());

  QList<QAction*> appActions;
  if (isGit(filename)) {
    QMenu* git = menu.addMenu(i18n("Git Tools"));
    if (!KStandardDirs::findExe("gitk").isEmpty()) {
      QAction* action = git->addAction(i18n("Launch gitk"));
      action->setData("gitk");
      appActions.append(action);
    }
    if (!KStandardDirs::findExe("qgit").isEmpty()) {
      QAction* action = git->addAction(i18n("Launch qgit"));
      action->setData("qgit");
      appActions.append(action);
    }
    if (!KStandardDirs::findExe("git-cola").isEmpty()) {
      QAction* action = git->addAction(i18n("Launch git-cola"));
      action->setData("git-cola");
      appActions.append(action);
    }

    if (appActions.size() == 0) {
      delete git;
    }
  }

  /**
   * run menu and handle the triggered action
   */
  if (QAction *action = menu.exec (pos)) {
    if (copyAction == action) {
      // handle copy
      QApplication::clipboard()->setText(filename);
    } else if (appActions.contains(action)) {
      // handle app action
      QFileInfo fi(filename);
      QDir dir (fi.absoluteDir());

      QStringList args;
      args << filename;

      QProcess::startDetached(action->data().toString(), QStringList(), dir.absolutePath());
    } else if(openDirectoryWithMenu == action->parentWidget()) {
      // handle open directory with
      const QString openDirWith = action->data().toString();
      if (!openDirWith.isEmpty()) {
        QFileInfo fi(filename);
        QStringList list;
        list << fi.dir().absolutePath();
        KToolInvocation::self()->startServiceByStorageId(openDirWith, list, parent);
      }
    } else {
      // open with
      const QString openWith = action->data().toString();
      if (!openWith.isEmpty()) {
        QStringList list;
        list << filename;
        KToolInvocation::self()->startServiceByStorageId(openWith, list, parent);
      }
    }
  }
}

// kate: space-indent on; indent-width 2; replace-tabs on;
