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

#include "lockout.h"
#include "kworkspace.h"
#include "kdisplaymanager.h"

#include <QVBoxLayout>
#include <QDBusInterface>
#include <QEventLoop>
#include <QTimer>
#include <QGraphicsGridLayout>
#include <Plasma/Svg>
#include <Plasma/Dialog>
#include <Plasma/Label>
#include <Plasma/Separator>
#include <Plasma/PushButton>
#include <Solid/PowerManagement>
#include <KWindowSystem>
#include <KDebug>

// standard issue margin/spacing
static const int s_spacing = 4;
// even panels do not get bellow that
static const QSizeF s_basesize = QSizeF(10, 10);
static const bool s_showswitch = true;
static const bool s_showshutdown = true;
static const bool s_showtoram = true;
static const bool s_showtodisk = true;
static const bool s_showhybrid = true;
static const bool s_confirmswitch = true;
static const bool s_confirmshutdown = true;
static const bool s_confirmtoram = true;
static const bool s_confirmtodisk = true;
static const bool s_confirmhybrid = true;
// delay for the dialog animation to complete. the animation duration is 250ms but the delay here
// is intentionally 500ms, see:
// kwin/effects/slide/slide.cpp
static const int s_dodelay = 500;

class LockoutDialog : public Plasma::Dialog
{
    Q_OBJECT
public:
    LockoutDialog(QWidget *parent = nullptr);
    ~LockoutDialog();

    void setup(const QString &icon, const QString &title, const QString &text);
    bool exec();
    void interrupt();

protected:
    // Plasma::Dialog reimplementation
    void hideEvent(QHideEvent *event) final;

private Q_SLOTS:
    void slotYes();
    void slotNo();

private:
    QGraphicsScene* m_scene;
    QGraphicsWidget* m_widget;
    QGraphicsGridLayout* m_layout;
    Plasma::IconWidget* m_iconwidget;
    Plasma::Separator* m_separator;
    Plasma::Label* m_label;
    Plasma::PushButton* m_yesbutton;
    Plasma::PushButton* m_nobutton;
    QEventLoop* m_eventloop;
};

LockoutDialog::LockoutDialog(QWidget *parent)
    : Plasma::Dialog(parent, Qt::Dialog),
    m_scene(nullptr),
    m_widget(nullptr),
    m_layout(nullptr),
    m_iconwidget(nullptr),
    m_separator(nullptr),
    m_label(nullptr),
    m_yesbutton(nullptr),
    m_nobutton(nullptr),
    m_eventloop(nullptr)
{
    m_scene = new QGraphicsScene(this);
    m_widget = new QGraphicsWidget();
    m_widget->setMinimumSize(280, 130);

    m_layout = new QGraphicsGridLayout(m_widget);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(s_spacing);
    m_iconwidget = new Plasma::IconWidget(m_widget);
    m_iconwidget->setOrientation(Qt::Horizontal);
    // NOTE: setPreferredIconSize() does not ensure the icon size is always the same
    const int paneliconsize = KIconLoader::global()->currentSize(KIconLoader::Panel);
    const QSizeF paneliconsizef = QSizeF(paneliconsize, paneliconsize);
    m_iconwidget->setMinimumIconSize(paneliconsizef);
    m_iconwidget->setMaximumIconSize(paneliconsizef);
    // disable hover effect
    m_iconwidget->setAcceptHoverEvents(false);
    // disable mouse click and such
    m_iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    m_layout->addItem(m_iconwidget, 0, 0, 1, 2);
    m_separator = new Plasma::Separator(m_widget);
    m_layout->addItem(m_separator, 1, 0, 1, 2);
    m_label = new Plasma::Label(m_widget);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addItem(m_label, 2, 0, 1, 2);
    m_yesbutton = new Plasma::PushButton(m_widget);
    m_yesbutton->setIcon(KIcon("dialog-ok"));
    m_yesbutton->setText(i18n("&Yes"));
    connect(
        m_yesbutton, SIGNAL(released()),
        this, SLOT(slotYes())
    );
    m_layout->addItem(m_yesbutton, 3, 0, 1, 1);
    m_nobutton = new Plasma::PushButton(m_widget);
    m_nobutton->setIcon(KIcon("process-stop"));
    m_nobutton->setText(i18n("&No"));
    connect(
        m_nobutton, SIGNAL(released()),
        this, SLOT(slotNo())
    );
    m_layout->addItem(m_nobutton, 3, 1, 1, 1);
    m_layout->setRowMaximumHeight(3, m_nobutton->preferredSize().height() + m_layout->verticalSpacing());
    m_widget->setLayout(m_layout);

    m_scene->addItem(m_widget);
    setGraphicsWidget(m_widget);

    adjustSize();
    KDialog::centerOnScreen(this, -3);
}

