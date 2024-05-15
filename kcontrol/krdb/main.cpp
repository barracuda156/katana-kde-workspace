/****************************************************************************
**
**
** KRDB - puts current KDE color scheme into preprocessor statements
** cats specially written application default files and uses xrdb -merge to
** write to RESOURCE_MANAGER. Thus it gives a  simple way to make non-KDE
** applications fit in with the desktop
**
** Copyright (C) 1998 by Mark Donohoe
** Copyright (C) 1999 by Dirk A. Mueller (reworked for KDE 2.0)
** Copyright (C) 2001 by Matthias Ettrich (add support for GTK applications )
** Copyright (C) 2001 by Waldo Bastian <bastian@kde.org>
** Copyright (C) 2002 by Karol Szwed <gallium@kde.org>
** This application is freely distributable under the GNU Public License.
**
*****************************************************************************/

#include <config-workspace.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <QBuffer>
#include <QDir>
#include <QSettings>
#include <QTextCodec>
#include <QToolTip>
#include <QPixmap>
#include <QByteArray>
#include <QTextStream>
#include <QDateTime>
#include <QProcess>

#include <ktoolinvocation.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kstandarddirs.h>
#include <ksavefile.h>
#include <ktemporaryfile.h>
#include <klocale.h>
#include <kstyle.h>

inline const char * gtkEnvVar(int version)
{
    return (2 == version ? "GTK2_RC_FILES" : "GTK_RC_FILES");
}

inline const char * sysGtkrc(int version)
{
    if (version == 2) {
        return "/etc/gtk-2.0/gtkrc";
    }
    return "/etc/gtk/gtkrc";
}

inline const char * userGtkrc(int version)
{
    if (version == 2) {
        return "/.gtkrc-2.0";
    }
    return "/.gtkrc";
}

// -----------------------------------------------------------------------------
static void applyGtkStyles(bool active, int version)
{
    QString gtkkde = KStandardDirs::locateLocal("config", version == 2 ? "gtkrc-2.0" : "gtkrc");
    QByteArray gtkrc = qgetenv(gtkEnvVar(version));
    QStringList list = QFile::decodeName(gtkrc).split(':', QString::SkipEmptyParts);
    QString userHomeGtkrc = QDir::homePath() + userGtkrc(version);
    if (!list.contains(userHomeGtkrc)) {
        list.prepend(userHomeGtkrc);
    }
    QLatin1String systemGtkrc = QLatin1String(sysGtkrc(version));
    if (!list.contains(systemGtkrc)) {
        list.prepend(systemGtkrc);
    }
    list.removeAll(gtkkde);
    list.append(gtkkde);

    // Pass env. var to klauncher.
    QString name = gtkEnvVar(version);
    QString value = list.join(":");
    KToolInvocation::self()->setLaunchEnv(name, value);
}

// -----------------------------------------------------------------------------

static void addColorDef(QString&s, const char *n, const QColor &col)
{
    QString tmp;
    tmp.sprintf("#define %s #%02x%02x%02x\n", n, col.red(), col.green(), col.blue());
    s += tmp;
}


// -----------------------------------------------------------------------------
static void copyFile(QFile &tmp, const QString &filename)
{
    QFile f(filename);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray buf(8192, ' ');
        while (!f.atEnd()) {
            int read = f.read(buf.data(), buf.size());
            if (read > 0) {
                tmp.write(buf.data(), read);
            }
        }
    }
}

// -----------------------------------------------------------------------------
static QString item(int i)
{
    return QString::number(i / 255.0, 'f', 3);
}

static QString color(const QColor &col)
{
    return QString::fromLatin1("{ %1, %2, %3 }").arg(item(col.red())).arg(item(col.green())).arg(item(col.blue()));
}

