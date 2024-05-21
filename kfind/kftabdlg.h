/*******************************************************************
* kftabdlg.h
* 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of 
* the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* 
******************************************************************/

#ifndef KFTABDLG_H
#define KFTABDLG_H

#include <QButtonGroup>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QString>
#include <QDate>
#include <QRegExp>
#include <QLabel>
#include <QValidator>

#include <kurl.h>
#include <ktabwidget.h>
#include <kmimetype.h>

class KUrlComboBox;
class KLineEdit;
class KDialog;
class KComboBox;
class KIntNumInput;
class KfDirDialog;
class KDateCombo;

class KfindTabWidget: public KTabWidget
{
  Q_OBJECT

public:
  KfindTabWidget(QWidget * parent = 0);
  virtual ~KfindTabWidget();
  void initMimeTypes();
  void initSpecialMimeTypes();
  void setQuery(class KQuery * query);
  void setDefaults();

  void beginSearch();
  void endSearch();
  void loadHistory();
  void saveHistory();
  bool isSearchRecursive();

  void setURL( const KUrl & url );

  virtual QSize sizeHint() const;

public Q_SLOTS:
  void setFocus();
  void slotUpdateDateLabelsForNumber(int value);
  void slotUpdateDateLabelsForType(int index);
  void slotUpdateByteComboBox(int value);

private Q_SLOTS:
  void getDirectory();
  void fixLayout();
  void slotSizeBoxChanged(int);

Q_SIGNALS:
    void startSearch();

protected:
public:
  KComboBox   *nameBox;
  KUrlComboBox   *dirBox;
  // for first page
  QCheckBox   *subdirsCb;
  QCheckBox *hiddenFilesCb;
  // for third page
  KComboBox *typeBox;
  KLineEdit * textEdit;
  QCheckBox *caseSensCb;
  KComboBox *m_usernameBox;
  KComboBox *m_groupBox;
  //for fourth page
  KLineEdit *metainfoEdit;
  KLineEdit *metainfokeyEdit;

private:
  bool isDateValid();

  QString date2String(const QDate &);
  QDate &string2Date(const QString &, QDate * );

  void updateDateLabels(int type, int value);

  QWidget *pages[3];

  //1st page
  QPushButton *browseB;

  KfDirDialog *dirselector;

  //2nd page
  QCheckBox   *findCreated;
  KComboBox   *betweenType;
  QLabel      *andL;
  QButtonGroup *bg;
  QRadioButton *rb[2];
  KDateCombo * fromDate;
  KDateCombo * toDate;
  KIntNumInput *timeBox;

  //3rd page
  KComboBox *sizeBox;
  KComboBox *sizeUnitBox;
  KIntNumInput *sizeEdit;
  QCheckBox *caseContextCb;
  QCheckBox *binaryContextCb;
  QCheckBox *regexpContentCb;
  KDialog *regExpDialog;

  KUrl m_url;

  KMimeType::List m_types;
  QStringList m_ImageTypes;
  QStringList m_VideoTypes;
  QStringList m_AudioTypes;
};

class KDigitValidator : public QValidator
{
  Q_OBJECT

public:
  KDigitValidator(QWidget * parent);
  ~KDigitValidator();

  QValidator::State validate(QString & input, int &) const;

 private:
  QRegExp *r;
};

#endif
