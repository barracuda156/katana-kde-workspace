/*  This file is part of the KDE libraries and the Kate part.
 *
 *  Copyright (C) 2002-2010 Anders Lund <anders@alweb.dk>
 *
 *  Rewritten based on code of Copyright (c) 2002 Michael Goffioul <kdeprint@swing.be>
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

#ifndef __KATE_PRINTER_H__
#define __KATE_PRINTER_H__

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPrinter>

class KateDocument;
class KColorButton;
class KLineEdit;
class KIntNumInput;
class KComboBox;
class KHBox;

class KatePrinter
{
  public:
    static bool print (KateDocument *doc);
    static void readSettings(QPrinter& printer);
    static void writeSettings(QPrinter& printer);
};

//BEGIN Text settings
/*
  Text settings page:
  - Print Line Numbers
    () Smart () Yes () No
*/
class KatePrintTextSettings : public QWidget
{
  Q_OBJECT
  public:
    explicit KatePrintTextSettings( QWidget *parent=0 );
    ~KatePrintTextSettings();

    bool printLineNumbers();
    bool printGuide();

  private:
    void readSettings();
    void writeSettings();
    
    QCheckBox *cbLineNumbers;
    QCheckBox *cbGuide;
};
//END Text Settings

//BEGIN Header/Footer
/*
  Header & Footer page:
  - enable header/footer
  - header/footer props
    o formats
    o colors
*/

class KatePrintHeaderFooter : public QWidget
{
  Q_OBJECT
  public:
    explicit KatePrintHeaderFooter( QWidget *parent=0 );
    ~KatePrintHeaderFooter();

    QFont font();

    bool useHeader();
    QStringList headerFormat();
    QColor headerForeground();
    QColor headerBackground();
    bool useHeaderBackground();

    bool useFooter();
    QStringList footerFormat();
    QColor footerForeground();
    QColor footerBackground();
    bool useFooterBackground();

  public Q_SLOTS:
    void setHFFont();
    void showContextMenu(const QPoint& pos);

  private:
    void readSettings();
    void writeSettings();
    
    QCheckBox *cbEnableHeader, *cbEnableFooter;
    KHBox *hbFontPreview;
    QLabel *lFontPreview;
    QGroupBox *gbHeader, *gbFooter;
    KLineEdit *leHeaderLeft, *leHeaderCenter, *leHeaderRight;
    KColorButton *kcbtnHeaderFg, *kcbtnHeaderBg;
    QCheckBox *cbHeaderEnableBgColor;
    KLineEdit *leFooterLeft, *leFooterCenter, *leFooterRight;
    KColorButton *kcbtnFooterFg, *kcbtnFooterBg;
    QCheckBox *cbFooterEnableBgColor;
};

//END Header/Footer

//BEGIN Layout
/*
  Layout page:
  - Color scheme
  - Use Box
  - Box properties
    o Width
    o Margin
    o Color
*/
class KatePrintLayout : public QWidget
{
  Q_OBJECT
  public:
    explicit KatePrintLayout( QWidget *parent=0 );
    ~KatePrintLayout();

    QString colorScheme();
    bool useBackground();
    bool useBox();
    int boxWidth();
    int boxMargin();
    QColor boxColor();

  private:
    void readSettings();
    void writeSettings();
    
    KComboBox *cmbSchema;
    QCheckBox *cbEnableBox, *cbDrawBackground;
    QGroupBox *gbBoxProps;
    KIntNumInput *sbBoxWidth, *sbBoxMargin;
    KColorButton* kcbtnBoxColor;
};
//END Layout

#endif
