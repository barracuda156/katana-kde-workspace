/***************************************************************************
                          componentchooserwm.h  -  description
                             -------------------
    copyright            : (C) 2002 by Joseph Wenninger
    email                : jowenn@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundationi                            *
 *                                                                         *
 ***************************************************************************/

#ifndef _COMPONENTCHOOSERWM_H_
#define _COMPONENTCHOOSERWM_H_

#include "ui_wmconfig_ui.h"
#include "componentchooser.h"

class CfgWm: public QWidget, public Ui::WmConfig_UI, public CfgPlugin
{
Q_OBJECT
public:
    CfgWm(QWidget *parent);
    virtual ~CfgWm();
    virtual void load(KConfig *cfg);
    virtual void save(KConfig *cfg);
    virtual void defaults();

protected Q_SLOTS:
    void configChanged();
    void configureWm();
    void checkConfigureWm();

Q_SIGNALS:
    void changed(bool);
private:
    bool tryWmLaunch();
    void loadWMs( const QString& current );
    QString currentWm() const;
    bool saveAndConfirm();
    struct WmData {
        QString internalName;
        QString exec;
        QString configureCommand;
        QString restartArgument;
        QString parentArgument;
    };
    WmData currentWmData() const;
    QHash< QString, WmData > wms; // i18n text -> data
    QString oldwm; // the original value
};

#endif
