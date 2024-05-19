/***************************************************************************
 *   Copyright 2007 by Marco Martin <notmart@gmail.com>                    *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "trash.h"

//QT
#include <QtGui/qgraphicssceneevent.h>
#include <QGraphicsLinearLayout>

//KDE
#include <KCModuleProxy>
#include <KConfigDialog>
#include <KDebug>
#include <KFilePlacesModel>
#include <KGlobalSettings>
#include <KIconLoader>
#include <KMessageBox>
#include <KLocale>
#include <KNotification>
#include <QProcess>
#include <KToolInvocation>
#include <KSharedConfig>
#include <KStandardDirs>
#include <KUrl>
#include <KWindowSystem>

#include <KIO/CopyJob>
#include <KIO/JobUiDelegate>

//Plasma
#include <Plasma/IconWidget>
#include <Plasma/Containment>
#include <Plasma/ToolTipManager>

//Solid
#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/predicate.h>
#include <solid/storageaccess.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>


Trash::Trash(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
      m_icon(0),
      m_dirLister(0),
      m_emptyAction(0),
      m_count(0),
      m_showText(false),
      m_places(0),
      m_proxy(0),
      m_emptyProcess(0)
{
    setHasConfigurationInterface(true);
    setAspectRatioMode(Plasma::ConstrainedSquare);

    m_icon = new Plasma::IconWidget(KIcon("user-trash"),QString(),this);
    m_icon->setNumDisplayLines(2);
    m_icon->setDrawBackground(true);
    setBackgroundHints(NoBackground);

    resize(m_icon->sizeFromIconSize(IconSize(KIconLoader::Desktop)));
    createMenu();
}

Trash::~Trash()
{
    delete m_dirLister;
}

void Trash::init()
{
    Plasma::Applet::init();

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addItem(m_icon);

    setAcceptDrops(true);
    installSceneEventFilter(m_icon);

    m_dirLister = new KDirLister();
    connect(m_dirLister, SIGNAL(clear()), this, SLOT(clear()));
    connect(m_dirLister, SIGNAL(completed()), this, SLOT(completed()));
    connect(m_dirLister, SIGNAL(itemsDeleted(KFileItemList)), this, SLOT(itemsDeleted(KFileItemList)));

    m_dirLister->openUrl(KUrl("trash:/"));

    connect(m_icon, SIGNAL(activated()), this, SLOT(open()));
    connect(KGlobalSettings::self(), SIGNAL(iconChanged(int)),
            this, SLOT(iconSizeChanged(int)));
}

void Trash::createConfigurationInterface(KConfigDialog *parent)
{
    m_proxy = new KCModuleProxy("kcmtrash");

    parent->addPage(m_proxy, i18n("Trash"), icon());
    connect(parent, SIGNAL(okClicked()), this, SLOT(applyConfig()));

    m_proxy->load();
}

void Trash::createMenu()
{
    QAction* open = new QAction(SmallIcon("document-open"), i18n("&Open"), this);
    actions.append(open);
    connect(open, SIGNAL(triggered(bool)), this , SLOT(open()));

    m_emptyAction = new QAction(SmallIcon("trash-empty"), i18n("&Empty Trashcan"), this);
    actions.append(m_emptyAction);
    connect(m_emptyAction, SIGNAL(triggered(bool)), this, SLOT(empty()));

    m_menu.addTitle(i18n("Trash"));
    m_menu.addAction(open);
    m_menu.addAction(m_emptyAction);

    //add the menu as an action icon
    QAction* menu = new QAction(SmallIcon("arrow-up-double"),i18n("&Menu"), this);
    connect(menu, SIGNAL(triggered(bool)), this , SLOT(popup()));
    m_icon->addIconAction(menu);

    connect(&m_menu, SIGNAL(aboutToHide()), m_icon, SLOT(setUnpressed()));
}

void Trash::popup()
{
    if (m_menu.isVisible()) {
        m_menu.hide();
        return;
    }
    m_menu.popup(popupPosition(m_menu.sizeHint()));
    m_icon->setPressed();
}

void Trash::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint) {
        disconnect(m_icon, SIGNAL(activated()), this, SLOT(open()));
        disconnect(m_icon, SIGNAL(clicked()), this, SLOT(open()));

        if (formFactor() == Plasma::Planar ||
            formFactor() == Plasma::MediaCenter) {

            connect(m_icon, SIGNAL(activated()), this, SLOT(open()));

            m_icon->setText(i18n("Trash"));
            m_icon->setInfoText(i18np("One item", "%1 items", m_count));
            m_showText = true;
            m_icon->setDrawBackground(true);
            //Adding an arbitrary width to make room for a larger count of items
            setMinimumSize(m_icon->sizeFromIconSize(IconSize(KIconLoader::Desktop))+=QSizeF(20,0));
        } else {
            //in a panel the icon always behaves like a button
            connect(m_icon, SIGNAL(clicked()), this, SLOT(open()));

            m_icon->setText(QString());
            m_icon->setInfoText(QString());
            m_showText = false;
            m_icon->setDrawBackground(false);

            setMinimumSize(m_icon->sizeFromIconSize(IconSize(KIconLoader::Small)));
        }
        updateIcon();
    }
}

void Trash::open()
{
    KToolInvocation::self()->startServiceForUrl(QString::fromLatin1("trash:/"));
}

void Trash::empty()
{
    if (m_emptyProcess) {
        return;
    }

    if (m_confirmEmptyDialog) {
        KWindowSystem::forceActiveWindow(m_confirmEmptyDialog.data()->winId());
    } else {
        const QString text(i18nc("@info", "Do you really want to empty the trash? All items will be deleted."));
        KDialog *dialog = new KDialog;
        dialog->setWindowTitle(i18nc("@title:window", "Empty Trash"));
        dialog->setButtons(KDialog::Yes|KDialog::No);
        dialog->setButtonText(KDialog::Yes, i18n("Empty Trash"));
        dialog->setButtonText(KDialog::No, i18n("Cancel"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, SIGNAL(yesClicked()), this, SLOT(emptyTrash()));

        KMessageBox::createKMessageBox(dialog, KIcon("user-trash"), text, QStringList(), QString(), 0, KMessageBox::NoExec);

        dialog->setModal(false);
        m_confirmEmptyDialog = dialog;
        dialog->show();
    }
}

void Trash::emptyTrash()
{
    // We can't use KonqOperations here. To avoid duplicating its code (small, though),
    // we can simply call ktrash.
    //KonqOperations::emptyTrash(&m_menu);
    m_emptyAction->setEnabled(false);
    m_emptyAction->setText(i18n("Emptying Trashcan..."));
    m_emptyProcess = new QProcess(this);
    connect(m_emptyProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(emptyFinished(int,QProcess::ExitStatus)));
    QString ktrash = KStandardDirs::findExe("ktrash");
    m_emptyProcess->start(ktrash, QStringList() << "--empty");
}

void Trash::emptyFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    KNotification::event("kde/TrashEmptied");

    //TODO: check the exit status and let the user know if it fails
    delete m_emptyProcess;
    m_emptyProcess = 0;
    m_emptyAction->setEnabled(false);
    m_emptyAction->setText(i18n("&Empty Trashcan"));
}

void Trash::updateIcon()
{
    Plasma::ToolTipContent data;
    data.setMainText(i18n("Trash"));

    if (m_count > 0) {
        m_icon->setIcon("user-trash-full");

        data.setSubText(i18np("One item", "%1 items", m_count));
        if (m_showText) {
            m_icon->setInfoText(i18np("One item", "%1 items", m_count));
        }
    } else {
        m_icon->setIcon("user-trash");

        data.setSubText(i18nc("The trash is empty. This is not an action, but a state", "Empty"));
        if (m_showText) {
            m_icon->setInfoText(i18nc("The trash is empty. This is not an action, but a state", "Empty"));
        }
    }

    m_icon->update();

    data.setImage(m_icon->icon().pixmap(IconSize(KIconLoader::Desktop)));

    if (!m_showText) {
        Plasma::ToolTipManager::self()->setContent(this, data);
    } else {
        Plasma::ToolTipManager::self()->clearContent(this);
    }

    m_emptyAction->setEnabled(m_count > 0);
}

void Trash::clear()
{
    m_count = 0;
    updateIcon();
}

void Trash::completed()
{
    m_count = m_dirLister->items(KDirLister::AllItems).count();
    updateIcon();
}

void Trash::itemsDeleted(const KFileItemList &items)
{
    Q_UNUSED(items);
    m_count--;
    updateIcon();
}

void Trash::applyConfig()
{
    m_proxy->save();
}

QList<QAction*> Trash::contextualActions()
{
    return actions;
}

void Trash::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (KUrl::List::canDecode(event->mimeData())) {
        const KUrl::List urls = KUrl::List::fromMimeData(event->mimeData());
        if (urls.count() == 1) {
            if (!m_places) {
                m_places = new KFilePlacesModel(this);
            }

            KUrl url = urls.at(0);

            const Solid::Predicate predicate(Solid::DeviceInterface::StorageAccess, "filePath", url.path());

            //query for mounted devices
            const QList<Solid::Device> devList = Solid::Device::listFromQuery(predicate, QString());

            //seek for an item in the places (e.g. Dolphin sidebar)
            const QModelIndex index = m_places->closestItem(url);

            if (!devList.isEmpty()) {
                m_icon->setIcon("arrow-up-double");
            } else if (m_places->bookmarkForIndex(index).url() == url) {
                m_icon->setIcon("edit-delete");
            }
        }
    }
}

void Trash::dragLeaveEvent(QGraphicsSceneDragDropEvent *)
{
    updateIcon();
}

void Trash::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (KUrl::List::canDecode(event->mimeData())) {
        const KUrl::List urls = KUrl::List::fromMimeData(event->mimeData());
        if (!urls.isEmpty()) {
            event->accept();

            //some special operation was done instead of simply deleting a file
            bool specialOperation = false;

            if (!m_places) {
                m_places = new KFilePlacesModel(this);
            }

            foreach (const KUrl& url, urls) {
                const Solid::Predicate predicate(Solid::DeviceInterface::StorageAccess, "filePath", url.path());

                //query for mounted devices
                const QList<Solid::Device> devList = Solid::Device::listFromQuery(predicate, QString());


                //seek for an item in the places (e.g. Dolphin sidebar)
                const QModelIndex index = m_places->closestItem(url);

                if (!devList.isEmpty()) {
                    //Assuming a mountpoint has a single device
                    Solid::Device device = devList.first();

                    if (device.is<Solid::OpticalDisc>()) {
                        device.as<Solid::OpticalDrive>()->eject();
                    } else {
                        device.as<Solid::StorageAccess>()->teardown();
                    }

                    specialOperation = true;
                //hide if there is exactly that item in the places model
                } else if (m_places->bookmarkForIndex(index).url() == url) {
                    m_places->removePlace(index);
                    specialOperation = true;
                }
            }

            if (specialOperation) {
                updateIcon();

            //finally, try to trash a file
            }else{
                KIO::Job* job = KIO::trash(urls);
                job->ui()->setWindow(0);
                job->ui()->setAutoErrorHandlingEnabled(true);
            }
        }
    }
}

QSizeF Trash::sizeHint(Qt::SizeHint which, const QSizeF & constraint) const
{
    if (which == Qt::PreferredSize) {
        int iconSize = 0;

        switch (formFactor()) {
            case Plasma::Planar:
            case Plasma::MediaCenter:
                iconSize = IconSize(KIconLoader::Desktop);
                break;

            case Plasma::Horizontal:
            case Plasma::Vertical:
                iconSize = IconSize(KIconLoader::Panel);
                break;
        }

        return QSizeF(iconSize, iconSize);
    }

    return Plasma::Applet::sizeHint(which, constraint);
}

void Trash::iconSizeChanged(int group)
{
    if (group == KIconLoader::Desktop || group == KIconLoader::Panel) {
        updateGeometry();
    }
}

#include "moc_trash.cpp"

