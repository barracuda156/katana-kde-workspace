/*  This file is part of the KDE project

    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies). <qt-info@nokia.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config-X11.h"

#include <KStandardDirs>
#include <KGlobal>
#include <KGlobalSettings>
#include <KStyle>
#include <KConfigGroup>
#include <KIcon>
#include <KIconTheme>
#include <KMimeType>
#include <KDebug>
#include <QFileInfo>
#include <QApplication>
#include <QToolButton>
#include <QToolBar>
#include <QMainWindow>
#include <QGuiPlatformPlugin>
#include <QX11Info>

#ifdef HAVE_XCURSOR
#  include <X11/Xlib.h>
#  include <X11/Xcursor/Xcursor.h>
#  include <fixx11h.h>
#endif

class KQGuiPlatformPlugin : public QGuiPlatformPlugin
{
    Q_OBJECT
public:
    KQGuiPlatformPlugin()
    {
        QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection);
    }

    QString styleName() final
    {
        const KConfigGroup pConfig(KGlobal::config(), "General");
        return pConfig.readEntry("widgetStyle", KStyle::defaultStyle());
    }

    QPalette palette() final
    {
        return KGlobalSettings::createApplicationPalette();
    }

    QString systemIconThemeName() final
    {
        return KIconTheme::current();
    }

    QStringList iconThemeSearchPaths() final
    {
        return KGlobal::dirs()->resourceDirs("icon");
    }

    QIcon systemIcon(const QString &name) final
    {
        return KIcon(name);
    }

    QIcon fileSystemIcon(const QFileInfo &file) final
    {
        KMimeType::Ptr mime = KMimeType::findByPath(file.filePath(), 0, true);
        if (!mime)
            return QIcon();
        return KIcon(mime->iconName());
    }

    int platformHint(QGuiPlatformPlugin::PlatformHint hint) final
    {
        switch(hint) {
            case PH_ToolButtonStyle: {
                KConfigGroup group(KGlobal::config(), "Toolbar style");
                const QByteArray style = group.readEntry("ToolButtonStyle", QByteArray("TextUnderIcon")).toLower();
                if (style == "textbesideicon" || style == "icontextright") {
                    return Qt::ToolButtonTextBesideIcon;
                } else if (style == "textundericon" || style == "icontextbottom") {
                    return Qt::ToolButtonTextUnderIcon;
                } else if (style == "textonly") {
                    return Qt::ToolButtonTextOnly;
                }
                return Qt::ToolButtonIconOnly;
            }
            case PH_ToolBarIconSize: {
                return KIconLoader::global()->currentSize(KIconLoader::MainToolbar);
            }
            case PH_ItemView_ActivateItemOnSingleClick: {
                return KGlobalSettings::singleClick();
            }
            default: {
                break;
            }
        }
        return QGuiPlatformPlugin::platformHint(hint);
    }

private slots:
    void init()
    {
        updateToolbarStyle();
        updateToolbarIcons();
        updateStyle();
        updatePalette();
        updateMouse();

        connect(KIconLoader::global(), SIGNAL(iconLoaderSettingsChanged()), this, SLOT(updateToolbarIcons()));
        connect(KGlobalSettings::self(), SIGNAL(toolbarAppearanceChanged(int)), this, SLOT(updateToolbarStyle()));
        connect(KGlobalSettings::self(), SIGNAL(kdisplayStyleChanged()), this, SLOT(updateStyle()));
        connect(KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()), this, SLOT(updatePalette()));
        connect(KGlobalSettings::self(), SIGNAL(mouseChanged()), this, SLOT(updateMouse()));
    }

    void updateToolbarStyle()
    {
        QWidgetList widgets = QApplication::allWidgets();
        for (int i = 0; i < widgets.size(); ++i) {
            QWidget *widget = widgets.at(i);
            if (qobject_cast<QToolButton*>(widget)) {
                QEvent event(QEvent::StyleChange);
                QApplication::sendEvent(widget, &event);
            }
        }
    }

    void updateToolbarIcons()
    {
        QWidgetList widgets = QApplication::allWidgets();
        for (int i = 0; i < widgets.size(); ++i) {
            QWidget *widget = widgets.at(i);
            if (qobject_cast<QToolBar*>(widget) || qobject_cast<QMainWindow*>(widget)) {
                QEvent event(QEvent::StyleChange);
                QApplication::sendEvent(widget, &event);
            }
        }
    }

    void updateEffects()
    {
        KGlobalSettings::GraphicEffects graphicEffects = KGlobalSettings::graphicEffectsLevel();
        bool effectsEnabled = (graphicEffects != KGlobalSettings::NoEffects);
        bool complexEffects = (graphicEffects & KGlobalSettings::ComplexAnimationEffects);
        if (effectsEnabled) {
            QApplication::setEffectEnabled(Qt::UI_General, true);
            // the fade effect requires compositor and as such is enabled only when complex animation is on
            if (complexEffects) {
                QApplication::setEffectEnabled(Qt::UI_FadeMenu, true);
                QApplication::setEffectEnabled(Qt::UI_FadeTooltip, true);
            }
        } else {
            QApplication::setEffectEnabled(Qt::UI_General, false);
            QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
            QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
        }
    }

    void updateStyle()
    {
        if (qApp) {
            if (qApp->style()->objectName() != styleName()) {
                qApp->setStyle(styleName());
            }
        }

        updateEffects();
    }

    void updatePalette()
    {
        QApplication::setPalette(palette());
    }

    void updateMouse()
    {
#if defined(HAVE_XCURSOR)
        {
            KConfig inputconfig("kcminputrc");
            KConfigGroup mousegroup = inputconfig.group("Mouse");
            const QByteArray cursortheme = mousegroup.readEntry("cursorTheme", QByteArray(KDE_DEFAULT_CURSOR_THEME));
            const int cursorsize = mousegroup.readEntry("cursorSize", -1);
            XcursorSetTheme(QX11Info::display(), cursortheme);
            if (cursorsize > 0) {
                XcursorSetDefaultSize(QX11Info::display(), cursorsize);
            }
        }
#endif

        KConfigGroup kdegroup(KGlobal::config(), "KDE");
        int num = kdegroup.readEntry("CursorBlinkRate", QApplication::cursorFlashTime());
        num = qBound(200, num, 2000);
        QApplication::setCursorFlashTime(num);
        num = kdegroup.readEntry("DoubleClickInterval", QApplication::doubleClickInterval());
        QApplication::setDoubleClickInterval(num);
        num = kdegroup.readEntry("StartDragTime", QApplication::startDragTime());
        QApplication::setStartDragTime(num);
        num = kdegroup.readEntry("StartDragDist", QApplication::startDragDistance());
        QApplication::setStartDragDistance(num);
        num = kdegroup.readEntry("WheelScrollLines", QApplication::wheelScrollLines());
        QApplication::setWheelScrollLines(num);
        bool showIcons = kdegroup.readEntry("ShowIconsInMenuItems", !QApplication::testAttribute(Qt::AA_DontShowIconsInMenus));
        QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, !showIcons);
    }
};

Q_EXPORT_PLUGIN(KQGuiPlatformPlugin)

#include "qguiplatformplugin_kde.moc"

