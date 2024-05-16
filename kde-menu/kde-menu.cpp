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

#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtDBus/QtDBus>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kservice.h>
#include <kservicegroup.h>
#include <kstandarddirs.h>
#include <ksycoca.h>

static const char appName[] = "kde-menu";
static const char appVersion[] = "1.0";
static bool utf8;

static bool bPrintMenuId;
static bool bPrintMenuName;

static void result(const QString &txt)
{
   if (utf8)
      puts( txt.toUtf8() );
   else
      puts( txt.toLocal8Bit() );
}

static void error(int exitCode, const QString &txt)
{
   qWarning("kde-menu: %s", txt.toLocal8Bit().data());
   exit(exitCode);
}

static void findMenuEntry(KServiceGroup::Ptr parent, const QString &name, const QString &menuId)
{
   const KServiceGroup::List list = parent->entries(true, true, false);
   KServiceGroup::List::ConstIterator it = list.constBegin();
   for (; it != list.constEnd(); ++it)
   {
      KSycocaEntry::Ptr e = (*it);

      if (e->isType(KST_KServiceGroup))
      {
         KServiceGroup::Ptr g = KServiceGroup::Ptr::staticCast( e );

         findMenuEntry(g, name.isEmpty() ? g->caption() : name+'/'+g->caption(), menuId);
      }
      else if (e->isType(KST_KService))
      {
         KService::Ptr s = KService::Ptr::staticCast( e );
         if (s->menuId() == menuId)
         {
            if (bPrintMenuId)
            {
               result(parent->relPath());
            }
            if (bPrintMenuName)
            {
               result(name);
            }
            exit(0);
         }
      }
   }
}


int main(int argc, char **argv)
{
   const char *description = I18N_NOOP("KDE Menu query tool.\n"
   "This tool can be used to find in which menu a specific application is shown.\n"
   "The --highlight option can be used to visually indicate to the user where\n"
   "in the KDE menu a specific application is located.");

   KAboutData d(appName, "kde-menu", ki18n("kde-menu"), appVersion,
                ki18n(description),
                KAboutData::License_GPL, ki18n("(c) 2003 Waldo Bastian"));
   d.addAuthor(ki18n("Waldo Bastian"), ki18n("Author"), "bastian@kde.org");

   KCmdLineArgs::init(argc, argv, &d);

   KCmdLineOptions options;
   options.add("utf8", ki18n("Output data in UTF-8 instead of local encoding"));
   options.add("print-menu-id", ki18n("Print menu-id of the menu that contains\nthe application"));
   options.add("print-menu-name", ki18n("Print menu name (caption) of the menu that\ncontains the application"));
   options.add("nocache-update", ki18n("Do not check if sycoca database is up to date"));
   options.add("+<application-id>", ki18n("The id of the menu entry to locate"));
   KCmdLineArgs::addCmdLineOptions(options);

   QCoreApplication app(argc, argv);

   KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
   if (args->count() != 1)
      KCmdLineArgs::usageError(i18n("You must specify an application-id such as 'konsole.desktop'"));

   utf8 = args->isSet("utf8");

   bPrintMenuId = args->isSet("print-menu-id");
   bPrintMenuName = args->isSet("print-menu-name");

   if (!bPrintMenuId && !bPrintMenuName)
      KCmdLineArgs::usageError(i18n("You must specify at least one of --print-menu-id or --print-menu-name"));

   if (args->isSet("cache-update"))
   {
      QStringList args;
      args.append("--incremental");
      args.append("--checkstamps");
      QString command = KStandardDirs::findExe(KBUILDSYCOCA_EXENAME);
      if (command.isEmpty())
      {
         error(4, i18n("Could not find '%1' executable.", QLatin1String(KBUILDSYCOCA_EXENAME)));
      }
      if (QProcess::execute(command, args) != 0) {
         error(5, i18n("Could not execute '%1'.", QLatin1String(KBUILDSYCOCA_EXENAME)));
      }
   }

   QString menuId = args->arg(0);
   KService::Ptr s = KService::serviceByMenuId(menuId);

   if (!s)
      error(1, i18n("No menu item '%1'.", menuId));

   findMenuEntry(KServiceGroup::root(), "", menuId);

   error(2, i18n("Menu item '%1' not found in menu.", menuId));
   return 2;
}

