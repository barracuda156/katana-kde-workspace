/***************************************************************************
 *   Copyright (C) 2006-2007 by Stephen Leaf                               *
 *   smileaf@gmail.com                                                     *
 *   Copyright (C) 2008 by Montel Laurent <montel@kde.org>                 *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA          *
 ***************************************************************************/

#include "autostart.h"
#include "autostartitem.h"
#include "advanceddialog.h"

#include <QDir>
#include <QTreeWidget>
#include <QStringList>

#include <KLocale>
#include <KConfig>
#include <KConfigGroup>
#include <KGlobalSettings>
#include <KShell>
#include <KStandardDirs>
#include <KOpenWithDialog>
#include <KPropertiesDialog>
#include <KDesktopFile>
#include <KMessageBox>
#include <KAboutData>
#include <KDebug>
#include <KIO/NetAccess>
#include <KIO/DeleteJob>
#include <KIO/CopyJob>


K_PLUGIN_FACTORY(AutostartFactory, registerPlugin<Autostart>();)
    K_EXPORT_PLUGIN(AutostartFactory( "kcmautostart", "kcm_autostart" ))

    Autostart::Autostart( QWidget* parent, const QVariantList& )
        : KCModule( AutostartFactory::componentData(), parent )
{
    widget = new Ui_AutostartConfig();
    widget->setupUi(this);

    QStringList lstHeader;
    lstHeader << i18n( "Name" )
              << i18n( "Command" )
              << i18n( "Status" );
    widget->listCMD->setHeaderLabels(lstHeader);
    widget->listCMD->setFocus();

    setButtons(Help);

    connect( widget->btnAddProgram, SIGNAL(clicked()), SLOT(slotAddProgram()) );
    connect( widget->btnRemove, SIGNAL(clicked()), SLOT(slotRemoveCMD()) );
    connect( widget->btnAdvanced, SIGNAL(clicked()), SLOT(slotAdvanced()) );
    connect( widget->listCMD, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), SLOT(slotEditCMD(QTreeWidgetItem*)) );
    connect( widget->listCMD, SIGNAL(itemClicked(QTreeWidgetItem*,int)),this,SLOT(slotItemClicked(QTreeWidgetItem*,int)) );
    connect( widget->btnProperties, SIGNAL(clicked()), SLOT(slotEditCMD()) );
    connect( widget->listCMD, SIGNAL(itemSelectionChanged()), SLOT(slotSelectionChanged()) );


    KAboutData* about = new KAboutData("Autostart", 0, ki18n("KDE Autostart Manager"), "1.0",
                                       ki18n("KDE Autostart Manager Control Panel Module"),
                                       KAboutData::License_GPL,
                                       ki18n("Copyright © 2006–2010 Autostart Manager team"));
    about->addAuthor(ki18n("Stephen Leaf"), KLocalizedString(), "smileaf@gmail.com");
    about->addAuthor(ki18n("Montel Laurent"), KLocalizedString(), "montel@kde.org");
    about->addAuthor(ki18n("Ivailo Monev"), ki18n( "Maintainer" ), "xakepa10@gmail.com");
    setAboutData( about );

}

Autostart::~Autostart()
{
   delete widget;
}


void Autostart::slotItemClicked(QTreeWidgetItem *item, int col)
{
    if ( item && col == COL_STATUS ) {
        AutoStartItem *entry = dynamic_cast<AutoStartItem*>(item);
        if (entry) {
            bool disable = (item->checkState(col) == Qt::Unchecked );
            KDesktopFile kc(entry->fileName().path());
            KConfigGroup grp = kc.desktopGroup();
            if (grp.hasKey("Hidden") && !disable) {
                grp.deleteEntry("Hidden");
            } else {
                grp.writeEntry("Hidden", disable);
            }

            kc.sync();
            if (disable) {
                item->setText(COL_STATUS, i18nc("The program won't be run", "Disabled"));
            } else {
                item->setText(COL_STATUS, i18nc("The program will be run", "Enabled"));
            }
        }
    }
}

