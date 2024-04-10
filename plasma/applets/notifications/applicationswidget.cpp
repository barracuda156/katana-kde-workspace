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

#include <QTimer>
#include <QGraphicsGridLayout>
#include <Plasma/Animation>
#include <KLocale>
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

ApplicationFrame::ApplicationFrame(const QString &name, QGraphicsWidget *parent)
    : Plasma::Frame(parent),
    m_name(name),
    m_iconwidget(nullptr),
    m_label(nullptr),
    m_removewidget(nullptr),
    m_configurewidget(nullptr)
{
    setFrameShadow(Plasma::Frame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QGraphicsGridLayout* framelayout = new QGraphicsGridLayout(this);

    m_iconwidget = new Plasma::IconWidget(this);
    m_iconwidget->setAcceptHoverEvents(false);
    m_iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    m_iconwidget->setIcon(KIcon("dialog-information"));
    const int desktopiconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    const QSizeF desktopiconsizef = QSizeF(desktopiconsize, desktopiconsize);
    m_iconwidget->setPreferredIconSize(desktopiconsizef);
    m_iconwidget->setMinimumSize(desktopiconsizef);
    m_iconwidget->setMaximumSize(desktopiconsizef);
    m_iconwidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    framelayout->addItem(m_iconwidget, 0, 0, 2, 1);

    m_label = new Plasma::Label(this);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_label->nativeWidget()->setOpenExternalLinks(true);
    framelayout->addItem(m_label, 0, 1, 3, 1);

    const int smalliconsize = KIconLoader::global()->currentSize(KIconLoader::Small);
    m_removewidget = new Plasma::IconWidget(this);
    m_removewidget->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    m_removewidget->setIcon(KIcon("dialog-close"));
    m_removewidget->setToolTip(i18n("Click to remove this notification."));
    m_removewidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    connect(
        m_removewidget, SIGNAL(activated()),
        this, SLOT(slotRemoveActivated())
    );
    framelayout->addItem(m_removewidget, 0, 2, 1, 1);

    m_configurewidget = new Plasma::IconWidget(this);
    m_configurewidget->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    m_configurewidget->setIcon(KIcon("configure"));
    m_configurewidget->setToolTip(i18n("Click to configure this notification."));
    m_configurewidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_configurewidget->setVisible(false);
    connect(
        m_configurewidget, SIGNAL(activated()),
        this, SLOT(slotConfigureActivated())
    );
    framelayout->addItem(m_configurewidget, 1, 2, 1, 1);

    setLayout(framelayout);

    connect(
        NotificationsAdaptor::self(), SIGNAL(notificationUpdated(QString,QVariantMap)),
        this, SLOT(slotNotificationUpdated(QString,QVariantMap))
    );
}

void ApplicationFrame::slotNotificationUpdated(const QString &name, const QVariantMap &data)
{
    if (m_name != name) {
        return;
    }
    const QString appicon = data.value("appIcon").toString();
    const QString apprealname = data.value("appRealName").toString();
    const QStringList actions = data.value("actions").toStringList();
    if (!appicon.isEmpty()) {
        m_iconwidget->setIcon(appicon);
    }
    QGraphicsGridLayout* framelayout = static_cast<QGraphicsGridLayout*>(layout());
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

        Plasma::PushButton* actionbutton = new Plasma::PushButton(this);
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
    m_label->setText(data.value("body").toString());
    if (apprealname.isEmpty()) {
        kWarning() << "notification is not configurable, something needs a fix";
        m_configurewidget->setVisible(false);
    } else {
        m_configurewidget->setVisible(true);
        m_configurewidget->setProperty("_k_apprealname", apprealname);
    }
    adjustSize();
}

void ApplicationFrame::slotRemoveActivated()
{
    NotificationsAdaptor::self()->closeNotification(m_name);
    QGraphicsGridLayout* framelayout = static_cast<QGraphicsGridLayout*>(layout());
    Q_ASSERT(framelayout != nullptr);
    kClearButtons(framelayout);
    Plasma::Animation *animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation);
    Q_ASSERT(animation != nullptr);
    disconnect(m_removewidget, 0, this, 0);
    disconnect(m_configurewidget, 0, this, 0);

    connect(animation, SIGNAL(finished()), this, SLOT(deleteLater()));
    animation->setTargetWidget(this);
    animation->setProperty("startOpacity", 1.0);
    animation->setProperty("targetOpacity", 0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ApplicationFrame::slotConfigureActivated()
{
    const QString frameapprealname = m_configurewidget->property("_k_apprealname").toString();
    KNotificationConfigWidget::configure(frameapprealname, nullptr);
}

void ApplicationFrame::slotActionReleased()
{
    const Plasma::PushButton* actionbutton = qobject_cast<Plasma::PushButton*>(sender());
    const QString actionid = actionbutton->property("_k_actionid").toString();
     NotificationsAdaptor::self()->invokeAction(m_name, actionid);
    // remove notification too (compat)
    QTimer::singleShot(200, m_removewidget, SIGNAL(activated()));
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

    m_adaptor = NotificationsAdaptor::self();
    connect(
        m_adaptor, SIGNAL(notificationAdded(QString)),
        this, SLOT(slotNotificationAdded(QString))
    );
    m_adaptor->registerObject();
}

ApplicationsWidget::~ApplicationsWidget()
{
    m_adaptor->unregisterObject();
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
        frame, SIGNAL(destroyed(QObject*)),
        this, SLOT(slotFrameDestroyed(QObject*))
    );
    m_frames.append(frame);
    m_label->setVisible(false);
    m_layout->insertItem(0, frame);
    adjustSize();

    emit countChanged();
}

void ApplicationsWidget::slotFrameDestroyed(QObject *object)
{
    QMutexLocker locker(&m_mutex);
    m_frames.removeAll(object);
    m_label->setVisible(m_frames.size() <= 0);
    adjustSize();
    emit countChanged();
}

#include "moc_applicationswidget.cpp"
