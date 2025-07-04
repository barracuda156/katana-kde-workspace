/*
 * KCMStyle
 * Copyright (C) 2002 Karol Szwed <gallium@kde.org>
 * Copyright (C) 2002 Daniel Molkentin <molkentin@kde.org>
 * Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
 * Copyright (C) 2009 by Davide Bettio <davide.bettio@kdemail.net>

 * Portions Copyright (C) 2007 Paolo Capriotti <p.capriotti@gmail.com>
 * Portions Copyright (C) 2007 Ivan Cukic <ivan.cukic+kde@gmail.com>
 * Portions Copyright (C) 2008 by Petri Damsten <damu@iki.fi>
 * Portions Copyright (C) 2000 TrollTech AS.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "kcmstyle.h"

#ifdef Q_WS_X11
#include <config-X11.h>
#endif

#include "styleconfdialog.h"
#include "ui_stylepreview.h"

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcombobox.h>
#include <kmessagebox.h>
#include <kstyle.h>
#include <kstandarddirs.h>
#include <KDebug>
#include <KColorScheme>
#include <KStandardDirs>
#ifdef Q_WS_X11
#include <kdecoration.h>
#endif
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <ktoolinvocation.h>

#include <QtCore/QFile>
#include <QtGui/QAbstractItemView>
#include <QtGui/QLabel>
#include <QtGui/QPainter>
#include <QtGui/QPixmapCache>
#include <QtGui/QStyleFactory>
#include <QtGui/QFormLayout>
#include <QtGui/QStandardItemModel>
#include <QtGui/QStyle>
#include <QtDBus/QDBusMessage>

K_PLUGIN_FACTORY(KCMStyleFactory, registerPlugin<KCMStyle>();)
K_EXPORT_PLUGIN(KCMStyleFactory("kcmstyle"))

class StylePreview : public QWidget, public Ui::StylePreview
{
public:
    StylePreview(QWidget *parent = 0)
    : QWidget(parent)
    {
        setupUi(this);

        // Ensure that the user can't toy with the child widgets.
        // Method borrowed from Qt's qtconfig.
        QList<QWidget*> widgets = findChildren<QWidget*>();
        foreach (QWidget* widget, widgets)
        {
            widget->installEventFilter(this);
            widget->setFocusPolicy(Qt::NoFocus);
        }
    }

    bool eventFilter( QObject* /* obj */, QEvent* ev )
    {
        switch( ev->type() )
        {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseMove:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::Enter:
            case QEvent::Leave:
            case QEvent::Wheel:
            case QEvent::ContextMenu:
                  return true; // ignore
            default:
                break;
        }
        return false;
    }
};


