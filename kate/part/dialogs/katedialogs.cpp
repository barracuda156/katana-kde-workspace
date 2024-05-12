/* This file is part of the KDE libraries
   Copyright (C) 2002, 2003 Anders Lund <anders.lund@lund.tdcadsl.dk>
   Copyright (C) 2003 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2001 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2006 Dominik Haumann <dhdev@gmx.de>
   Copyright (C) 2007 Mirko Stocker <me@misto.ch>
   Copyright (C) 2009 Michel Ludwig <michel.ludwig@kdemail.net>
   Copyright (C) 2009 Erlend Hamberg <ehamberg@gmail.com>

   Based on work of:
     Copyright (C) 1999 Jochen Wilhelmy <digisnap@cs.tu-berlin.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

//BEGIN Includes
#include "katedialogs.h"
#include "moc_katedialogs.cpp"

#include "kateautoindent.h"
#include "katebuffer.h"
#include "kateconfig.h"
#include "katedocument.h"
#include "kateglobal.h"
#include "kateschema.h"
#include "katesyntaxdocument.h"
#include "katemodeconfigpage.h"
#include "kateview.h"
#include "katepartpluginmanager.h"
#include "kpluginselector.h"
#include "spellcheck/spellcheck.h"

// auto generated ui files
#include "ui_modonhdwidget.h"
#include "ui_textareaappearanceconfigwidget.h"
#include "ui_bordersappearanceconfigwidget.h"
#include "ui_navigationconfigwidget.h"
#include "ui_editconfigwidget.h"
#include "ui_indentationconfigwidget.h"
#include "ui_completionconfigtab.h"
#include "ui_opensaveconfigwidget.h"
#include "ui_opensaveconfigadvwidget.h"
#include "ui_spellcheckconfigwidget.h"

#include <ktexteditor/plugin.h>

#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kio/netaccess.h>
#include <kapplication.h>
#include <kcharsets.h>
#include <kcolorbutton.h>
#include <kcombobox.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kmimetypechooser.h>
#include <knuminput.h>
#include <kmenu.h>
#include <ktoolinvocation.h>
#include <kseparator.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>
#include <kpushbutton.h>
#include <kvbox.h>
#include <kactioncollection.h>
#include <kplugininfo.h>
#include <ktabwidget.h>
#include <kspeller.h>

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QDialog>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QPainter>
#include <QtGui/QRadioButton>
#include <QtGui/QSlider>
#include <QtGui/QTabWidget>
#include <QtGui/QToolButton>
#include <QtGui/QWhatsThis>
#include <QtGui/qevent.h>

//END

//BEGIN KateConfigPage
KateConfigPage::KateConfigPage ( QWidget *parent, const char * )
  : KTextEditor::ConfigPage (parent)
  , m_changed (false)
{
  connect (this, SIGNAL(changed()), this, SLOT(somethingHasChanged()));
}

KateConfigPage::~KateConfigPage ()
{
}

void KateConfigPage::slotChanged()
{
  emit changed();
}

void KateConfigPage::somethingHasChanged ()
{
  m_changed = true;
  kDebug (13000) << "TEST: something changed on the config page: " << this;
}
//END KateConfigPage

//BEGIN KateIndentConfigTab
KateIndentConfigTab::KateIndentConfigTab(QWidget *parent)
  : KateConfigPage(parent)
{
  // This will let us have more separation between this page and
  // the KTabWidget edge (ereslibre)
  QVBoxLayout *layout = new QVBoxLayout;
  QWidget *newWidget = new QWidget(this);

  ui = new Ui::IndentationConfigWidget();
  ui->setupUi( newWidget );

  ui->cmbMode->addItems (KateAutoIndent::listModes());

  ui->label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  connect(ui->label, SIGNAL(linkActivated(QString)), this, SLOT(showWhatsThis(QString)));

  // What's This? help can be found in the ui file

  reload ();

  //
  // after initial reload, connect the stuff for the changed () signal
  //

  connect(ui->cmbMode, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  connect(ui->rbIndentWithTabs, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->rbIndentWithSpaces, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->rbIndentMixed, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->rbIndentWithTabs, SIGNAL(toggled(bool)), ui->sbIndentWidth, SLOT(setDisabled(bool)));

  connect(ui->chkKeepExtraSpaces, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->chkIndentPaste, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->chkBackspaceUnindents, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

  connect(ui->sbTabWidth, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect(ui->sbIndentWidth, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));

  connect(ui->rbTabAdvances, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->rbTabIndents, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->rbTabSmart, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

  layout->addWidget(newWidget);
  setLayout(layout);
}

KateIndentConfigTab::~KateIndentConfigTab()
{
  delete ui;
}

void KateIndentConfigTab::slotChanged()
{
  if (ui->rbIndentWithTabs->isChecked())
    ui->sbIndentWidth->setValue(ui->sbTabWidth->value());

  KateConfigPage::slotChanged();
}

void KateIndentConfigTab::showWhatsThis(const QString& text)
{
  QWhatsThis::showText(QCursor::pos(), text);
}

void KateIndentConfigTab::apply ()
{
  // nothing changed, no need to apply stuff
  if (!hasChanged())
    return;
  m_changed = false;

  KateDocumentConfig::global()->configStart ();

  KateDocumentConfig::global()->setKeepExtraSpaces(ui->chkKeepExtraSpaces->isChecked());
  KateDocumentConfig::global()->setBackspaceIndents(ui->chkBackspaceUnindents->isChecked());
  KateDocumentConfig::global()->setIndentPastedText(ui->chkIndentPaste->isChecked());
  KateDocumentConfig::global()->setIndentationWidth(ui->sbIndentWidth->value());
  KateDocumentConfig::global()->setIndentationMode(KateAutoIndent::modeName(ui->cmbMode->currentIndex()));
  KateDocumentConfig::global()->setTabWidth(ui->sbTabWidth->value());
  KateDocumentConfig::global()->setReplaceTabsDyn(ui->rbIndentWithSpaces->isChecked());

  if (ui->rbTabAdvances->isChecked())
    KateDocumentConfig::global()->setTabHandling( KateDocumentConfig::tabInsertsTab );
  else if (ui->rbTabIndents->isChecked())
    KateDocumentConfig::global()->setTabHandling( KateDocumentConfig::tabIndents );
  else
    KateDocumentConfig::global()->setTabHandling( KateDocumentConfig::tabSmart );

  KateDocumentConfig::global()->configEnd ();
}

void KateIndentConfigTab::reload ()
{
  ui->sbTabWidth->setSuffix(ki18np(" character", " characters"));
  ui->sbTabWidth->setValue(KateDocumentConfig::global()->tabWidth());
  ui->sbIndentWidth->setSuffix(ki18np(" character", " characters"));
  ui->sbIndentWidth->setValue(KateDocumentConfig::global()->indentationWidth());
  ui->chkKeepExtraSpaces->setChecked(KateDocumentConfig::global()->keepExtraSpaces());
  ui->chkIndentPaste->setChecked(KateDocumentConfig::global()->indentPastedText());
  ui->chkBackspaceUnindents->setChecked(KateDocumentConfig::global()->backspaceIndents());

  ui->rbTabAdvances->setChecked( KateDocumentConfig::global()->tabHandling() == KateDocumentConfig::tabInsertsTab );
  ui->rbTabIndents->setChecked( KateDocumentConfig::global()->tabHandling() == KateDocumentConfig::tabIndents );
  ui->rbTabSmart->setChecked( KateDocumentConfig::global()->tabHandling() == KateDocumentConfig::tabSmart );

  ui->cmbMode->setCurrentIndex (KateAutoIndent::modeNumber (KateDocumentConfig::global()->indentationMode()));

  if (KateDocumentConfig::global()->replaceTabsDyn())
    ui->rbIndentWithSpaces->setChecked (true);
  else
  {
    if (KateDocumentConfig::global()->indentationWidth() == KateDocumentConfig::global()->tabWidth())
      ui->rbIndentWithTabs->setChecked (true);
    else
      ui->rbIndentMixed->setChecked (true);
  }

  ui->sbIndentWidth->setEnabled(!ui->rbIndentWithTabs->isChecked());
}
//END KateIndentConfigTab

//BEGIN KateCompletionConfigTab
KateCompletionConfigTab::KateCompletionConfigTab(QWidget *parent)
  : KateConfigPage(parent)
{
  // This will let us have more separation between this page and
  // the KTabWidget edge (ereslibre)
  QVBoxLayout *layout = new QVBoxLayout;
  QWidget *newWidget = new QWidget(this);

  ui = new Ui::CompletionConfigTab ();
  ui->setupUi( newWidget );

  // What's This? help can be found in the ui file

  reload ();

  //
  // after initial reload, connect the stuff for the changed () signal
  //

  connect(ui->chkAutoCompletionEnabled, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->gbWordCompletion, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->gbKeywordCompletion, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->minimalWordLength, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect(ui->removeTail, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

  layout->addWidget(newWidget);
  setLayout(layout);
}

KateCompletionConfigTab::~KateCompletionConfigTab()
{
  delete ui;
}

void KateCompletionConfigTab::showWhatsThis(const QString& text)
{
  QWhatsThis::showText(QCursor::pos(), text);
}

void KateCompletionConfigTab::apply ()
{
  // nothing changed, no need to apply stuff
  if (!hasChanged())
    return;
  m_changed = false;

  KateViewConfig::global()->configStart ();
  KateViewConfig::global()->setAutomaticCompletionInvocation (ui->chkAutoCompletionEnabled->isChecked());
  KateViewConfig::global()->setWordCompletion (ui->gbWordCompletion->isChecked());
  KateViewConfig::global()->setWordCompletionMinimalWordLength (ui->minimalWordLength->value());
  KateViewConfig::global()->setWordCompletionRemoveTail (ui->removeTail->isChecked());
  KateViewConfig::global()->setKeywordCompletion (ui->gbKeywordCompletion->isChecked());
  KateViewConfig::global()->configEnd ();
}

void KateCompletionConfigTab::reload ()
{
  ui->chkAutoCompletionEnabled->setChecked( KateViewConfig::global()->automaticCompletionInvocation () );
  ui->gbWordCompletion->setChecked( KateViewConfig::global()->wordCompletion () );
  ui->gbKeywordCompletion->setChecked( KateViewConfig::global()->keywordCompletion () );
  ui->minimalWordLength->setValue (KateViewConfig::global()->wordCompletionMinimalWordLength ());
  ui->removeTail->setChecked (KateViewConfig::global()->wordCompletionRemoveTail ());
}
//END KateCompletionConfigTab

//BEGIN KateSpellCheckConfigTab
KateSpellCheckConfigTab::KateSpellCheckConfigTab(QWidget *parent)
  : KateConfigPage(parent)
{
  // This will let us have more separation between this page and
  // the KTabWidget edge (ereslibre)
  QVBoxLayout *layout = new QVBoxLayout;
  QWidget *newWidget = new QWidget(this);

  ui = new Ui::SpellCheckConfigWidget();
  ui->setupUi(newWidget);

  // What's This? help can be found in the ui file
  reload();

  //
  // after initial reload, connect the stuff for the changed () signal

  m_spellConfigWidget = new KSpellConfigWidget(KGlobal::config().data(), this);
  connect(m_spellConfigWidget, SIGNAL(configChanged()), this, SLOT(slotChanged()));
  layout->addWidget(m_spellConfigWidget);

  layout->addWidget(newWidget);
  setLayout(layout);
}

KateSpellCheckConfigTab::~KateSpellCheckConfigTab()
{
  delete ui;
}

void KateSpellCheckConfigTab::showWhatsThis(const QString& text)
{
  QWhatsThis::showText(QCursor::pos(), text);
}

void KateSpellCheckConfigTab::apply()
{
  if (!hasChanged()) {
    // nothing changed, no need to apply stuff
    return;
  }
  m_changed = false;

  KateDocumentConfig::global()->configStart();
  m_spellConfigWidget->save();
  KateDocumentConfig::global()->configEnd();
  foreach (KateDocument *doc, KateGlobal::self()->kateDocuments()) {
    doc->refreshOnTheFlyCheck();
  }
}

void KateSpellCheckConfigTab::reload()
{
  // does nothing
}
//END KateSpellCheckConfigTab

//BEGIN KateNavigationConfigTab
KateNavigationConfigTab::KateNavigationConfigTab(QWidget *parent)
  : KateConfigPage(parent)
{
  // This will let us having more separation between this page and
  // the KTabWidget edge (ereslibre)
  QVBoxLayout *layout = new QVBoxLayout;
  QWidget *newWidget = new QWidget(this);

  ui = new Ui::NavigationConfigWidget();
  ui->setupUi( newWidget );

  // What's This? Help is in the ui-files

  reload ();

  //
  // after initial reload, connect the stuff for the changed () signal
  //

  connect(ui->cbTextSelectionMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChanged()));
  connect(ui->chkSmartHome, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->chkPagingMovesCursor, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->sbAutoCenterCursor, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect(ui->chkScrollPastEnd, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

  layout->addWidget(newWidget);
  setLayout(layout);
}

KateNavigationConfigTab::~KateNavigationConfigTab()
{
  delete ui;
}

void KateNavigationConfigTab::apply ()
{
  // nothing changed, no need to apply stuff
  if (!hasChanged())
    return;
  m_changed = false;

  KateViewConfig::global()->configStart ();
  KateDocumentConfig::global()->configStart ();

  KateDocumentConfig::global()->setSmartHome(ui->chkSmartHome->isChecked());

  KateViewConfig::global()->setAutoCenterLines(qMax(0, ui->sbAutoCenterCursor->value()));
  KateDocumentConfig::global()->setPageUpDownMovesCursor(ui->chkPagingMovesCursor->isChecked());

  KateViewConfig::global()->setPersistentSelection (ui->cbTextSelectionMode->currentIndex() == 1);

  KateViewConfig::global()->setScrollPastEnd(ui->chkScrollPastEnd->isChecked());

  KateDocumentConfig::global()->configEnd ();
  KateViewConfig::global()->configEnd ();
}

void KateNavigationConfigTab::reload ()
{
  ui->cbTextSelectionMode->setCurrentIndex( KateViewConfig::global()->persistentSelection() ? 1 : 0 );

  ui->chkSmartHome->setChecked(KateDocumentConfig::global()->smartHome());
  ui->chkPagingMovesCursor->setChecked(KateDocumentConfig::global()->pageUpDownMovesCursor());
  ui->sbAutoCenterCursor->setValue(KateViewConfig::global()->autoCenterLines());
  ui->chkScrollPastEnd->setChecked(KateViewConfig::global()->scrollPastEnd());
}
//END KateNavigationConfigTab

//BEGIN KateEditGeneralConfigTab
KateEditGeneralConfigTab::KateEditGeneralConfigTab(QWidget *parent)
  : KateConfigPage(parent)
{
  QVBoxLayout *layout = new QVBoxLayout;
  QWidget *newWidget = new QWidget(this);
  ui = new Ui::EditConfigWidget();
  ui->setupUi( newWidget );

  reload();

  connect(ui->chkStaticWordWrap, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->chkShowStaticWordWrapMarker, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(ui->sbWordWrap, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect(ui->chkSmartCopyCut, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

  // "What's this?" help is in the ui-file

  layout->addWidget(newWidget);
  setLayout(layout);
}

KateEditGeneralConfigTab::~KateEditGeneralConfigTab()
{
  delete ui;
}

void KateEditGeneralConfigTab::apply ()
{
  // nothing changed, no need to apply stuff
  if (!hasChanged())
    return;
  m_changed = false;

  KateViewConfig::global()->configStart ();
  KateDocumentConfig::global()->configStart ();

  KateDocumentConfig::global()->setWordWrapAt(ui->sbWordWrap->value());
  KateDocumentConfig::global()->setWordWrap(ui->chkStaticWordWrap->isChecked());

  KateRendererConfig::global()->setWordWrapMarker (ui->chkShowStaticWordWrapMarker->isChecked());

  KateDocumentConfig::global()->configEnd ();
  KateViewConfig::global()->setSmartCopyCut(ui->chkSmartCopyCut->isChecked());
  KateViewConfig::global()->configEnd ();
}

void KateEditGeneralConfigTab::reload ()
{
  ui->chkStaticWordWrap->setChecked(KateDocumentConfig::global()->wordWrap());
  ui->chkShowStaticWordWrapMarker->setChecked( KateRendererConfig::global()->wordWrapMarker() );
  ui->sbWordWrap->setSuffix(ki18ncp("Wrap words at", " character", " characters"));
  ui->sbWordWrap->setValue( KateDocumentConfig::global()->wordWrapAt() );
  ui->chkSmartCopyCut->setChecked( KateViewConfig::global()->smartCopyCut() );
}
//END KateEditGeneralConfigTab


//BEGIN KateEditConfigTab
KateEditConfigTab::KateEditConfigTab(QWidget *parent)
  : KateConfigPage(parent)
  , editConfigTab(new KateEditGeneralConfigTab(this))
  , navigationConfigTab(new KateNavigationConfigTab(this))
  , indentConfigTab(new KateIndentConfigTab(this))
  , completionConfigTab (new KateCompletionConfigTab(this))
  , spellCheckConfigTab(new KateSpellCheckConfigTab(this))
{
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setMargin(0);
  KTabWidget *tabWidget = new KTabWidget(this);

  // add all tabs
  tabWidget->insertTab(0, editConfigTab, i18n("General"));
  tabWidget->insertTab(1, navigationConfigTab, i18n("Text Navigation"));
  tabWidget->insertTab(2, indentConfigTab, i18n("Indentation"));
  tabWidget->insertTab(3, completionConfigTab, i18n("Auto Completion"));
  tabWidget->insertTab(4, spellCheckConfigTab, i18n("Spellcheck"));

  connect(editConfigTab, SIGNAL(changed()), this, SLOT(slotChanged()));
  connect(navigationConfigTab, SIGNAL(changed()), this, SLOT(slotChanged()));
  connect(indentConfigTab, SIGNAL(changed()), this, SLOT(slotChanged()));
  connect(completionConfigTab, SIGNAL(changed()), this, SLOT(slotChanged()));
  connect(spellCheckConfigTab, SIGNAL(changed()), this, SLOT(slotChanged()));

  layout->addWidget(tabWidget);
  setLayout(layout);
}

KateEditConfigTab::~KateEditConfigTab()
{
}

void KateEditConfigTab::apply ()
{
  // try to update the rest of tabs
  editConfigTab->apply();
  navigationConfigTab->apply();
  indentConfigTab->apply();
  completionConfigTab->apply();
  spellCheckConfigTab->apply();
}

void KateEditConfigTab::reload ()
{
  editConfigTab->reload();
  navigationConfigTab->reload();
  indentConfigTab->reload();
  completionConfigTab->reload();
  spellCheckConfigTab->reload();
}

void KateEditConfigTab::reset ()
{
  editConfigTab->reset();
  navigationConfigTab->reset();
  indentConfigTab->reset();
  completionConfigTab->reset();
  spellCheckConfigTab->reset();
}

void KateEditConfigTab::defaults ()
{
  editConfigTab->defaults();
  navigationConfigTab->defaults();
  indentConfigTab->defaults();
  completionConfigTab->defaults();
  spellCheckConfigTab->defaults();
}
//END KateEditConfigTab

//BEGIN KateViewDefaultsConfig
KateViewDefaultsConfig::KateViewDefaultsConfig(QWidget *parent)
  : KateConfigPage(parent)
  , textareaUi(new Ui::TextareaAppearanceConfigWidget())
  , bordersUi(new Ui::BordersAppearanceConfigWidget())
{
  QLayout *layout = new QVBoxLayout( this );
  QTabWidget *tabWidget = new QTabWidget( this );
  layout->addWidget( tabWidget );
  layout->setMargin( 0 );

  QWidget *textareaTab = new QWidget( tabWidget );
  textareaUi->setupUi( textareaTab );
  tabWidget->addTab( textareaTab, i18n("General") );

  QWidget *bordersTab = new QWidget( tabWidget );
  bordersUi->setupUi( bordersTab );
  tabWidget->addTab( bordersTab, i18n("Borders") );

  if (KateDocument::simpleMode ())
    bordersUi->gbSortBookmarks->hide ();

  textareaUi->cmbDynamicWordWrapIndicator->addItem( i18n("Off") );
  textareaUi->cmbDynamicWordWrapIndicator->addItem( i18n("Follow Line Numbers") );
  textareaUi->cmbDynamicWordWrapIndicator->addItem( i18n("Always On") );

  // hide power user mode if activated anyway
  if (!KateGlobal::self()->simpleMode ())
    textareaUi->chkDeveloperMode->hide ();

  // What's This? help is in the ui-file

  reload();

  //
  // after initial reload, connect the stuff for the changed () signal
  //

  connect(textareaUi->gbWordWrap, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->cmbDynamicWordWrapIndicator, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  connect(textareaUi->sbDynamicWordWrapDepth, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect(textareaUi->chkShowTabs, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->chkShowSpaces, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->chkShowIndentationLines, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->chkShowWholeBracketExpression, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->chkAnimateBracketMatching, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->chkDeveloperMode, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(textareaUi->chkFoldFirstLine,  SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

  connect(bordersUi->chkIconBorder, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->chkScrollbarMarks, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->chkScrollbarMiniMap, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->chkScrollbarMiniMapAll, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  bordersUi->chkScrollbarMiniMapAll->hide(); // this is temporary until the feature is done
  connect(bordersUi->spBoxMiniMapWidth, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect(bordersUi->chkLineNumbers, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->chkShowLineModification, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->chkShowFoldingMarkers, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->rbSortBookmarksByPosition, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->rbSortBookmarksByCreation, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect(bordersUi->cmbShowScrollbars, SIGNAL(activated(int)), this, SLOT(slotChanged()));
}

KateViewDefaultsConfig::~KateViewDefaultsConfig()
{
  delete bordersUi;
  delete textareaUi;
}

void KateViewDefaultsConfig::apply ()
{
  // nothing changed, no need to apply stuff
  if (!hasChanged())
    return;
  m_changed = false;

  KateViewConfig::global()->configStart ();
  KateRendererConfig::global()->configStart ();

  KateViewConfig::global()->setDynWordWrap (textareaUi->gbWordWrap->isChecked());
  KateViewConfig::global()->setDynWordWrapIndicators (textareaUi->cmbDynamicWordWrapIndicator->currentIndex ());
  KateViewConfig::global()->setDynWordWrapAlignIndent(textareaUi->sbDynamicWordWrapDepth->value());
  KateDocumentConfig::global()->setShowTabs (textareaUi->chkShowTabs->isChecked());
  KateDocumentConfig::global()->setShowSpaces (textareaUi->chkShowSpaces->isChecked());
  KateViewConfig::global()->setLineNumbers (bordersUi->chkLineNumbers->isChecked());
  KateViewConfig::global()->setIconBar (bordersUi->chkIconBorder->isChecked());
  KateViewConfig::global()->setScrollBarMarks (bordersUi->chkScrollbarMarks->isChecked());
  KateViewConfig::global()->setScrollBarMiniMap (bordersUi->chkScrollbarMiniMap->isChecked());
  KateViewConfig::global()->setScrollBarMiniMapAll (bordersUi->chkScrollbarMiniMapAll->isChecked());
  KateViewConfig::global()->setScrollBarMiniMapWidth (bordersUi->spBoxMiniMapWidth->value());
  KateViewConfig::global()->setFoldingBar (bordersUi->chkShowFoldingMarkers->isChecked());
  KateViewConfig::global()->setLineModification(bordersUi->chkShowLineModification->isChecked());
  KateViewConfig::global()->setShowScrollbars( bordersUi->cmbShowScrollbars->currentIndex() );

  KateViewConfig::global()->setBookmarkSort (bordersUi->rbSortBookmarksByPosition->isChecked()?0:1);
  KateRendererConfig::global()->setShowIndentationLines(textareaUi->chkShowIndentationLines->isChecked());
  KateRendererConfig::global()->setShowWholeBracketExpression(textareaUi->chkShowWholeBracketExpression->isChecked());
  KateRendererConfig::global()->setAnimateBracketMatching(textareaUi->chkAnimateBracketMatching->isChecked());
  KateViewConfig::global()->setFoldFirstLine(textareaUi->chkFoldFirstLine->isChecked());

  // warn user that he needs restart the application
  if (!textareaUi->chkDeveloperMode->isChecked() != KateDocumentConfig::global()->allowSimpleMode())
  {
    // inform...
    KMessageBox::information(
                this,
                i18n("Changing the power user mode affects only newly opened / created documents. In KWrite a restart is recommended."),
                i18n("Power user mode changed"));

    KateDocumentConfig::global()->setAllowSimpleMode (!textareaUi->chkDeveloperMode->isChecked());
  }

  KateRendererConfig::global()->configEnd ();
  KateViewConfig::global()->configEnd ();
}

void KateViewDefaultsConfig::reload ()
{
  textareaUi->gbWordWrap->setChecked(KateViewConfig::global()->dynWordWrap());
  textareaUi->cmbDynamicWordWrapIndicator->setCurrentIndex( KateViewConfig::global()->dynWordWrapIndicators() );
  textareaUi->sbDynamicWordWrapDepth->setValue(KateViewConfig::global()->dynWordWrapAlignIndent());
  textareaUi->chkShowTabs->setChecked(KateDocumentConfig::global()->showTabs());
  textareaUi->chkShowSpaces->setChecked(KateDocumentConfig::global()->showSpaces());
  bordersUi->chkLineNumbers->setChecked(KateViewConfig::global()->lineNumbers());
  bordersUi->chkIconBorder->setChecked(KateViewConfig::global()->iconBar());
  bordersUi->chkScrollbarMarks->setChecked(KateViewConfig::global()->scrollBarMarks());
  bordersUi->chkScrollbarMiniMap->setChecked(KateViewConfig::global()->scrollBarMiniMap());
  bordersUi->chkScrollbarMiniMapAll->setChecked(KateViewConfig::global()->scrollBarMiniMapAll());
  bordersUi->spBoxMiniMapWidth->setValue(KateViewConfig::global()->scrollBarMiniMapWidth());
  bordersUi->chkShowFoldingMarkers->setChecked(KateViewConfig::global()->foldingBar());
  bordersUi->chkShowLineModification->setChecked(KateViewConfig::global()->lineModification());
  bordersUi->rbSortBookmarksByPosition->setChecked(KateViewConfig::global()->bookmarkSort()==0);
  bordersUi->rbSortBookmarksByCreation->setChecked(KateViewConfig::global()->bookmarkSort()==1);
  bordersUi->cmbShowScrollbars->setCurrentIndex( KateViewConfig::global()->showScrollbars() );
  textareaUi->chkShowIndentationLines->setChecked(KateRendererConfig::global()->showIndentationLines());
  textareaUi->chkShowWholeBracketExpression->setChecked(KateRendererConfig::global()->showWholeBracketExpression());
  textareaUi->chkAnimateBracketMatching->setChecked(KateRendererConfig::global()->animateBracketMatching());
  textareaUi->chkDeveloperMode->setChecked(!KateDocumentConfig::global()->allowSimpleMode());
  textareaUi->chkFoldFirstLine->setChecked(KateViewConfig::global()->foldFirstLine());
}

void KateViewDefaultsConfig::reset () {;}

void KateViewDefaultsConfig::defaults (){;}
//END KateViewDefaultsConfig

//BEGIN KateSaveConfigTab
KateSaveConfigTab::KateSaveConfigTab( QWidget *parent )
  : KateConfigPage( parent )
  , modeConfigPage( new ModeConfigPage( this ) )
{
  // FIXME: Is really needed to move all this code below to another class,
  // since it is another tab itself on the config dialog. This means we should
  // initialize, add and work with as we do with modeConfigPage (ereslibre)
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setMargin(0);
  KTabWidget *tabWidget = new KTabWidget(this);

  QWidget *tmpWidget = new QWidget(tabWidget);
  QVBoxLayout *internalLayout = new QVBoxLayout;
  QWidget *newWidget = new QWidget(tabWidget);
  ui = new Ui::OpenSaveConfigWidget();
  ui->setupUi( newWidget );

  QWidget *tmpWidget2 = new QWidget(tabWidget);
  QVBoxLayout *internalLayout2 = new QVBoxLayout;
  QWidget *newWidget2 = new QWidget(tabWidget);
  uiadv = new Ui::OpenSaveConfigAdvWidget();
  uiadv->setupUi( newWidget2 );

  // What's this help is added in ui/opensaveconfigwidget.ui
  reload();

  //
  // after initial reload, connect the stuff for the changed () signal
  //

  connect( ui->cmbEncoding, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  connect( ui->cmbEncodingFallback, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  connect( ui->cmbEOL, SIGNAL(activated(int)), this, SLOT(slotChanged()));
  connect( ui->chkDetectEOL, SIGNAL(toggled(bool)), this, SLOT(slotChanged()) );
  connect( ui->chkEnableBOM, SIGNAL(toggled(bool)), this, SLOT(slotChanged()) );
  connect( ui->lineLengthLimit, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect( ui->cbRemoveTrailingSpaces, SIGNAL(currentIndexChanged(int)), this, SLOT(slotChanged()));
  connect( ui->chkNewLineAtEof, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));
  connect( uiadv->chkBackupLocalFiles, SIGNAL(toggled(bool)), this, SLOT(slotChanged()) );
  connect( uiadv->chkBackupRemoteFiles, SIGNAL(toggled(bool)), this, SLOT(slotChanged()) );
  connect( uiadv->sbConfigFileSearchDepth, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
  connect( uiadv->edtBackupPrefix, SIGNAL(textChanged(QString)), this, SLOT(slotChanged()) );
  connect( uiadv->edtBackupSuffix, SIGNAL(textChanged(QString)), this, SLOT(slotChanged()) );
  connect( uiadv->chkNoSync, SIGNAL(toggled(bool)), this, SLOT(slotChanged()) );

  internalLayout->addWidget(newWidget);
  tmpWidget->setLayout(internalLayout);
  internalLayout2->addWidget(newWidget2);
  tmpWidget2->setLayout(internalLayout2);

  // add all tabs
  tabWidget->insertTab(0, tmpWidget, i18n("General"));
  tabWidget->insertTab(1, tmpWidget2, i18n("Advanced"));
  tabWidget->insertTab(2, modeConfigPage, i18n("Modes && Filetypes"));

  connect(modeConfigPage, SIGNAL(changed()), this, SLOT(slotChanged()));

  layout->addWidget(tabWidget);
  setLayout(layout);
}

KateSaveConfigTab::~KateSaveConfigTab()
{
  delete ui;
}

void KateSaveConfigTab::apply()
{
  modeConfigPage->apply();

  // nothing changed, no need to apply stuff
  if (!hasChanged())
    return;
  m_changed = false;

  KateGlobalConfig::global()->configStart ();
  KateDocumentConfig::global()->configStart ();

  if ( uiadv->edtBackupSuffix->text().isEmpty() && uiadv->edtBackupPrefix->text().isEmpty() ) {
    KMessageBox::information(
                this,
                i18n("You did not provide a backup suffix or prefix. Using default suffix: '~'"),
                i18n("No Backup Suffix or Prefix")
                        );
    uiadv->edtBackupSuffix->setText( "~" );
  }

  uint f( 0 );
  if ( uiadv->chkBackupLocalFiles->isChecked() )
    f |= KateDocumentConfig::LocalFiles;
  if ( uiadv->chkBackupRemoteFiles->isChecked() )
    f |= KateDocumentConfig::RemoteFiles;

  KateDocumentConfig::global()->setBackupFlags(f);
  KateDocumentConfig::global()->setBackupPrefix(uiadv->edtBackupPrefix->text());
  KateDocumentConfig::global()->setBackupSuffix(uiadv->edtBackupSuffix->text());

  KateDocumentConfig::global()->setSwapFileNoSync(uiadv->chkNoSync->isChecked());

  KateDocumentConfig::global()->setSearchDirConfigDepth(uiadv->sbConfigFileSearchDepth->value());

  KateDocumentConfig::global()->setRemoveSpaces(ui->cbRemoveTrailingSpaces->currentIndex());

  KateDocumentConfig::global()->setNewLineAtEof(ui->chkNewLineAtEof->isChecked());

  // set both standard and fallback encoding
  KateDocumentConfig::global()->setEncoding(KGlobal::charsets()->encodingForName(ui->cmbEncoding->currentText()));

  KateGlobalConfig::global()->setFallbackEncoding(KGlobal::charsets()->encodingForName(ui->cmbEncodingFallback->currentText()));

  KateDocumentConfig::global()->setEol(ui->cmbEOL->currentIndex());
  KateDocumentConfig::global()->setAllowEolDetection(ui->chkDetectEOL->isChecked());
  KateDocumentConfig::global()->setBom(ui->chkEnableBOM->isChecked());

  KateDocumentConfig::global()->setLineLengthLimit(ui->lineLengthLimit->value());

  KateDocumentConfig::global()->configEnd ();
  KateGlobalConfig::global()->configEnd ();
}

void KateSaveConfigTab::reload()
{
  modeConfigPage->reload();

  // encodings
  ui->cmbEncoding->clear ();
  ui->cmbEncodingFallback->clear ();
  QStringList encodings (KGlobal::charsets()->descriptiveEncodingNames());
  for (int i=0; i < encodings.count(); i++)
  {
    bool found = false;
    QTextCodec *codecForEnc = KGlobal::charsets()->codecForName(KGlobal::charsets()->encodingForName(encodings[i]), found);

    if (found)
    {
      ui->cmbEncoding->addItem (encodings[i]);
      ui->cmbEncodingFallback->addItem (encodings[i]);

      if ( codecForEnc->name() == KateDocumentConfig::global()->encoding() )
      {
        ui->cmbEncoding->setCurrentIndex(i);
      }

      if ( codecForEnc == KateGlobalConfig::global()->fallbackCodec() )
      {
        // adjust index for fallback config, has no default!
        ui->cmbEncodingFallback->setCurrentIndex(i);
      }
    }
  }

  // eol
  ui->cmbEOL->setCurrentIndex(KateDocumentConfig::global()->eol());
  ui->chkDetectEOL->setChecked(KateDocumentConfig::global()->allowEolDetection());
  ui->chkEnableBOM->setChecked(KateDocumentConfig::global()->bom());
  ui->lineLengthLimit->setValue(KateDocumentConfig::global()->lineLengthLimit());

  ui->cbRemoveTrailingSpaces->setCurrentIndex(KateDocumentConfig::global()->removeSpaces());
  ui->chkNewLineAtEof->setChecked(KateDocumentConfig::global()->newLineAtEof());
  uiadv->sbConfigFileSearchDepth->setValue(KateDocumentConfig::global()->searchDirConfigDepth());

  // other stuff
  uint f ( KateDocumentConfig::global()->backupFlags() );
  uiadv->chkBackupLocalFiles->setChecked( f & KateDocumentConfig::LocalFiles );
  uiadv->chkBackupRemoteFiles->setChecked( f & KateDocumentConfig::RemoteFiles );
  uiadv->edtBackupPrefix->setText( KateDocumentConfig::global()->backupPrefix() );
  uiadv->edtBackupSuffix->setText( KateDocumentConfig::global()->backupSuffix() );
  uiadv->chkNoSync->setChecked( KateDocumentConfig::global()->swapFileNoSync() );
}

void KateSaveConfigTab::reset()
{
  modeConfigPage->reset();
}

void KateSaveConfigTab::defaults()
{
  modeConfigPage->defaults();

  ui->cbRemoveTrailingSpaces->setCurrentIndex(0);

  uiadv->chkBackupLocalFiles->setChecked( true );
  uiadv->chkBackupRemoteFiles->setChecked( false );
  uiadv->edtBackupPrefix->setText( "" );
  uiadv->edtBackupSuffix->setText( "~" );
  uiadv->chkNoSync->setChecked( false );
}

//END KateSaveConfigTab

//BEGIN KatePartPluginConfigPage
KatePartPluginConfigPage::KatePartPluginConfigPage (QWidget *parent)
  : KateConfigPage (parent, "")
{
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setMargin(0);

  plugins.clear();

  foreach (const KatePartPluginInfo &info, KatePartPluginManager::self()->pluginList())
  {
    KPluginInfo it(info.service());
    it.setPluginEnabled(info.load);
    plugins.append(it);
  }

  selector = new KPluginSelector(0);

  connect(selector, SIGNAL(changed(bool)), this, SLOT(slotChanged()));
  connect(selector, SIGNAL(configCommitted(QByteArray)), this, SLOT(slotChanged()));

  selector->addPlugins(plugins, KPluginSelector::IgnoreConfigFile, i18n("Editor Plugins"), "Editor");

  layout->addWidget(selector);
  setLayout(layout);
}

KatePartPluginConfigPage::~KatePartPluginConfigPage ()
{
}

void KatePartPluginConfigPage::apply ()
{
  selector->updatePluginsState();

  KatePartPluginList &katePluginList = KatePartPluginManager::self()->pluginList();
  for (int i=0; i < plugins.count(); i++) {
    if (plugins[i].isPluginEnabled()) {
      if (!katePluginList[i].load) {
        KatePartPluginManager::self()->loadPlugin(katePluginList[i]);
        KatePartPluginManager::self()->enablePlugin(katePluginList[i]);
      }
    } else {
      if (katePluginList[i].load) {
        KatePartPluginManager::self()->disablePlugin(katePluginList[i]);
        KatePartPluginManager::self()->unloadPlugin(katePluginList[i]);
      }
    }
  }
}

void KatePartPluginConfigPage::reload ()
{
  selector->load();
}

void KatePartPluginConfigPage::reset ()
{
  selector->load();
}

void KatePartPluginConfigPage::defaults ()
{
  selector->defaults();
}
//END KatePartPluginConfigPage

//BEGIN KateGotoBar
KateGotoBar::KateGotoBar(KTextEditor::View *view, QWidget *parent)
  : KateViewBarWidget( true, parent )
  , m_view( view )
{
  Q_ASSERT( m_view != 0 );  // this bar widget is pointless w/o a view

  QHBoxLayout *topLayout = new QHBoxLayout( centralWidget() );
  topLayout->setMargin(0);
  gotoRange = new KIntSpinBox(centralWidget());

  QLabel *label = new QLabel(i18n("&Go to line:"), centralWidget() );
  label->setBuddy(gotoRange);

  QToolButton *btnOK = new QToolButton(centralWidget());
  btnOK->setAutoRaise(true);
  btnOK->setIcon(QIcon(SmallIcon("go-jump")));
  btnOK->setText(i18n("Go"));
  btnOK->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  connect(btnOK, SIGNAL(clicked()), this, SLOT(gotoLine()));

  topLayout->addWidget(label);
  topLayout->addWidget(gotoRange, 1);
  topLayout->setStretchFactor( gotoRange, 0 );
  topLayout->addWidget(btnOK);
  topLayout->addStretch();

  setFocusProxy(gotoRange);
}

void KateGotoBar::updateData()
{
  gotoRange->setMaximum(m_view->document()->lines());
  if (!isVisible())
  {
    gotoRange->setValue(m_view->cursorPosition().line() + 1);
    gotoRange->adjustSize(); // ### does not respect the range :-(
  }
  gotoRange->setFocus(Qt::OtherFocusReason);
  gotoRange->selectAll();
}

void KateGotoBar::keyPressEvent(QKeyEvent* event)
{
  int key = event->key();
  if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    gotoLine();
    return;
  }
  KateViewBarWidget::keyPressEvent(event);
}

void KateGotoBar::gotoLine()
{
  KateView *kv = qobject_cast<KateView*>(m_view);
  if (kv && kv->selection() && !kv->config()->persistentSelection()) {
    kv->clearSelection();
  }

  m_view->setCursorPosition( KTextEditor::Cursor(gotoRange->value() - 1, 0) );
  m_view->setFocus();
  emit hideMe();
}
//END KateGotoBar

//BEGIN KateDictionaryBar
KateDictionaryBar::KateDictionaryBar(KateView* view, QWidget *parent)
  : KateViewBarWidget( true, parent )
  , m_view( view )
{
  Q_ASSERT(m_view != 0); // this bar widget is pointless w/o a view

  QHBoxLayout *topLayout = new QHBoxLayout(centralWidget());
  topLayout->setMargin(0);
  //topLayout->setSpacing(spacingHint());
  m_dictionaryComboBox = new KSpellDictionaryComboBox(centralWidget());
  connect(m_dictionaryComboBox, SIGNAL(dictionaryChanged(QString)),
          this, SLOT(dictionaryChanged(QString)));
  connect(view->doc(), SIGNAL(defaultDictionaryChanged(KateDocument*)),
          this, SLOT(updateData()));
  QLabel *label = new QLabel(i18n("Dictionary:"), centralWidget());
  label->setBuddy(m_dictionaryComboBox);

  topLayout->addWidget(label);
  topLayout->addWidget(m_dictionaryComboBox, 1);
  topLayout->setStretchFactor(m_dictionaryComboBox, 0);
  topLayout->addStretch();
}

KateDictionaryBar::~KateDictionaryBar()
{
}

void KateDictionaryBar::updateData()
{
  KateDocument *document = m_view->doc();
  QString dictionary = document->defaultDictionary();
  if(dictionary.isEmpty()) {
    dictionary = KSpeller::defaultLanguage();
  }
  m_dictionaryComboBox->setCurrentByDictionary(dictionary);
}

void KateDictionaryBar::dictionaryChanged(const QString& dictionary)
{
  KTextEditor::Range selection = m_view->selectionRange();
  if(selection.isValid() && !selection.isEmpty()) {
    m_view->doc()->setDictionary(dictionary, selection);
  }
  else {
    m_view->doc()->setDefaultDictionary(dictionary);
  }
}

//END KateGotoBar


//BEGIN KateModOnHdPrompt
KateModOnHdPrompt::KateModOnHdPrompt( KateDocument *doc,
                                      KTextEditor::ModificationInterface::ModifiedOnDiskReason modtype,
                                      const QString &reason,
                                      QWidget *parent )
  : KDialog( parent ),
    m_doc( doc ),
    m_modtype ( modtype ),
    m_proc( 0 ),
    m_diffFile( 0 )
{
  setButtons( Ok | Apply | Cancel | User1 );

  QString title, okText, okIcon, okToolTip;
  if ( modtype == KTextEditor::ModificationInterface::OnDiskDeleted )
  {
    title = i18n("File Was Deleted on Disk");
    okText = i18n("&Save File As...");
    okIcon = "document-save-as";
    okToolTip = i18n("Lets you select a location and save the file again.");
  } else {
    title = i18n("File Changed on Disk");
    okText = i18n("&Reload File");
    okIcon = "view-refresh";
    okToolTip = i18n("Reload the file from disk. If you have unsaved changes, "
        "they will be lost.");
  }

  setButtonText( Ok, okText );
  setButtonIcon( Ok, KIcon( okIcon ) );
  setButtonText( Apply, i18n("&Ignore Changes") );
  setButtonIcon( Apply, KIcon( "dialog-warning" ) );

  setButtonToolTip( Ok, okToolTip );
  setButtonToolTip( Apply, i18n("Ignore the changes. You will not be prompted again.") );
  setButtonToolTip( Cancel, i18n("Do nothing. Next time you focus the file, "
      "or try to save it or close it, you will be prompted again.") );

  setCaption( title );

  QWidget *w = new QWidget(this);
  ui = new Ui::ModOnHdWidget();
  ui->setupUi( w );
  setMainWidget( w );

  ui->lblIcon->setPixmap( DesktopIcon("dialog-warning" ) );
  ui->lblText->setText( reason + "\n\n" + i18n("What do you want to do?") );

  // If the file isn't deleted, present a diff button, and a overwrite action.
  if ( modtype != KTextEditor::ModificationInterface::OnDiskDeleted )
  {
    setButtonGuiItem( User1, KStandardGuiItem::overwrite() );
    setButtonToolTip( User1, i18n("Overwrite the disk file with the editor content.") );
    connect( ui->btnDiff, SIGNAL(clicked()), this, SLOT(slotDiff()) );
  }
  else
  {
    ui->chkIgnoreWhiteSpaces->setVisible( false );
    ui->btnDiff->setVisible( false );
    showButton( User1, false );
  }
}

KateModOnHdPrompt::~KateModOnHdPrompt()
{
  delete m_proc;
  m_proc = 0;
  if (m_diffFile) {
    m_diffFile->setAutoRemove(true);
    delete m_diffFile;
    m_diffFile = 0;
  }
  delete ui;
}

void KateModOnHdPrompt::slotDiff()
{
  if (m_diffFile)
    return;

  m_diffFile = new KTemporaryFile();
  m_diffFile->open();

  // Start a QProcess that creates a diff
  m_proc = new QProcess( this );
  m_proc->setProcessChannelMode( QProcess::MergedChannels );
  connect( m_proc, SIGNAL(readyRead()), this, SLOT(slotDataAvailable()) );
  connect( m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotPDone()) );

  setCursor( Qt::WaitCursor );
  // disable the button and checkbox, to hinder the user to run it twice.
  ui->chkIgnoreWhiteSpaces->setEnabled( false );
  ui->btnDiff->setEnabled( false );

  QStringList procargs;
  procargs << QString(ui->chkIgnoreWhiteSpaces->isChecked() ? "-ub" : "-u")
     << "-" <<  m_doc->url().toLocalFile();
  qDebug() << "diff" << procargs;
  m_proc->start("diff", procargs);

  QTextStream ts(m_proc);
  int lastln = m_doc->lines() - 1;
  for ( int l = 0; l < lastln; ++l ) {
    ts << m_doc->line( l ) << '\n';
  }
  ts << m_doc->line(lastln);
  ts.flush();
  m_proc->closeWriteChannel();
}

void KateModOnHdPrompt::slotDataAvailable()
{
  m_diffFile->write(m_proc->readAll());
}

void KateModOnHdPrompt::slotPDone()
{
  setCursor( Qt::ArrowCursor );
  ui->chkIgnoreWhiteSpaces->setEnabled( true );
  ui->btnDiff->setEnabled( true );

  const QProcess::ExitStatus es = m_proc->exitStatus();
  delete m_proc;
  m_proc = 0;

  if ( es != QProcess::NormalExit )
  {
    KMessageBox::sorry( this,
                        i18n("The diff command failed. Please make sure that "
                             "diff(1) is installed and in your PATH."),
                        i18n("Error Creating Diff") );
    delete m_diffFile;
    m_diffFile = 0;
    return;
  }

  if ( m_diffFile->size() == 0 )
  {
    if (ui->chkIgnoreWhiteSpaces->isChecked()) {
      KMessageBox::information( this,
                                i18n("The files are identical."),
                                i18n("Diff Output") );
    } else {
      KMessageBox::information( this,
                                i18n("Ignoring amount of white space changed, the files are identical."),
                                i18n("Diff Output") );
    }
    delete m_diffFile;
    m_diffFile = 0;
    return;
  }

  m_diffFile->setAutoRemove(false);
  QString url = m_diffFile->fileName();
  delete m_diffFile;
  m_diffFile = 0;

  KToolInvocation::self()->startServiceForUrl( url, this, true );
}

void KateModOnHdPrompt::slotButtonClicked(int button)
{
  switch(button)
  {
    case Default:
    case Ok:
      done( (m_modtype == KTextEditor::ModificationInterface::OnDiskDeleted) ?
            Save : Reload );
      break;
    case Apply:
    {
      if ( KMessageBox::warningContinueCancel(
           this,
           i18n("Ignoring means that you will not be warned again (unless "
           "the disk file changes once more): if you save the document, you "
           "will overwrite the file on disk; if you do not save then the disk "
           "file (if present) is what you have."),
           i18n("You Are on Your Own"),
           KStandardGuiItem::cont(),
           KStandardGuiItem::cancel(),
           "kate_ignore_modonhd" ) != KMessageBox::Continue )
        return;
      done( Ignore );
      break;
    }
    case User1:
      done( Overwrite );
      break;
    default:
      KDialog::slotButtonClicked(button);
  }
}

//END KateModOnHdPrompt

// kate: space-indent on; indent-width 2; replace-tabs on;