static void createGtkrc(bool exportColors, const QPalette &cg, int version)
{
    // lukas: why does it create in ~/.kde/share/config ???
    // pfeiffer: so that we don't overwrite the user's gtkrc.
    // it is found via the GTK_RC_FILES environment variable.
    KSaveFile saveFile(KStandardDirs::locateLocal("config", version == 2 ? "gtkrc-2.0" : "gtkrc"));
    if (!saveFile.open()) {
        return;
    }

    QTextStream t(&saveFile);
    t.setCodec(QTextCodec::codecForLocale());

    t << i18n(
            "# created by KDE, %1\n"
            "#\n"
            "# If you do not want KDE to override your GTK settings, select\n"
            "# Appearance -> Colors in the System Settings and disable the checkbox\n"
            "# \"Apply colors to non-KDE4 applications\"\n"
            "#\n"
            "#\n", QDateTime::currentDateTime().toString());

    if (version == 2) {
        t << endl;
        t << "gtk-alternative-button-order = 1" << endl;
        t << endl;
    }

    if (exportColors) {
        t << "style \"default\"" << endl;
        t << "{" << endl;
        t << "  bg[NORMAL] = " << color(cg.color(QPalette::Active, QPalette::Background)) << endl;
        t << "  bg[SELECTED] = " << color(cg.color(QPalette::Active, QPalette::Highlight)) << endl;
        t << "  bg[INSENSITIVE] = " << color(cg.color(QPalette::Active, QPalette::Background)) << endl;
        t << "  bg[ACTIVE] = " << color(cg.color(QPalette::Active, QPalette::Mid)) << endl;
        t << "  bg[PRELIGHT] = " << color(cg.color(QPalette::Active, QPalette::Background)) << endl;
        t << endl;
        t << "  base[NORMAL] = " << color(cg.color(QPalette::Active, QPalette::Base)) << endl;
        t << "  base[SELECTED] = " << color(cg.color(QPalette::Active, QPalette::Highlight)) << endl;
        t << "  base[INSENSITIVE] = " << color(cg.color( QPalette::Active, QPalette::Background)) << endl;
        t << "  base[ACTIVE] = " << color(cg.color(QPalette::Active, QPalette::Highlight)) << endl;
        t << "  base[PRELIGHT] = " << color(cg.color(QPalette::Active, QPalette::Highlight)) << endl;
        t << endl;
        t << "  text[NORMAL] = " << color(cg.color(QPalette::Active, QPalette::Text) ) << endl;
        t << "  text[SELECTED] = " << color(cg.color(QPalette::Active, QPalette::HighlightedText)) << endl;
        t << "  text[INSENSITIVE] = " << color(cg.color(QPalette::Active, QPalette::Mid)) << endl;
        t << "  text[ACTIVE] = " << color(cg.color(QPalette::Active, QPalette::HighlightedText)) << endl;
        t << "  text[PRELIGHT] = " << color(cg.color(QPalette::Active, QPalette::HighlightedText)) << endl;
        t << endl;
        t << "  fg[NORMAL] = " << color (cg.color(QPalette::Active, QPalette::Foreground)) << endl;
        t << "  fg[SELECTED] = " << color(cg.color(QPalette::Active, QPalette::HighlightedText)) << endl;
        t << "  fg[INSENSITIVE] = " << color(cg.color(QPalette::Active, QPalette::Mid)) << endl;
        t << "  fg[ACTIVE] = " << color(cg.color(QPalette::Active, QPalette::Foreground)) << endl;
        t << "  fg[PRELIGHT] = " << color(cg.color(QPalette::Active, QPalette::Foreground)) << endl;
        t << "}" << endl;
        t << endl;
        t << "class \"*\" style \"default\"" << endl;
        t << endl;

        // tooltips don't have the standard background color
        t << "style \"ToolTip\"" << endl;
        t << "{" << endl;
        QPalette group = QToolTip::palette();
        t << "  bg[NORMAL] = " << color(group.color(QPalette::Active, QPalette::Background)) << endl;
        t << "  base[NORMAL] = " << color(group.color(QPalette::Active, QPalette::Base)) << endl;
        t << "  text[NORMAL] = " << color(group.color(QPalette::Active, QPalette::Text)) << endl;
        t << "  fg[NORMAL] = " << color(group.color(QPalette::Active, QPalette::Foreground)) << endl;
        t << "}" << endl;
        t << endl;
        t << "widget \"gtk-tooltip\" style \"ToolTip\"" << endl;
        t << "widget \"gtk-tooltips\" style \"ToolTip\"" << endl;
        t << endl;


        // highlight the current (mouse-hovered) menu-item
        // not every button, checkbox, etc.
        t << "style \"MenuItem\"" << endl;
        t << "{" << endl;
        t << "  bg[PRELIGHT] = " << color(cg.color(QPalette::Highlight)) << endl;
        t << "  fg[PRELIGHT] = " << color(cg.color(QPalette::HighlightedText)) << endl;
        t << "}" << endl;
        t << endl;
        t << "class \"*MenuItem\" style \"MenuItem\"" << endl;
        t << endl;
    }

}