void Autostart::addItem(AutoStartItem* item, const QString& name, const QString& command, bool disabled)
{
    Q_ASSERT( item );
    item->setText(COL_NAME, name);
    item->setText(COL_COMMAND, command);
    item->setCheckState( COL_STATUS, disabled ? Qt::Unchecked : Qt::Checked);
    item->setText(COL_STATUS, disabled ? i18nc("The program won't be run", "Disabled") : i18nc("The program will be run", "Enabled"));

    // It is important to ensure that we make an exact copy of an existing
    // desktop file (if selected) to enable users to override global autostarts.
    // Also see:
    // https://bugs.launchpad.net/ubuntu/+source/kde-workspace/+bug/923360
    //
    // However, if program that has non-local .desktop file is uninstalled the
    // local autostart .desktop will be invalid which is why it is not done and
    // if the file is not writable it is simply disabled
    const QFileInfo fileinfo(item->fileName().path());
    item->setDisabled(!fileinfo.isWritable());
}

void Autostart::load()
{
    // share/autostart may *only* contain .desktop files
    m_paths << componentData().dirs()->resourceDirs("autostart"); //xdg-config autostart dir

    widget->listCMD->clear();

    m_programItem = new QTreeWidgetItem(widget->listCMD);
    m_programItem->setText(0, i18n( "Desktop File"));
    m_programItem->setFlags(m_programItem->flags() ^ Qt::ItemIsSelectable);

    QFont boldFont =  m_programItem->font(0);
    boldFont.setBold(true);
    m_programItem->setData(0, Qt::FontRole, boldFont);

    widget->listCMD->expandItem(m_programItem);

    foreach (const QString& path, m_paths) {
        if (!KStandardDirs::exists(path)) {
            continue;
        }

        QDir autostartdir( path );
        autostartdir.setFilter( QDir::Files );
        const QFileInfoList list = autostartdir.entryInfoList();
        for (int i = 0; i < list.size(); ++i) {
            QFileInfo fi = list.at(i);
            QString filename = fi.fileName();
            bool desktopFile = filename.endsWith(".desktop");
            if (desktopFile) {
                KDesktopFile config(fi.absoluteFilePath());
                // kDebug() << fi.absoluteFilePath() << "trying" << config.desktopGroup().readEntry("Exec");
                QStringList commandLine = KShell::splitArgs(config.desktopGroup().readEntry("Exec"));
                if (commandLine.isEmpty()) {
                    continue;
                }

                const QString exe = commandLine.first();
                if (exe.isEmpty() || KStandardDirs::findExe(exe).isEmpty()) {
                    continue;
                }

                AutoStartItem *item = new AutoStartItem(fi.absoluteFilePath(), m_programItem);

                const KConfigGroup grp = config.desktopGroup();
                const bool hidden = grp.readEntry("Hidden", false);
                const QStringList notShowList = grp.readXdgListEntry("NotShowIn");
                const QStringList onlyShowList = grp.readXdgListEntry("OnlyShowIn");

                const bool disabled = hidden ||
                                      notShowList.contains("KDE") ||
                                      (!onlyShowList.isEmpty() && !onlyShowList.contains("KDE"));

                int indexPath = m_paths.indexOf(item->fileName().directory() + '/');
                if (indexPath > 0) {
                    indexPath = 0; // .config/autostart load destkop at startup
                }
                addItem(item, config.readName(), grp.readEntry("Exec"), disabled);
            }
        }
    }
    // Update button
    slotSelectionChanged();
    widget->listCMD->resizeColumnToContents(COL_NAME);
    // widget->listCMD->resizeColumnToContents(COL_COMMAND);
    widget->listCMD->resizeColumnToContents(COL_STATUS);
}

