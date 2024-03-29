
// Own
#include "netpref.h"

// Qt
#include <QLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QBoxLayout>
#include <QFormLayout>

// KDE
#include <kio/ioslave_defaults.h>
#include <knuminput.h>
#include <klocale.h>
#include <kdialog.h>
#include <kconfig.h>
#include <kpluginfactory.h>

// Local
#include "ksaveioconfig.h"

#define MAX_TIMEOUT_VALUE  3600

K_PLUGIN_FACTORY_DECLARATION(KioConfigFactory)

KIOPreferences::KIOPreferences(QWidget *parent, const QVariantList &)
               :KCModule(KioConfigFactory::componentData(), parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    mainLayout->setMargin(0);
    gb_Timeout = new QGroupBox( i18n("Timeout Values"), this );
    gb_Timeout->setWhatsThis( i18np("Here you can set timeout values. "
                                    "You might want to tweak them if your "
                                    "connection is very slow. The maximum "
                                    "allowed value is 1 second." ,
                                    "Here you can set timeout values. "
                                    "You might want to tweak them if your "
                                    "connection is very slow. The maximum "
                                    "allowed value is %1 seconds.", MAX_TIMEOUT_VALUE));
    mainLayout->addWidget( gb_Timeout );

    QFormLayout* timeoutLayout = new QFormLayout(gb_Timeout);
    sb_serverConnect = new KIntNumInput( 0, this );
    sb_serverConnect->setSuffix( ki18np( " second", " seconds" ) );
    connect(sb_serverConnect, SIGNAL(valueChanged(int)), SLOT(configChanged()));
    timeoutLayout->addRow(i18n("Server co&nnect:"), sb_serverConnect);

    sb_serverResponse = new KIntNumInput( 0, this );
    sb_serverResponse->setSuffix( ki18np( " second", " seconds" ) );
    connect(sb_serverResponse, SIGNAL(valueChanged(int)), SLOT(configChanged()));
    timeoutLayout->addRow(i18n("&Server response:"), sb_serverResponse);

    gb_Ftp = new QGroupBox( i18n( "FTP Options" ), this );
    mainLayout->addWidget( gb_Ftp );
    QVBoxLayout* ftpLayout = new QVBoxLayout(gb_Ftp);

    cb_ftpEnablePasv = new QCheckBox( i18n( "Enable passive &mode (PASV)" ), this );
    cb_ftpEnablePasv->setWhatsThis( i18n("Enables FTP's \"passive\" mode. "
                                         "This is required to allow FTP to "
                                         "work from behind firewalls.") );
    connect(cb_ftpEnablePasv, SIGNAL(toggled(bool)), SLOT(configChanged()));
    ftpLayout->addWidget(cb_ftpEnablePasv);

    cb_ftpMarkPartial = new QCheckBox( i18n( "Mark &partially uploaded files" ), this );
    cb_ftpMarkPartial->setWhatsThis( i18n( "<p>Marks partially uploaded FTP "
                                           "files.</p><p>When this option is "
                                           "enabled, partially uploaded files "
                                           "will have a \".part\" extension. "
                                           "This extension will be removed "
                                           "once the transfer is complete.</p>") );
    connect(cb_ftpMarkPartial, SIGNAL(toggled(bool)), SLOT(configChanged()));
    ftpLayout->addWidget(cb_ftpMarkPartial);

    gb_Misc = new QGroupBox( i18n( "Miscellaneous" ), this );
    mainLayout->addWidget( gb_Misc );
    QFormLayout* miscLayout = new QFormLayout(gb_Misc);

    sb_minimumKeepSize = new KIntNumInput( 0, this );
    sb_minimumKeepSize->setSuffix( ki18np( " bytes", " bytes" ) );
    connect(sb_minimumKeepSize, SIGNAL(valueChanged(int)), SLOT(configChanged()));
    miscLayout->addRow(i18n( "Minimum keep size:" ), sb_minimumKeepSize);

    cb_AutoResume = new QCheckBox( this );
    cb_AutoResume->setWhatsThis( i18n("<p>Transfers will be auto-resumed.<p>") );
    connect(cb_AutoResume, SIGNAL(toggled(bool)), SLOT(configChanged()));
    miscLayout->addRow(i18n( "Enable auto-resuming" ), cb_AutoResume);

    mainLayout->addStretch( 1 );
}

KIOPreferences::~KIOPreferences()
{
}

void KIOPreferences::load()
{
  KProtocolManager proto;

  sb_serverResponse->setRange( MIN_TIMEOUT_VALUE, MAX_TIMEOUT_VALUE );
  sb_serverConnect->setRange( MIN_TIMEOUT_VALUE, MAX_TIMEOUT_VALUE );

  sb_serverResponse->setValue( proto.responseTimeout() );
  sb_serverConnect->setValue( proto.connectTimeout() );

  KConfig config( "kio_ftprc", KConfig::NoGlobals );
  cb_ftpEnablePasv->setChecked( !config.group("").readEntry( "DisablePassiveMode", false ) );
  cb_ftpMarkPartial->setChecked( config.group("").readEntry( "MarkPartial", true ) );

  sb_minimumKeepSize->setValue( proto.minimumKeepSize() );
  cb_AutoResume->setChecked( proto.autoResume() );

  emit changed( false );
}

void KIOPreferences::save()
{
  KSaveIOConfig::setResponseTimeout( sb_serverResponse->value() );
  KSaveIOConfig::setConnectTimeout( sb_serverConnect->value() );

  KConfig config("kio_ftprc", KConfig::NoGlobals);
  config.group("").writeEntry( "DisablePassiveMode", !cb_ftpEnablePasv->isChecked() );
  config.group("").writeEntry( "MarkPartial", cb_ftpMarkPartial->isChecked() );
  config.sync();

  KSaveIOConfig::setMinimumKeepSize( sb_minimumKeepSize->value() );
  KSaveIOConfig::setAutoResume( cb_AutoResume->isChecked() );

  KSaveIOConfig::updateRunningIOSlaves(this);

  emit changed( false );
}

void KIOPreferences::defaults()
{
  sb_serverResponse->setValue( DEFAULT_RESPONSE_TIMEOUT );
  sb_serverConnect->setValue( DEFAULT_CONNECT_TIMEOUT );

  cb_ftpEnablePasv->setChecked( true );
  cb_ftpMarkPartial->setChecked( true );

  sb_minimumKeepSize->setValue( DEFAULT_MINIMUM_KEEP_SIZE );
  cb_AutoResume->setChecked( true );

  emit changed(true);
}

QString KIOPreferences::quickHelp() const
{
  return i18n("<h1>Network Preferences</h1>Here you can define"
              " the behavior of KDE programs when using Internet"
              " and network connections. If you experience timeouts"
              " or use a modem to connect to the Internet, you might"
              " want to adjust these settings." );
}

#include "moc_netpref.cpp"
