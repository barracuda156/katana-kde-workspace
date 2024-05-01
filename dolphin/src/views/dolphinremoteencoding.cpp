/***************************************************************************
 *   Copyright (C) 2009 by Rahman Duran <rahman.duran@gmail.com>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

 /*
 * This code is largely based on the kremoteencodingplugin
 * Copyright (c) 2003 Thiago Macieira <thiago.macieira@kdemail.net>
 * Distributed under the same terms.
 */

#include "dolphinremoteencoding.h"
#include "dolphinviewactionhandler.h"

#include <KDebug>
#include <KActionMenu>
#include <KActionCollection>
#include <KIcon>
#include <KLocale>
#include <KGlobal>
#include <KMimeType>
#include <KConfig>
#include <KCharsets>
#include <KMenu>
#include <KProtocolInfo>
#include <KProtocolManager>
#include <KConfigGroup>

#define DATA_KEY        QLatin1String("Charset")

DolphinRemoteEncoding::DolphinRemoteEncoding(QObject* parent, DolphinViewActionHandler* actionHandler)
   :QObject(parent),
    m_actionHandler(actionHandler),
    m_loaded(false)
{
    m_menu = new KActionMenu(KIcon("character-set"), i18n("Select Remote Charset"), this);
    m_actionHandler->actionCollection()->addAction("change_remote_encoding", m_menu);
    connect(m_menu->menu(), SIGNAL(aboutToShow()),
          this, SLOT(slotAboutToShow()));

    m_menu->setEnabled(false);
    m_menu->setDelayed(false);
}

DolphinRemoteEncoding::~DolphinRemoteEncoding()
{
}

void DolphinRemoteEncoding::slotReload()
{
    loadSettings();
}

void DolphinRemoteEncoding::loadSettings()
{
    m_loaded = true;

    fillMenu();
}

void DolphinRemoteEncoding::slotAboutToOpenUrl()
{
    KUrl oldURL = m_currentURL;
    m_currentURL = m_actionHandler->currentView()->url();

    if (m_currentURL.protocol() != oldURL.protocol()) {
        // This works on ftp, sftp, etc.
        if (!m_currentURL.isLocalFile() && KProtocolManager::isSourceProtocol(m_currentURL)) {
            m_menu->setEnabled(true);
            loadSettings();
        } else {
            m_menu->setEnabled(false);
        }
        return;
    }

    if (m_currentURL.host() != oldURL.host()) {
        updateMenu();
    }
}

void DolphinRemoteEncoding::fillMenu()
{
    KMenu* menu = m_menu->menu();
    menu->clear();

#warning TODO: split into sub-menus based on script
    foreach (const QString &description, KGlobal::charsets()->descriptiveEncodingNames()) {
        QAction* action = new QAction(description, this);
        action->setCheckable(true);
        action->setData(KGlobal::charsets()->encodingForName(description));
        menu->addAction(action);
    }
    menu->addSeparator();

    menu->addAction(i18n("Reload"), this, SLOT(slotReload()), 0);
    menu->addAction(i18n("Default"), this, SLOT(slotDefault()), 0)->setCheckable(true);

    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(slotItemSelected(QAction*)));
}

void DolphinRemoteEncoding::updateMenu()
{
    if (!m_loaded) {
        loadSettings();
    }

    const QList<QAction*> menuActions = m_menu->menu()->actions();
    // uncheck everything
    foreach (QAction *action, menuActions) {
        action->setChecked(false);
    }

    const QString charset = KProtocolManager::charsetFor(m_currentURL);;
    if (!charset.isEmpty()) {
        bool isFound = false;
        foreach (QAction* action, menuActions) {
            if (action->data().toString() == charset) {
                isFound = true;
                action->setChecked(true);
                break;
            }
        }

        kDebug() << "URL=" << m_currentURL << " charset=" << charset;

        if (!isFound) {
            kWarning() << "could not find entry for charset=" << charset ;
        }
    } else {
        m_menu->menu()->actions().last()->setChecked(true);
    }

}

void DolphinRemoteEncoding::slotAboutToShow()
{
    if (!m_loaded) {
        loadSettings();
    }
    updateMenu();
}

void DolphinRemoteEncoding::slotItemSelected(QAction* action)
{
    if (action) {
        if (action->isChecked()) {
            KConfig config(("kio_" + m_currentURL.protocol() + "rc").toLatin1());
            QString host = m_currentURL.host();
            QString charset = action->data().toString();
            KConfigGroup cg(&config, host);
            cg.writeEntry(DATA_KEY, charset);
            config.sync();

            // Update the io-slaves...
            updateView();
        }
    }
}

void DolphinRemoteEncoding::slotDefault()
{
    // We have no choice but delete all higher domain level
    // settings here since it affects what will be matched.
    KConfig config(("kio_" + m_currentURL.protocol() + "rc").toLatin1());

    QStringList partList = m_currentURL.host().split('.', QString::SkipEmptyParts);
    if (!partList.isEmpty()) {
        partList.erase(partList.begin());

        QStringList domains;
        // Remove the exact name match...
        domains << m_currentURL.host();

        while (!partList.isEmpty()) {
            if (partList.count() == 2) {
                if (partList[0].length() <= 2 && partList[1].length() == 2) {
                    break;
                }
            }

            if (partList.count() == 1) {
                break;
            }

            domains << partList.join(".");
            partList.erase(partList.begin());
        }

        for (QStringList::const_iterator it = domains.constBegin(); it != domains.constEnd();++it) {
            kDebug() << "Domain to remove: " << *it;
            if (config.hasGroup(*it)) {
                config.deleteGroup(*it);
            } else if (config.group("").hasKey(*it)) {
                config.group("").deleteEntry(*it); //don't know what group name is supposed to be XXX
            }
        }
    }
    config.sync();

    // Update the io-slaves.
    updateView();
}

void DolphinRemoteEncoding::updateView()
{
    KProtocolManager::reparseConfiguration();
    // Reload the page with the new charset
    m_actionHandler->currentView()->setUrl(m_currentURL);
    m_actionHandler->currentView()->reload();
}

#include "moc_dolphinremoteencoding.cpp"
