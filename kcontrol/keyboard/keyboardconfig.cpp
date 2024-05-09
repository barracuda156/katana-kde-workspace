/*  This file is part of the KDE project
    Copyright (C) 2023 Ivailo Monev <xakepa10@gmail.com>

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

#include "keyboardconfig.h"
#include "keyboardlayoutdialog.h"
#include "keyboardoptionsdialog.h"
#include "keyboardconfig_common.h"

#include <QHeaderView>
#include <kstandarddirs.h>
#include <kicon.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kdialog.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <kdebug.h>

static void kFillTreeFromLayouts(QTreeWidget *treewidget, const QList<KKeyboardType> &layouts)
{
    treewidget->clear();
    int itemcounter = 0;
    foreach (const KKeyboardType &layout, layouts) {
        const QString layoutstring = QString::fromLatin1(layout.layout.constData(), layout.layout.size());
        const QString layoutflag = KStandardDirs::locate(
            "locale",
            QString::fromLatin1("l10n/%1/flag.png").arg(layoutstring)
        );;
        QTreeWidgetItem* layoutitem = new QTreeWidgetItem();
        layoutitem->setData(0, Qt::UserRole, layout.layout);
        layoutitem->setData(0, Qt::UserRole + 1, itemcounter);
        layoutitem->setIcon(0, KIcon(layoutflag));
        layoutitem->setText(0, KKeyboardLayout::layoutDescription(layout.layout));
        layoutitem->setData(1, Qt::UserRole, layout.variant);
        layoutitem->setText(1, KKeyboardLayout::variantDescription(layout.layout, layout.variant));
        treewidget->addTopLevelItem(layoutitem);
        itemcounter++;
    }
}

static QList<KKeyboardType> kGetLayoutsFromTree(QTreeWidget *treewidget, const QByteArray &model)
{
    QList<KKeyboardType> result;
    for (int i = 0; i < treewidget->topLevelItemCount(); i++) {
        QTreeWidgetItem* layoutitem = treewidget->topLevelItem(i);
        KKeyboardType kkeyboardtype;
        kkeyboardtype.model = model;
        kkeyboardtype.layout = layoutitem->data(0, Qt::UserRole).toByteArray();
        kkeyboardtype.variant = layoutitem->data(1, Qt::UserRole).toByteArray();
        result.append(kkeyboardtype);
    }
    return result;
}

K_PLUGIN_FACTORY(KCMKeyboardFactory, registerPlugin<KCMKeyboard>();)
K_EXPORT_PLUGIN(KCMKeyboardFactory("kcmkeyboard", "kcm_keyboard"))

KCMKeyboard::KCMKeyboard(QWidget *parent, const QVariantList &args)
    : KCModule(KCMKeyboardFactory::componentData(), parent),
    m_layout(nullptr),
    m_repeatgroup(nullptr),
    m_repeatdelaylabel(nullptr),
    m_repeatdelayinput(nullptr),
    m_repeatratelabel(nullptr),
    m_repeatrateinput(nullptr),
    m_layoutsgroup(nullptr),
    m_layoutsmodellabel(nullptr),
    m_layoutsmodelbox(nullptr),
    m_layoutstree(nullptr),
    m_layoutbuttonsbox(nullptr),
    m_layoutsaddbutton(nullptr),
    m_layoutseditbutton(nullptr),
    m_layoutsremovebutton(nullptr),
    m_layoutsupbutton(nullptr),
    m_layoutsdownbutton(nullptr),
    m_layoutsbuttonsspacer(nullptr),
    m_layoutsavdancedbutton(nullptr)
{
    Q_UNUSED(args);

    KGlobal::locale()->insertCatalog("kxkb");

    setButtons(KCModule::Default | KCModule::Apply);
    setQuickHelp(i18n("<h1>Keyboard</h1> This control module can be used to configure keyboard parameters and layouts."));

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmkeyboard"), 0,
        ki18n("KDE Keyboard Control Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2023, Ivailo Monev xakepa10@gmail.com")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    m_layout = new QVBoxLayout(this);
    setLayout(m_layout);

    m_repeatgroup = new QGroupBox(this);
    m_repeatgroup->setTitle(i18n("Repeat"));
    QGridLayout* repeatgrouplayout = new QGridLayout(m_repeatgroup);
    m_repeatgroup->setLayout(repeatgrouplayout);
    m_layout->addWidget(m_repeatgroup);

    m_repeatdelaylabel = new QLabel(m_repeatgroup);
    m_repeatdelaylabel->setText(i18n("Repeat delay:"));
    repeatgrouplayout->addWidget(m_repeatdelaylabel, 0, 0);
    m_repeatdelayinput = new KIntNumInput(m_repeatgroup);
    m_repeatdelayinput->setSliderEnabled(true);
    m_repeatdelayinput->setSuffix(i18n(" ms"));
    const QString repeatdelayhelp = i18n("The delay after which a pressed key will start generating keycodes.");
    m_repeatdelayinput->setToolTip(repeatdelayhelp);
    m_repeatdelayinput->setWhatsThis(repeatdelayhelp);
    m_repeatdelayinput->setRange(10, 10000);
    m_repeatdelayinput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(
        m_repeatdelayinput, SIGNAL(valueChanged(int)),
        this, SLOT(slotEmitChanged())
    );
    repeatgrouplayout->addWidget(m_repeatdelayinput, 0, 1);

    m_repeatratelabel = new QLabel(m_repeatgroup);
    m_repeatratelabel->setText(i18n("Repeat rate:"));
    repeatgrouplayout->addWidget(m_repeatratelabel, 1, 0);
    m_repeatrateinput = new KIntNumInput(m_repeatgroup);
    m_repeatrateinput->setSliderEnabled(true);
    m_repeatrateinput->setSuffix(i18n(" ms"));
    const QString repeatratehelp = i18n("The rate at which keycodes are generated while a key is pressed.");
    m_repeatrateinput->setToolTip(repeatratehelp);
    m_repeatrateinput->setWhatsThis(repeatratehelp);
    m_repeatrateinput->setRange(10, 5000);
    m_repeatrateinput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(
        m_repeatrateinput, SIGNAL(valueChanged(int)),
        this, SLOT(slotEmitChanged())
    );
    repeatgrouplayout->addWidget(m_repeatrateinput, 1, 1);

    m_layoutsgroup = new QGroupBox(this);
    m_layoutsgroup->setTitle(i18n("Layouts"));
    QGridLayout* layoutgrouplayout = new QGridLayout(m_layoutsgroup);
    m_layoutsgroup->setLayout(layoutgrouplayout);
    m_layout->addWidget(m_layoutsgroup);

    m_layoutsmodellabel = new QLabel(m_layoutsgroup);
    m_layoutsmodellabel->setText(i18n("Model:"));
    layoutgrouplayout->addWidget(m_layoutsmodellabel, 0, 0);
    m_layoutsmodelbox = new QComboBox(m_layoutsgroup);
    m_layoutsmodelbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const QString layoutmodelhelp = i18n(
        "<p>Here you can choose a keyboard model. This setting is independent of your keyboard layout "
        "and refers to the &quot;hardware&quot; model, i.e. the way your keyboard is manufactured.</p>"
        "<p>Modern keyboards that come with your computer usually have two extra keys and are referred "
        "to as &quot;104-key&quot; models, which is probably what you want if you do not know what "
        "kind of keyboard you have.</p>"
    );
    m_layoutsmodelbox->setToolTip(layoutmodelhelp);
    m_layoutsmodelbox->setWhatsThis(layoutmodelhelp);
    foreach (const QByteArray &layoutmodel, KKeyboardLayout::modelNames()) {
        m_layoutsmodelbox->addItem(KKeyboardLayout::modelDescription(layoutmodel), layoutmodel);
    }
    connect(
        m_layoutsmodelbox, SIGNAL(currentIndexChanged(int)),
        this, SLOT(slotEmitChanged())
    );
    layoutgrouplayout->addWidget(m_layoutsmodelbox, 0, 1);

    m_layoutstree = new QTreeWidget(m_layoutsgroup);
    m_layoutstree->setColumnCount(2);
    QStringList treeheaders = QStringList()
        << i18n("Layout")
        << i18n("Variant");
    m_layoutstree->setHeaderLabels(treeheaders);
    m_layoutstree->setRootIsDecorated(false);
    m_layoutstree->header()->setMovable(false);
    m_layoutstree->header()->setStretchLastSection(false);
    m_layoutstree->header()->setResizeMode(0, QHeaderView::Stretch);
    m_layoutstree->header()->setResizeMode(1, QHeaderView::Stretch);
    connect(
        m_layoutstree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
        this, SLOT(slotEmitChanged())
    );
    connect(
        m_layoutstree, SIGNAL(itemSelectionChanged()),
        this, SLOT(slotItemSelectionChanged())
    );
    layoutgrouplayout->addWidget(m_layoutstree, 1, 0, 1, 2);

    m_layoutbuttonsbox = new KHBox(m_layoutsgroup);
    layoutgrouplayout->addWidget(m_layoutbuttonsbox, 2, 0, 1, 2);

    m_layoutsaddbutton = new QPushButton(m_layoutbuttonsbox);
    m_layoutsaddbutton->setText(i18n("Add"));
    m_layoutsaddbutton->setIcon(KIcon("list-add"));
    connect(
        m_layoutsaddbutton, SIGNAL(pressed()),
        this, SLOT(slotAddPressed())
    );

    m_layoutseditbutton = new QPushButton(m_layoutbuttonsbox);
    m_layoutseditbutton->setText(i18n("Edit"));
    m_layoutseditbutton->setIcon(KIcon("document-edit"));
    m_layoutseditbutton->setEnabled(false);
    connect(
        m_layoutseditbutton, SIGNAL(pressed()),
        this, SLOT(slotEditPressed())
    );

    m_layoutsremovebutton = new QPushButton(m_layoutbuttonsbox);
    m_layoutsremovebutton->setText(i18n("Remove"));
    m_layoutsremovebutton->setIcon(KIcon("list-remove"));
    m_layoutsremovebutton->setEnabled(false);
    connect(
        m_layoutsremovebutton, SIGNAL(pressed()),
        this, SLOT(slotRemovePressed())
    );

    m_layoutsupbutton = new QPushButton(m_layoutbuttonsbox);
    m_layoutsupbutton->setText(i18n("Move Up"));
    m_layoutsupbutton->setIcon(KIcon("go-up"));
    m_layoutsupbutton->setEnabled(false);
    connect(
        m_layoutsupbutton, SIGNAL(pressed()),
        this, SLOT(slotUpPressed())
    );

    m_layoutsdownbutton = new QPushButton(m_layoutbuttonsbox);
    m_layoutsdownbutton->setText(i18n("Move Down"));
    m_layoutsdownbutton->setIcon(KIcon("go-down"));
    m_layoutsdownbutton->setEnabled(false);
    connect(
        m_layoutsdownbutton, SIGNAL(pressed()),
        this, SLOT(slotDownPressed())
    );

    m_layoutsbuttonsspacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_layoutbuttonsbox->layout()->addItem(m_layoutsbuttonsspacer);

    m_layoutsavdancedbutton = new QPushButton(m_layoutbuttonsbox);
    m_layoutsavdancedbutton->setText(i18n("Advanced"));
    m_layoutsavdancedbutton->setIcon(KIcon("configure"));
    connect(
        m_layoutsavdancedbutton, SIGNAL(pressed()),
        this, SLOT(slotAdvancedPressed())
    );
}

void KCMKeyboard::load()
{
    KConfig kconfig("kcminputrc", KConfig::NoGlobals);
    KConfigGroup kconfiggroup(&kconfig, "Keyboard");
    const float repeatdelay = kconfiggroup.readEntry("RepeatDelay", s_defaultrepeatdelay);
    const float repeatrate = kconfiggroup.readEntry("RepeatRate", s_defaultrepeatrate);
    m_repeatdelayinput->setValue(qRound(repeatdelay));
    m_repeatrateinput->setValue(qRound(repeatrate));

    const QList<KKeyboardType> layouts = kLayoutsFromConfig();
    const KKeyboardType modellayout = (layouts.size() > 0 ? layouts.first() : KKeyboardLayout::defaultLayout());
    m_layoutmodel = modellayout.model;
    m_layoutoptions = modellayout.option;
    const int layoutsmodelindex = m_layoutsmodelbox->findData(m_layoutmodel);
    if (layoutsmodelindex >= 0) {
        m_layoutsmodelbox->setCurrentIndex(layoutsmodelindex);
    } else {
        kWarning() << "Could not find the keyboard layout model" << m_layoutmodel;
    }

    kFillTreeFromLayouts(m_layoutstree, layouts);

    emit changed(false);
}

void KCMKeyboard::save()
{
    KConfig kconfig("kcminputrc", KConfig::NoGlobals);
    KConfigGroup kconfiggroup(&kconfig, "Keyboard");
    kconfiggroup.writeEntry("RepeatDelay", m_repeatdelayinput->value());
    kconfiggroup.writeEntry("RepeatRate", m_repeatrateinput->value());
    m_layoutmodel = m_layoutsmodelbox->itemData(m_layoutsmodelbox->currentIndex()).toByteArray();
    QStringList layoutslayouts;
    QStringList layoutsvariants;
    for (int i = 0; i < m_layoutstree->topLevelItemCount(); i++) {
        QTreeWidgetItem* layoutitem = m_layoutstree->topLevelItem(i);
        layoutslayouts.append(layoutitem->data(0, Qt::UserRole).toString());
        layoutsvariants.append(layoutitem->data(1, Qt::UserRole).toString());
    }
    kconfiggroup.writeEntry("LayoutsModel", m_layoutmodel);
    kconfiggroup.writeEntry("LayoutsOptions", m_layoutoptions);
    kconfiggroup.writeEntry("LayoutsLayouts", layoutslayouts);
    kconfiggroup.writeEntry("LayoutsVariants", layoutsvariants);
    kconfig.sync();

    kApplyKeyboardConfig();

    emit changed(false);
}

void KCMKeyboard::defaults()
{
    m_repeatdelayinput->setValue(qRound(s_defaultrepeatdelay));
    m_repeatrateinput->setValue(qRound(s_defaultrepeatrate));

    const KKeyboardType defaultlayout = KKeyboardLayout::defaultLayout();
    const int defaultmodelindex = m_layoutsmodelbox->findData(defaultlayout.model);
    if (defaultmodelindex >= 0) {
        m_layoutsmodelbox->setCurrentIndex(defaultmodelindex);
    } else {
        kWarning() << "Could not find the default keyboard layout model" << defaultlayout.model;
    }

    const QList<KKeyboardType> layouts = QList<KKeyboardType>()
        << defaultlayout;
    kFillTreeFromLayouts(m_layoutstree, layouts);

    emit changed(true);
}

void KCMKeyboard::slotEmitChanged()
{
    emit changed(true);
}

void KCMKeyboard::slotItemSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedlayouts = m_layoutstree->selectedItems();
    if (selectedlayouts.size() == 0) {
        m_layoutseditbutton->setEnabled(false);
        m_layoutsremovebutton->setEnabled(false);
        m_layoutsupbutton->setEnabled(false);
        m_layoutsdownbutton->setEnabled(false);
        return;
    }

    m_layoutseditbutton->setEnabled(true);
    m_layoutsremovebutton->setEnabled(true);
    if (m_layoutstree->topLevelItemCount() == 1) {
        m_layoutsupbutton->setEnabled(false);
        m_layoutsdownbutton->setEnabled(false);
        return;
    }

    const int selectedrow = selectedlayouts.at(0)->data(0, Qt::UserRole + 1).toInt();
    m_layoutsupbutton->setEnabled(selectedrow > 0);
    m_layoutsdownbutton->setEnabled((selectedrow + 1) < m_layoutstree->topLevelItemCount());
}

void KCMKeyboard::slotAddPressed()
{
    QList<KKeyboardType> layouts = kGetLayoutsFromTree(m_layoutstree, m_layoutmodel);
    KCMKeyboardLayoutDialog keyboarddialog(layouts, this);
    keyboarddialog.setKeyboardType(KKeyboardLayout::defaultLayout());
    if (keyboarddialog.exec() != QDialog::Accepted) {
        return;
    }
    layouts.append(keyboarddialog.keyboardType());
    kFillTreeFromLayouts(m_layoutstree, layouts);
    emit changed(true);
}

void KCMKeyboard::slotEditPressed()
{
    QList<QTreeWidgetItem*> selectedlayouts = m_layoutstree->selectedItems();
    if (selectedlayouts.size() == 0) {
        return;
    }
    const int selectedrow = selectedlayouts.at(0)->data(0, Qt::UserRole + 1).toInt();
    QList<KKeyboardType> layouts = kGetLayoutsFromTree(m_layoutstree, m_layoutmodel);
    const KKeyboardType currentlayout = layouts.at(selectedrow);
    layouts.removeAt(selectedrow);
    KCMKeyboardLayoutDialog keyboarddialog(layouts, this);
    keyboarddialog.setKeyboardType(currentlayout);
    if (keyboarddialog.exec() != QDialog::Accepted) {
        return;
    }
    layouts.insert(selectedrow, keyboarddialog.keyboardType());
    kFillTreeFromLayouts(m_layoutstree, layouts);
    emit changed(true);
}

void KCMKeyboard::slotRemovePressed()
{
    QList<QTreeWidgetItem*> selectedlayouts = m_layoutstree->selectedItems();
    if (selectedlayouts.size() == 0) {
        return;
    }
    const int selectedrow = selectedlayouts.at(0)->data(0, Qt::UserRole + 1).toInt();
    QList<KKeyboardType> layouts = kGetLayoutsFromTree(m_layoutstree, m_layoutmodel);
    layouts.removeAt(selectedrow);
    kFillTreeFromLayouts(m_layoutstree, layouts);
    emit changed(true);
}

void KCMKeyboard::slotUpPressed()
{
    QList<QTreeWidgetItem*> selectedlayouts = m_layoutstree->selectedItems();
    if (selectedlayouts.size() == 0) {
        return;
    }
    const int selectedrow = selectedlayouts.at(0)->data(0, Qt::UserRole + 1).toInt();
    QList<KKeyboardType> layouts = kGetLayoutsFromTree(m_layoutstree, m_layoutmodel);
    layouts.move(selectedrow, selectedrow - 1);
    kFillTreeFromLayouts(m_layoutstree, layouts);
    emit changed(true);
}

void KCMKeyboard::slotDownPressed()
{
    QList<QTreeWidgetItem*> selectedlayouts = m_layoutstree->selectedItems();
    if (selectedlayouts.size() == 0) {
        return;
    }
    const int selectedrow = selectedlayouts.at(0)->data(0, Qt::UserRole + 1).toInt();
    QList<KKeyboardType> layouts = kGetLayoutsFromTree(m_layoutstree, m_layoutmodel);
    layouts.move(selectedrow, selectedrow + 1);
    kFillTreeFromLayouts(m_layoutstree, layouts);
    emit changed(true);
}

void KCMKeyboard::slotAdvancedPressed()
{
    KCMKeyboardOptionsDialog keyboarddialog(this);
    keyboarddialog.setOptions(m_layoutoptions);
    if (keyboarddialog.exec() != QDialog::Accepted) {
        return;
    }
    m_layoutoptions = keyboarddialog.options();
    emit changed(true);
}

#include "moc_keyboardconfig.cpp"