KCMStyle::KCMStyle( QWidget* parent, const QVariantList& )
    : KCModule( KCMStyleFactory::componentData(), parent ), appliedStyle(NULL)
{
    setQuickHelp( i18n("<h1>Style</h1>"
            "This module allows you to modify the visual appearance "
            "of user interface elements, such as the widget style "
            "and effects."));

    m_bStyleDirty= false;
    m_bToolbarDirty = false;


    KGlobal::dirs()->addResourceType("themes", "data", "kstyle/themes");

    KAboutData *about =
        new KAboutData( I18N_NOOP("kcmstyle"), 0,
                        ki18n("KDE Style Module"),
                        0, KLocalizedString(), KAboutData::License_GPL,
                        ki18n("(c) 2002 Karol Szwed, Daniel Molkentin"));

    about->addAuthor(ki18n("Karol Szwed"), KLocalizedString(), "gallium@kde.org");
    about->addAuthor(ki18n("Daniel Molkentin"), KLocalizedString(), "molkentin@kde.org");
    about->addAuthor(ki18n("Ralf Nolden"), KLocalizedString(), "nolden@kde.org");
    setAboutData( about );

    // Setup pages and mainLayout
    mainLayout = new QVBoxLayout( this );
    mainLayout->setMargin(0);

    tabWidget  = new QTabWidget( this );
    mainLayout->addWidget( tabWidget );

    // Add Page1 (Applications Style)
    // -----------------
    //gbWidgetStyle = new QGroupBox( i18n("Widget Style"), page1 );
    page1 = new QWidget;
    page1Layout = new QVBoxLayout( page1 );

    QWidget* gbWidgetStyle = new QWidget( page1 );
    QVBoxLayout *widgetLayout = new QVBoxLayout(gbWidgetStyle);

    gbWidgetStyleLayout = new QVBoxLayout;
        widgetLayout->addLayout( gbWidgetStyleLayout );
    gbWidgetStyleLayout->setAlignment( Qt::AlignTop );
    hbLayout = new QHBoxLayout( );
    hbLayout->setObjectName( "hbLayout" );

    QLabel* label=new QLabel(i18n("Widget style:"),this);
    hbLayout->addWidget( label );

    cbStyle = new KComboBox( gbWidgetStyle );
        cbStyle->setObjectName( "cbStyle" );
    cbStyle->setEditable( false );
    hbLayout->addWidget( cbStyle );
    hbLayout->setStretchFactor( cbStyle, 1 );
    label->setBuddy(cbStyle);

    pbConfigStyle = new QPushButton( i18n("Con&figure..."), gbWidgetStyle );
    pbConfigStyle->setEnabled( false );
    hbLayout->addWidget( pbConfigStyle );

    gbWidgetStyleLayout->addLayout( hbLayout );

    lblStyleDesc = new QLabel( gbWidgetStyle );
    gbWidgetStyleLayout->addWidget( lblStyleDesc );

    QGroupBox *gbPreview = new QGroupBox( i18n( "Preview" ), page1 );
    QVBoxLayout *previewLayout = new QVBoxLayout(gbPreview);
    previewLayout->setMargin( 0 );
    stylePreview = new StylePreview( gbPreview );
    gbPreview->layout()->addWidget( stylePreview );

    page1Layout->addWidget( gbWidgetStyle );
    page1Layout->addWidget( gbPreview );
    page1Layout->addStretch();

    connect( cbStyle, SIGNAL(activated(int)), this, SLOT(styleChanged()) );
    connect( cbStyle, SIGNAL(activated(int)), this, SLOT(updateConfigButton()));
    connect( pbConfigStyle, SIGNAL(clicked()), this, SLOT(styleSpecificConfig()));

    // Add Page2 (Effects)
    // -------------------
    page2 = new QWidget;
    fineTuningUi.setupUi(page2);

    fineTuningUi.comboGraphicEffectsLevel->setObjectName( "cbGraphicEffectsLevel" );
    fineTuningUi.comboGraphicEffectsLevel->setEditable( false );
    fineTuningUi.comboGraphicEffectsLevel->addItem(i18n("Low display resolution and Low CPU"), KGlobalSettings::NoEffects);
    fineTuningUi.comboGraphicEffectsLevel->addItem(i18n("Low display resolution and High CPU"), KGlobalSettings::SimpleAnimationEffects);
    fineTuningUi.comboGraphicEffectsLevel->addItem(i18n("High display resolution and High CPU"), static_cast<int>(KGlobalSettings::SimpleAnimationEffects | KGlobalSettings::ComplexAnimationEffects));

    connect(cbStyle, SIGNAL(activated(int)), this, SLOT(setStyleDirty()));
    connect(fineTuningUi.cbIconsOnButtons,     SIGNAL(toggled(bool)),   this, SLOT(setToolbarDirty()));
    connect(fineTuningUi.cbIconsInMenus,     SIGNAL(toggled(bool)),   this, SLOT(setToolbarDirty()));
    connect(fineTuningUi.comboGraphicEffectsLevel, SIGNAL(activated(int)),   this, SLOT(setStyleDirty()));
    connect(fineTuningUi.comboToolbarIcons,    SIGNAL(activated(int)), this, SLOT(setToolbarDirty()));
    connect(fineTuningUi.comboSecondaryToolbarIcons,    SIGNAL(activated(int)), this, SLOT(setToolbarDirty()));

    // Page1
    cbStyle->setWhatsThis( i18n("Here you can choose from a list of"
                            " predefined widget styles (e.g. the way buttons are drawn) which"
                            " may or may not be combined with a theme (additional information"
                            " like a marble texture or a gradient).") );
    stylePreview->setWhatsThis( i18n("This area shows a preview of the currently selected style "
                            "without having to apply it to the whole desktop.") );
    // Page2
    page2->setWhatsThis( i18n("This page allows you to choose details about the widget style options") );
    fineTuningUi.comboToolbarIcons->setWhatsThis( i18n( "<p><b>No Text:</b> Shows only icons on toolbar buttons. "
                            "Best option for low resolutions.</p>"
                            "<p><b>Text Only: </b>Shows only text on toolbar buttons.</p>"
                            "<p><b>Text Beside Icons: </b> Shows icons and text on toolbar buttons. "
                            "Text is aligned beside the icon.</p>"
                            "<b>Text Below Icons: </b> Shows icons and text on toolbar buttons. "
                            "Text is aligned below the icon.") );
    fineTuningUi.cbIconsOnButtons->setWhatsThis( i18n( "If you enable this option, KDE Applications will "
                            "show small icons alongside some important buttons.") );
    fineTuningUi.cbIconsInMenus->setWhatsThis( i18n( "If you enable this option, KDE Applications will "
                            "show small icons alongside most menu items.") );
    fineTuningUi.comboGraphicEffectsLevel->setWhatsThis( i18n( "If you enable this option, KDE Applications will "
                            "run internal animations.") );

    // Insert the pages into the tabWidget
    tabWidget->addTab(page1, i18nc("@title:tab", "&Applications"));
    tabWidget->addTab(page2, i18nc("@title:tab", "&Fine Tuning"));
}


