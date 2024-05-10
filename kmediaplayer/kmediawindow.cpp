/*  This file is part of the KDE libraries
    Copyright (C) 2016 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <KIcon>
#include <KLocale>
#include <KDebug>
#include <KAction>
#include <KStandardShortcut>
#include <KStandardAction>
#include <KActionCollection>
#include <KCMultiDialog>
#include <KInputDialog>
#include <KFileDialog>
#include <KToolBar>
#include <KStatusBar>
#include <KMessageBox>
#include <KConfigGroup>
#include <Solid/PowerManagement>
#include <QApplication>
#include <QMenuBar>

#include "kmediawindow.h"

// NOTE: even tho the MIME type for some of these is different the comment is the same
static const char* s_mimetypes =
    "application/x-ogg "
    "application/sdp "
    "application/x-smil "
    "application/x-streamingmedia "
    "application/vnd.rn-realmedia "
    "application/vnd.rn-realmedia-vbr "
    "audio/x-aac "
    "audio/x-m4a "
    "audio/x-mp1 "
    "audio/x-mp2 "
    "audio/x-mp3 "
    "audio/x-mpeg "
    "audio/x-mpegurl "
    "audio/x-mpg "
    "audio/rn-mpeg "
    "audio/ogg "
    "audio/x-scpls "
    "audio/vnd.rn-realaudio "
    "audio/x-pn-windows-pcm "
    "audio/x-realaudio "
    "audio/x-pn-realaudio "
    "audio/x-ms-wma "
    "audio/x-pls "
    "audio/x-wav "
    "video/x-mpeg "
    "video/x-mpeg2 "
    "video/mp4 "
    "video/x-msvideo "
    "video/ogg "
    "video/quicktime "
    "video/vnd.rn-realvideo "
    "video/x-ms-afs "
    "video/x-ms-asf "
    "video/x-ms-wmv "
    "video/x-ms-wmx "
    "video/x-ms-wvxvideo "
    "video/x-avi "
    "video/x-fli "
    "video/x-flv "
    "video/x-theora "
    "video/x-matroska "
    "video/webm "
    "audio/x-flac "
    "audio/x-vorbis+ogg "
    "video/x-ogm+ogg "
    "audio/x-shorten "
    "audio/x-ape "
    "audio/x-wavpack "
    "audio/x-tta "
    "audio/AMR "
    "audio/ac3 "
    "video/mp2t "
    "audio/mp4";


KMediaWindow::KMediaWindow(QWidget *parent, Qt::WindowFlags flags)
    : KXmlGuiWindow(parent, flags),
    m_config(nullptr),
    m_player(nullptr),
    m_recentfiles(nullptr),
    m_menu(nullptr),
    m_currenttime(float(0.0)),
    m_playing(true),
    m_inhibition(0)
{
    m_config = new KConfig("kmediaplayerrc", KConfig::SimpleConfig);

    m_player = new KMediaWidget(this, KMediaWidget::AllOptions);
    m_player->player()->setPlayerID("kmediaplayer");
    connect(m_player->player(), SIGNAL(loaded()), this, SLOT(slotInhibit()));
    connect(m_player->player(), SIGNAL(paused(bool)), this, SLOT(slotMaybeInhibit(bool)));
    connect(m_player->player(), SIGNAL(finished()), this, SLOT(slotUninhibit()));

    setCentralWidget(m_player);

    KAction *action = actionCollection()->addAction(KStandardAction::Open, "file_open_path", this, SLOT(slotOpenPath()));
    action->setWhatsThis(i18n("Open a path."));

    action = actionCollection()->addAction("file_open_url", this, SLOT(slotOpenURL()));
    action->setText(i18n("Open URL..."));
    action->setIcon(KIcon("document-open-remote"));
    action->setWhatsThis(i18n("Open a URL."));

    action = actionCollection()->addAction(KStandardAction::Close, "file_close", this, SLOT(slotClosePath()));
    action->setWhatsThis(i18n("Close the current path/URL."));

    action = actionCollection()->addAction(KStandardAction::Quit, "file_quit", this, SLOT(slotQuit()));

    action = actionCollection()->addAction("settings_player", this, SLOT(slotConfigure()));
    action->setText(i18n("Configure KMediaPlayer"));
    action->setIcon(KIcon("preferences-desktop-sound"));
    action->setWhatsThis(i18n("Configure KMediaPlayer and applications that use it."));

    m_recentfiles = new KRecentFilesAction(KIcon("document-open-recent"), "Open recent", this);
    m_recentfiles->setShortcut(KStandardShortcut::shortcut(KStandardShortcut::OpenRecent));
    m_recentfiles->setWhatsThis(i18n("Open recently opened files."));
    connect(m_recentfiles, SIGNAL(urlSelected(KUrl)), this, SLOT(slotOpenURL(KUrl)));
    actionCollection()->addAction("file_open_recent", m_recentfiles);

    KConfigGroup firstrungroup(m_config, "KMediaPlayer");
    const bool firstrun = firstrungroup.readEntry("firstrun", true);
    if (firstrun) {
        // no toolbar unless explicitly enabled
        toolBar()->setVisible(false);
        // also set a decent window size
        resize(640, 480);
    }

    m_menu = new QMenu(this);
    m_menu->addAction(KIcon("show-menu"), i18n("Show/hide menubar"), this, SLOT(slotMenubar()));
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotMenu(QPoint)));

    setupGUI(KXmlGuiWindow::Keys | KXmlGuiWindow::StatusBar | KXmlGuiWindow::Save | KXmlGuiWindow::Create);
    setAutoSaveSettings();

    KConfigGroup recentfilesgroup(m_config, "RecentFiles");
    m_recentfiles->loadEntries(recentfilesgroup);
}

KMediaWindow::~KMediaWindow()
{
    saveAutoSaveSettings();

    KConfigGroup recentfilesgroup(m_config, "RecentFiles");
    m_recentfiles->saveEntries(recentfilesgroup);
    KConfigGroup firstrungroup(m_config, "KMediaPlayer");
    firstrungroup.writeEntry("firstrun", false);

    delete m_player;
    m_recentfiles->deleteLater();
    m_menu->deleteLater();
    delete m_config;
}

void KMediaWindow::slotOpenPath()
{
    const QString path = KFileDialog::getOpenFileName(
        KUrl("kfiledialog:///kmediaplayer"),
        QString::fromLatin1(s_mimetypes),
        this,
        i18n("Select paths")
    );
    if (!path.isEmpty()) {
        if (!m_player->player()->isPathSupported(path)) {
            KMessageBox::error(this, i18n("The path <tt>%1</tt> is not supported.", path), i18n("Invalid path"));
        } else {
            m_player->open(path);
            m_recentfiles->addUrl(KUrl(path));
        }
    }
}

void KMediaWindow::slotOpenURL()
{
    bool dummy = false;
    QString protocols = m_player->player()->protocols().join(QLatin1String(", "));
    KUrl url = KInputDialog::getText(
        i18n("Input URL"),
        i18n("Supported protocols are:\n\n%1", protocols),
        QString(), &dummy, this
    );
    if (!url.isEmpty()) {
        const QString urlstring = url.prettyUrl();
        if (!m_player->player()->isPathSupported(urlstring)) {
            KMessageBox::error(this, i18n("The URL <tt>%1</tt> is not supported.", urlstring), i18n("Invalid URL"));
        } else {
            m_player->open(urlstring);
            m_recentfiles->addUrl(url);
        }
    }
}

void KMediaWindow::slotOpenURL(KUrl url)
{
    m_player->open(url.prettyUrl());
    m_recentfiles->addUrl(url);
}

void KMediaWindow::slotClosePath()
{
    m_player->player()->stop();
    statusBar()->showMessage("");
}

void KMediaWindow::slotConfigure()
{
    KCMultiDialog kcmdialg(this);
    kcmdialg.addModule("kcmplayer");
    kcmdialg.exec();
}

void KMediaWindow::slotMenubar()
{
    menuBar()->setVisible(!menuBar()->isVisible());
}

void KMediaWindow::slotMenu(QPoint position)
{
    // it is bogus, just ignore it
    Q_UNUSED(position);
    m_menu->exec(QCursor::pos());
}

void KMediaWindow::slotQuit()
{
    slotUninhibit();

    KMediaWindow::close();
    qApp->quit();
}

void KMediaWindow::slotInhibit()
{
    if (!m_inhibition) {
        m_inhibition = Solid::PowerManagement::beginSuppressingScreenPowerManagement(QString::fromLatin1("KMediaPlayer playing"));
        if (!m_inhibition) {
            kWarning() << "Could not inhibit";
        }
    }
}

void KMediaWindow::slotMaybeInhibit(bool paused)
{
    if (paused) {
        slotUninhibit();
    } else {
        slotInhibit();
    }
}

void KMediaWindow::slotUninhibit()
{
    if (m_inhibition) {
        const bool diduninhibit = Solid::PowerManagement::stopSuppressingScreenPowerManagement(m_inhibition);
        if (diduninhibit) {
            m_inhibition = 0;
        } else {
            kWarning() << "Could not uninhibit";
        }
    }
}

void KMediaWindow::slotDelayedRestore()
{
    disconnect(m_player->player(), SIGNAL(loaded()), this, SLOT(slotDelayedRestore()));
    m_player->setPlay(int(m_playing));
    m_player->setPosition(m_currenttime);
}

void KMediaWindow::saveProperties(KConfigGroup &configgroup)
{
    const QString path = m_player->player()->path();
    const float currenttime = m_player->player()->currentTime();
    const bool playing = m_player->player()->isPlaying();
    configgroup.writeEntry("Path", path);
    configgroup.writeEntry("Position", currenttime);
    configgroup.writeEntry("Playing", playing);
}

void KMediaWindow::readProperties(const KConfigGroup &configgroup)
{
    const QString path = configgroup.readEntry("Path", QString());
    m_currenttime = configgroup.readEntry("Position", float(0.0));
    m_playing = configgroup.readEntry("Playing", true);
    kDebug() << path << m_currenttime << m_playing;
    if (!path.isEmpty()) {
        connect(m_player->player(), SIGNAL(loaded()), this, SLOT(slotDelayedRestore()));
        m_player->open(path);
    }
}