LockoutDialog::~LockoutDialog()
{
    delete m_widget;
}

void LockoutDialog::setup(const QString &icon, const QString &title, const QString &text)
{
    // force-update before showing
    m_iconwidget->setIcon(KIcon());
    m_iconwidget->setIcon(icon);
    m_iconwidget->setText(title);
    m_label->setText(text);
}

bool LockoutDialog::exec()
{
    KWindowSystem::setState(winId(), NET::SkipPager | NET::SkipTaskbar);
    // default to yes like KDialog defaults to KDialog::Ok
    m_yesbutton->setFocus();
    // NOTE: this also animates hide
    animatedShow(Plasma::locationToDirection(Plasma::Location::Desktop));
    if (m_eventloop) {
        m_eventloop->exit(1);
        m_eventloop->deleteLater();
    }
    m_eventloop = new QEventLoop(this);
    return (m_eventloop->exec() == 0);
}

void LockoutDialog::interrupt()
{
    if (!m_eventloop) {
        return;
    }
    m_eventloop->exit(1);
}

// for when closed by means other than clicking a button
void LockoutDialog::hideEvent(QHideEvent *event)
{
    interrupt();
    Plasma::Dialog::hideEvent(event);
}

void LockoutDialog::slotYes()
{
    Q_ASSERT(m_eventloop);
    m_eventloop->exit(0);
    m_eventloop->deleteLater();
    m_eventloop = nullptr;
    close();
}

void LockoutDialog::slotNo()
{
    Q_ASSERT(m_eventloop);
    m_eventloop->exit(1);
    m_eventloop->deleteLater();
    m_eventloop = nullptr;
    close();
}


LockoutApplet::LockoutApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_layout(nullptr),
    m_switchwidget(nullptr),
    m_shutdownwidget(nullptr),
    m_toramwidget(nullptr),
    m_todiskwidget(nullptr),
    m_hybridwidget(nullptr),
    m_showswitch(s_showswitch),
    m_showshutdown(s_showshutdown),
    m_showtoram(s_showtoram),
    m_showtodisk(s_showtodisk),
    m_showhybrid(s_showhybrid),
    m_confirmswitch(s_confirmswitch),
    m_confirmshutdown(s_confirmshutdown),
    m_confirmtoram(s_confirmtoram),
    m_confirmtodisk(s_confirmtodisk),
    m_confirmhybrid(s_confirmhybrid),
    m_buttonsmessage(nullptr),
    m_switchbox(nullptr),
    m_shutdownbox(nullptr),
    m_torambox(nullptr),
    m_todiskbox(nullptr),
    m_hybridbox(nullptr),
    m_spacer(nullptr),
    m_dialog(nullptr),
    m_dowhat(LockoutApplet::DoNothing)
{
    KGlobal::locale()->insertCatalog("plasma_applet_lockout");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
}

LockoutApplet::~LockoutApplet()
{
    if (m_dialog) {
        m_dialog->interrupt();
    }
}