void Autostart::slotAddProgram()
{
    KOpenWithDialog owdlg( this );
    if (owdlg.exec() != QDialog::Accepted) {
        return;
    }

    KService::Ptr service = owdlg.service();

    Q_ASSERT(service);
    if (!service) {
        return; // Don't crash if KOpenWith wasn't able to create service.
    }

    QString desktopPath;
    KUrl desktopTemplate;
    const QString localautostartdir = KGlobal::dirs()->saveLocation("autostart");
    if (service->desktopEntryName().isEmpty() ) {
        // Build custom desktop file (e.g. when the user entered an executable
        // name in the OpenWithDialog).
        desktopPath = localautostartdir + service->name() + ".desktop";
        desktopTemplate = KUrl(desktopPath);
        KConfig kc(desktopTemplate.path(), KConfig::SimpleConfig);
        KConfigGroup kcg = kc.group("Desktop Entry");
        kcg.writeEntry("Name", service->exec());
        kcg.writeEntry("Exec", service->exec());
        kcg.writeEntry("Icon", "system-run");
        kcg.writeEntry("Path", "");
        kcg.writeEntry("Terminal", false);
        kcg.writeEntry("Type", "Application");
        kc.sync();

        KPropertiesDialog dlg(desktopTemplate, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
    } else {
        // Use existing desktop file and use same file name to enable overrides.
        desktopPath = localautostartdir + service->desktopEntryName() + ".desktop";
        desktopTemplate = KUrl( KStandardDirs::locate("apps", service->entryPath()) );

        KPropertiesDialog dlg( desktopTemplate, KUrl(localautostartdir), service->desktopEntryName() + ".desktop", this );
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
    }
    AutoStartItem * item = new AutoStartItem(desktopPath, m_programItem);
    addItem(item, service->name(), service->exec(), false);
}

void Autostart::slotRemoveCMD()
{
    QTreeWidgetItem* item = widget->listCMD->currentItem();
    if (!item) {
        return;
    }
    AutoStartItem *startItem = dynamic_cast<AutoStartItem*>(item);
    if (startItem) {
        m_programItem->takeChild(m_programItem->indexOfChild(startItem));
        KIO::DeleteJob* deljob = KIO::del(startItem->fileName().path());
        deljob->setAutoDelete(false);
        if (!deljob->exec()) {
            KMessageBox::detailedError(
                this,
                i18n("Could not remove the autostart file."),
                deljob->errorString()
            );
            delete deljob;
            return;
        }
        delete deljob;
        delete item;
    }
}

void Autostart::slotEditCMD(QTreeWidgetItem* ent)
{
    if (!ent) return;
    AutoStartItem *entry = dynamic_cast<AutoStartItem*>( ent );
    if (entry) {
        if (!slotEditCMD(KFileItem(KUrl(entry->fileName())))) {
            return;
        }

        KService service(entry->fileName().path());
        addItem(entry, service.name(), service.exec(), false);
    }
}

bool Autostart::slotEditCMD(const KFileItem &item)
{
    KPropertiesDialog dlg(item, this);
    return (dlg.exec() == QDialog::Accepted);
}

void Autostart::slotEditCMD()
{
    if (widget->listCMD->currentItem() == 0) {
        return;
    }
    slotEditCMD((AutoStartItem*)widget->listCMD->currentItem());
}

void Autostart::slotAdvanced()
{
    if (widget->listCMD->currentItem() == 0) {
        return;
    }

    AutoStartItem *entry = static_cast<AutoStartItem *>(widget->listCMD->currentItem());
    KDesktopFile kc(entry->fileName().path());
    KConfigGroup grp = kc.desktopGroup();
    bool status = false;
    QStringList lstEntry;
    if (grp.hasKey("OnlyShowIn")) {
        lstEntry = grp.readXdgListEntry("OnlyShowIn");
        status = lstEntry.contains("KDE");
    }

    AdvancedDialog *dlg = new AdvancedDialog( this,status );
    if (dlg->exec()) {
        status = dlg->onlyInKde();
        if (lstEntry.contains( "KDE" ) && !status) {
            lstEntry.removeAll( "KDE" );
            grp.writeXdgListEntry( "OnlyShowIn", lstEntry);
        } else if (!lstEntry.contains( "KDE" ) && status) {
            lstEntry.append( "KDE" );
            grp.writeXdgListEntry("OnlyShowIn", lstEntry);
        }
    }
    delete dlg;
}

void Autostart::slotSelectionChanged()
{
    const bool isDesktopItem = (dynamic_cast<AutoStartItem*>(widget->listCMD->currentItem()) != 0);
    widget->btnRemove->setEnabled(isDesktopItem);
    widget->btnProperties->setEnabled(isDesktopItem);
    widget->btnAdvanced->setEnabled(isDesktopItem) ;
}

void Autostart::defaults()
{
}

void Autostart::save()
{
}

#include "moc_autostart.cpp"
