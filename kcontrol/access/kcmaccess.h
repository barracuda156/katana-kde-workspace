/**
 * kcmaccess.h
 *
 * Copyright (c) 2000 Matthias H�zer-Klpfel <hoelzer@kde.org>
 *
 */

#ifndef __kcmaccess_h__
#define __kcmaccess_h__


#include <kcmodule.h>
#include <knuminput.h>

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

class KColorButton;

class KAccessConfig : public KCModule
{
  Q_OBJECT

public:

  KAccessConfig( QWidget *parent, const QVariantList& );
  virtual ~KAccessConfig();

  void load();
  void save();
  void defaults();

protected Q_SLOTS:

  void configChanged();
  void checkAccess();
  void invertClicked();
  void flashClicked();
  void selectSound();
  void changeFlashScreenColor();
  void configureKNotify();

private:

  QCheckBox *systemBell, *customBell, *visibleBell;
  QRadioButton *invertScreen, *flashScreen;
  QLabel    *soundLabel;
  QLineEdit *soundEdit;
  QPushButton *soundButton;
  KColorButton *colorButton;
  QLabel *durationLabel;
  KDoubleNumInput *durationSlider;

  QCheckBox *stickyKeys, *stickyKeysLock, *stickyKeysAutoOff;
  QCheckBox *stickyKeysBeep, *toggleKeysBeep, *kNotifyModifiers;
  QPushButton *kNotifyModifiersButton;

  QCheckBox *slowKeys, *bounceKeys;
  QLabel *slowKeysLabel, *bounceKeysLabel;
  KDoubleNumInput *slowKeysDelay, *bounceKeysDelay;
  QCheckBox *slowKeysPressBeep, *slowKeysAcceptBeep;
  QCheckBox *slowKeysRejectBeep, *bounceKeysRejectBeep;

  QCheckBox *gestures, *gestureConfirmation;
  QCheckBox *timeout;
  QLabel *timeoutLabel;
  KIntNumInput *timeoutDelay;
  QCheckBox *accessxBeep, *kNotifyAccessX;
  QPushButton *kNotifyAccessXButton;
};


#endif