KCMStyle::~KCMStyle()
{
    qDeleteAll(styleEntries);
    delete appliedStyle;
}

void KCMStyle::updateConfigButton()
{
    if (!styleEntries[currentStyle()] || styleEntries[currentStyle()]->configPage.isEmpty()) {
        pbConfigStyle->setEnabled(false);
        return;
    }

    // We don't check whether it's loadable here -
    // lets us report an error and not waste time
    // loading things if the user doesn't click the button
    pbConfigStyle->setEnabled( true );
}

void KCMStyle::styleSpecificConfig()
{
    QString libname = styleEntries[currentStyle()]->configPage;

#warning port loader to KPluginLoader
    QLibrary library(libname); // , KCMStyleFactory::componentData());
    if (!library.load()) {
        KMessageBox::detailedError(this,
            i18n("There was an error loading the configuration dialog for this style."),
            library.errorString(),
            i18n("Unable to Load Dialog"));
        return;
    }

    void *allocPtr = library.resolve("allocate_kstyle_config");

    if (!allocPtr)
    {
        KMessageBox::detailedError(this,
            i18n("There was an error loading the configuration dialog for this style."),
            library.errorString(),
            i18n("Unable to Load Dialog"));
        return;
    }

    //Create the container dialog
    StyleConfigDialog* dial = new StyleConfigDialog(this, styleEntries[currentStyle()]->name);

    typedef QWidget*(* factoryRoutine)( QWidget* parent );

    //Get the factory, and make the widget.
    factoryRoutine factory      = (factoryRoutine)(allocPtr); //Grmbl. So here I am on my
    //"never use C casts" moralizing streak, and I find that one can't go void* -> function ptr
    //even with a reinterpret_cast.

    QWidget*       pluginConfig = factory( dial );

    //Insert it in...
    dial->setMainWidget( pluginConfig );

    //..and connect it to the wrapper
    connect(pluginConfig, SIGNAL(changed(bool)), dial, SLOT(setDirty(bool)));
    connect(dial, SIGNAL(defaults()), pluginConfig, SLOT(defaults()));
    connect(dial, SIGNAL(save()), pluginConfig, SLOT(save()));

    if (dial->exec() == QDialog::Accepted  && dial->isDirty() ) {
        // Force re-rendering of the preview, to apply settings
        switchStyle(currentStyle(), true);

        //For now, ask all KDE apps to recreate their styles to apply the setitngs
        KGlobalSettings::self()->emitChange(KGlobalSettings::StyleChanged);

        // We call setStyleDirty here to make sure we force style re-creation
        setStyleDirty();
    }

    delete dial;
}

