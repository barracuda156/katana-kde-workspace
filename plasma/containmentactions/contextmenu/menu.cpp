/*
 *   Copyright 2009 by Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "menu.h"

#include <QAction>
#include <QCheckBox>
#include <QtGui/qgraphicssceneevent.h>
#include <QVBoxLayout>
#include <QSignalMapper>
#include <QtCore/qtimer.h>

#include <KDebug>
#include <KIcon>
#include <KMenu>

#include <Plasma/Containment>
#include <Plasma/Corona>
#include <Plasma/Wallpaper>

#include "kworkspace/kworkspace.h"

ContextMenu::ContextMenu(QObject *parent, const QVariantList &args)
    : Plasma::ContainmentActions(parent, args),
    m_logoutAction(0),
    m_separator1(0),
    m_separator2(0),
    m_separator3(0),
    m_buttons(0)
{
}

void ContextMenu::init(const KConfigGroup &config)
{
    Plasma::Containment *c = containment();
    Q_ASSERT(c);

    m_actions.clear();
    QHash<QString, bool> actions;
    QSet<QString> disabled;

    if (c->containmentType() == Plasma::Containment::PanelContainment ||
        c->containmentType() == Plasma::Containment::CustomPanelContainment) {
        m_actionOrder << "add widgets" << "_add panel" << "lock widgets" << "_context" << "remove";
    } else {
        actions.insert("configure shortcuts", false);
        m_actionOrder << "_context" << "add widgets" << "_add panel"
                      << "remove" << "lock widgets" << "_sep1"
                      << "_logout" << "_sep2" << "configure"
                      << "configure shortcuts" << "_sep3" << "_wallpaper";
        disabled.insert("configure shortcuts");
    }

    foreach (const QString &name, m_actionOrder) {
        actions.insert(name, !disabled.contains(name));
    }

    QHashIterator<QString, bool> it(actions);
    while (it.hasNext()) {
        it.next();
        m_actions.insert(it.key(), config.readEntry(it.key(), it.value()));
    }

    // everything below should only happen once, so check for it
    if (c->containmentType() == Plasma::Containment::PanelContainment ||
        c->containmentType() == Plasma::Containment::CustomPanelContainment) {
        //FIXME: panel does its own config action atm...
    } else if (!m_logoutAction) {
        m_logoutAction = new QAction(i18n("Leave..."), this);
        m_logoutAction->setIcon(KIcon("system-shutdown"));
        connect(m_logoutAction, SIGNAL(triggered(bool)), this, SLOT(startLogout()));

        m_separator1 = new QAction(this);
        m_separator1->setSeparator(true);
        m_separator2 = new QAction(this);
        m_separator2->setSeparator(true);
        m_separator3 = new QAction(this);
        m_separator3->setSeparator(true);
    }
}

void ContextMenu::contextEvent(QEvent *event)
{
    QList<QAction *> actions = contextualActions();
    if (actions.isEmpty()) {
        return;
    }

    KMenu desktopMenu;
    desktopMenu.addActions(actions);
    desktopMenu.adjustSize();
    desktopMenu.exec(popupPosition(desktopMenu.size(), event));
}

QList<QAction*> ContextMenu::contextualActions()
{
    Plasma::Containment *c = containment();
    Q_ASSERT(c);
    QList<QAction*> actions;
    foreach (const QString &name, m_actionOrder) {
        if (!m_actions.value(name)) {
            continue;
        }

        if (name == "_context") {
            actions << c->contextualActions();
        } if (name == "_wallpaper") {
            if (c->wallpaper()) {
                actions << c->wallpaper()->contextualActions();
            }
        } else if (QAction *a = action(name)) {
            actions << a;
        }
    }

    return actions;
}

QAction *ContextMenu::action(const QString &name)
{
    Plasma::Containment *c = containment();
    Q_ASSERT(c);
    if (name == "_sep1") {
        return m_separator1;
    } else if (name == "_sep2") {
        return m_separator2;
    } else if (name == "_sep3") {
        return m_separator3;
    } else if (name == "_add panel") {
        if (c->corona() && c->corona()->immutability() == Plasma::Mutable) {
            return c->corona()->action("add panel");
        }
    } else if (name == "_logout") {
        return m_logoutAction;
    } else {
        //FIXME: remove action: make removal of current activity possible
        return c->action(name);
    }
    return 0;
}

// this short delay is due to two issues:
// a) KWorkSpace's DBus alls are all syncronous
// b) the destrution of the menu that this action is in is delayed
//
// (a) leads to the menu hanging out where everyone can see it because the even loop doesn't get
// returned to allowing it to close.
//
// (b) leads to a 0ms timer not working since a 0ms timer just appends to the event queue, and then
// the menu closing event gets appended to that.
//
// ergo a timer with small timeout
void ContextMenu::startLogout()
{
    QTimer::singleShot(10, this, SLOT(logout()));
}

void ContextMenu::logout()
{
    KWorkSpace::requestShutDown();
}

QWidget* ContextMenu::createConfigurationInterface(QWidget* parent)
{
    QWidget *widget = new QWidget(parent);
    QVBoxLayout *lay = new QVBoxLayout();
    widget->setLayout(lay);
    widget->setWindowTitle(i18n("Configure Contextual Menu Plugin"));
    m_buttons = new QButtonGroup(widget);
    m_buttons->setExclusive(false);

    foreach (const QString &name, m_actionOrder) {
        QCheckBox *item = 0;

        if (name == "_context") {
            item = new QCheckBox(widget);
            //FIXME better text
            item->setText(i18n("[Other Actions]"));
        } else if (name == "_wallpaper") {
            item = new QCheckBox(widget);
            item->setText(i18n("Wallpaper Actions"));
            item->setIcon(KIcon("user-desktop"));
        } else if (name == "_sep1" || name =="_sep2" || name == "_sep3") {
            item = new QCheckBox(widget);
            item->setText(i18n("[Separator]"));
        } else {
            QAction *a = action(name);
            if (a) {
                item = new QCheckBox(widget);
                item->setText(a->text());
                item->setIcon(a->icon());
            }
        }

        if (item) {
            item->setChecked(m_actions.value(name));
            item->setProperty("actionName", name);
            lay->addWidget(item);
            m_buttons->addButton(item);
        }
    }

    return widget;
}

void ContextMenu::configurationAccepted()
{
    QList<QAbstractButton *> buttons = m_buttons->buttons();
    QListIterator<QAbstractButton *> it(buttons);
    while (it.hasNext()) {
        QAbstractButton *b = it.next();
        if (b) {
            m_actions.insert(b->property("actionName").toString(), b->isChecked());
        }
    }
}

void ContextMenu::save(KConfigGroup &config)
{
    QHashIterator<QString, bool> it(m_actions);
    while (it.hasNext()) {
        it.next();
        config.writeEntry(it.key(), it.value());
    }
}


#include "moc_menu.cpp"
