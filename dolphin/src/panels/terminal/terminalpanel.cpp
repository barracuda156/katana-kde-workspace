/***************************************************************************
 *   Copyright (C) 2007-2010 by Peter Penz <peter.penz19@gmail.com>        *
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

#include "terminalpanel.h"

#include <signal.h>

#include <KPluginLoader>
#include <KPluginFactory>
#include <KService>
#include <kde_terminal_interface.h>
#include <KParts/Part>
#include <KShell>

#include <QBoxLayout>
#include <QDir>
#include <QtGui/qevent.h>

TerminalPanel::TerminalPanel(QWidget* parent) :
    Panel(parent),
    m_clearTerminal(true),
    m_layout(0),
    m_terminal(0),
    m_terminalWidget(0),
    m_konsolePart(0),
    m_konsolePartCurrentDirectory()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setMargin(0);
}

TerminalPanel::~TerminalPanel()
{
}

void TerminalPanel::terminalExited()
{
    m_terminal = 0;
    emit hideTerminalPanel();
}

void TerminalPanel::dockVisibilityChanged()
{
    // Only react when the DockWidget itself (not some parent) is hidden. This way we don't
    // respond when e.g. Dolphin is minimized.
    if (parentWidget() && parentWidget()->isHidden() &&
        m_terminal && (m_terminal->foregroundProcessId() == -1)) {
        // Make sure that the following "cd /" command will not affect the view.
        disconnect(m_konsolePart, SIGNAL(currentDirectoryChanged(QString)),
                   this, SLOT(slotKonsolePartCurrentDirectoryChanged(QString)));

        // Make sure this terminal does not prevent unmounting any removable drives
        changeDir(KUrl::fromPath("/"));

        // Because we have disconnected from the part's currentDirectoryChanged()
        // signal, we have to update m_konsolePartCurrentDirectory manually. If this
        // was not done, showing the panel again might not set the part's working
        // directory correctly.
        m_konsolePartCurrentDirectory = '/';
    }
}

bool TerminalPanel::urlChanged()
{
    if (!url().isValid()) {
        return false;
    }

    const bool sendInput = m_terminal && (m_terminal->foregroundProcessId() == -1) && isVisible();
    if (sendInput) {
        changeDir(url());
    }

    return true;
}

void TerminalPanel::showEvent(QShowEvent* event)
{
    if (event->spontaneous()) {
        Panel::showEvent(event);
        return;
    }

    if (!m_terminal) {
        m_clearTerminal = true;
        KPluginFactory* factory = KPluginLoader("konsolepart").factory();
        m_konsolePart = factory ? (factory->create<KParts::ReadOnlyPart>(this)) : 0;
        if (m_konsolePart) {
            connect(m_konsolePart, SIGNAL(destroyed(QObject*)), this, SLOT(terminalExited()));
            m_terminalWidget = m_konsolePart->widget();
            m_layout->addWidget(m_terminalWidget);
            m_terminal = qobject_cast<TerminalInterface *>(m_konsolePart);
        }
    }
    if (m_terminal) {
        m_terminal->showShellInDir(url().toLocalFile());
        changeDir(url());
        m_terminalWidget->setFocus();
        connect(m_konsolePart, SIGNAL(currentDirectoryChanged(QString)),
                this, SLOT(slotKonsolePartCurrentDirectoryChanged(QString)));
    }

    Panel::showEvent(event);
}

void TerminalPanel::changeDir(const KUrl& url)
{
    if (url.isLocalFile()) {
        sendCdToTerminal(url.toLocalFile());
    }
}

void TerminalPanel::sendCdToTerminal(const QString& dir)
{
    if (dir == m_konsolePartCurrentDirectory) {
        m_clearTerminal = false;
        return;
    }

    if (!m_clearTerminal) {
        // The Terminal interface does not provide a way to delete the
        // current line before sending a new input. This is mandatory,
        // otherwise sending a 'cd x' to a existing 'rm -rf *' might
        // result in data loss. As workaround SIGINT is send.
        const int processId = m_terminal->terminalProcessId();
        if (processId > 0) {
            kill(processId, SIGINT);
        }
    }

    m_terminal->sendInput(" cd " + KShell::quoteArg(dir) + '\n');
    m_konsolePartCurrentDirectory = dir;

    if (m_clearTerminal) {
        m_terminal->sendInput(" clear\n");
        m_clearTerminal = false;
    }
}

void TerminalPanel::slotKonsolePartCurrentDirectoryChanged(const QString& dir)
{
    m_konsolePartCurrentDirectory = dir;

    // Only change the view URL if 'dir' is different from the current view URL.
    // Note that the current view URL could also be a symbolic link to 'dir'
    // -> use QDir::canonicalPath() to check that.
    const KUrl oldUrl(url());
    const KUrl newUrl(dir);
    if (newUrl != oldUrl && dir != QDir(oldUrl.path()).canonicalPath()) {
        emit changeUrl(newUrl);
    }
}

#include "moc_terminalpanel.cpp"
