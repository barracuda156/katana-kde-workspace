/***************************************************************************
                          kdesudo.cpp  -  description
                             -------------------
    begin                : Sam Feb 15 15:42:12 CET 2003
    copyright            : (C) 2003 by Robert Gruber <rgruber@users.sourceforge.net>
                           (C) 2007 by Martin Böhm <martin.bohm@kubuntu.org>
                                       Anthony Mercatante <tonio@kubuntu.org>
                                       Canonical Ltd (Jonathan Riddell <jriddell@ubuntu.com>)

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KDESUDO_H
#define KDESUDO_H

#include <QProcess>
#include <QWidget>

#include <kpassworddialog.h>
#include <knewpassworddialog.h>

/*
* KdeSudo is the base class of the project
*
* @version 3.1
*/

class KdeSudo : QObject
{
    Q_OBJECT
public:
    KdeSudo(const QString &icon = QString(), const QString &generic = QString());
    ~KdeSudo();

private slots:
    /**
     * This slot gets executed if sudo creates some output
     * -- well, in theory it should. Even though the code
     *  seems to be doing what the API says, it doesn't
     *  yet do what we need.
     **/
    void parseOutput();

    /**
     * This slot gets exectuted when sudo exits
     **/
    void procExited(int exitCode);

    /**
     * This slot overrides the slot from KPasswordDialog
     * @see KPasswordDialog
     **/
    void pushPassword(const QString &);
    void slotCancel();

private:
    static QString validArg(QString arg);
    void error(const QString &);

    QProcess *m_process;
    bool m_error;
    QString m_tmpName;
    KPasswordDialog *m_dialog;
};

#endif // KDESUDO_H
