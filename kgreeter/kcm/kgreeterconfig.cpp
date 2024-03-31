/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "kgreeterconfig.h"
#include "kgreeter.h"

#include <QSettings>
#include <QStyleFactory>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <klocale.h>
#include <kimageio.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kstyle.h>
#include <kglobalsettings.h>
#include <kauthorization.h>
#include <kaboutdata.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

#include "config-workspace.h"

K_PLUGIN_FACTORY(KCMGreeterFactory, registerPlugin<KCMGreeter>();)
K_EXPORT_PLUGIN(KCMGreeterFactory("kcmgreeterconfig", "kcm_greeterconfig"))

KCMGreeter::KCMGreeter(QWidget* parent, const QVariantList& args)
    : KCModule(KCMGreeterFactory::componentData(), parent),
    m_lightdmexe(KStandardDirs::findRootExe("lightdm")),
    m_lightdmproc(nullptr),
    m_changed(false)
{
    Q_UNUSED(args);

    setQuickHelp(i18n("<h1>KGreeter</h1> This module allows you to change KDE greeter options."));

    setupUi(this);

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmkgreeter"), 0,
        ki18n("KDE Greeter Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2022, Ivailo Monev <email>xakepa10@gmail.com</email>")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    if (!KAuthorization::isAuthorized("org.kde.kcontrol.kcmkgreeter")) {
        setUseRootOnlyMessage(true);
        setRootOnlyMessage(i18n("You are not allowed to save the configuration"));
        setDisabled(true);
    }

    connect(this, SIGNAL(changed(bool)), this, SLOT(slotChanged(bool)));

    load();

    connect(fontchooser, SIGNAL(currentFontChanged(QFont)), this, SLOT(slotFontChanged(QFont)));

    const QStringList kthemercs = KGlobal::dirs()->findAllResources("data", "kstyle/themes/*.themerc");
    foreach (const QString &style, QStyleFactory::keys()) {
        QString kthemename = style;
        foreach (const QString &kthemerc, kthemercs) {
            KConfig kconfig(kthemerc, KConfig::SimpleConfig);
            const QString kthemercwidgetstyle = kconfig.group("KDE").readEntry("WidgetStyle");
            if (kthemercwidgetstyle.isEmpty() || kthemercwidgetstyle.toLower() != style.toLower()) {
                continue;
            }
            kthemename = kconfig.group("Misc").readEntry("Name");
        }
        stylesbox->addItem(kthemename, QVariant(style));
    }
    connect(stylesbox, SIGNAL(currentIndexChanged(QString)), this, SLOT(slotStyleChanged(QString)));

    colorsbox->addItem(i18n("Default"), QVariant(QString::fromLatin1("default")));
    const QStringList kcolorschemes = KGlobal::dirs()->findAllResources("data", "color-schemes/*.colors");
    foreach (const QString &kcolorscheme, kcolorschemes) {
        const QString kcolorschemename = QSettings(kcolorscheme).string("General/Name");
        const QString kcolorschemebasename = QFileInfo(kcolorscheme).baseName();
        colorsbox->addItem(kcolorschemename, QVariant(kcolorschemebasename));
    }
    connect(colorsbox, SIGNAL(currentIndexChanged(QString)), this, SLOT(slotColorChanged(QString)));

    cursorbox->addItem(i18n("Default"), QVariant(QString::fromLatin1("default")));
    const QStringList cursorthemes = KGlobal::dirs()->findAllResources("icon", "*/index.theme");
    foreach (const QString &cursortheme, cursorthemes) {
        const QString cursorthemename = QSettings(cursortheme).string("Icon Theme/Name");
        QDir cursorthemedir(cursortheme);
        cursorthemedir.cdUp();
        if (!cursorthemedir.exists(QString::fromLatin1("cursors"))) {
            continue;
        }
        const QString cursorthemebasename = QFileInfo(cursorthemedir.dirName()).baseName();
        cursorbox->addItem(cursorthemename, QVariant(cursorthemebasename));
    }
    connect(cursorbox, SIGNAL(currentIndexChanged(QString)), this, SLOT(slotCursorChanged(QString)));

    backgroundrequester->setFilter(KImageIO::pattern(KImageIO::Reading));
    connect(backgroundrequester, SIGNAL(textChanged(QString)), this, SLOT(slotURLChanged(QString)));
    connect(backgroundrequester, SIGNAL(urlSelected(KUrl)), this, SLOT(slotURLChanged(KUrl)));

    rectanglerequester->setFilter(KImageIO::pattern(KImageIO::Reading));
    connect(rectanglerequester, SIGNAL(textChanged(QString)), this, SLOT(slotURLChanged(QString)));
    connect(rectanglerequester, SIGNAL(urlSelected(KUrl)), this, SLOT(slotURLChanged(KUrl)));

    m_lightdmproc = new QProcess(this);
    connect(
        m_lightdmproc, SIGNAL(stateChanged(QProcess::ProcessState)),
        this, SLOT(slotProcessStateChanged(QProcess::ProcessState))
    );
    connect(m_lightdmproc, SIGNAL(finished(int)), this, SLOT(slotProcessFinished(int)));
    setProcessRunning(false);
    connect(testbutton, SIGNAL(pressed()), this, SLOT(slotTest()));
}

KCMGreeter::~KCMGreeter()
{
    killLightDM();
}

