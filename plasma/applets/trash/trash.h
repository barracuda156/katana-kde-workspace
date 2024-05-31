/***************************************************************************
 *   Copyright 2007 by Marco Martin <notmart@gmail.com>                    *
 *                                                                         *
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

#ifndef TRASH_H
#define TRASH_H

#include <QAction>
#include <KMenu>
#include <KFileItem>
#include <KDirLister>
#include <Plasma/Applet>

class KCModuleProxy;
class KFilePlacesModel;

namespace Plasma
{
    class IconWidget;
}

class Trash : public Plasma::Applet
{
    Q_OBJECT
public:
    Trash(QObject *parent, const QVariantList &args);
    ~Trash();

    void init();
    void constraintsEvent(Plasma::Constraints constraints);
    QList<QAction*> contextualActions();

protected:
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);
    void createConfigurationInterface(KConfigDialog *parent);
    void createMenu();
    void updateIcon();
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF & constraint = QSizeF()) const;

protected slots:
    void popup();
    void open();
    void empty();
    void clear();
    void completed();
    void itemsDeleted(const KFileItemList &items);
    void applyConfig();

private slots:
    void iconSizeChanged(int group);

private:
    Plasma::IconWidget* m_icon;
    QList<QAction*> actions;
    KDirLister *m_dirLister;
    KMenu m_menu;
    QAction *m_emptyAction;
    int m_count;
    bool m_showText;
    KFilePlacesModel *m_places;
    KCModuleProxy *m_proxy;
};

K_EXPORT_PLASMA_APPLET(trash, Trash)

#endif
