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
#include <KComponentData>
#include <KGlobalSettings>
#include <KStyle>
#include <KConfigGroup>
#include <KIcon>
#include <KFileDialog>
#include <KColorDialog>
#include <KDebug>
#include <QtCore/QHash>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtGui/QFileDialog>
#include <QtGui/QColorDialog>
#include <QtGui/QApplication>
#include <QtGui/QToolButton>
#include <QtGui/QToolBar>
#include <QtGui/QMainWindow>
#include <QtGui/QGuiPlatformPlugin>
#include <QtGui/QX11Info>

#ifdef HAVE_XCURSOR
#  include <X11/Xlib.h>
#  include <X11/Xcursor/Xcursor.h>
#  include <fixx11h.h>
#endif

/*
 * Map a Katie filter string into a KDE one.
 * (from kfiledialog.cpp)
*/
static QString qt2KdeFilter(const QString &f)
{
    QString filter;
    QTextStream str(&filter, QIODevice::WriteOnly);
    const QStringList list(f.split(";;").replaceInStrings("/", "\\/"));
    bool first = true;

    foreach (const QString &it, list) {
        int ob = it.lastIndexOf('('), cb = it.lastIndexOf(')');

        if (cb != -1 && ob < cb) {
            if (first) {
                first = false;
            } else {
                str << '\n';
            }
            str << it.mid(ob+1, (cb-ob)-1) << '|' << it.mid(0, ob);
        }
    }
    return filter;
}

/*
 * Map a KDE filter string into a Qt one.
 * (from kfiledialog.cpp)
 */
static void kde2QtFilter(const QString &orig, const QString &kde, QString *sel)
{
    if (sel) {
        const QStringList list(orig.split(";;"));
        int pos;

        foreach (const QString &it, list) {
            pos = it.indexOf(kde);
            if (pos != -1 && pos > 0 &&
                (it[pos-1] == '(' || it[pos-1] == ' ') &&
                it.length() >= (kde.length() + pos) &&
                (it[pos+kde.length()] == ')' || it[pos+kde.length()] == ' ')) {
                *sel = it;
                return;
            }
        }
    }
}


class KFileDialogBridge : public KFileDialog
{
    Q_OBJECT
public:
    KFileDialogBridge (const KUrl &startDir, const QString &filter, QFileDialog *original_)
     :  KFileDialog (startDir, filter, original_), original(original_)
     {
         connect(this, SIGNAL(fileSelected(KUrl)), this, SLOT(slotFileSelected(KUrl)));
     }

    virtual void accept()
    {
        KFileDialog::accept();
        QMetaObject::invokeMethod(original, "accept"); //workaround protected
    }

    virtual void reject()
    {
        KFileDialog::reject();
        QMetaObject::invokeMethod(original, "reject"); //workaround protected
    }

    QFileDialog *original;

private Q_SLOTS:
    void slotFileSelected(const KUrl &url)
    {
        QMetaObject::invokeMethod(original, "currentChanged", Q_ARG(QString, url.url())); //workaround protected
    }
};

class KColorDialogBridge : public KColorDialog
{
public:
    KColorDialogBridge(QColorDialog* original_ = 0L) : KColorDialog(original_, true) , original(original_)
    {
        connect(this, SIGNAL(colorSelected(QColor)), original, SIGNAL(currentColorChanged(QColor)));
    }

    QColorDialog *original;

    virtual void accept()
    {
        KColorDialog::accept();
        original->setCurrentColor(color());
        QMetaObject::invokeMethod(original, "accept"); //workaround protected
    }

    virtual void reject()
    {
        KColorDialog::reject();
        QMetaObject::invokeMethod(original, "reject"); //workaround protected
    }
};

Q_DECLARE_METATYPE(KFileDialogBridge *)
Q_DECLARE_METATYPE(KColorDialogBridge *)

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

