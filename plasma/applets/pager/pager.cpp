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

#include "pager.h"

#include <QX11Info>
#include <QGridLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <Plasma/Svg>
#include <Plasma/FrameSvg>
#include <Plasma/SvgWidget>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <KCModuleInfo>
#include <KWindowSystem>
#include <KIcon>
#include <KDebug>
#include <netwm.h>

// standard issue margin/spacing
static const int s_spacing = 4;
static PagerApplet::PagerMode s_defaultpagermode = PagerApplet::ShowName;
// NOTE: same as the one in:
// kdelibs/plasma/animations/animation.cpp
static const int s_animationduration = 250;

static QString kElementPrefixForDesktop(const int desktop)
{
    if (KWindowSystem::currentDesktop() == desktop) {
        return QString::fromLatin1("active");
    }
    return QString::fromLatin1("normal");
}

static QRectF kAdjustRect(const QRectF &rect, const bool vertical)
{
    QRectF result = rect;
    if (vertical) {
        result.setHeight(result.height() - (s_spacing * 2));
        qreal resultx = 0.0;
        qreal resulty = 0.0;
        qreal resultw = 0.0;
        qreal resulth = 0.0;
        result.getRect(&resultx, &resulty, &resultw, &resulth);
        result.setX(-(resultw / resulth));
        result.setY(-resultw);
        result.setWidth(resulth);
        result.setHeight(resultw);
    } else {
        result.setWidth(result.width() - (s_spacing * 2));
    }
    return result;
}

static QFont kGetFont()
{
    QFont font = KGlobalSettings::smallestReadableFont();
    font.setBold(true);
    return font;
}

static bool kHandleMouseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        NETRootInfo netrootinfo(QX11Info::display(), NET::WM2ShowingDesktop | NET::Supported);
        if (!netrootinfo.isSupported(NET::WM2ShowingDesktop)) {
            kWarning() << "NET::WM2ShowingDesktop is not supported";
            return false;
        }
        // that is how the showdesktop does it - it tracks the state internally
        static bool s_didshow = false;
        if (netrootinfo.showingDesktop() || s_didshow) {
            s_didshow = false;
            netrootinfo.setShowingDesktop(false);
        } else {
            s_didshow = true;
            netrootinfo.setShowingDesktop(true);
        }
        return true;
    }
    return false;
}

class PagerSvg : public Plasma::SvgWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    PagerSvg(const int desktop, const PagerApplet::PagerMode pagermode, QGraphicsItem *parent = nullptr);

    qreal hover() const;
    void setHover(qreal hover);

    void setup(const PagerApplet::PagerMode pagermode);
    void updateSize(const QSizeF &appletsize, const Plasma::FormFactor formfactor);

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const final;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) final;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) final;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) final;
    // handled here too
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) final;

private Q_SLOTS:
    void slotClicked(const Qt::MouseButton button);
    void slotUpdate();
    void slotUpdateSvgAndToolTip();

private:
    int m_desktop;
    Plasma::FrameSvg* m_framesvg;
    Plasma::FrameSvg* m_hoversvg;
    QPropertyAnimation* m_animation;
    qreal m_hover;
    PagerApplet::PagerMode m_pagermode;
    QSizeF m_appletsize;
    Plasma::FormFactor m_formfactor;
};

PagerSvg::PagerSvg(const int desktop, const PagerApplet::PagerMode pagermode, QGraphicsItem *parent)
    : Plasma::SvgWidget(parent),
    m_desktop(desktop),
    m_framesvg(nullptr),
    m_hoversvg(nullptr),
    m_animation(nullptr),
    m_hover(0.0),
    m_pagermode(pagermode),
    m_formfactor(Plasma::FormFactor::Application)
{
    setAcceptHoverEvents(true);
    slotUpdateSvgAndToolTip();

    connect(
        this, SIGNAL(clicked(Qt::MouseButton)),
        this, SLOT(slotClicked(Qt::MouseButton))
    );
    connect(
        KWindowSystem::self(), SIGNAL(desktopNamesChanged()),
        this, SLOT(slotUpdate())
    );
    connect(
        KWindowSystem::self(), SIGNAL(currentDesktopChanged(int)),
        this, SLOT(slotUpdate())
    );
    connect(
        Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
        this, SLOT(slotUpdateSvgAndToolTip())
    );
}