// -----------------------------------------------------------------------------

QT_BEGIN_NAMESPACE
extern void qt_x11_apply_settings_in_all_apps();
QT_END_NAMESPACE

int main(int argc, char *argv[])
{
    // application instance for QX11Info::display()
    QApplication app(argc, argv);

    KConfig kcmdisplayconfig( "kcmdisplayrc", KConfig::NoGlobals  );
    KConfigGroup kcmdisplayx11(&kcmdisplayconfig, "X11");

    // This key is written by the "colors" module.
    bool exportColors      = kcmdisplayx11.readEntry("exportKDEColors", true);
    bool exportXftSettings = true;

    // Obtain the application palette that is about to be set.
    KSharedConfigPtr kglobalcfg = KSharedConfig::openConfig("kdeglobals");
    KConfigGroup kglobals(kglobalcfg, "KDE");
    QPalette newPal = KGlobalSettings::createApplicationPalette(kglobalcfg);

    KTemporaryFile tmpFile;
    if (!tmpFile.open()) {
        kWarning() << "Couldn't open temp file";
        return 1;
    }

    KConfigGroup generalCfgGroup(kglobalcfg, "General");

    createGtkrc(exportColors, newPal, 1);
    createGtkrc(exportColors, newPal, 2);

    // Export colors to non-(KDE/Qt) apps (e.g. Motif, GTK+ apps)
    if (exportColors) {
        KGlobal::dirs()->addResourceType("appdefaults", "data", "kdisplay/app-defaults/");
        KGlobal::locale()->insertCatalog("krdb");

        QString preproc;
        QColor backCol = newPal.color(QPalette::Active, QPalette::Background);
        addColorDef(preproc, "FOREGROUND"         , newPal.color(QPalette::Active, QPalette::Foreground));
        addColorDef(preproc, "BACKGROUND"         , backCol);
        addColorDef(preproc, "HIGHLIGHT"          , backCol.light(100 + (2 * KGlobalSettings::contrast() + 4) * 16 / 1));
        addColorDef(preproc, "LOWLIGHT"           , backCol.dark(100 + (2 * KGlobalSettings::contrast() + 4 )* 10));
        addColorDef(preproc, "SELECT_BACKGROUND"  , newPal.color(QPalette::Active, QPalette::Highlight));
        addColorDef(preproc, "SELECT_FOREGROUND"  , newPal.color(QPalette::Active, QPalette::HighlightedText));
        addColorDef(preproc, "WINDOW_BACKGROUND"  , newPal.color(QPalette::Active, QPalette::Base));
        addColorDef(preproc, "WINDOW_FOREGROUND"  , newPal.color(QPalette::Active, QPalette::Text));
        addColorDef(preproc, "INACTIVE_BACKGROUND", KGlobalSettings::inactiveTitleColor());
        addColorDef(preproc, "INACTIVE_FOREGROUND", KGlobalSettings::inactiveTitleColor());
        addColorDef(preproc, "ACTIVE_BACKGROUND"  , KGlobalSettings::activeTitleColor());
        addColorDef(preproc, "ACTIVE_FOREGROUND"  , KGlobalSettings::activeTitleColor());
        //---------------------------------------------------------------

        tmpFile.write(preproc.toLatin1(), preproc.length());

        QStringList list;
        const QStringList adPaths = KGlobal::dirs()->findDirs("appdefaults", "");
        for (QStringList::ConstIterator it = adPaths.constBegin(); it != adPaths.constEnd(); ++it) {
            QDir dSys(*it);

            if (dSys.exists()) {
                dSys.setFilter(QDir::Files);
                dSys.setSorting(QDir::Name);
                dSys.setNameFilters(QStringList("*.ad"));
                list += dSys.entryList();
            }
        }

        for (QStringList::ConstIterator it = list.constBegin(); it != list.constEnd(); ++it) {
            copyFile(tmpFile, KStandardDirs::locate("appdefaults", *it));
        }
    }

    // Merge ~/.Xresources or fallback to ~/.Xdefaults
    QString homeDir = QDir::homePath();
    QString xResources = homeDir + "/.Xresources";

    // very primitive support for ~/.Xresources by appending it
    if (QFile::exists(xResources)) {
        copyFile(tmpFile, xResources);
    } else {
        copyFile(tmpFile, homeDir + "/.Xdefaults");
    }

    // Export the Xcursor theme & size settings
    KConfigGroup mousecfg(KSharedConfig::openConfig("kcminputrc"), "Mouse");
    QString theme = mousecfg.readEntry("cursorTheme", QString::fromLatin1(KDE_DEFAULT_CURSOR_THEME));
    QString size  = mousecfg.readEntry("cursorSize", QString());
    QString contents;

    if (!theme.isNull()) {
        contents = "Xcursor.theme: " + theme + '\n';
    }

    if (!size.isNull()) {
        contents += "Xcursor.size: " + size + '\n';
    }

    if (exportXftSettings) {
        if (generalCfgGroup.hasKey("XftAntialias")) {
            contents += "Xft.antialias: ";
            if (generalCfgGroup.readEntry("XftAntialias", true)) {
                contents += "1\n";
            } else {
                contents += "0\n";
            }
        }

        if (generalCfgGroup.hasKey("XftHintStyle")) {
            QString hintStyle = generalCfgGroup.readEntry("XftHintStyle", "hintmedium");
            contents += "Xft.hinting: ";
            if (hintStyle.isEmpty()) {
                contents += "-1\n";
            } else {
                if (hintStyle != "hintnone") {
                    contents += "1\n";
                } else {
                    contents += "0\n";
                }
                contents += "Xft.hintstyle: " + hintStyle + '\n';
            }
        }

        if (generalCfgGroup.hasKey("XftSubPixel")) {
            QString subPixel = generalCfgGroup.readEntry("XftSubPixel");
            if (!subPixel.isEmpty()) {
                contents += "Xft.rgba: " + subPixel + '\n';
            }
        }

        KConfig _cfgfonts("kcmfonts");
        KConfigGroup cfgfonts(&_cfgfonts, "General");

        if (cfgfonts.readEntry("forceFontDPI", 0) != 0) {
            contents += "Xft.dpi: " + cfgfonts.readEntry("forceFontDPI") + '\n';
        } else {
            QProcess proc;
            proc.start("xrdb", QStringList() << "-quiet" << "-remove" << "-nocpp");
            if (proc.waitForStarted()) {
                proc.write(QByteArray("Xft.dpi\n"));
                proc.closeWriteChannel();
                proc.waitForFinished();
            }
        }
    }

    if (contents.length() > 0) {
        tmpFile.write(contents.toLatin1(), contents.length());
    }

    tmpFile.flush();

    QStringList procargs;
#ifndef NDEBUG
    procargs << "-merge" << tmpFile.fileName();
#else
    procargs << "-quiet" << "-merge" << tmpFile.fileName();
#endif
    QProcess::execute("xrdb", procargs);

    applyGtkStyles(exportColors, 1);
    applyGtkStyles(exportColors, 2);

    // Send notification for settings change to Katie applications
    QApplication::flush();
    qt_x11_apply_settings_in_all_apps();
    QApplication::flush();

    return 0;
}

