/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "kernel.h"
#include "config-workspace.h"

#include <QHBoxLayout>
#include <QFile>
#include <QDir>
#include <kaboutdata.h>
#include <kglobal.h>
#include <kicon.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <KPluginFactory>
#include <KPluginLoader>

#include <libkmod.h>

static const QStringList s_firmwarepaths = QStringList()
    << "/lib/firmware"
    << "/usr/lib/firmware"
    << KStandardDirs::installPath("lib") + "firmware";

K_PLUGIN_FACTORY(KCMKernelFactory, registerPlugin<KCMKernel>();)
K_EXPORT_PLUGIN(KCMKernelFactory("kcmkernel"))

KCMKernel::KCMKernel(QWidget *parent, const QVariantList &)
    : KCModule(KCMKernelFactory::componentData(), parent)
{
    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmkernel"), 0,
        ki18n("Kernel Modules Information Control Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("(c) 2022 Ivailo Monev")
    );

    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    
    m_treewidget = new QTreeWidget(this);
    layout->addWidget(m_treewidget);
    m_treewidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treewidget->setAllColumnsShowFocus(true);
    m_treewidget->setWhatsThis(i18n("This list displays kernel modules information."));

    QStringList treeheaders;
    treeheaders << i18n("Information") << i18n("Value");
    m_treewidget->setHeaderLabels(treeheaders);
    m_treewidget->setRootIsDecorated(true);
}