void KCMGreeter::load()
{
    QSettings kgreetersettings(KDE_SYSCONFDIR "/lightdm/lightdm-kgreeter-greeter.conf");
    const QString kgreeterfontstring = kgreetersettings.string("greeter/font", KGreeterDefaultFont().toString());
    const QString kgreeterstyle = kgreetersettings.string("greeter/style", KGreeterDefaultStyle());
    const QString kgreetercolor = kgreetersettings.string("greeter/colorscheme");
    const QString kgreetercursortheme = kgreetersettings.string("greeter/cursortheme", KGreeterDefaultCursorTheme());
    const QString kgreeterbackground = kgreetersettings.string("greeter/background", KGreeterDefaultBackground());
    const QString kgreeterrectangle = kgreetersettings.string("greeter/rectangle", KGreeterDefaultRectangle());
    loadSettings(
        kgreeterfontstring,
        kgreeterstyle,
        kgreetercolor,
        kgreetercursortheme,
        kgreeterbackground, kgreeterrectangle
    );

    enableTest(true);
    emit changed(false);
}

void KCMGreeter::save()
{
    QVariantMap kgreeterarguments;
    kgreeterarguments.insert("font", fontchooser->font().toString());
    kgreeterarguments.insert("style", stylesbox->itemData(stylesbox->currentIndex()).toString());
    kgreeterarguments.insert("colorscheme", colorsbox->itemData(colorsbox->currentIndex()).toString());
    kgreeterarguments.insert("cursortheme", cursorbox->itemData(cursorbox->currentIndex()).toString());
    kgreeterarguments.insert("background", backgroundrequester->url().path());
    kgreeterarguments.insert("rectangle", rectanglerequester->url().path());
    int kgreeterreply = KAuthorization::execute(
        "org.kde.kcontrol.kcmkgreeter", "save", kgreeterarguments
    );
    // qDebug() << kgreeterreply;

    if (kgreeterreply != KAuthorization::NoError) {
        KMessageBox::error(this, i18n("Could not save settings"));
    }

    enableTest(true);
    emit changed(false);
}

void KCMGreeter::defaults()
{
    loadSettings(
        KGreeterDefaultFont().toString(),
        KGreeterDefaultStyle(),
        QString(),
        KGreeterDefaultCursorTheme(),
        KGreeterDefaultBackground(), KGreeterDefaultRectangle()
    );

    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotFontChanged(const QFont &font)
{
    Q_UNUSED(font);
    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotStyleChanged(const QString &style)
{
    Q_UNUSED(style);
    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotColorChanged(const QString &color)
{
    Q_UNUSED(color);
    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotCursorChanged(const QString &cursor)
{
    Q_UNUSED(cursor);
    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotURLChanged(const QString &url)
{
    Q_UNUSED(url);
    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotURLChanged(const KUrl &url)
{
    Q_UNUSED(url);
    enableTest(false);
    emit changed(true);
}

void KCMGreeter::slotTest()
{
    if (m_lightdmproc->state() == QProcess::Running) {
        killLightDM();
        return;
    }

    m_lightdmproc->start(
        m_lightdmexe,
        QStringList() << QString::fromLatin1("--test-mode")
    );
    if (!m_lightdmproc->waitForStarted(10000)) {
        KMessageBox::error(this, i18n("Could not start LightDM"));
    }
}

void KCMGreeter::slotProcessStateChanged(QProcess::ProcessState state)
{
    setProcessRunning(state == QProcess::Running);
    enableTest(!m_changed);
}

void KCMGreeter::slotProcessFinished(const int exitcode)
{
    if (exitcode != 0) {
        KMessageBox::error(this, i18n("LightDM finished with error: %1", exitcode));
    }
}

void KCMGreeter::slotChanged(bool state)
{
    m_changed = state;
}

void KCMGreeter::loadSettings(const QString &font, const QString &style, const QString &color,
                              const QString &cursor, const QString &background, const QString &rectangle)
{
    QFont kgreeterfont;
    if (!kgreeterfont.fromString(font)) {
        kgreeterfont = KGreeterDefaultFont();
    }
    fontchooser->setCurrentFont(kgreeterfont);

    for (int i = 0; i < stylesbox->count(); i++) {
        if (stylesbox->itemData(i).toString().toLower() == style.toLower()) {
            stylesbox->setCurrentIndex(i);
            break;
        }
    }

    colorsbox->setCurrentIndex(0); // default
    if (!color.isEmpty()) {
        for (int i = 0; i < colorsbox->count(); i++) {
            if (colorsbox->itemData(i).toString().toLower() == color.toLower()) {
                colorsbox->setCurrentIndex(i);
                break;
            }
        }
    }

    cursorbox->setCurrentIndex(0); // default
    if (!cursor.isEmpty()) {
        for (int i = 0; i < cursorbox->count(); i++) {
            if (cursorbox->itemData(i).toString().toLower() == cursor.toLower()) {
                cursorbox->setCurrentIndex(i);
                break;
            }
        }
    }

    backgroundrequester->setUrl(KUrl(background));

    rectanglerequester->setUrl(KUrl(rectangle));
}

void KCMGreeter::enableTest(const bool enable)
{
    if (enable) {
        testbutton->setEnabled(!m_lightdmexe.isEmpty());
    } else {
        testbutton->setEnabled(m_lightdmproc->state() == QProcess::Running);
    }
}

void KCMGreeter::killLightDM()
{
    if (m_lightdmproc) {
        m_lightdmproc->terminate();
        if (!m_lightdmproc->waitForFinished(3000)) {
            m_lightdmproc->kill();
        }
    }
}

void KCMGreeter::setProcessRunning(const bool running)
{
    if (running) {
        testbutton->setText(i18n("Kill"));
        testbutton->setIcon(KIcon("process-stop"));
        testbutton->setToolTip(i18n("Press this button if LightDM refuses to stop when closing its window"));
    } else {
        testbutton->setText(i18n("Test"));
        testbutton->setIcon(KIcon("debug-run"));
        testbutton->setToolTip(i18n("Press this button to test the LightDM greeter"));
    }
}

#include "moc_kgreeterconfig.cpp"