void LockoutApplet::init()
{
    Plasma::Applet::init();

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(s_spacing);

    m_switchwidget = new Plasma::IconWidget(this);
    m_switchwidget->setIcon("system-switch-user");
    m_switchwidget->setToolTip(i18n("Start a parallel session as a different user"));
    connect(
        m_switchwidget, SIGNAL(activated()),
        this, SLOT(slotSwitch())
    );
    m_layout->addItem(m_switchwidget);

    m_shutdownwidget = new Plasma::IconWidget(this);
    m_shutdownwidget->setIcon("system-shutdown");
    m_shutdownwidget->setToolTip(i18n("Logout, turn off or restart the computer"));
    connect(
        m_shutdownwidget, SIGNAL(activated()),
        this, SLOT(slotShutdown())
    );
    m_layout->addItem(m_shutdownwidget);

    m_toramwidget = new Plasma::IconWidget(this);
    m_toramwidget->setIcon("system-suspend");
    m_toramwidget->setToolTip(i18n("Sleep (suspend to RAM)"));
    connect(
        m_toramwidget, SIGNAL(activated()),
        this, SLOT(slotToRam())
    );
    m_layout->addItem(m_toramwidget);

    m_todiskwidget = new Plasma::IconWidget(this);
    m_todiskwidget->setIcon("system-suspend-hibernate");
    m_todiskwidget->setToolTip(i18n("Hibernate (suspend to disk)"));
    connect(
        m_todiskwidget, SIGNAL(activated()),
        this, SLOT(slotToDisk())
    );
    m_layout->addItem(m_todiskwidget);

    m_hybridwidget = new Plasma::IconWidget(this);
    m_hybridwidget->setIcon("system-suspend");
    m_hybridwidget->setToolTip(i18n("Hybrid Suspend (Suspend to RAM and put the system in sleep mode)"));
    connect(
        m_hybridwidget, SIGNAL(activated()),
        this, SLOT(slotHybrid())
    );
    m_layout->addItem(m_hybridwidget);
    setLayout(m_layout);

    KConfigGroup configgroup = config();
    m_showswitch = configgroup.readEntry("showSwitchButton", s_showswitch);
    m_showshutdown = configgroup.readEntry("showShutdownButton", s_showshutdown);
    m_showtoram = configgroup.readEntry("showToRamButton", s_showtoram);
    m_showtodisk = configgroup.readEntry("showToDiskButton", s_showtodisk);
    m_showhybrid = configgroup.readEntry("showHybridButton", s_showhybrid);
    m_confirmswitch = configgroup.readEntry("confirmSwitchButton", s_confirmswitch);
    m_confirmshutdown = configgroup.readEntry("confirmShutdownButton", s_confirmshutdown);
    m_confirmtoram = configgroup.readEntry("confirmToRamButton", s_confirmtoram);
    m_confirmtodisk = configgroup.readEntry("confirmToDiskButton", s_confirmtodisk);
    m_confirmhybrid = configgroup.readEntry("confirmHybridButton", s_confirmhybrid);

    slotUpdateButtons();

    connect(
        Solid::PowerManagement::notifier(), SIGNAL(supportedSleepStatesChanged()),
        this, SLOT(slotUpdateButtons())
    );
}

void LockoutApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QVBoxLayout* widgetlayout = new QVBoxLayout(widget);
    m_buttonsmessage = new KMessageWidget(widget);
    m_buttonsmessage->setCloseButtonVisible(false);
    m_buttonsmessage->setMessageType(KMessageWidget::Information);
    m_buttonsmessage->setWordWrap(true);
    m_buttonsmessage->setText(
        i18n(
            "If a button is not enabled that is because what it does <b>is not supported on the current host</b>."
        )
    );
    widgetlayout->addWidget(m_buttonsmessage);
    m_switchbox = new QCheckBox(widget);
    m_switchbox->setText(i18n("Show the “Switch” button"));
    m_switchbox->setChecked(m_showswitch);
    widgetlayout->addWidget(m_switchbox);
    m_shutdownbox = new QCheckBox(widget);
    m_shutdownbox->setText(i18n("Show the “Shutdown” button"));
    m_shutdownbox->setChecked(m_showshutdown);
    widgetlayout->addWidget(m_shutdownbox);
    m_torambox = new QCheckBox(widget);
    m_torambox->setText(i18n("Show the “Suspend to RAM” button"));
    m_torambox->setChecked(m_showtoram);
    widgetlayout->addWidget(m_torambox);
    m_todiskbox = new QCheckBox(widget);
    m_todiskbox->setText(i18n("Show the “Suspend to Disk” button"));
    m_todiskbox->setChecked(m_showtodisk);
    widgetlayout->addWidget(m_todiskbox);
    m_hybridbox = new QCheckBox(widget);
    m_hybridbox->setText(i18n("Show the “Hybrid Suspend” button"));
    m_hybridbox->setChecked(m_showhybrid);
    widgetlayout->addWidget(m_hybridbox);
    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addSpacerItem(m_spacer);
    widget->setLayout(widgetlayout);
    // insert-button is 16x16 only
    parent->addPage(widget, i18n("Buttons"), "applications-graphics");

    slotCheckButtons();

    connect(m_switchbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_switchbox, SIGNAL(stateChanged(int)), this, SLOT(slotCheckButtons()));
    connect(m_shutdownbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_shutdownbox, SIGNAL(stateChanged(int)), this, SLOT(slotCheckButtons()));
    connect(m_torambox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_torambox, SIGNAL(stateChanged(int)), this, SLOT(slotCheckButtons()));
    connect(m_todiskbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_todiskbox, SIGNAL(stateChanged(int)), this, SLOT(slotCheckButtons()));
    connect(m_hybridbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_hybridbox, SIGNAL(stateChanged(int)), this, SLOT(slotCheckButtons()));

    widget = new QWidget();
    widgetlayout = new QVBoxLayout(widget);
    m_switchconfirmbox = new QCheckBox(widget);
    m_switchconfirmbox->setText(i18n("Confirm the “Switch” action"));
    m_switchconfirmbox->setChecked(m_confirmswitch);
    widgetlayout->addWidget(m_switchconfirmbox);
    m_shutdownconfirmbox = new QCheckBox(widget);
    m_shutdownconfirmbox->setText(i18n("Confirm the “Shutdown” action"));
    m_shutdownconfirmbox->setChecked(m_confirmshutdown);
    widgetlayout->addWidget(m_shutdownconfirmbox);
    m_toramconfirmbox = new QCheckBox(widget);
    m_toramconfirmbox->setText(i18n("Confirm the “Suspend to RAM” action"));
    m_toramconfirmbox->setChecked(m_confirmtoram);
    widgetlayout->addWidget(m_toramconfirmbox);
    m_todiskconfirmbox = new QCheckBox(widget);
    m_todiskconfirmbox->setText(i18n("Confirm the “Suspend to Disk” action"));
    m_todiskconfirmbox->setChecked(m_confirmtodisk);
    widgetlayout->addWidget(m_todiskconfirmbox);
    m_hybridconfirmbox = new QCheckBox(widget);
    m_hybridconfirmbox->setText(i18n("Confirm the “Hybrid Suspend” action"));
    m_hybridconfirmbox->setChecked(m_confirmhybrid);
    widgetlayout->addWidget(m_hybridconfirmbox);
    m_spacer2 = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addSpacerItem(m_spacer2);
    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("Confirmation"), "task-accepted");

    connect(m_switchconfirmbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_shutdownconfirmbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_toramconfirmbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_todiskconfirmbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_hybridconfirmbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
}

void LockoutApplet::updateSizes()
{
    switch (m_layout->orientation()) {
        case Qt::Horizontal: {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            break;
        }
        case Qt::Vertical: {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            break;
        }
    }
    const QSizeF contentsrect = contentsRect().size();
    const qreal iconsizereal = qMin(contentsrect.width(), contentsrect.height());
    const QSizeF iconsizef = QSizeF(iconsizereal, iconsizereal);
    m_switchwidget->setPreferredSize(iconsizef);
    m_shutdownwidget->setPreferredSize(iconsizef);
    m_toramwidget->setPreferredSize(iconsizef);
    m_todiskwidget->setPreferredSize(iconsizef);
    m_hybridwidget->setPreferredSize(iconsizef);
    emit sizeHintChanged(Qt::PreferredSize);
}

void LockoutApplet::constraintsEvent(Plasma::Constraints constraints)
{
    // perfect size finder
    // qDebug() << Q_FUNC_INFO << size();
    if (constraints & Plasma::SizeConstraint || constraints & Plasma::FormFactorConstraint) {
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal: {
                m_layout->setOrientation(Qt::Horizontal);
                m_layout->setSpacing(0);
                updateSizes();
                return;
            }
            case Plasma::FormFactor::Vertical: {
                m_layout->setOrientation(Qt::Vertical);
                m_layout->setSpacing(0);
                updateSizes();
                return;
            }
            default: {
                m_layout->setSpacing(s_spacing);
                break;
            }
        }

        const QSizeF appletsize = size();
        if (appletsize.width() >= appletsize.height()) {
            m_layout->setOrientation(Qt::Horizontal);
        } else {
            m_layout->setOrientation(Qt::Vertical);
        }
        updateSizes();
    }
}

void LockoutApplet::slotUpdateButtons()
{
    // no signals for these
    KDisplayManager kdisplaymanager;
    m_switchwidget->setVisible(m_showswitch);
    m_switchwidget->setEnabled(kdisplaymanager.isSwitchable());
    m_shutdownwidget->setVisible(m_showshutdown);
    m_shutdownwidget->setEnabled(KWorkSpace::canShutDown());

    QSet<Solid::PowerManagement::SleepState> sleepstates = Solid::PowerManagement::supportedSleepStates();
    m_toramwidget->setVisible(m_showtoram);
    m_toramwidget->setEnabled(sleepstates.contains(Solid::PowerManagement::SuspendState));
    m_todiskwidget->setVisible(m_showtodisk);
    m_todiskwidget->setEnabled(sleepstates.contains(Solid::PowerManagement::HibernateState));
    m_hybridwidget->setVisible(m_showhybrid);
    m_hybridwidget->setEnabled(sleepstates.contains(Solid::PowerManagement::HybridSuspendState));
}

void LockoutApplet::slotSwitch()
{
    if (m_dowhat != LockoutApplet::DoNothing) {
        // disallow another action while there is one queued
        return;
    }

    if (m_confirmswitch) {
        if (!m_dialog) {
            m_dialog = new LockoutDialog();
        } else {
            m_dialog->interrupt();
        }
        m_dialog->setup(
            QString::fromLatin1("system-switch-user"),
            i18n("Switch"),
            i18n("Do you want to switch to different user?")
        );
        if (!m_dialog->exec()) {
            return;
        }
    }

    m_dowhat = LockoutApplet::DoSwitch;
    QTimer::singleShot(s_dodelay, this, SLOT(slotDoIt()));
}

void LockoutApplet::slotShutdown()
{
    // NOTE: KDisplayManager::shutdown() does not involve the session manager
    if (m_confirmshutdown) {
        KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmYes);
        return;
    }
    KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmNo);
}

void LockoutApplet::slotToRam()
{
    if (m_dowhat != LockoutApplet::DoNothing) {
        return;
    }

    if (m_confirmtoram) {
        if (!m_dialog) {
            m_dialog = new LockoutDialog();
        } else {
            m_dialog->interrupt();
        }
        m_dialog->setup(
            QString::fromLatin1("system-suspend"),
            i18n("Suspend"),
            i18n("Do you want to suspend to RAM?")
        );
        if (!m_dialog->exec()) {
            return;
        }
    }

    m_dowhat = LockoutApplet::DoToRam;
    QTimer::singleShot(s_dodelay, this, SLOT(slotDoIt()));
}

void LockoutApplet::slotToDisk()
{
    if (m_dowhat != LockoutApplet::DoNothing) {
        return;
    }

    if (m_confirmtodisk) {
        if (!m_dialog) {
            m_dialog = new LockoutDialog();
        } else {
            m_dialog->interrupt();
        }
        m_dialog->setup(
            QString::fromLatin1("system-suspend-hibernate"),
            i18n("Hibernate"),
            i18n("Do you want to suspend to disk?")
        );
        if (!m_dialog->exec()) {
            return;
        }
    }

    m_dowhat = LockoutApplet::DoToDisk;
    QTimer::singleShot(s_dodelay, this, SLOT(slotDoIt()));
}

void LockoutApplet::slotHybrid()
{
    if (m_dowhat != LockoutApplet::DoNothing) {
        return;
    }

    if (m_confirmhybrid) {
        if (!m_dialog) {
            m_dialog = new LockoutDialog();
        } else {
            m_dialog->interrupt();
        }
        m_dialog->setup(
            QString::fromLatin1("system-suspend"),
            i18n("Hybrid Suspend"),
            i18n("Do you want to hybrid suspend?")
        );
        if (!m_dialog->exec()) {
            return;
        }
    }

    m_dowhat = LockoutApplet::DoHybrid;
    QTimer::singleShot(s_dodelay, this, SLOT(slotDoIt()));
}

void LockoutApplet::slotDoIt()
{
    switch (m_dowhat) {
        case LockoutApplet::DoSwitch: {
            m_dowhat = LockoutApplet::DoNothing;
            KDisplayManager kdisplaymanager;
            kdisplaymanager.newSession();
            break;
        }
        case LockoutApplet::DoToRam: {
            m_dowhat = LockoutApplet::DoNothing;
            Solid::PowerManagement::requestSleep(Solid::PowerManagement::SuspendState);
            break;
        }
        case LockoutApplet::DoToDisk: {
            m_dowhat = LockoutApplet::DoNothing;
            Solid::PowerManagement::requestSleep(Solid::PowerManagement::HibernateState);
            break;
        }
        case LockoutApplet::DoHybrid: {
            m_dowhat = LockoutApplet::DoNothing;
            Solid::PowerManagement::requestSleep(Solid::PowerManagement::HybridSuspendState);
            break;
        }
    }
}

void LockoutApplet::slotCheckButtons()
{
    int checkedcount = 0;
    if (m_switchbox->isChecked()) {
        checkedcount++;
    }
    if (m_shutdownbox->isChecked()) {
        checkedcount++;
    }
    if (m_torambox->isChecked()) {
        checkedcount++;
    }
    if (m_todiskbox->isChecked()) {
        checkedcount++;
    }
    if (m_hybridbox->isChecked()) {
        checkedcount++;
    }

    if (checkedcount > 1) {
        m_switchbox->setEnabled(true);
        m_shutdownbox->setEnabled(true);
        m_torambox->setEnabled(true);
        m_todiskbox->setEnabled(true);
        m_hybridbox->setEnabled(true);
        return;
    }

    if (m_switchbox->isChecked()) {
        m_switchbox->setEnabled(false);
        return;
    }
    if (m_shutdownbox->isChecked()) {
        m_shutdownbox->setEnabled(false);
        return;
    }
    if (m_torambox->isChecked()) {
        m_torambox->setEnabled(false);
        return;
    }
    if (m_todiskbox->isChecked()) {
        m_todiskbox->setEnabled(false);
        return;
    }
    if (m_hybridbox->isChecked()) {
        m_hybridbox->setEnabled(false);
    }
}

void LockoutApplet::slotConfigAccepted()
{
    m_showswitch = m_switchbox->isChecked();
    m_showshutdown = m_shutdownbox->isChecked();
    m_showtoram = m_torambox->isChecked();
    m_showtodisk = m_todiskbox->isChecked();
    m_showhybrid = m_hybridbox->isChecked();
    m_confirmswitch = m_switchconfirmbox->isChecked();
    m_confirmshutdown = m_shutdownconfirmbox->isChecked();
    m_confirmtoram = m_toramconfirmbox->isChecked();
    m_confirmtodisk = m_todiskconfirmbox->isChecked();
    m_confirmhybrid = m_hybridconfirmbox->isChecked();

    slotUpdateButtons();
    updateSizes();

    KConfigGroup configgroup = config();
    configgroup.writeEntry("showSwitchButton", m_showswitch);
    configgroup.writeEntry("showShutdownButton", m_showshutdown);
    configgroup.writeEntry("showToRamButton", m_showtoram);
    configgroup.writeEntry("showToDiskButton", m_showtodisk);
    configgroup.writeEntry("showHybridButton", m_showhybrid);
    configgroup.writeEntry("confirmSwitchButton", m_confirmswitch);
    configgroup.writeEntry("confirmShutdownButton", m_confirmshutdown);
    configgroup.writeEntry("confirmToRamButton", m_confirmtoram);
    configgroup.writeEntry("confirmToDiskButton", m_confirmtodisk);
    configgroup.writeEntry("confirmHybridButton", m_confirmhybrid);

    emit configNeedsSaving();
}

#include "moc_lockout.cpp"
#include "lockout.moc"
