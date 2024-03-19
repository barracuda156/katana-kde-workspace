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

#include "jobswidget.h"

#include <QGraphicsGridLayout>
#include <Plasma/DataEngineManager>
#include <Plasma/Animation>
#include <KRun>
#include <KIconLoader>
#include <KIcon>
#include <KMimeType>
#include <KDebug>

JobFrame::JobFrame(const QString &_name, QGraphicsWidget *parent)
    : Plasma::Frame(parent),
    iconwidget(nullptr),
    label(nullptr),
    iconwidget0(nullptr),
    iconwidget1(nullptr),
    meter(nullptr),
    name(_name)
{
    JobsWidget* jobswidget = qobject_cast<JobsWidget*>(parent);

    setFrameShadow(Plasma::Frame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QGraphicsGridLayout* framelayout = new QGraphicsGridLayout(this);

    iconwidget = new Plasma::IconWidget(this);
    iconwidget->setAcceptHoverEvents(false);
    iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    iconwidget->setIcon(KIcon("services"));
    const int desktopiconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    const QSizeF desktopiconsizef = QSizeF(desktopiconsize, desktopiconsize);
    iconwidget->setPreferredIconSize(desktopiconsizef);
    iconwidget->setMinimumSize(desktopiconsizef);
    iconwidget->setMaximumSize(desktopiconsizef);
    iconwidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    framelayout->addItem(iconwidget, 0, 0, 2, 1);

    label = new Plasma::Label(this);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->nativeWidget()->setOpenExternalLinks(true);
    framelayout->addItem(label, 0, 1, 3, 1);

    const int smalliconsize = KIconLoader::global()->currentSize(KIconLoader::Small);
    iconwidget0 = new Plasma::IconWidget(this);
    iconwidget0->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    iconwidget0->setIcon(KIcon("task-reject"));
    iconwidget0->setToolTip(i18n("Click to stop the job."));
    iconwidget0->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    iconwidget0->setVisible(false);
    connect(
        iconwidget0, SIGNAL(activated()),
        jobswidget, SLOT(slotIcon0Activated())
    );
    framelayout->addItem(iconwidget0, 0, 2, 1, 1);

    iconwidget1 = new Plasma::IconWidget(this);
    iconwidget1->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    iconwidget1->setIcon(KIcon("task-complete"));
    iconwidget1->setToolTip(i18n("The job has completed."));
    iconwidget1->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    iconwidget1->setVisible(false);
    connect(
        iconwidget1, SIGNAL(activated()),
        jobswidget, SLOT(slotIcon1Activated())
    );
    framelayout->addItem(iconwidget1, 1, 2, 1, 1);

    meter = new Plasma::Meter(this);
    meter->setMeterType(Plasma::Meter::BarMeterHorizontal);
    meter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    meter->setMinimum(0);
    meter->setMaximum(100);
    meter->setVisible(false);
    framelayout->addItem(meter, 4, 0, 1, 3);

    setLayout(framelayout);
}

void JobFrame::animateRemove()
{
    Plasma::Animation *animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation);
    Q_ASSERT(animation != nullptr);
    JobsWidget* jobswidget = qobject_cast<JobsWidget*>(parentObject());
    disconnect(iconwidget0, 0, jobswidget, 0);
    disconnect(iconwidget1, 0, jobswidget, 0);

    connect(animation, SIGNAL(finished()), this, SLOT(deleteLater()));
    animation->setTargetWidget(this);
    animation->setProperty("startOpacity", 1.0);
    animation->setProperty("targetOpacity", 0.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}


JobsWidget::JobsWidget(QGraphicsItem *parent, NotificationsWidget *notificationswidget)
    : QGraphicsWidget(parent),
    m_notificationswidget(notificationswidget),
    m_layout(nullptr),
    m_label(nullptr),
    m_dataengine(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout = new QGraphicsLinearLayout(Qt::Vertical, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_label = new Plasma::Label(this);
    m_label->setText(i18n("No job notifications"));
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addItem(m_label);
    setLayout(m_layout);

    m_dataengine = Plasma::DataEngineManager::self()->loadEngine("applicationjobs");
    if (!m_dataengine) {
        kWarning() << "Could not load applicationjobs data engine";
        return;
    }
    connect(
        m_dataengine, SIGNAL(sourceAdded(QString)),
        this, SLOT(sourceAdded(QString))
    );
}

JobsWidget::~JobsWidget()
{
    if (m_dataengine) {
        Plasma::DataEngineManager::self()->unloadEngine("applicationjobs");
    }
}

int JobsWidget::count() const
{
    return m_frames.size();
}

void JobsWidget::sourceAdded(const QString &name)
{
    QMutexLocker locker(&m_mutex);
    JobFrame* frame = new JobFrame(name, this);
    connect(
        frame, SIGNAL(destroyed()),
        this, SLOT(slotFrameDestroyed())
    );
    m_frames.append(frame);
    m_label->setVisible(false);
    m_layout->insertItem(0, frame);
    adjustSize();

    emit countChanged();
    locker.unlock();
    m_dataengine->connectSource(name, this);
}

void JobsWidget::dataUpdated(const QString &name, const Plasma::DataEngine::Data &data)
{
    QMutexLocker locker(&m_mutex);
    foreach (JobFrame* frame, m_frames) {
        if (frame->name == name) {
            const QString infomessage = data.value("infoMessage").toString();
            frame->setText(infomessage);
            const QString appiconname = data.value("appIconName").toString();
            if (!appiconname.isEmpty()) {
                frame->iconwidget->setIcon(appiconname);
            }
            const QString labelname0 = data.value("labelName0").toString();
            const QString labelname1 = data.value("labelName1").toString();
            if (!labelname0.isEmpty() && !labelname1.isEmpty()) {
                frame->label->setText(
                    i18n(
                        "<p><b>%1:</b> <i>%2</i></p><p><b>%3:</b> <i>%4</i></p>",
                        labelname0, data.value("label0").toString(),
                        labelname1, data.value("label1").toString()
                    )
                );
            } else if (!labelname0.isEmpty()) {
                frame->label->setText(
                    i18n(
                        "<b>%1:</b> <i>%2</i>",
                        labelname0, data.value("label0").toString()
                    )
                );
            } else if (!labelname1.isEmpty()) {
                frame->label->setText(
                    i18n(
                        "<b>%1:</b> <i>%2</i>",
                        labelname1, data.value("label1").toString()
                    )
                );
            }
            const uint percentage = data.value("percentage").toUInt();
            if (percentage > 0) {
                frame->meter->setVisible(true);
                frame->meter->setValue(percentage);
            }
            const QByteArray state = data.value("state").toByteArray();
            const bool killable = data.value("killable").toBool();
            if (killable) {
                frame->iconwidget0->setVisible(true);
            }
            if (state == "stopped") {
                frame->iconwidget0->setIcon(KIcon("dialog-close"));
                frame->iconwidget0->setToolTip(i18n("Click to remove this job notification."));
                frame->iconwidget0->setProperty("_k_stopped", true);

                frame->iconwidget1->setVisible(true);
                const QString desturl = data.value("destUrl").toString();
                if (!desturl.isEmpty()) {
                    frame->iconwidget1->setProperty("_k_desturl", desturl);
                    frame->iconwidget1->setIcon(KIcon("system-file-manager"));
                    frame->iconwidget1->setToolTip(i18n("Click to open the destination of the job."));
                } else {
                    frame->iconwidget1->setAcceptHoverEvents(false);
                    frame->iconwidget1->setAcceptedMouseButtons(Qt::NoButton);
                }
            }
            // error overrides everything iconwidget1 does
            const QString error = data.value("error").toString();
            if (!error.isEmpty()) {
                frame->iconwidget1->setVisible(false);
                frame->iconwidget1->setAcceptHoverEvents(false);
                frame->iconwidget1->setAcceptedMouseButtons(Qt::NoButton);
                frame->iconwidget1->setIcon(KIcon("task-attention"));
                frame->iconwidget1->setToolTip(error);
            }
            frame->adjustSize();
            adjustSize();
            break;
        }
    }
}

void JobsWidget::slotFrameDestroyed()
{
    m_label->setVisible(m_frames.size() <= 0);
    adjustSize();
    emit countChanged();
}

void JobsWidget::slotIcon0Activated()
{
    QMutexLocker locker(&m_mutex);
    const Plasma::IconWidget* iconwidget0 = qobject_cast<Plasma::IconWidget*>(sender());
    const bool stopped = iconwidget0->property("_k_stopped").toBool();
    JobFrame* jobframe = qobject_cast<JobFrame*>(iconwidget0->parentObject());
    Q_ASSERT(jobframe != nullptr);
    if (!stopped) {
        Plasma::Service* plasmaservice = m_dataengine->serviceForSource(jobframe->name);
        if (!plasmaservice) {
            kWarning() << "Could not get service for" << jobframe->name;
        } else {
            plasmaservice->setParent(this);
            QVariantMap plasmaserviceargs = plasmaservice->operationParameters("stop");
            (void)plasmaservice->startOperationCall("stop", plasmaserviceargs);
        }
    } else {
        QMutableListIterator<JobFrame*> iter(m_frames);
        while (iter.hasNext()) {
            JobFrame* frame = iter.next();
            if (frame == jobframe) {
                m_dataengine->disconnectSource(frame->name, this);
                iter.remove();
                frame->animateRemove();
                break;
            }
        }
    }
}

void JobsWidget::slotIcon1Activated()
{
    QMutexLocker locker(&m_mutex);
    const Plasma::IconWidget* iconwidget1 = qobject_cast<Plasma::IconWidget*>(sender());
    const KUrl desturl = KUrl(iconwidget1->property("_k_desturl").toString());
    locker.unlock();
    const KMimeType::Ptr kmimetype = KMimeType::findByUrl(desturl);
    Q_ASSERT(kmimetype);
    KRun::runUrl(desturl, kmimetype->name(), nullptr);
}

#include "moc_jobswidget.cpp"