void KCMStyle::changeEvent( QEvent *event )
{
    KCModule::changeEvent( event );
    if ( event->type() == QEvent::PaletteChange ) {
        // Force re-rendering of the preview, to apply new palette
        switchStyle(currentStyle(), true);
    }
}

void KCMStyle::load()
{
    KConfig config( "kdeglobals", KConfig::FullConfig );

    loadStyle( config );
    loadEffects( config );

    m_bStyleDirty= false;
    m_bToolbarDirty = false;
    //Enable/disable the button for the initial style
    updateConfigButton();

    emit changed( false );
}


void KCMStyle::save()
{
    // Don't do anything if we don't need to.
    if ( !(m_bStyleDirty | m_bToolbarDirty ) )
        return;

    const bool showMenuIcons = !QApplication::testAttribute(Qt::AA_DontShowIconsInMenus);
    if (fineTuningUi.cbIconsInMenus->isChecked() != showMenuIcons) {
        KMessageBox::information(this,
          i18n("<p>Changes to the visibility of menu icons will only affect newly started applications.</p>"),
          i18nc("@title:window", "Menu Icons Changed"), "MenuIconsChanged");
    }

    // Save effects.
    KConfig      _config("kdeglobals", KConfig::NoGlobals);
    KConfigGroup config(&_config, "KDE");
    // Effects page
    config.writeEntry( "ShowIconsOnPushButtons", fineTuningUi.cbIconsOnButtons->isChecked());
    config.writeEntry( "ShowIconsInMenuItems", fineTuningUi.cbIconsInMenus->isChecked());
    KConfigGroup g( &_config, "KDE-Global GUI Settings" );
    g.writeEntry( "GraphicEffectsLevel", fineTuningUi.comboGraphicEffectsLevel->itemData(fineTuningUi.comboGraphicEffectsLevel->currentIndex()));


    KConfigGroup generalGroup(&_config, "General");
    generalGroup.writeEntry("widgetStyle", currentStyle());

    KConfigGroup toolbarStyleGroup(&_config, "Toolbar style");
    toolbarStyleGroup.writeEntry("ToolButtonStyle",
                            toolbarButtonText(fineTuningUi.comboToolbarIcons->currentIndex()));
    toolbarStyleGroup.writeEntry("ToolButtonStyleOtherToolbars",
                            toolbarButtonText(fineTuningUi.comboSecondaryToolbarIcons->currentIndex()));

    _config.sync();

    // Now allow KDE apps to reconfigure themselves.
    if ( m_bStyleDirty)
        KGlobalSettings::self()->emitChange(KGlobalSettings::StyleChanged);

    if ( m_bToolbarDirty ) {
        KGlobalSettings::self()->emitChange(KGlobalSettings::ToolbarStyleChanged);

#ifdef Q_WS_X11
        // Send signal to all kwin instances
        QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
#endif
    }

    // Export the changes we made to qtrc, and update all qt-only
    // applications on the fly, ensuring that we still follow the user's
    // export fonts/colors settings.
    if (m_bStyleDirty | m_bToolbarDirty)    // Export only if necessary
    {
        KToolInvocation::self()->startProgram("krdb");
    }

    // Clean up
    m_bStyleDirty    = false;
    m_bToolbarDirty  = false;
    emit changed( false );
}


bool KCMStyle::findStyle( const QString& str, int& combobox_item )
{
    StyleEntry* se   = styleEntries[str.toLower()];

    QString     name = se ? se->name : str;

    combobox_item = 0;

    //look up name
    for( int i = 0; i < cbStyle->count(); i++ )
    {
        if ( cbStyle->itemText(i) == name )
        {
            combobox_item = i;
            return true;
        }
    }

    return false;
}