qreal PagerSvg::hover() const
{
    return m_hover;
}

void PagerSvg::setHover(qreal hover)
{
    m_hover = hover;
    update();
}

void PagerSvg::setup(const PagerApplet::PagerMode pagermode)
{
    m_pagermode = pagermode;
    update();
}

void PagerSvg::updateSize(const QSizeF &appletsize, const Plasma::FormFactor formfactor)
{
    m_appletsize = appletsize;
    m_formfactor = formfactor;
    updateGeometry();
}

QSizeF PagerSvg::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    bool inpanel = false;
    switch (m_formfactor) {
        case Plasma::FormFactor::Vertical: {
            inpanel = true;
            break;
        }
        case Plasma::FormFactor::Horizontal: {
            inpanel = true;
            break;
        }
        default: {
            break;
        }
    }
    // the panel containment completely ignores minimum size so no hint for that
    if (inpanel && which == Qt::MinimumSize) {
        return Plasma::SvgWidget::sizeHint(which, constraint);
    }
    if (which != Qt::MaximumSize) {
        const QSizeF currentsize = size();
        // hints are based on the mode and longest text of all virtual desktops
        qreal textwidth = 0;
        const int numberofdesktops = KWindowSystem::numberOfDesktops();
        const QFontMetricsF fontmetricsf(kGetFont());
        for (int i = 0; i < numberofdesktops; i++) {
            if (m_pagermode == PagerApplet::ShowNumber) {
                textwidth = qMax(textwidth, fontmetricsf.width(QString::number(i)));
            } else {
                textwidth = qMax(textwidth, fontmetricsf.width(KWindowSystem::desktopName(i)));
            }
        }
        // the applet layout spacing + the text spacing
        static const int spacingx4 = (s_spacing * 4);
        bool vertical = false;
        const qreal totaltextwidth = (textwidth + spacingx4);
        const qreal totaltextheight = (fontmetricsf.height() + spacingx4);
        switch (m_formfactor) {
            case Plasma::FormFactor::Vertical: {
                vertical = (currentsize.width() < totaltextwidth);
                break;
            }
            case Plasma::FormFactor::Horizontal: {
                vertical = (currentsize.height() >= totaltextwidth);
                break;
            }
            default: {
                vertical = (m_appletsize.width() < m_appletsize.height() && m_appletsize.height() >= totaltextwidth);
                break;
            }
        }
        if (vertical) {
            return QSizeF(totaltextheight, totaltextwidth);
        }
        return QSizeF(totaltextwidth, totaltextheight);
    }
    return Plasma::SvgWidget::sizeHint(which, constraint);
}

void PagerSvg::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(kGetFont());
    const QRectF brect = boundingRect();
    const QSizeF brectsize = brect.size();
    m_framesvg->setElementPrefix(kElementPrefixForDesktop(m_desktop));
    m_framesvg->resizeFrame(brectsize);
    m_framesvg->paintFrame(painter, brect);
    const qreal oldopacity = painter->opacity();
    painter->setOpacity(m_hover);
    m_hoversvg->resizeFrame(brectsize);
    m_hoversvg->paintFrame(painter, brect);
    painter->setOpacity(oldopacity);
    QString pagertext;
    switch (m_pagermode) {
        case PagerApplet::ShowNumber: {
            pagertext = QString::number(m_desktop);
            break;
        }
        case PagerApplet::ShowName: {
            pagertext = KWindowSystem::desktopName(m_desktop);
            break;
        }
    }
    const bool vertical = (brectsize.width() < brectsize.height());
    if (vertical) {
        painter->rotate(90);
    }
    painter->translate(s_spacing, 0);
    painter->setPen(Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor));
    painter->drawText(
        kAdjustRect(brect, vertical),
        pagertext,
        QTextOption(Qt::AlignCenter)
    );
}

void PagerSvg::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_animation) {
        m_animation->stop();
    } else {
        m_animation = new QPropertyAnimation(this, "hover", this);
        m_animation->setDuration(s_animationduration);
    }
    m_animation->setStartValue(m_hover);
    m_animation->setEndValue(1.0);
    m_animation->start(QAbstractAnimation::KeepWhenStopped);
}

