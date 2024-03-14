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

#include <Plasma/PopupApplet>
#include <Plasma/ToolTipManager>
#include <KSycoca>
#include <KIconLoader>
#include <KDebug>

// standard issue margin/spacing
static const int s_margin = 4;
static const int s_spacing = 2;

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
    Q_ASSERT(false);
    return QString();
}

static void kSaveApplet(Plasma::Applet *plasmaapplet)
{
    KConfigGroup dummy;
    plasmaapplet->save(dummy);
}

// HACK: updateGeometry() is protected thus the hack
class PlasmaAppletHack: public Plasma::Applet
{
    Q_OBJECT
public:
    void updateGeometryHack();
};

void PlasmaAppletHack::updateGeometryHack()
{
    updateGeometry();
}

SystemTrayApplet::SystemTrayApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_layout(nullptr),
    m_arrowicon(nullptr),
    m_showinghidden(false)
{
    KGlobal::locale()->insertCatalog("plasma_applet_systemtray");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

SystemTrayApplet::~SystemTrayApplet()
{
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        kSaveApplet(plasmaapplet);
    }
}

void SystemTrayApplet::init()
{
    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(s_margin, s_margin, s_margin, s_margin);
    m_layout->setSpacing(s_spacing);
    setLayout(m_layout);

    slotUpdateLayout();
    connect(
        KSycoca::self(), SIGNAL(databaseChanged(QStringList)),
        this, SLOT(slotUpdateLayout())
    );
}

void SystemTrayApplet::updateArrow()
{
    Q_ASSERT(m_arrowicon != nullptr);
    m_arrowicon->setSvg("widgets/arrows", kElementForArrow(m_layout->orientation(), m_showinghidden));
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
    // ensure the applet has a preferred size, an icon-like one which is the case for popup applets
    // (unless applets are not shown in icon mode, that is decided by the applets minimum size) but
    // not for non-pupup applets
    const QSizeF appletsize = size();
    int iconsize = qMin(appletsize.width(), appletsize.height());
    if (iconsize <= 0) {
        iconsize = KIconLoader::global()->currentSize(KIconLoader::Panel);
    }
    iconsize = (iconsize - (s_margin * 2));
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        Plasma::PopupApplet* plasmapopupapplet = qobject_cast<Plasma::PopupApplet*>(plasmaapplet);
        const QSizeF plasmaappletsize = plasmaapplet->preferredSize();
        if (!plasmapopupapplet || plasmaappletsize.isNull()) {
            plasmaapplet->setPreferredSize(iconsize, iconsize);
            PlasmaAppletHack* plasmaapplethack = reinterpret_cast<PlasmaAppletHack*>(plasmaapplet);
            plasmaapplethack->updateGeometryHack();
        }
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
                updateArrow();
                updateApplets(constraints);
                return;
            }
            case Plasma::FormFactor::Vertical: {
                m_layout->setOrientation(Qt::Vertical);
                updateArrow();
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
    updateArrow();
    updateApplets(constraints);
}

void SystemTrayApplet::slotUpdateLayout()
{
    QStringList trayplugins;
    foreach (const KPluginInfo &appletinfo, Plasma::Applet::listAppletInfo()) {
        KService::Ptr appletservice = appletinfo.service();
        const bool notificationarea = appletservice->property("X-Plasma-NotificationArea", QVariant::Bool).toBool();
        if (notificationarea) {
            trayplugins.append(appletinfo.pluginName());
        }
    }
    QMutexLocker locker(&m_mutex);
    QMutableListIterator<Plasma::Applet*> iter(m_applets);
    while (iter.hasNext()) {
        Plasma::Applet* plasmaapplet = iter.next();
        if (trayplugins.contains(plasmaapplet->pluginName())) {
            continue;
        }
        kDebug() << "removing" << plasmaapplet->pluginName() << "from tray";
        disconnect(plasmaapplet, 0, this, 0);
        kSaveApplet(plasmaapplet);
        iter.remove();
        plasmaapplet->destroy();
    }

    if (!m_arrowicon) {
        m_arrowicon = new Plasma::IconWidget(this);
        connect(
            m_arrowicon, SIGNAL(clicked()),
            this, SLOT(slotShowHidden())
        );
        m_layout->insertItem(0, m_arrowicon);
    }

    foreach (const QString &appletplugin, trayplugins) {
        bool alreadyloaded = false;
        iter.toFront();
        while (iter.hasNext()) {
            Plasma::Applet* plasmaapplet = iter.next();
            if (plasmaapplet->pluginName() == appletplugin) {
                kDebug() << appletplugin << "already loaded into tray";
                alreadyloaded = true;
                break;
            }
        }
        if (alreadyloaded) {
            continue;
        }

        Plasma::Applet* plasmaapplet = Plasma::Applet::load(appletplugin);
        if (!plasmaapplet) {
            kWarning() << "Could not load applet" << appletplugin;
            continue;
        }

        kDebug() << appletplugin << "loading" << appletplugin << "into tray";
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
            this, SLOT(slotUpdateLayout())
        );
        connect(
            plasmaapplet, SIGNAL(activate()),
            this, SLOT(slotUpdateVisibility())
        );
        connect(
            plasmaapplet, SIGNAL(newStatus(Plasma::ItemStatus)),
            this, SLOT(slotUpdateVisibility())
        );
        m_applets.append(plasmaapplet);
        m_layout->addItem(plasmaapplet);
    }
    locker.unlock();
    slotUpdateVisibility();
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
        }
    }
    // should be non-null at this point
    Q_ASSERT(m_arrowicon != nullptr);
    if (!hashidden) {
        m_arrowicon->setVisible(false);
    } else {
        m_arrowicon->setVisible(true);
        updateArrow();
    }
}

void SystemTrayApplet::slotShowHidden()
{
    // TODO: animation, perhaps via layout item to animate all hidden applets via single animation
    QMutexLocker locker(&m_mutex);
    foreach (Plasma::Applet* plasmaapplet, m_applets) {
        if (plasmaapplet->status() == Plasma::PassiveStatus && !plasmaapplet->isPopupShowing()) {
            plasmaapplet->setVisible(!m_showinghidden);
        }
    }
    m_showinghidden = !m_showinghidden;
    updateArrow();
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(
        QString::fromLatin1("<center>%1</center>").arg(m_showinghidden ? i18n("Hide icons") : i18n("Show hidden icons"))
    );
    Plasma::ToolTipManager::self()->setContent(m_arrowicon, plasmatooltip);
}

K_EXPORT_PLASMA_APPLET(systemtray, SystemTrayApplet)

#include "moc_systemtray.cpp"
#include "systemtray.moc"