void KCMKernel::load()
{
    struct kmod_ctx *kmodctx = kmod_new(NULL, NULL);
    if (!kmodctx) {
        kWarning() << "Could not create kmod context";
        return;
    }
    struct kmod_list *kmodlist = NULL;
    int kmodresult = kmod_module_new_from_loaded(kmodctx, &kmodlist);
    if (kmodresult == 0) {
        struct kmod_list *kmodit = NULL;
        for (kmodit = kmodlist; kmodit != NULL; kmodit = kmod_list_next(kmodlist, kmodit)) {
            struct kmod_module *kmodmodule = kmod_module_get_module(kmodit);
            // qDebug() << Q_FUNC_INFO << kmod_module_get_name(kmodmodule);

            QStringList modulewidgetitemlist;
            modulewidgetitemlist << QString::fromAscii(kmod_module_get_name(kmodmodule));
            QTreeWidgetItem* modulewidgetitem = new QTreeWidgetItem(m_treewidget, modulewidgetitemlist);

            QStringList modulepathwidgetitemlist;
            modulepathwidgetitemlist << i18n("Path") << QString::fromAscii(kmod_module_get_path(kmodmodule));
            (void)new QTreeWidgetItem(modulewidgetitem, modulepathwidgetitemlist);

            QStringList moduleoptionswidgetitemlist;
            moduleoptionswidgetitemlist << i18n("Size") << KGlobal::locale()->formatByteSize(kmod_module_get_size(kmodmodule), 1);
            (void)new QTreeWidgetItem(modulewidgetitem, moduleoptionswidgetitemlist);

            QTreeWidgetItem* modulelicenseswidgetitem = nullptr;
            QTreeWidgetItem* moduledescriptionswidgetitem = nullptr;
            QTreeWidgetItem* moduleauthorswidgetitem = nullptr;
            QTreeWidgetItem* modulesignerswidgetitem = nullptr;
            QTreeWidgetItem* modulefirmwareswidgetitem = nullptr;

            struct kmod_list *kseclist = NULL;
            kmodresult = kmod_module_get_info(kmodmodule, &kseclist);
            if (kmodresult >= 0) {
                struct kmod_list *ksecit = NULL;
                for (ksecit = kseclist; ksecit != NULL; ksecit = kmod_list_next(kseclist, ksecit)) {
                    const char* kmodkey = kmod_module_info_get_key(ksecit);
                    const char* kmodvalue = kmod_module_info_get_value(ksecit);
                    // qDebug() << Q_FUNC_INFO << kmodkey << kmodvalue;

                    QStringList modulesectionwidgetitemlist;
                    if (qstrcmp(kmodkey, "license") == 0) {
                        if (!modulelicenseswidgetitem) {
                            QStringList modulelicenseswidgetitemlist;
                            modulelicenseswidgetitemlist << i18n("Licenses");
                            modulelicenseswidgetitem = new QTreeWidgetItem(modulewidgetitem, modulelicenseswidgetitemlist);
                        }

                        modulesectionwidgetitemlist << i18n("License") << QString::fromAscii(kmodvalue);
                        (void)new QTreeWidgetItem(modulelicenseswidgetitem, modulesectionwidgetitemlist);
                    } else if (qstrcmp(kmodkey, "description") == 0) {
                        if (!moduledescriptionswidgetitem) {
                            QStringList moduledescriptionswidgetitemlist;
                            moduledescriptionswidgetitemlist << i18n("Descriptions");
                            moduledescriptionswidgetitem = new QTreeWidgetItem(modulewidgetitem, moduledescriptionswidgetitemlist);
                        }

                        modulesectionwidgetitemlist << i18n("Description") << QString::fromAscii(kmodvalue);
                        (void)new QTreeWidgetItem(moduledescriptionswidgetitem, modulesectionwidgetitemlist);
                    } else if (qstrcmp(kmodkey, "author") == 0) {
                        if (!moduleauthorswidgetitem) {
                            QStringList moduleauthorswidgetitemlist;
                            moduleauthorswidgetitemlist << i18n("Authors");
                            moduleauthorswidgetitem = new QTreeWidgetItem(modulewidgetitem, moduleauthorswidgetitemlist);
                        }

                        modulesectionwidgetitemlist << i18n("Author") << QString::fromAscii(kmodvalue);
                        (void)new QTreeWidgetItem(moduleauthorswidgetitem, modulesectionwidgetitemlist);
                    } else if (qstrcmp(kmodkey, "signer") == 0) {
                        if (!modulesignerswidgetitem) {
                            QStringList modulesignerswidgetitemlist;
                            modulesignerswidgetitemlist << i18n("Signers");
                            modulesignerswidgetitem = new QTreeWidgetItem(modulewidgetitem, modulesignerswidgetitemlist);
                        }

                        modulesectionwidgetitemlist << i18n("Signer") << QString::fromAscii(kmodvalue);
                        (void)new QTreeWidgetItem(modulesignerswidgetitem, modulesectionwidgetitemlist);
                    } else if (qstrcmp(kmodkey, "firmware") == 0) {
                        if (!modulefirmwareswidgetitem) {
                            QStringList modulefirmwareswidgetitemlist;
                            modulefirmwareswidgetitemlist << i18n("Firmwares");
                            modulefirmwareswidgetitem = new QTreeWidgetItem(modulewidgetitem, modulefirmwareswidgetitemlist);
                        }

                        modulesectionwidgetitemlist << i18n("Firmware") << QString::fromAscii(kmodvalue);
                        QTreeWidgetItem* modulefirmwarewidgetitem = new QTreeWidgetItem(
                            modulefirmwareswidgetitem, modulesectionwidgetitemlist
                        );
                        bool isfirmwarepresent = false;
                        foreach (const QString &firmwarepath, s_firmwarepaths) {
                            const QString modulefirmwarepath = firmwarepath + QDir::separator() + QString::fromAscii(kmodvalue);
                            if (QFile::exists(modulefirmwarepath)) {
                                isfirmwarepresent = true;
                                break;
                            }
                        }
                        if (!isfirmwarepresent) {
                            modulewidgetitem->setIcon(1, KIcon("dialog-warning"));
                            modulefirmwareswidgetitem->setIcon(1, KIcon("dialog-warning"));
                            modulefirmwarewidgetitem->setIcon(1, KIcon("dialog-warning"));
                            modulefirmwarewidgetitem->setToolTip(1, i18n("Missing firmware"));
                        }
                        // qDebug() << Q_FUNC_INFO << kmod_get_dirname(kmodctx);
                    }
                }
                kmod_module_info_free_list(kseclist);
            } else {
                kWarning() << "Could not get module sections list";
            }

            kmod_module_unref(kmodmodule);
        }
        kmod_module_unref_list(kmodlist);
    } else {
        kWarning() << "Could not get loaded modules list";
    }
    kmod_unref(kmodctx);

    // Resize the column width to the maximum needed
    m_treewidget->expandAll();
    m_treewidget->resizeColumnToContents(0);
    m_treewidget->collapseAll();

    m_treewidget->sortItems(0, Qt::AscendingOrder);
}

#include "moc_kernel.cpp"