void PagerSvg::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_animation) {
        m_animation->stop();
    } else {
        m_animation = new QPropertyAnimation(this, "hover", this);
        m_animation->setDuration(s_animationduration);
    }
    m_animation->setStartValue(m_hover);
    m_animation->setEndValue(0.0);
    m_animation->start(QAbstractAnimation::KeepWhenStopped);
}

void PagerSvg::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (kHandleMouseEvent(event)) {
        event->accept();
        return;
    }
    Plasma::SvgWidget::mouseReleaseEvent(event);
}

void PagerSvg::slotClicked(const Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        KWindowSystem::setCurrentDesktop(m_desktop);
    }
}

void PagerSvg::slotUpdate()
{
    update();
}

void PagerSvg::slotUpdateSvgAndToolTip()
{
    delete m_framesvg;
    delete m_hoversvg;
    m_framesvg = new Plasma::FrameSvg(this);
    m_framesvg->setImagePath("widgets/pager");
    setSvg(m_framesvg);
    m_hoversvg = new Plasma::FrameSvg(this);
    m_hoversvg->setImagePath("widgets/pager");
    m_hoversvg->setElementPrefix(QString::fromLatin1("hover"));
    Plasma::ToolTipContent plasmatooltip;
    plasmatooltip.setMainText(QString::fromLatin1("<center>%1</center>").arg(KWindowSystem::desktopName(m_desktop)));
    Plasma::ToolTipManager::self()->setContent(this, plasmatooltip);
}

PagerApplet::PagerApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
    m_layout(nullptr),
    m_adddesktopaction(nullptr),
    m_removedesktopaction(nullptr),
    m_pagermode(s_defaultpagermode),
    m_pagermodebox(nullptr),
    m_spacer(nullptr),
    m_kcmdesktopproxy(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_pager");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(s_spacing, s_spacing, s_spacing, s_spacing);
    m_layout->setSpacing(s_spacing);

    // early setup to get proper initial size
    slotUpdateLayout();
    adjustSize();
}

void PagerApplet::init()
{
    Plasma::Applet::init();

    KConfigGroup configgroup = config();
    const PagerApplet::PagerMode oldpagermode = m_pagermode;
    m_pagermode = static_cast<PagerApplet::PagerMode>(configgroup.readEntry("pagerMode", static_cast<int>(s_defaultpagermode)));

    if (oldpagermode != m_pagermode) {
        // layout again with the new mode for size hints to apply correctly
        slotUpdateLayout();
    }

    connect(
        KWindowSystem::self(), SIGNAL(numberOfDesktopsChanged(int)),
        this, SLOT(slotUpdateLayout())
    );
}

void PagerApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QGridLayout* widgetlayout = new QGridLayout(widget);
    QLabel* pagermodelabel = new QLabel(widget);
    pagermodelabel->setText(i18n("Text:"));
    widgetlayout->addWidget(pagermodelabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_pagermodebox = new QComboBox(widget);
    m_pagermodebox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_pagermodebox->addItem(i18n("Desktop number"), static_cast<int>(PagerApplet::ShowNumber));
    m_pagermodebox->addItem(i18n("Desktop name"), static_cast<int>(PagerApplet::ShowName));
    m_pagermodebox->setCurrentIndex(static_cast<int>(m_pagermode));
    widgetlayout->addWidget(m_pagermodebox, 0, 1);
    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addItem(m_spacer, 1, 0, 1, 2);
    widget->setLayout(widgetlayout);
    // doesn't look like media visualization but ok.. (that's what the clock applet uses)
    parent->addPage(widget, i18n("Appearance"), "view-media-visualization");

    m_kcmdesktopproxy = new KCModuleProxy("desktop");
    parent->addPage(
        m_kcmdesktopproxy, m_kcmdesktopproxy->moduleInfo().moduleName(),
        m_kcmdesktopproxy->moduleInfo().icon()
    );

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_pagermodebox, SIGNAL(currentIndexChanged(int)), parent, SLOT(settingsModified()));
    connect(m_kcmdesktopproxy, SIGNAL(changed(bool)), parent, SLOT(settingsModified()));
}

