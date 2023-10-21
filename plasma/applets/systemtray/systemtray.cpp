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

#include "systemtray.h"

#include <Plasma/ToolTipManager>
#include <KDebug>

// standard issue margin/spacing
static const int s_margin = 4;
static const int s_spacing = 2;
// there is no signal for when the popup is shown so it has to be checked on timer
static const int s_popuptimeout = 500;

static QString kElementForArrow(const Qt::Orientation orientation, const bool reverse)
{
    switch (orientation) {
        case Qt::Horizontal: {
            if (reverse) {
                return QString::fromLatin1("right-arrow");
            }
            return QString::fromLatin1("left-arrow");
        }
        case Qt::Vertical: {
            if (reverse) {
                return QString::fromLatin1("down-arrow");
            }
            return QString::fromLatin1("up-arrow");
        }
    }
    // just in case
    if (reverse) {
        return QString::fromLatin1("down-arrow");
    }
    return QString::fromLatin1("up-arrow");
}

SystemTrayApplet::SystemTrayApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_layout(nullptr),
    m_arrowicon(nullptr),
    m_showinghidden(false),
    m_popuptimer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_systemtray");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_popuptimer = new QTimer(this);
    m_popuptimer->setInterval(s_popuptimeout);
    connect(
        m_popuptimer, SIGNAL(timeout()),
        this, SLOT(slotUpdateVisibility())
    );
}

SystemTrayApplet::~SystemTrayApplet()
{
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        KConfigGroup dummy;
        plasmaapplet->save(dummy);
    }
}

void SystemTrayApplet::init()
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(s_margin, s_margin, s_margin, s_margin);
    m_layout->setSpacing(s_spacing);
    setLayout(m_layout);

    // TODO: update layout on ksycoca services update
    updateLayout();
}

void SystemTrayApplet::updateLayout()
{
    m_popuptimer->stop();
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        m_layout->removeItem(plasmaapplet);
    }
    qDeleteAll(m_applets);
    m_applets.clear();

    if (!m_arrowicon) {
        m_arrowicon = new Plasma::IconWidget(this);
        connect(
            m_arrowicon, SIGNAL(clicked()),
            this, SLOT(slotShowHidden())
        );
        m_layout->insertItem(0, m_arrowicon);
    }

    foreach (const KPluginInfo &appletinfo, Plasma::Applet::listAppletInfo()) {
        KService::Ptr appletservice = appletinfo.service();
        const bool notificationarea = appletservice->property("X-Plasma-NotificationArea", QVariant::Bool).toBool();
        if (notificationarea) {
            Plasma::Applet* plasmaapplet = Plasma::Applet::load(appletinfo.pluginName());
            if (!plasmaapplet) {
                kWarning() << "Could not load applet" << appletinfo.pluginName();
                continue;
            }

            plasmaapplet->setParent(this);
            plasmaapplet->setParentItem(this);
            plasmaapplet->setFlag(QGraphicsItem::ItemIsMovable, false);
            plasmaapplet->setBackgroundHints(Plasma::Applet::NoBackground);
            KConfigGroup plasmaappletconfig = plasmaapplet->config();
            plasmaappletconfig = plasmaappletconfig.parent();
            plasmaapplet->restore(plasmaappletconfig);
            plasmaapplet->init();
            plasmaapplet->updateConstraints(Plasma::AllConstraints);
            plasmaapplet->flushPendingConstraintsEvents();
            connect(
                plasmaapplet, SIGNAL(appletDestroyed(Plasma::Applet*)),
                this, SLOT(slotAppletDestroyed(Plasma::Applet*))
            );
            connect(
                plasmaapplet, SIGNAL(newStatus(Plasma::ItemStatus)),
                this, SLOT(slotUpdateVisibility())
            );
            m_applets.append(plasmaapplet);
            m_layout->addItem(plasmaapplet);
        }
    }
    locker.unlock();
    slotUpdateVisibility();
    m_popuptimer->start();
}

void SystemTrayApplet::updateApplets(const Plasma::Constraints constraints)
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
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        plasmaapplet->updateConstraints(constraints);
        plasmaapplet->flushPendingConstraintsEvents();
    }
    emit sizeHintChanged(Qt::PreferredSize);
}

void SystemTrayApplet::constraintsEvent(Plasma::Constraints constraints)
{
    // perfect size finder
    // qDebug() << Q_FUNC_INFO << size();
    if (constraints & Plasma::SizeConstraint || constraints & Plasma::FormFactorConstraint) {
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal: {
                m_layout->setOrientation(Qt::Horizontal);
                updateApplets(constraints);
                return;
            }
            case Plasma::FormFactor::Vertical: {
                m_layout->setOrientation(Qt::Vertical);
                updateApplets(constraints);
                return;
            }
            default: {
                break;
            }
        }

        const QSizeF appletsize = size();
        if (appletsize.width() >= appletsize.height()) {
            m_layout->setOrientation(Qt::Horizontal);
        } else {
            m_layout->setOrientation(Qt::Vertical);
        }
    }
    updateApplets(constraints);
}

void SystemTrayApplet::slotAppletDestroyed(Plasma::Applet *plasmaapplet)
{
    Q_UNUSED(plasmaapplet);
    updateLayout();
}

void SystemTrayApplet::slotUpdateVisibility()
{
    if (m_showinghidden) {
        // status updated while hidden applets are shown
        return;
    }
    m_showinghidden = false;
    bool hashidden = false;
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        if (plasmaapplet->status() == Plasma::PassiveStatus && !plasmaapplet->isPopupShowing()) {
            hashidden = true;
            plasmaapplet->setVisible(false);
            // move hidden items to the front
            m_layout->removeItem(plasmaapplet);
            m_layout->insertItem(1, plasmaapplet);
        } else {
            plasmaapplet->setVisible(true);
            // visible to the back
            m_layout->addItem(plasmaapplet);
        }
    }
    if (!hashidden) {
        m_arrowicon->setVisible(false);
    } else {
        m_arrowicon->setVisible(true);
        m_arrowicon->setSvg("widgets/arrows", kElementForArrow(m_layout->orientation(), m_showinghidden));
    }
}

void SystemTrayApplet::slotShowHidden()
{
    // TODO: animation, perhaps via layout item to animate all applets via single animation
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        if (plasmaapplet->status() == Plasma::PassiveStatus) {
            plasmaapplet->setVisible(!m_showinghidden);
        }
    }
    m_showinghidden = !m_showinghidden;
    m_arrowicon->setSvg("widgets/arrows", kElementForArrow(m_layout->orientation(), m_showinghidden));
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(
        QString::fromLatin1("<center>%1</center>").arg(m_showinghidden ? i18n("Hide icons") : i18n("Show hidden icons"))
    );
    Plasma::ToolTipManager::self()->setContent(m_arrowicon, plasmatooltip);
}

K_EXPORT_PLASMA_APPLET(systemtray, SystemTrayApplet)

#include "moc_systemtray.cpp"