void KCMStyle::defaults()
{
    // Select default style
    int item = 0;
    bool found;

    found = findStyle( KStyle::defaultStyle(), item );
    if (!found)
        found = findStyle( "oxygen", item );
    if (!found)
        found = findStyle( "cleanlooks", item );
    if (!found)
        found = findStyle( "windows", item );

    cbStyle->setCurrentIndex( item );

    m_bStyleDirty = true;
    switchStyle( currentStyle() );  // make resets visible

    // Effects
    fineTuningUi.comboToolbarIcons->setCurrentIndex(toolbarButtonIndex("TextBesideIcon"));
    fineTuningUi.comboSecondaryToolbarIcons->setCurrentIndex(toolbarButtonIndex("TextBesideIcon"));
    fineTuningUi.cbIconsOnButtons->setChecked(true);
    fineTuningUi.cbIconsInMenus->setChecked(true);
    fineTuningUi.comboGraphicEffectsLevel->setCurrentIndex(fineTuningUi.comboGraphicEffectsLevel->findData(static_cast<int>(KGlobalSettings::graphicEffectsLevelDefault())));
    emit changed(true);
}

void KCMStyle::setToolbarDirty()
{
    m_bToolbarDirty = true;
    emit changed(true);
}

void KCMStyle::setStyleDirty()
{
    m_bStyleDirty = true;
    emit changed(true);
}

// ----------------------------------------------------------------
// All the Style Switching / Preview stuff
// ----------------------------------------------------------------

void KCMStyle::loadStyle( KConfig& config )
{
    cbStyle->clear();
    // Create a dictionary of WidgetStyle to Name and Desc. mappings,
    // as well as the config page info
    qDeleteAll(styleEntries);
    styleEntries.clear();

    QString strWidgetStyle;
    QStringList list = KGlobal::dirs()->findAllResources("themes", "*.themerc",
                                                         KStandardDirs::Recursive |
                                                         KStandardDirs::NoDuplicates);
    for (QStringList::iterator it = list.begin(); it != list.end(); ++it)
    {
        KConfig config(  *it, KConfig::SimpleConfig);
        if ( !(config.hasGroup("KDE") && config.hasGroup("Misc")) )
            continue;

        KConfigGroup configGroup = config.group("KDE");

        strWidgetStyle = configGroup.readEntry("WidgetStyle");
        if (strWidgetStyle.isNull())
            continue;

        // We have a widgetstyle, so lets read the i18n entries for it...
        StyleEntry* entry = new StyleEntry;
        configGroup = config.group("Misc");
        entry->name = configGroup.readEntry("Name");
        entry->desc = configGroup.readEntry("Comment", i18n("No description available."));
        entry->configPage = configGroup.readEntry("ConfigPage", QString());

        // Insert the entry into our dictionary.
        styleEntries.insert(strWidgetStyle.toLower(), entry);
    }

    // Obtain all style names
    QStringList allStyles = QStyleFactory::keys();

    // Get translated names
    QStringList styles;
    StyleEntry* entry;
    for (QStringList::iterator it = allStyles.begin(); it != allStyles.end(); ++it)
    {
        QString id = (*it).toLower();
        // Find the entry.
        if ( (entry = styleEntries[id]) != 0 )
        {
            styles += entry->name;

            nameToStyleKey[entry->name] = id;
        }
        else
        {
            styles += (*it); //Fall back to the key (but in original case)
            nameToStyleKey[*it] = id;
        }
    }

    // Sort the style list, and add it to the combobox
    styles.sort();
    cbStyle->addItems( styles );

    // Find out which style is currently being used
    KConfigGroup configGroup = config.group( "General" );
    QString defaultStyle = KStyle::defaultStyle();
    QString cfgStyle = configGroup.readEntry( "widgetStyle", defaultStyle );

    // Select the current style
    // Do not use cbStyle->listBox() as this may be NULL for some styles when
    // they use QPopupMenus for the drop-down list!

    // ##### Since Trolltech likes to seemingly copy & paste code,
    // QStringList::findItem() doesn't have a Qt::StringComparisonMode field.
    // We roll our own (yuck)
    cfgStyle = cfgStyle.toLower();
    int item = 0;
    for( int i = 0; i < cbStyle->count(); i++ )
    {
        QString id = nameToStyleKey[cbStyle->itemText(i)];
        item = i;
        if ( id == cfgStyle )   // ExactMatch
            break;
        else if ( id.contains( cfgStyle ) )
            break;
        else if ( id.contains( QApplication::style()->metaObject()->className() ) )
            break;
        item = 0;
    }
    cbStyle->setCurrentIndex( item );
    m_bStyleDirty = false;

    switchStyle( currentStyle() );  // make resets visible
}