QList<QAction*> PagerApplet::contextualActions()
{
    return m_actions;
}

void PagerApplet::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    const int currentdesktop = KWindowSystem::currentDesktop();
    if (event->delta() < 0) {
        KWindowSystem::setCurrentDesktop(currentdesktop + 1);
    } else {
        KWindowSystem::setCurrentDesktop(currentdesktop - 1);
    }
}

void PagerApplet::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (kHandleMouseEvent(event)) {
        event->accept();
        return;
    }
    Plasma::Applet::mouseReleaseEvent(event);
}

void PagerApplet::constraintsEvent(Plasma::Constraints constraints)
{
    // update once
    bool update = false;
    if (constraints & Plasma::SizeConstraint) {
        update = true;
    }
    if (constraints & Plasma::FormFactorConstraint) {
        switch (formFactor()) {
            case Plasma::FormFactor::Horizontal: {
                m_layout->setOrientation(Qt::Horizontal);
                break;
            }
            case Plasma::FormFactor::Vertical: {
                m_layout->setOrientation(Qt::Vertical);
                break;
            }
            default: {
                m_layout->setOrientation(Qt::Horizontal);
                break;
            }
        }
        update = true;
    }
    if (update) {
        if (m_layout->orientation() == Qt::Horizontal) {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        } else {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }
        QMutexLocker locker(&m_mutex);
        foreach (PagerSvg* pagersvg, m_pagersvgs) {
            pagersvg->updateSize(size(), formFactor());
        }
    }
}

void PagerApplet::slotUpdateLayout()
{
    QMutexLocker locker(&m_mutex);
    foreach (PagerSvg* pagersvg, m_pagersvgs) {
        m_layout->removeItem(pagersvg);
    }
    qDeleteAll(m_pagersvgs);
    m_pagersvgs.clear();

    const int numberofdesktops = KWindowSystem::numberOfDesktops();
    for (int i = 0; i < numberofdesktops; i++) {
        PagerSvg* pagersvg = new PagerSvg(i + 1, m_pagermode, this);
        m_layout->addItem(pagersvg);
        m_pagersvgs.append(pagersvg);
    }

    m_actions.clear();
    if (!m_adddesktopaction) {
        m_adddesktopaction = new QAction(
            KIcon("list-add"), i18n("&Add Virtual Desktop"),
            this
        );
    }
    m_actions.append(m_adddesktopaction);
    connect(
        m_adddesktopaction, SIGNAL(triggered(bool)),
        this , SLOT(slotAddDesktop())
    );
    if (numberofdesktops > 1) {
        if (!m_removedesktopaction) {
            m_removedesktopaction = new QAction(
                KIcon("list-remove"), i18n("&Remove Last Virtual Desktop"),
                this
            );
        }
        m_actions.append(m_removedesktopaction);
        connect(
            m_removedesktopaction, SIGNAL(triggered(bool)),
            this , SLOT(slotRemoveDesktop())
        );
    }
}

void PagerApplet::slotAddDesktop()
{
    NETRootInfo netrootinfo(QX11Info::display(), NET::NumberOfDesktops);
    netrootinfo.setNumberOfDesktops(netrootinfo.numberOfDesktops() + 1);
}

void PagerApplet::slotRemoveDesktop()
{
    NETRootInfo netrootinfo(QX11Info::display(), NET::NumberOfDesktops);
    const int numberofdesktops = netrootinfo.numberOfDesktops();
    if (numberofdesktops > 1) {
        netrootinfo.setNumberOfDesktops(netrootinfo.numberOfDesktops() - 1);
    } else {
        kWarning() << "there is only one desktop";
    }
}

void PagerApplet::slotConfigAccepted()
{
    Q_ASSERT(m_pagermodebox != nullptr);
    const int pagermodeindex = m_pagermodebox->currentIndex();
    m_pagermode = static_cast<PagerApplet::PagerMode>(pagermodeindex);
    KConfigGroup configgroup = config();
    configgroup.writeEntry("pagerMode", pagermodeindex);
    slotUpdateLayout();
    m_kcmdesktopproxy->save();
    emit configNeedsSaving();
}

#include "moc_pager.cpp"
#include "pager.moc"
