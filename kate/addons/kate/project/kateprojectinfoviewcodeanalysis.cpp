/*  This file is part of the Kate project.
 *
 *  Copyright (C) 2012 Christoph Cullmann <cullmann@kde.org>
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

#include "kateprojectinfoviewcodeanalysis.h"
#include "kateprojectpluginview.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QFileInfo>

#include <klocale.h>
#include <kmessagewidget.h>
#include <kstandarddirs.h>
#include <ktexteditor/view.h>

KateProjectInfoViewCodeAnalysis::KateProjectInfoViewCodeAnalysis (KateProjectPluginView *pluginView, KateProject *project)
  : QWidget ()
  , m_pluginView (pluginView)
  , m_project (project)
  , m_messageWidget (0)
  , m_startStopAnalysis (new QPushButton(i18n("Start Analysis...")))
  , m_treeView (new QTreeView())
  , m_model (new QStandardItemModel (m_treeView))
  , m_analyzer (0)
{
  /**
   * default style
   */
  m_treeView->setEditTriggers (QAbstractItemView::NoEditTriggers);
  m_treeView->setUniformRowHeights (true);
  m_treeView->setRootIsDecorated (false);
  m_model->setHorizontalHeaderLabels (QStringList () << "File" << "Line" << "Severity" << "Message");

  /**
   * attach model
   * kill selection model
   */
  QItemSelectionModel *m = m_treeView->selectionModel();
  m_treeView->setModel (m_model);
  delete m;

  /**
   * layout widget
   */
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setSpacing (0);
  layout->addWidget (m_treeView);
  QHBoxLayout *hlayout = new QHBoxLayout;
  layout->addLayout (hlayout);
  hlayout->setSpacing (0);
  hlayout->addStretch();
  hlayout->addWidget (m_startStopAnalysis);
  setLayout (layout);

  /**
   * connect needed signals
   */
  connect (m_startStopAnalysis, SIGNAL(clicked (bool)), this, SLOT(slotStartStopClicked ()));
  connect (m_treeView, SIGNAL(clicked (const QModelIndex &)), this, SLOT(slotClicked (const QModelIndex &)));
}

KateProjectInfoViewCodeAnalysis::~KateProjectInfoViewCodeAnalysis ()
{
}

void KateProjectInfoViewCodeAnalysis::slotStartStopClicked ()
{
  /**
   * stop analyzer
   */
  if (m_analyzer) {
    m_startStopAnalysis->setText(i18n("Start Analysis..."));
    m_analyzer->terminate();
    delete m_analyzer;
    m_analyzer=0;
    return;
  }

  /**
   * display a message to install cppcheck, but after stop has been performed
   * since cppcheck may have been uninstalled after the analyzer has been run
   */
  if (m_messageWidget) {
    delete m_messageWidget;
    m_messageWidget = 0;
  }

  if (KStandardDirs::findExe("cppcheck").isEmpty()) {
    m_messageWidget = new KMessageWidget();
    m_messageWidget->setCloseButtonVisible(true);
    m_messageWidget->setMessageType(KMessageWidget::Warning);
    m_messageWidget->setWordWrap(false);
    m_messageWidget->setText(i18n("Please install 'cppcheck'."));
    static_cast<QVBoxLayout*>(layout ())->insertWidget(0, m_messageWidget);
    m_messageWidget->show();
    return;
  }


  m_treeView->setSortingEnabled (false);

  /**
   * get files for cppcheck
   */
  QStringList files = m_project->files ().filter (QRegExp ("\\.(cpp|cxx|cc|c\\+\\+|c|tpp|txx)$"));

  /**
   * clear existing entries
   */
  m_model->removeRows(0,m_model->rowCount(),QModelIndex());

  /**
   * launch cppcheck
   */
  m_analyzer = new QProcess (this);
  m_analyzer->setProcessChannelMode(QProcess::MergedChannels);

  connect (m_analyzer, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));

  QStringList args;
  args << "-q" << "--inline-suppr" << "--enable=all" << "--template={file}////{line}////{severity}////{message}" << "--file-list=-";
  m_analyzer->start("cppcheck", args);

  /**
   * TODO: if failure occured stop now and display the error
   */
  if (m_analyzer->waitForStarted()) {
    m_startStopAnalysis->setText(i18n("Stop Analysis"));
  }

  /**
   * write files list and close write channel
   */
  m_analyzer->write(files.join("\n").toLocal8Bit());
  m_analyzer->closeWriteChannel();
}

void KateProjectInfoViewCodeAnalysis::slotReadyRead ()
{
  /**
   * get results of analysis
   */
  while (m_analyzer->canReadLine()) {
    /**
     * get one line, split it, skip it, if too few elements
     */
    QString line = QString::fromLocal8Bit (m_analyzer->readLine());
    QStringList elements = line.split (QRegExp("////"), QString::SkipEmptyParts);
    if (elements.size() < 4)
      continue;

    /**
     * feed into model
     */
    QList<QStandardItem*> items;
    QStandardItem *fileNameItem = new QStandardItem (QFileInfo (elements[0]).fileName());
    fileNameItem->setToolTip (elements[0]);
    items << fileNameItem;
    items << new QStandardItem (elements[1]);
    items << new QStandardItem (elements[2]);
    items << new QStandardItem (elements[3].simplified());
    m_model->appendRow (items);
  }

  /**
   * tree view polish ;)
   */
  m_treeView->resizeColumnToContents (2);
  m_treeView->resizeColumnToContents (1);
  m_treeView->resizeColumnToContents (0);
  // TODO: resort view
  m_treeView->setSortingEnabled (true);
}

void KateProjectInfoViewCodeAnalysis::slotClicked (const QModelIndex &index)
{
  /**
   * get path
   */
  QString filePath = m_model->item (index.row(), 0)->toolTip();
  if (filePath.isEmpty())
    return;

  /**
   * create view
   */
  KTextEditor::View *view = m_pluginView->mainWindow()->openUrl (KUrl::fromPath (filePath));
  if (!view)
    return;

  /**
   * set cursor, if possible
   */
  int line = m_model->item (index.row(), 1)->text().toInt();
  if (line >= 1)
    view->setCursorPosition (KTextEditor::Cursor (line - 1, 0));
}


// kate: space-indent on; indent-width 2; replace-tabs on;
