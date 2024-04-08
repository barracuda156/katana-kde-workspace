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

#include "applicationswidget.h"

#include <QDBusConnection>
#include <QTimer>
#include <QGraphicsGridLayout>
#include <Plasma/Animation>
#include <Plasma/Service>
#include <KIconLoader>
#include <KIcon>
#include <KNotificationConfigWidget>
#include <KDebug>

static void kClearButtons(QGraphicsGridLayout *framelayout)
{
    // row insertation starts at 0, count is +1
    if (framelayout->rowCount() >= 4) {
        QGraphicsLinearLayout* buttonslayout = static_cast<QGraphicsLinearLayout*>(framelayout->itemAt(3, 0));
        QList<Plasma::PushButton*> actionbuttons;
        for (int i = 0; i < buttonslayout->count(); i++) {
            Plasma::PushButton* actionbutton = static_cast<Plasma::PushButton*>(buttonslayout->itemAt(i));
            if (actionbutton) {
                actionbuttons.append(actionbutton);
            }
            buttonslayout->removeAt(i);
        }
        qDeleteAll(actionbuttons);
        actionbuttons.clear();
        framelayout->removeItem(buttonslayout);
        delete buttonslayout;
    }
}

ApplicationFrame::ApplicationFrame(const QString &_name, QGraphicsWidget *parent)
    : Plasma::Frame(parent),
    iconwidget(nullptr),
    label(nullptr),
    removewidget(nullptr),
    configurewidget(nullptr),
    name(_name)
{
    ApplicationsWidget* applicationswidget = qobject_cast<ApplicationsWidget*>(parent);

    setFrameShadow(Plasma::Frame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QGraphicsGridLayout* framelayout = new QGraphicsGridLayout(this);

    iconwidget = new Plasma::IconWidget(this);
    iconwidget->setAcceptHoverEvents(false);
    iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    iconwidget->setIcon(KIcon("dialog-information"));
    const int desktopiconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    const QSizeF desktopiconsizef = QSizeF(desktopiconsize, desktopiconsize);
    iconwidget->setPreferredIconSize(desktopiconsizef);
    iconwidget->setMinimumSize(desktopiconsizef);
    iconwidget->setMaximumSize(desktopiconsizef);
    iconwidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    framelayout->addItem(iconwidget, 0, 0, 2, 1);

    label = new Plasma::Label(this);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    label->nativeWidget()->setOpenExternalLinks(true);
    framelayout->addItem(label, 0, 1, 3, 1);

    const int smalliconsize = KIconLoader::global()->currentSize(KIconLoader::Small);
    removewidget = new Plasma::IconWidget(this);
    removewidget->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    removewidget->setIcon(KIcon("dialog-close"));
    removewidget->setToolTip(i18n("Click to remove this notification."));
    removewidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    connect(
        removewidget, SIGNAL(activated()),
        applicationswidget, SLOT(slotRemoveActivated())
    );
    framelayout->addItem(removewidget, 0, 2, 1, 1);

    configurewidget = new Plasma::IconWidget(this);
    configurewidget->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    configurewidget->setIcon(KIcon("configure"));
    configurewidget->setToolTip(i18n("Click to configure this notification."));
    configurewidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    configurewidget->setVisible(false);
    connect(
        configurewidget, SIGNAL(activated()),
        applicationswidget, SLOT(slotConfigureActivated())
    );
    framelayout->addItem(configurewidget, 1, 2, 1, 1);

    setLayout(framelayout);
}

void ApplicationFrame::animateRemove()
{
    Plasma::Animation *animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation);
    Q_ASSERT(animation != nullptr);
    ApplicationsWidget* applicationswidget = qobject_cast<ApplicationsWidget*>(parentObject());
    disconnect(removewidget, 0, applicationswidget, 0);
    disconnect(configurewidget, 0, applicationswidget, 0);

    connect(animation, SIGNAL(finished()), this, SLOT(deleteLater()));
    animation->setTargetWidget(this);
    animation->setProperty("startOpacity", 1.0);
    animation->setProperty("targetOpacity", 0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}


ApplicationsWidget::ApplicationsWidget(QGraphicsItem *parent, NotificationsWidget *notificationswidget)
    : QGraphicsWidget(parent),
    m_notificationswidget(notificationswidget),
    m_layout(nullptr),
    m_label(nullptr),
    m_adaptor(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_label = new Plasma::Label(this);
    m_label->setText(i18n("No application notifications"));
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addItem(m_label);
    m_layout->addStretch();
    setLayout(m_layout);

    m_adaptor = new NotificationsAdaptor(this);
    connect(
        m_adaptor, SIGNAL(notificationAdded(QString)),
        this, SLOT(slotNotificationAdded(QString))
    );
    connect(
        m_adaptor, SIGNAL(notificationUpdated(QString,QVariantMap)),
        this, SLOT(slotNotificationUpdated(QString,QVariantMap))
    );
    QDBusConnection session = QDBusConnection::sessionBus();
    session.registerObject("/Notifications", this);
}

ApplicationsWidget::~ApplicationsWidget()
{
}

int ApplicationsWidget::count() const
{
    return m_frames.size();
}

void ApplicationsWidget::slotNotificationAdded(const QString &name)
{
    QMutexLocker locker(&m_mutex);
    ApplicationFrame* frame = new ApplicationFrame(name, this);
    connect(
        frame, SIGNAL(destroyed()),
        this, SLOT(slotFrameDestroyed())
    );
    m_frames.append(frame);
    m_label->setVisible(false);
    m_layout->insertItem(0, frame);
    adjustSize();

    emit countChanged();
}

void ApplicationsWidget::slotNotificationUpdated(const QString &name, const QVariantMap &data)
{
    QMutexLocker locker(&m_mutex);
    foreach (ApplicationFrame* frame, m_frames) {
        if (frame->name == name) {
            const QString appicon = data.value("appIcon").toString();
            if (!appicon.isEmpty()) {
                frame->iconwidget->setIcon(appicon);
            }
            const QStringList actions = data.value("actions").toStringList();
            QGraphicsGridLayout* framelayout = static_cast<QGraphicsGridLayout*>(frame->layout());
            Q_ASSERT(framelayout != nullptr);
            // redo the buttons layout in case of notification update
            kClearButtons(framelayout);
            QGraphicsLinearLayout* buttonslayout = nullptr;
            for (int i = 0; i < actions.size(); i++) {
                const QString actionid = actions[i];
                i++;
                const QString actionname = (i < actions.size() ? actions.at(i) : QString());
                if (actionid.isEmpty() || actionname.isEmpty()) {
                    kWarning() << "Empty action ID or name" << actionid << actionname;
                    continue;
                }

                Plasma::PushButton* actionbutton = new Plasma::PushButton(frame);
                actionbutton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                actionbutton->setProperty("_k_actionid", actionid);
                actionbutton->setText(actionname);
                connect(
                    actionbutton, SIGNAL(released()),
                    this, SLOT(slotActionReleased())
                );
                if (!buttonslayout) {
                    buttonslayout = new QGraphicsLinearLayout(Qt::Horizontal, framelayout);
                    buttonslayout->addStretch();
                }
                buttonslayout->addItem(actionbutton);
            }
            if (buttonslayout) {
                buttonslayout->addStretch();
                framelayout->addItem(buttonslayout, 3, 0, 1, 3);
                framelayout->setAlignment(buttonslayout, Qt::AlignCenter);
            }
            frame->label->setText(data.value("body").toString());
            const bool configurable = data.value("configurable").toBool();
            frame->configurewidget->setVisible(configurable);
            if (configurable) {
                frame->configurewidget->setProperty("_k_apprealname", data.value("appRealName"));
            }
            frame->adjustSize();
            adjustSize();
            break;
        }
    }
}

void ApplicationsWidget::slotFrameDestroyed()
{
    m_label->setVisible(m_frames.size() <= 0);
    adjustSize();
    emit countChanged();
}

void ApplicationsWidget::slotRemoveActivated()
{
    QMutexLocker locker(&m_mutex);
    const Plasma::IconWidget* removewidget = qobject_cast<Plasma::IconWidget*>(sender());
    ApplicationFrame* applicationframe = qobject_cast<ApplicationFrame*>(removewidget->parentObject());
    Q_ASSERT(applicationframe != nullptr);
    QMutableListIterator<ApplicationFrame*> iter(m_frames);
    while (iter.hasNext()) {
        ApplicationFrame* frame = iter.next();
        if (frame == applicationframe) {
            m_adaptor->closeNotification(applicationframe->name);
            QGraphicsGridLayout* framelayout = static_cast<QGraphicsGridLayout*>(frame->layout());
            Q_ASSERT(framelayout != nullptr);
            kClearButtons(framelayout);
            iter.remove();
            frame->animateRemove();
            break;
        }
    }
}

void ApplicationsWidget::slotConfigureActivated()
{
    QMutexLocker locker(&m_mutex);
    const Plasma::IconWidget* configurewidget = qobject_cast<Plasma::IconWidget*>(sender());
    const QString frameapprealname = configurewidget->property("_k_apprealname").toString();
    locker.unlock();
    KNotificationConfigWidget::configure(frameapprealname, nullptr);
}

void ApplicationsWidget::slotActionReleased()
{
    QMutexLocker locker(&m_mutex);
    const Plasma::PushButton* actionbutton = qobject_cast<Plasma::PushButton*>(sender());
    ApplicationFrame* actionframe = qobject_cast<ApplicationFrame*>(actionbutton->parentObject());
    Q_ASSERT(actionframe != nullptr);
    const QString actionid = actionbutton->property("_k_actionid").toString();
     m_adaptor->invokeAction(actionframe->name, actionid);
    locker.unlock();
    // remove notification too (compat)
    QTimer::singleShot(200, actionframe->removewidget, SIGNAL(activated()));
}

#include "moc_applicationswidget.cpp"
