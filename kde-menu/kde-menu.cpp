// -*- c-basic-offset: 3 -*-
/*  This file is part of the KDE libraries
 *  Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
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
 **/

#include <stdlib.h>

#include <QCoreApplication>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kservice.h>
#include <kservicegroup.h>
#include <kstandarddirs.h>
#include <ksycoca.h>
#include <kdebug.h>

static const char appName[] = "kde-menu";
static const char appVersion[] = "1.0";

static bool utf8 = false;
static bool bPrintMenuId = false;
static bool bPrintMenuName = false;

static QByteArray convert(const QString &txt)
{
    return (utf8 ? txt.toUtf8() : txt.toLocal8Bit());
}

static void result(const QString &txt)
{
    const QByteArray txtbytes = convert(txt);
    puts(txtbytes.constData());
}

static void error(int exitCode, const QString &txt)
{
    const QByteArray txtbytes = convert(txt);
    qWarning("kde-menu: %s", txtbytes.constData());
    exit(exitCode);
}

static void findMenuEntry(KServiceGroup::Ptr parent, const QString &name, const QString &menuId)
{
    const KServiceGroup::List list = parent->entries(true, true, false);
    KServiceGroup::List::ConstIterator it = list.constBegin();
    for (; it != list.constEnd(); ++it) {
        KSycocaEntry::Ptr e = (*it);

        if (e->isType(KST_KServiceGroup)) {
            KServiceGroup::Ptr g = KServiceGroup::Ptr::staticCast(e);

            findMenuEntry(g, name.isEmpty() ? g->caption() : name + '/' + g->caption(), menuId);
        } else if (e->isType(KST_KService)) {
            KService::Ptr s = KService::Ptr::staticCast(e);
            if (s->menuId() == menuId) {
                if (bPrintMenuId) {
                    result(parent->relPath());
                }
                if (bPrintMenuName) {
                    result(name);
                }
                exit(0);
            }
        }
    }
}

int main(int argc, char **argv)
{
    const char *description = I18N_NOOP(
        "KDE Menu query tool.\n"
        "This tool can be used to find in which menu a specific application is shown.\n"
    );

    KAboutData d(
        appName, "kde-menu", ki18n("kde-menu"), appVersion,
        ki18n(description),
        KAboutData::License_GPL, ki18n("(c) 2003 Waldo Bastian"));
    d.addAuthor(ki18n("Waldo Bastian"), ki18n("Author"), "bastian@kde.org");

    KCmdLineArgs::init(argc, argv, &d);

    KCmdLineOptions options;
    options.add("utf8", ki18n("Output data in UTF-8 instead of local encoding"));
    options.add("print-menu-id", ki18n("Print menu-id of the menu that contains\nthe application"));
    options.add("print-menu-name", ki18n("Print menu name (caption) of the menu that\ncontains the application"));
    options.add("+<application-id>", ki18n("The id of the menu entry to locate"));
    KCmdLineArgs::addCmdLineOptions(options);

    QCoreApplication app(argc, argv);

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if (args->count() != 1) {
        KCmdLineArgs::usageError(i18n("You must specify an application-id such as 'konsole.desktop'"));
    }

    utf8 = args->isSet("utf8");
    bPrintMenuId = args->isSet("print-menu-id");
    bPrintMenuName = args->isSet("print-menu-name");

    if (!bPrintMenuId && !bPrintMenuName) {
        KCmdLineArgs::usageError(i18n("You must specify at least one of --print-menu-id or --print-menu-name"));
    }

    QString menuId = args->arg(0);
    KService::Ptr s = KService::serviceByMenuId(menuId);

    if (!s) {
        error(1, i18n("No menu item '%1'.", menuId));
    }

    findMenuEntry(KServiceGroup::root(), "", menuId);

    error(2, i18n("Menu item '%1' not found in menu.", menuId));
    return 2;
}

