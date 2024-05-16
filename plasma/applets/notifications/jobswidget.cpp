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
#include <Plasma/Animation>
#include <KLocale>
#include <KToolInvocation>
#include <KIconLoader>
#include <KIcon>
#include <KUrl>
#include <KDebug>

// NOTE: this is for generic jobs such as trash jobs which provide next to no info
static QString kJobState(const QByteArray &state)
{
    if (state == "running") {
        return i18n("Job running");
    } else if (state == "stopped") {
        return i18n("Job finished");
    } else if (state == "suspended") {
        return i18n("Job suspended");
    }
    kWarning() << "unknown job state" << state;
    Q_ASSERT(false);
    return QString::fromLatin1(state.constData(), state.size());
}

JobFrame::JobFrame(const QString &name, QGraphicsWidget *parent)
    : Plasma::Frame(parent),
    m_name(name),
    m_iconwidget(nullptr),
    m_label(nullptr),
    m_iconwidget0(nullptr),
    m_iconwidget1(nullptr),
    m_meter(nullptr)
{
    setFrameShadow(Plasma::Frame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QGraphicsGridLayout* framelayout = new QGraphicsGridLayout(this);

    m_iconwidget = new Plasma::IconWidget(this);
    m_iconwidget->setAcceptHoverEvents(false);
    m_iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    m_iconwidget->setIcon(KIcon("services"));
    const int desktopiconsize = KIconLoader::global()->currentSize(KIconLoader::Desktop);
    const QSizeF desktopiconsizef = QSizeF(desktopiconsize, desktopiconsize);
    m_iconwidget->setPreferredIconSize(desktopiconsizef);
    m_iconwidget->setMinimumSize(desktopiconsizef);
    m_iconwidget->setMaximumSize(desktopiconsizef);
    m_iconwidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    framelayout->addItem(m_iconwidget, 0, 0, 2, 1);

    m_label = new Plasma::Label(this);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_label->nativeWidget()->setOpenExternalLinks(true);
    framelayout->addItem(m_label, 0, 1, 3, 1);

    const int smalliconsize = KIconLoader::global()->currentSize(KIconLoader::Small);
    m_iconwidget0 = new Plasma::IconWidget(this);
    m_iconwidget0->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    m_iconwidget0->setIcon(KIcon("task-reject"));
    m_iconwidget0->setToolTip(i18n("Click to stop the job."));
    m_iconwidget0->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_iconwidget0->setVisible(false);
    connect(
        m_iconwidget0, SIGNAL(activated()),
        this, SLOT(slotIcon0Activated())
    );
    framelayout->addItem(m_iconwidget0, 0, 2, 1, 1);

    m_iconwidget1 = new Plasma::IconWidget(this);
    m_iconwidget1->setMaximumIconSize(QSize(smalliconsize, smalliconsize));
    m_iconwidget1->setIcon(KIcon("task-complete"));
    m_iconwidget1->setToolTip(i18n("The job has completed."));
    m_iconwidget1->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_iconwidget1->setVisible(false);
    connect(
        m_iconwidget1, SIGNAL(activated()),
        this, SLOT(slotIcon1Activated())
    );
    framelayout->addItem(m_iconwidget1, 1, 2, 1, 1);

    m_meter = new Plasma::Meter(this);
    m_meter->setMeterType(Plasma::Meter::BarMeterHorizontal);
    m_meter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_meter->setMinimum(0);
    m_meter->setMaximum(100);
    m_meter->setVisible(false);
    framelayout->addItem(m_meter, 4, 0, 1, 3);

    setLayout(framelayout);

    connect(
        JobTrackerAdaptor::self(), SIGNAL(jobUpdated(QString,QVariantMap)),
        this, SLOT(slotJobUpdated(QString,QVariantMap))
    );
}

void JobFrame::slotJobUpdated(const QString &name, const QVariantMap &data)
{
    if (m_name != name) {
        return;
    }
    const QString appiconname = data.value("appIconName").toString();
    const QString labelname0 = data.value("labelName0").toString();
    const QString labelname1 = data.value("labelName1").toString();
    const QString infomessage = data.value("infoMessage").toString();
    const uint percentage = data.value("percentage").toUInt();
    const QByteArray state = data.value("state").toByteArray();
    const bool killable = data.value("killable").toBool();
    const QString desturl = data.value("destUrl").toString();
    const QString error = data.value("error").toString();
    if (!appiconname.isEmpty()) {
        m_iconwidget->setIcon(appiconname);
    }
    setText(infomessage);
    if (!labelname0.isEmpty() && !labelname1.isEmpty()) {
        m_label->setText(
            i18n(
                "<p><b>%1:</b> <i>%2</i></p><p><b>%3:</b> <i>%4</i></p>",
                labelname0, data.value("label0").toString(),
                labelname1, data.value("label1").toString()
            )
        );
    } else if (!labelname0.isEmpty()) {
        m_label->setText(
            i18n(
                "<b>%1:</b> <i>%2</i>",
                labelname0, data.value("label0").toString()
            )
        );
    } else if (!labelname1.isEmpty()) {
        m_label->setText(
            i18n(
                "<b>%1:</b> <i>%2</i>",
                labelname1, data.value("label1").toString()
            )
        );
    } else if (!desturl.isEmpty()) {
        m_label->setText(
            i18n(
                "<b>%1:</b> <i>%2</i>",
                kJobState(state), desturl
            )
        );
    } else {
        m_label->setText(
            i18n(
                "<i>%1</i>",
                kJobState(state)
            )
        );
    }
    if (percentage > 0) {
        m_meter->setVisible(true);
        m_meter->setValue(percentage);
    }
    if (killable) {
        m_iconwidget0->setVisible(true);
    }
    if (state == "stopped") {
        m_iconwidget0->setIcon(KIcon("dialog-close"));
        m_iconwidget0->setToolTip(i18n("Click to remove this job notification."));
        m_iconwidget0->setProperty("_k_stopped", true);

        m_iconwidget1->setVisible(true);
        if (!desturl.isEmpty()) {
            m_iconwidget1->setProperty("_k_desturl", desturl);
            m_iconwidget1->setIcon(KIcon("system-run"));
            m_iconwidget1->setToolTip(i18n("Click to open the destination of the job."));
        } else {
            m_iconwidget1->setAcceptHoverEvents(false);
            m_iconwidget1->setAcceptedMouseButtons(Qt::NoButton);
        }
    }
    // error overrides everything iconwidget1 does
    if (!error.isEmpty()) {
        m_iconwidget1->setVisible(false);
        m_iconwidget1->setAcceptHoverEvents(false);
        m_iconwidget1->setAcceptedMouseButtons(Qt::NoButton);
        m_iconwidget1->setIcon(KIcon("task-attention"));
        m_iconwidget1->setToolTip(error);
    }
    adjustSize();
}

void JobFrame::slotIcon0Activated()
{
    const bool stopped = m_iconwidget0->property("_k_stopped").toBool();
    if (!stopped) {
        JobTrackerAdaptor::self()->stopJob(m_name);
    } else {
        Plasma::Animation *animation = Plasma::Animator::create(Plasma::Animator::FadeAnimation, this);
        Q_ASSERT(animation != nullptr);
        disconnect(m_iconwidget0, 0, this, 0);
        disconnect(m_iconwidget1, 0, this, 0);

        connect(animation, SIGNAL(finished()), this, SLOT(deleteLater()));
        animation->setTargetWidget(this);
        animation->setProperty("startOpacity", 1.0);
        animation->setProperty("targetOpacity", 0.0);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void JobFrame::slotIcon1Activated()
{
    const KUrl desturl = KUrl(m_iconwidget1->property("_k_desturl").toString());
    KToolInvocation::self()->startServiceForUrl(desturl.url());
}


JobsWidget::JobsWidget(QGraphicsItem *parent)
    : QGraphicsWidget(parent),
    m_layout(nullptr),
    m_label(nullptr),
    m_adaptor(nullptr)
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

    m_adaptor = JobTrackerAdaptor::self();
    connect(
        m_adaptor, SIGNAL(jobAdded(QString)),
        this, SLOT(slotJobAdded(QString))
    );
    m_adaptor->registerObject();
}

JobsWidget::~JobsWidget()
{
    m_adaptor->unregisterObject();
}

int JobsWidget::count() const
{
    return m_frames.size();
}

void JobsWidget::slotJobAdded(const QString &name)
{
    QMutexLocker locker(&m_mutex);
    JobFrame* jobframe = new JobFrame(name, this);
    connect(
        jobframe, SIGNAL(destroyed(QObject*)),
        this, SLOT(slotFrameDestroyed(QObject*))
    );
    m_frames.append(jobframe);
    m_label->setVisible(false);
    m_layout->insertItem(0, jobframe);
    adjustSize();

    emit countChanged();
}

void JobsWidget::slotFrameDestroyed(QObject *object)
{
    QMutexLocker locker(&m_mutex);
    m_frames.removeAll(object);
    m_label->setVisible(m_frames.size() <= 0);
    adjustSize();
    emit countChanged();
}

#include "moc_jobswidget.cpp"
