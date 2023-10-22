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

#include "keyboardoptionsdialog.h"

#include <QHeaderView>
#include <klocale.h>
#include <kkeyboardlayout.h>
#include <kdebug.h>

static const char s_optionsseparator = ',';
static const char s_optionsgroupseparator = ':';
static const QLatin1String s_optionskeyplaceholder = QLatin1String("\"< >\"");

KCMKeyboardOptionsDialog::KCMKeyboardOptionsDialog(QWidget *parent)
    : KDialog(parent),
    m_widget(nullptr),
    m_layout(nullptr),
    m_optionswidget(nullptr),
    m_optionstree(nullptr)
{
    setCaption(i18n("Keyboard Options"));
    setButtons(KDialog::Ok | KDialog::Cancel);

    m_enabledi18n = i18n("Enabled");
    m_disabledi18n = i18n("Disabled");

    m_widget = new QWidget(this);
    m_layout = new QGridLayout(m_widget);

    m_optionswidget = new KMessageWidget(m_widget);
    m_optionswidget->setCloseButtonVisible(false);
    m_optionswidget->setMessageType(KMessageWidget::Warning);
    m_optionswidget->setWordWrap(true);
    m_optionswidget->setText(
        i18n(
            "<warning>enabling options can have various bad effects. For example the <i>%1</i> "
            "option does not update the XKB property on the root window meaning that programs "
            "will not be able to detect the keyboard layout switch.</warning>",
            KKeyboardLayout::optionDescription("grp:alt_space_toggle")
        )
    );
    m_layout->addWidget(m_optionswidget, 0, 0);

    m_optionstree = new QTreeWidget(m_widget);
    m_optionstree->setColumnCount(2);
    QStringList treeheaders = QStringList()
        << i18n("Option")
        << i18n("State");
    m_optionstree->setHeaderLabels(treeheaders);
    m_optionstree->setRootIsDecorated(true);
    m_optionstree->header()->setMovable(false);
    m_optionstree->header()->setStretchLastSection(false);
    m_optionstree->header()->setResizeMode(0, QHeaderView::Stretch);
    m_optionstree->header()->setResizeMode(1, QHeaderView::ResizeToContents);
    const QFontMetrics fontmetrics(m_optionstree->header()->font());
    int minsectionsize = fontmetrics.width(m_enabledi18n);
    minsectionsize = qMax(minsectionsize, fontmetrics.width(m_disabledi18n));
    // some space for the checkbox
    minsectionsize = (minsectionsize + 20);
    m_optionstree->header()->setMinimumSectionSize(minsectionsize);
    connect(
        m_optionstree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
        this, SLOT(slotItemChanged(QTreeWidgetItem*,int))
    );
    m_layout->addWidget(m_optionstree, 1, 0);

    setMainWidget(m_widget);

    adjustSize();
    KConfigGroup kconfiggroup(KGlobal::config(), "KCMKeyboardOptionsDialog");
    restoreDialogSize(kconfiggroup);
}

KCMKeyboardOptionsDialog::~KCMKeyboardOptionsDialog()
{
    KConfigGroup kconfiggroup(KGlobal::config(), "KCMKeyboardOptionsDialog");
    saveDialogSize(kconfiggroup);
    KGlobal::config()->sync();
}

QByteArray KCMKeyboardOptionsDialog::options() const
{
    QByteArray result;
    foreach (const QByteArray &option, m_options) {
        if (!result.isEmpty()) {
            result.append(s_optionsseparator);
        }
        result.append(option);
    }
    return result;
}

void KCMKeyboardOptionsDialog::setOptions(const QByteArray &options)
{
    m_options = options.split(s_optionsseparator);

    m_optionstree->blockSignals(true);
    m_optionstree->clear();
    QMap<QByteArray,QTreeWidgetItem*> groupitems;
    const QList<QByteArray> layoutoptions = KKeyboardLayout::optionNames();
    foreach (const QByteArray &option, layoutoptions) {
        if (option.contains(s_optionsgroupseparator)) {
            continue;
        }
        QTreeWidgetItem* groupitem = new QTreeWidgetItem();
        groupitem->setData(0, Qt::UserRole, option);
        groupitem->setText(0, KKeyboardLayout::optionDescription(option));
        m_optionstree->addTopLevelItem(groupitem);
        groupitems.insert(option, groupitem);
    }
    foreach (const QByteArray &option, layoutoptions) {
        const int indexofgroupseparator = option.indexOf(s_optionsgroupseparator);
        if (indexofgroupseparator <= 0) {
            continue;
        }
        const QString optiondescription = KKeyboardLayout::optionDescription(option);
        if (optiondescription.contains(s_optionskeyplaceholder)) {
            // placeholder for something
            continue;
        }
        const QByteArray group = option.mid(0, indexofgroupseparator);
        QTreeWidgetItem* groupitem = groupitems.value(group);
        Q_ASSERT(groupitem);
        QTreeWidgetItem* optionitem = new QTreeWidgetItem(groupitem);
        optionitem->setData(0, Qt::UserRole, option);
        optionitem->setText(0, optiondescription);
        const bool isoptionenabled = m_options.contains(option);
        optionitem->setText(1, isoptionenabled ? m_enabledi18n : m_disabledi18n);
        optionitem->setCheckState(1, isoptionenabled ? Qt::Checked : Qt::Unchecked);
    }
    QMutableMapIterator<QByteArray,QTreeWidgetItem*> iter(groupitems);
    while (iter.hasNext()) {
        iter.next();
        QTreeWidgetItem* groupitem = iter.value();
        if (groupitem->childCount() <= 0) {
            delete groupitem;
            iter.remove();
        }
    }
    m_optionstree->blockSignals(false);
}

void KCMKeyboardOptionsDialog::slotItemChanged(QTreeWidgetItem *optionitem, const int column)
{
    Q_UNUSED(column);
    const QByteArray option = optionitem->data(0, Qt::UserRole).toByteArray();
    if (!option.contains(s_optionsgroupseparator)) {
        return;
    }
    const bool enableoption = (optionitem->checkState(1) == Qt::Checked);
    optionitem->setText(1, enableoption ? m_enabledi18n : m_disabledi18n);
    if (enableoption) {
        if (!m_options.contains(option)) {
            m_options.append(option);
        }
    } else {
        m_options.removeAll(option);
    }
}

#include "moc_keyboardoptionsdialog.cpp"