QString KCMStyle::currentStyle()
{
    return nameToStyleKey[cbStyle->currentText()];
}


void KCMStyle::styleChanged()
{
    switchStyle( currentStyle() );
}


void KCMStyle::switchStyle(const QString& styleName, bool force)
{
    // Don't flicker the preview if the same style is chosen in the cb
    if (!force && appliedStyle && appliedStyle->objectName() == styleName)
        return;

    // Create an instance of the new style...
    QStyle* style = QStyleFactory::create(styleName);
    if (!style)
        return;

    // Prevent Qt from wrongly caching radio button images
    QPixmapCache::clear();

    setStyleRecursive( stylePreview, style );

    // this flickers, but reliably draws the widgets correctly.
    stylePreview->resize( stylePreview->sizeHint() );

    delete appliedStyle;
    appliedStyle = style;

    // Set the correct style description
    StyleEntry* entry = styleEntries[ styleName ];
    QString desc;
    desc = i18n("Description: %1", entry ? entry->desc : i18n("No description available.") );
    lblStyleDesc->setText( desc );
}

void KCMStyle::setStyleRecursive(QWidget* w, QStyle* s)
{
    // Don't let broken styles kill the palette
    // for other styles being previewed. (e.g SGI style)
    w->setPalette(QPalette());

    QPalette newPalette(KGlobalSettings::createApplicationPalette());
    s->polish( newPalette );
    w->setPalette(newPalette);

    // Apply the new style.
    w->setStyle(s);

    // Recursively update all children.
    const QObjectList children = w->children();

    // Apply the style to each child widget.
    foreach (QObject* child, children)
    {
        if (child->isWidgetType())
            setStyleRecursive((QWidget *) child, s);
    }
}

// ----------------------------------------------------------------
// All the Effects stuff
// ----------------------------------------------------------------
QString KCMStyle::toolbarButtonText(int index)
{
    switch (index) {
        case 1:
            return "TextOnly";
        case 2:
            return "TextBesideIcon";
        case 3:
            return "TextUnderIcon";
        default:
            break;
    }

    return "NoText";
}

int KCMStyle::toolbarButtonIndex(const QString &text)
{
    if (text == "TextOnly") {
        return 1;
    } else if (text == "TextBesideIcon") {
        return 2;
    } else if (text == "TextUnderIcon") {
        return 3;
    }

    return 0;
}

void KCMStyle::loadEffects( KConfig& config )
{
    // KDE's Part via KConfig
    KConfigGroup configGroup = config.group("Toolbar style");

    QString tbIcon = configGroup.readEntry("ToolButtonStyle", "TextBesideIcon");
    fineTuningUi.comboToolbarIcons->setCurrentIndex(toolbarButtonIndex(tbIcon));
    tbIcon = configGroup.readEntry("ToolButtonStyleOtherToolbars", "TextBesideIcon");
    fineTuningUi.comboSecondaryToolbarIcons->setCurrentIndex(toolbarButtonIndex(tbIcon));

    configGroup = config.group("KDE");
    fineTuningUi.cbIconsOnButtons->setChecked(configGroup.readEntry("ShowIconsOnPushButtons", true));
    fineTuningUi.cbIconsInMenus->setChecked(configGroup.readEntry("ShowIconsInMenuItems", true));

    KConfigGroup graphicConfigGroup = config.group("KDE-Global GUI Settings");
    fineTuningUi.comboGraphicEffectsLevel->setCurrentIndex(fineTuningUi.comboGraphicEffectsLevel->findData(graphicConfigGroup.readEntry("GraphicEffectsLevel", ((int) KGlobalSettings::graphicEffectsLevel()))));

    m_bToolbarDirty = false;
}

#include "moc_kcmstyle.cpp"

// vim: set noet ts=4:
