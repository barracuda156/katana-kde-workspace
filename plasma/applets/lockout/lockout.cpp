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
static const bool s_showlock = true;
static const bool s_showswitch = true;
static const bool s_showshutdown = true;
static const bool s_showtoram = true;
static const bool s_showtodisk = true;
static const bool s_showhybrid = true;
static const bool s_confirmlock = true;
static const bool s_confirmswitch = true;
static const bool s_confirmshutdown = true;
static const bool s_confirmtoram = true;
static const bool s_confirmtodisk = true;
static const bool s_confirmhybrid = true;
static const QString s_screensaver = QString::fromLatin1("org.freedesktop.ScreenSaver");

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
    bool m_result;
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
    m_eventloop(nullptr),
    m_result(false)
{
    m_scene = new QGraphicsScene(this);
    m_widget = new QGraphicsWidget();
    m_widget->setMinimumSize(280, 130);

    m_layout = new QGraphicsGridLayout(m_widget);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_iconwidget = new Plasma::IconWidget(m_widget);
    m_iconwidget->setOrientation(Qt::Horizontal);
    // disable hover effect
    m_iconwidget->setAcceptHoverEvents(false);
    m_layout->addItem(m_iconwidget, 0, 0, 1, 2);
    m_separator = new Plasma::Separator(m_widget);
    m_layout->addItem(m_separator, 1, 0, 1, 2);
    m_label = new Plasma::Label(m_widget);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_layout->addItem(m_label, 2, 0, 1, 2);
    m_yesbutton = new Plasma::PushButton(m_widget);
    m_yesbutton->setIcon(KIcon("dialog-ok"));
    m_yesbutton->setText(i18n("Yes"));
    connect(
        m_yesbutton, SIGNAL(released()),
        this, SLOT(slotYes())
    );
    m_layout->addItem(m_yesbutton, 3, 0, 1, 1);
    m_nobutton = new Plasma::PushButton(m_widget);
    m_nobutton->setIcon(KIcon("process-stop"));
    m_nobutton->setText(i18n("No"));
    connect(
        m_nobutton, SIGNAL(released()),
        this, SLOT(slotNo())
    );
    m_layout->addItem(m_nobutton, 3, 1, 1, 1);
    m_widget->setLayout(m_layout);

    m_scene->addItem(m_widget);
    setGraphicsWidget(m_widget);
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
    m_result = false;
    KWindowSystem::setState(winId(), NET::SkipPager | NET::SkipTaskbar);
    // default to yes like KDialog defaults to KDialog::Ok
    m_yesbutton->setFocus();
    show();
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
    m_result = true;
    Q_ASSERT(m_eventloop);
    m_eventloop->exit(0);
    delete m_eventloop;
    m_eventloop = nullptr;
    close();
}

void LockoutDialog::slotNo()
{
    Q_ASSERT(m_eventloop);
    m_eventloop->exit(1);
    delete m_eventloop;
    m_eventloop = nullptr;
    close();
}


LockoutApplet::LockoutApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_layout(nullptr),
    m_lockwidget(nullptr),
    m_switchwidget(nullptr),
    m_shutdownwidget(nullptr),
    m_toramwidget(nullptr),
    m_todiskwidget(nullptr),
    m_hybridwidget(nullptr),
    m_showlock(s_showlock),
    m_showswitch(s_showswitch),
    m_showshutdown(s_showshutdown),
    m_showtoram(s_showtoram),
    m_showtodisk(s_showtodisk),
    m_showhybrid(s_showhybrid),
    m_confirmlock(s_confirmlock),
    m_confirmswitch(s_confirmswitch),
    m_confirmshutdown(s_confirmshutdown),
    m_confirmtoram(s_confirmtoram),
    m_confirmtodisk(s_confirmtodisk),
    m_confirmhybrid(s_confirmhybrid),
    m_buttonsmessage(nullptr),
    m_lockbox(nullptr),
    m_switchbox(nullptr),
    m_shutdownbox(nullptr),
    m_torambox(nullptr),
    m_todiskbox(nullptr),
    m_hybridbox(nullptr),
    m_spacer(nullptr),
    m_screensaverwatcher(nullptr),
    m_dialog(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_lockout");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setPreferredSize(40, 110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

LockoutApplet::~LockoutApplet()
{
    if (m_dialog) {
        m_dialog->interrupt();
    }
}

void LockoutApplet::init()
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(s_spacing);

    m_lockwidget = new Plasma::IconWidget(this);
    m_lockwidget->setIcon("system-lock-screen");
    m_lockwidget->setToolTip(i18n("Lock the screen"));
    connect(
        m_lockwidget, SIGNAL(activated()),
        this, SLOT(slotLock())
    );
    m_layout->addItem(m_lockwidget);

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
    m_showlock = configgroup.readEntry("showLockButton", s_showlock);
    m_showswitch = configgroup.readEntry("showSwitchButton", s_showswitch);
    m_showshutdown = configgroup.readEntry("showShutdownButton", s_showshutdown);
    m_showtoram = configgroup.readEntry("showToRamButton", s_showtoram);
    m_showtodisk = configgroup.readEntry("showToDiskButton", s_showtodisk);
    m_showhybrid = configgroup.readEntry("showHybridButton", s_showhybrid);
    m_confirmlock = configgroup.readEntry("confirmLockButton", s_confirmlock);
    m_confirmswitch = configgroup.readEntry("confirmSwitchButton", s_confirmswitch);
    m_confirmshutdown = configgroup.readEntry("confirmShutdownButton", s_confirmshutdown);
    m_confirmtoram = configgroup.readEntry("confirmToRamButton", s_confirmtoram);
    m_confirmtodisk = configgroup.readEntry("confirmToDiskButton", s_confirmtodisk);
    m_confirmhybrid = configgroup.readEntry("confirmHybridButton", s_confirmhybrid);

    slotUpdateButtons();

    m_screensaverwatcher = new QDBusServiceWatcher(
        s_screensaver,
        QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration
    );
    connect(
        m_screensaverwatcher, SIGNAL(serviceRegistered(QString)),
        this, SLOT(slotScreensaverRegistered(QString))
    );
    connect(
        m_screensaverwatcher, SIGNAL(serviceUnregistered(QString)),
        this, SLOT(slotScreensaverUnregistered(QString))
    );
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
    m_lockbox = new QCheckBox(widget);
    m_lockbox->setText(i18n("Show the “Lock” button"));
    m_lockbox->setChecked(m_showlock);
    widgetlayout->addWidget(m_lockbox);
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

    connect(m_lockbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
    connect(m_lockbox, SIGNAL(stateChanged(int)), this, SLOT(slotCheckButtons()));
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
    m_lockconfirmbox = new QCheckBox(widget);
    m_lockconfirmbox->setText(i18n("Confirm the “Lock” action"));
    m_lockconfirmbox->setChecked(m_confirmlock);
    widgetlayout->addWidget(m_lockconfirmbox);
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

    connect(m_lockconfirmbox, SIGNAL(stateChanged(int)), parent, SLOT(settingsModified()));
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
    int visiblebuttons = 0;
    QSizeF basesize;
    if (m_showlock) {
        basesize = m_lockwidget->preferredSize();
        visiblebuttons++;
    }
    if (m_showswitch) {
        if (basesize.isNull()) {
            basesize = m_switchwidget->preferredSize();
        }
        visiblebuttons++;
    }
    if (m_showshutdown) {
        if (basesize.isNull()) {
            basesize = m_shutdownwidget->preferredSize();
        }
        visiblebuttons++;
    }
    if (m_showtoram) {
        if (basesize.isNull()) {
            basesize = m_toramwidget->preferredSize();
        }
        visiblebuttons++;
    }
    if (m_showtodisk) {
        if (basesize.isNull()) {
            basesize = m_todiskwidget->preferredSize();
        }
        visiblebuttons++;
    }
    if (m_showhybrid) {
        if (basesize.isNull()) {
            basesize = m_hybridwidget->preferredSize();
        }
        visiblebuttons++;
    }
    if (basesize.isNull()) {
        basesize = s_basesize;
    }
    Q_ASSERT(visiblebuttons > 0);

    // for non-panel expand to the widget size depending on the orientation
    const bool hasspacing = (m_layout->spacing() != 0);
    switch (m_layout->orientation()) {
        case Qt::Horizontal: {
            basesize.setWidth(basesize.width() * visiblebuttons);
            setPreferredSize(hasspacing ? basesize.expandedTo(size()) : basesize);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            break;
        }
        case Qt::Vertical: {
            basesize.setHeight(basesize.height() * visiblebuttons);
            setPreferredSize(hasspacing ? basesize.expandedTo(size()) : basesize);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            break;
        }
    }
    emit sizeHintChanged(Qt::PreferredSize);
}

void LockoutApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::SizeConstraint || constraints & Plasma::FormFactorConstraint) {
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal: {
                m_layout->setOrientation(Qt::Horizontal);
                m_layout->setSpacing(0);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                updateSizes();
                return;
            }
            case Plasma::FormFactor::Vertical: {
                m_layout->setOrientation(Qt::Vertical);
                m_layout->setSpacing(0);
                setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        } else {
            m_layout->setOrientation(Qt::Vertical);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        }
        updateSizes();
    }
}

void LockoutApplet::slotUpdateButtons()
{
    QDBusInterface screensaver(
        s_screensaver, "/ScreenSaver", s_screensaver,
        QDBusConnection::sessionBus()
    );
    m_lockwidget->setVisible(m_showlock);
    m_lockwidget->setEnabled(screensaver.isValid());
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

void LockoutApplet::slotScreensaverRegistered(const QString &service)
{
    if (service == s_screensaver) {
        slotUpdateButtons();
        updateSizes();
    }
}

void LockoutApplet::slotScreensaverUnregistered(const QString &service)
{
    if (service == s_screensaver) {
        slotUpdateButtons();
        updateSizes();
    }
}

void LockoutApplet::slotLock()
{
    if (m_confirmlock) {
        if (!m_dialog) {
            m_dialog = new LockoutDialog();
        } else {
            m_dialog->interrupt();
        }
        m_dialog->setup(
            QString::fromLatin1("system-lock-screen"),
            i18n("Lock"),
            i18n("Do you want to lock?")
        );
        if (!m_dialog->exec()) {
            return;
        }
    }
    QDBusInterface screensaver(
        s_screensaver, "/ScreenSaver", s_screensaver,
        QDBusConnection::sessionBus()
    );
    if (screensaver.isValid()) {
        screensaver.call("Lock");
    }
}

void LockoutApplet::slotSwitch()
{
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
    KDisplayManager kdisplaymanager;
    kdisplaymanager.newSession();
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
    Solid::PowerManagement::requestSleep(Solid::PowerManagement::SuspendState);
}

void LockoutApplet::slotToDisk()
{
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
    Solid::PowerManagement::requestSleep(Solid::PowerManagement::HibernateState);
}

void LockoutApplet::slotHybrid()
{
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
    Solid::PowerManagement::requestSleep(Solid::PowerManagement::HybridSuspendState);
}

void LockoutApplet::slotCheckButtons()
{
    int checkedcount = 0;
    if (m_lockbox->isChecked()) {
        checkedcount++;
    }
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
        m_lockbox->setEnabled(true);
        m_switchbox->setEnabled(true);
        m_shutdownbox->setEnabled(true);
        m_torambox->setEnabled(true);
        m_todiskbox->setEnabled(true);
        m_hybridbox->setEnabled(true);
        return;
    }

    if (m_lockbox->isChecked()) {
        m_lockbox->setEnabled(false);
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
    m_showlock = m_lockbox->isChecked();
    m_showswitch = m_switchbox->isChecked();
    m_showshutdown = m_shutdownbox->isChecked();
    m_showtoram = m_torambox->isChecked();
    m_showtodisk = m_todiskbox->isChecked();
    m_showhybrid = m_hybridbox->isChecked();
    m_confirmlock = m_lockconfirmbox->isChecked();
    m_confirmswitch = m_switchconfirmbox->isChecked();
    m_confirmshutdown = m_shutdownconfirmbox->isChecked();
    m_confirmtoram = m_toramconfirmbox->isChecked();
    m_confirmtodisk = m_todiskconfirmbox->isChecked();
    m_confirmhybrid = m_hybridconfirmbox->isChecked();

    slotUpdateButtons();
    updateSizes();

    KConfigGroup configgroup = config();
    configgroup.writeEntry("showLockButton", m_showlock);
    configgroup.writeEntry("showSwitchButton", m_showswitch);
    configgroup.writeEntry("showShutdownButton", m_showshutdown);
    configgroup.writeEntry("showToRamButton", m_showtoram);
    configgroup.writeEntry("showToDiskButton", m_showtodisk);
    configgroup.writeEntry("showHybridButton", m_showhybrid);
    configgroup.writeEntry("confirmLockButton", m_confirmlock);
    configgroup.writeEntry("confirmSwitchButton", m_confirmswitch);
    configgroup.writeEntry("confirmShutdownButton", m_confirmshutdown);
    configgroup.writeEntry("confirmToRamButton", m_confirmtoram);
    configgroup.writeEntry("confirmToDiskButton", m_confirmtodisk);
    configgroup.writeEntry("confirmHybridButton", m_confirmhybrid);

    emit configNeedsSaving();
}

#include "moc_lockout.cpp"
#include "lockout.moc"