public: // File Dialog integration
#define K_FD(QFD) KFileDialogBridge *kdefd = qvariant_cast<KFileDialogBridge *>(QFD->property("_k_bridge"))
    void fileDialogDelete(QFileDialog *qfd) final
    {
        K_FD(qfd);
        delete kdefd;
    }

    bool fileDialogSetVisible(QFileDialog *qfd, bool visible) final
    {
        K_FD(qfd);
        if (!kdefd && visible) {
            kdefd = new KFileDialogBridge(KUrl::fromPath(qfd->directory().canonicalPath()),
                                          qt2KdeFilter(qfd->nameFilters().join(";;")), qfd);

            qfd->setProperty("_k_bridge", QVariant::fromValue(kdefd));
        }

        if (visible) {
            switch (qfd->fileMode()) {
                case QFileDialog::AnyFile: {
                    kdefd->setMode(KFile::LocalOnly | KFile::File);
                    break;
                }
                case QFileDialog::ExistingFile: {
                    kdefd->setMode(KFile::LocalOnly | KFile::File | KFile::ExistingOnly);
                    break;
                }
                case QFileDialog::ExistingFiles: {
                    kdefd->setMode(KFile::LocalOnly | KFile::Files | KFile::ExistingOnly);
                    break;
                }
                case QFileDialog::Directory:
                case QFileDialog::DirectoryOnly: {
                    kdefd->setMode(KFile::LocalOnly | KFile::Directory);
                    break;
                }
            }

            kdefd->setOperationMode((qfd->acceptMode() == QFileDialog::AcceptSave) ? KFileDialog::Saving : KFileDialog::Opening);
            kdefd->setCaption(qfd->windowTitle());
            kdefd->setConfirmOverwrite(qfd->confirmOverwrite());
            kdefd->setSelection(qfd->selectedFiles().value(0));
        }
        kdefd->setVisible(visible);
        return true;
    }

    QDialog::DialogCode fileDialogResultCode(QFileDialog *qfd) final
    {
        K_FD(qfd);
        Q_ASSERT(kdefd);
        return QDialog::DialogCode(kdefd->result());
    }

    void fileDialogSetDirectory(QFileDialog *qfd, const QString &directory) final
    {
        K_FD(qfd);
        kdefd->setUrl(KUrl::fromPath(directory));
    }

    QString fileDialogDirectory(const QFileDialog *qfd) const final
    {
        K_FD(qfd);
        Q_ASSERT(kdefd);
        return kdefd->baseUrl().pathOrUrl();
    }

    void fileDialogSelectFile(QFileDialog *qfd, const QString &filename) final
    {
        K_FD(qfd);
        Q_ASSERT(kdefd);
        kdefd->setSelection(filename);
    }

    virtual QStringList fileDialogSelectedFiles(const QFileDialog *qfd) const
    {
        K_FD(qfd);
        Q_ASSERT(kdefd);
        return kdefd->selectedFiles();
    }

#if 0
    virtual void fileDialogSetFilter(QFileDialog *qfd)
    {
        K_FD(qfd);
    }
#endif

    void fileDialogSetNameFilters(QFileDialog *qfd, const QStringList &filters) final
    {
        K_FD(qfd);
        Q_ASSERT(kdefd);
        kdefd->setFilter(qt2KdeFilter(filters.join(";;")));
    }

#if 0
    void fileDialogSelectNameFilter(QFileDialog *qfd, const QString &filter) final
    {
        K_FD(qfd);
    }
#endif

    QString fileDialogSelectedNameFilter(const QFileDialog *qfd) const final
    {
        K_FD(qfd);
        Q_ASSERT(kdefd);
        QString ret;
        kde2QtFilter(qfd->nameFilters().join(";;"), kdefd->currentFilter(), &ret);
        return ret;
    }

public: // ColorDialog
#define K_CD(QCD) KColorDialogBridge *kdecd = qvariant_cast<KColorDialogBridge *>(QCD->property("_k_bridge"))
    void colorDialogDelete(QColorDialog *qcd) final
    {
        K_CD(qcd);
        delete kdecd;

    }

    bool colorDialogSetVisible(QColorDialog *qcd, bool visible) final
    {
        K_CD(qcd);
        if (!kdecd) {
            kdecd = new KColorDialogBridge(qcd);
            kdecd->setColor(qcd->currentColor());
            if (qcd->options() & QColorDialog::NoButtons) {
                kdecd->setButtons(KDialog::None);
            }
            kdecd->setModal(qcd->isModal());
            qcd->setProperty("_k_bridge", QVariant::fromValue(kdecd));
        }
        if (visible) {
            kdecd->setCaption(qcd->windowTitle());
            kdecd->setAlphaChannelEnabled(qcd->options() & QColorDialog::ShowAlphaChannel);
        }
        kdecd->setVisible(visible);
        return true;
    }

    void colorDialogSetCurrentColor(QColorDialog *qcd, const QColor &color) final
    {
        K_CD(qcd);
        if (kdecd) {
            kdecd->setColor(color);
        }
    }

private slots:
    void init()
    {
        updateEffects();
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

