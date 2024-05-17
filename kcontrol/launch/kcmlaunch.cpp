/*
 *  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 */

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

//Added by qt3to4:
#include <QVBoxLayout>
#include <QGridLayout>

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kdialog.h>
#include <knuminput.h>

#include "kcmlaunch.h"
#include "kwin_interface.h"
#include <KPluginFactory>
#include <KPluginLoader>

static bool s_busycurosr = true;

K_PLUGIN_FACTORY(LaunchFactory, registerPlugin<LaunchConfig>();)
K_EXPORT_PLUGIN(LaunchFactory("kcmlaunch"))

LaunchConfig::LaunchConfig(QWidget * parent, const QVariantList &)
  : KCModule(LaunchFactory::componentData(), parent)
{
    QVBoxLayout* Form1Layout = new QVBoxLayout(this);
    Form1Layout->setMargin(0);

    setQuickHelp(i18n("<h1>Launch Feedback</h1>"
        " You can configure the application-launch feedback here."));

    QGroupBox* GroupBox1 = new QGroupBox(i18n("Bus&y Cursor"));
    GroupBox1->setWhatsThis( i18n(
        "<h1>Busy Cursor</h1>\n"
        "KDE offers a busy cursor for application startup notification.\n"
        "To enable the busy cursor, select one kind of visual feedback\n"
        "from the combobox.\n"
        "It may occur, that some applications are not aware of this startup\n"
        "notification. In this case, the cursor stops blinking after the time\n"
        "given in the section 'Startup indication timeout'"));

    QGridLayout* GroupBox1Layout = new QGridLayout();
    GroupBox1->setLayout( GroupBox1Layout );
    Form1Layout->addWidget(GroupBox1);

    GroupBox1Layout->setColumnStretch(1, 1);

    cb_busyCursor = new QComboBox(GroupBox1);
    cb_busyCursor->setObjectName("cb_busyCursor");
    cb_busyCursor->insertItem(0, i18n( "No Busy Cursor"));
    cb_busyCursor->insertItem(1, i18n( "Passive Busy Cursor"));
    GroupBox1Layout->addWidget( cb_busyCursor, 0, 0 );
    connect(cb_busyCursor, SIGNAL(activated(int)), this, SLOT (slotBusyCursor(int)));
    connect(cb_busyCursor, SIGNAL(activated(int)), this, SLOT(checkChanged()));

    lbl_cursorTimeout = new QLabel( GroupBox1 );
    lbl_cursorTimeout->setObjectName("TextLabel1" );
    lbl_cursorTimeout->setText(i18n("&Startup indication timeout:"));
    GroupBox1Layout->addWidget(lbl_cursorTimeout, 2, 0);
    sb_cursorTimeout = new KIntNumInput( GroupBox1);
    sb_cursorTimeout->setRange(1, 99);
    sb_cursorTimeout->setSuffix(i18n(" sec"));
    GroupBox1Layout->addWidget(sb_cursorTimeout, 2, 1);
    lbl_cursorTimeout->setBuddy(sb_cursorTimeout);
    connect(sb_cursorTimeout, SIGNAL(valueChanged(int)), this, SLOT(checkChanged()));

    Form1Layout->addStretch();
}

LaunchConfig::~LaunchConfig()
{
}

void LaunchConfig::slotBusyCursor(int i)
{
    lbl_cursorTimeout->setEnabled(i != 0);
    sb_cursorTimeout->setEnabled(i != 0);
}

void LaunchConfig::load()
{
  KConfig conf("klaunchrc", KConfig::NoGlobals);
  KConfigGroup c = conf.group("FeedbackStyle");

  bool busyCursor = c.readEntry("BusyCursor", s_busycurosr);

  c= conf.group("BusyCursorSettings");
  sb_cursorTimeout->setValue(c.readEntry( "Timeout", 10));
  if ( !busyCursor )
     cb_busyCursor->setCurrentIndex(0);
  else
     cb_busyCursor->setCurrentIndex(1);

  slotBusyCursor(cb_busyCursor->currentIndex());

  emit changed(false);
}

  void
LaunchConfig::save()
{
  KConfig conf("klaunchrc", KConfig::NoGlobals);
  KConfigGroup  c = conf.group("FeedbackStyle");

  c.writeEntry("BusyCursor",   cb_busyCursor->currentIndex() != 0);

  c = conf.group("BusyCursorSettings");
  c.writeEntry("Timeout", sb_cursorTimeout->value());

  c.sync();

  emit changed(false);

  org::kde::KWin kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
  kwin.reconfigureEffect("kwin4_effect_startupfeedback");
  
}

  void
LaunchConfig::defaults()
{
    cb_busyCursor->setCurrentIndex(2);
    sb_cursorTimeout->setValue(10);
    slotBusyCursor(2);

    checkChanged();
}

void LaunchConfig::checkChanged()
{
    KConfig conf("klaunchrc", KConfig::NoGlobals);
    KConfigGroup c = conf.group("FeedbackStyle");

    bool savedBusyCursor = c.readEntry("BusyCursor", s_busycurosr);
    c = conf.group("BusyCursorSettings");
    unsigned int savedCursorTimeout = c.readEntry("Timeout", 10);

    bool newBusyCursor = (cb_busyCursor->currentIndex() != 0);
    unsigned int newCursorTimeout = sb_cursorTimeout->value();

    emit changed(savedBusyCursor != newBusyCursor || savedCursorTimeout  != newCursorTimeout);
}

#include "moc_kcmlaunch.cpp"
