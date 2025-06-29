/*  -*- c-basic-offset: 2 -*-

    kshorturifilter.h

    This file is part of the KDE project
    Copyright (C) 2000 Dawit Alemayehu <adawit@kde.org>
    Copyright (C) 2000 Malte Starostik <starosti@zedat.fu-berlin.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "kshorturifilter.h"

#include <QtCore/QDir>
#include <QtDBus/QtDBus>

#include <kdebug.h>
#include <klocalizedstring.h>
#include <kpluginfactory.h>
#include <kprotocolinfo.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kmimetype.h>
#include <kuser.h>
#include <kde_file.h>

#define QL1S(x) QLatin1String(x)
#define QL1C(x) QLatin1Char(x)

 /**
  * IMPORTANT:
  *  If you change anything here, please run the regression test
  *  ../tests/kurifiltertest.
  *
  *  If you add anything here, make sure to add a corresponding
  *  test code to ../tests/kurifiltertest.
  */

typedef QMap<QString,QString> EntryMap;

static QRegExp sEnvVarExp (QL1S("\\$[a-zA-Z_][a-zA-Z0-9_]*"));

static bool isPotentialShortURL(const QString& cmd)
{
    // Host names and IPv4 address...
    if (cmd.contains(QLatin1Char('.'))) {
        return true;
    }

    // IPv6 Address...
    if (cmd.startsWith(QLatin1Char('[')) && cmd.contains(QLatin1Char(':'))) {
        return true;
    }

    return false;
}

static QString removeArgs( const QString& _cmd )
{
  QString cmd( _cmd );

  if( cmd[0] != '\'' && cmd[0] != '"' )
  {
    // Remove command-line options (look for first non-escaped space)
    int spacePos = 0;

    do
    {
      spacePos = cmd.indexOf( ' ', spacePos+1 );
    } while ( spacePos > 1 && cmd[spacePos - 1] == '\\' );

    if( spacePos > 0 )
    {
      cmd = cmd.left( spacePos );
      //kDebug(7023) << "spacePos=" << spacePos << " returning " << cmd;
    }
  }

  return cmd;
}

KShortUriFilter::KShortUriFilter( QObject *parent, const QVariantList & /*args*/ )
                :KUriFilterPlugin( "kshorturifilter", parent )
{
    QDBusConnection::sessionBus().connect(QString(), "/", "org.kde.KUriFilterPlugin",
                                "configure", this, SLOT(configure()));
    configure();
}

bool KShortUriFilter::filterUri( KUriFilterData& data ) const
{
 /*
  * Here is a description of how the shortURI deals with the supplied
  * data.  First it expands any environment variable settings and then
  * deals with special shortURI cases. These special cases are the "smb:"
  * URL scheme which is very specific to KDE, "#" and "##" which are
  * shortcuts for man:/ and info:/ protocols respectively. It then handles
  * local files.  Then it checks to see if the URL is valid and one that is
  * supported by KDE's IO system.  If all the above checks fails, it simply
  * lookups the URL in the user-defined list and returns without filtering
  * if it is not found. TODO: the user-defined table is currently only manually
  * hackable and is missing a config dialog.
  */

  KUrl url = data.uri();
  QString cmd = data.typedString();

  /*
   * Extra match to be performed on the path for mailto:foo@bar.baz, the
   * pattern for it requires fully qualified mail link (without the scheme) but
   * the URL includes the scheme
   */
  QString cmd2 = url.path();

  // WORKAROUND: Allow the use of '@' in the username component of a URL since
  // other browsers such as firefox in their infinite wisdom allow such blatant
  // violations of RFC 3986. BR# 69326/118413.
  if (cmd.count(QLatin1Char('@')) > 1) {
    const int lastIndex = cmd.lastIndexOf(QLatin1Char('@'));
    // Percent encode all but the last '@'.
    QString encodedCmd = QUrl::toPercentEncoding(cmd.left(lastIndex), ":/");
    encodedCmd += cmd.mid(lastIndex);
    KUrl u (encodedCmd);
    if (u.isValid()) {
      cmd = encodedCmd;
      url = u;
    }
  }

  const bool isMalformed = !url.isValid();
  QString protocol = url.protocol();

  kDebug(7023) << cmd;

  // Fix misparsing of "foo:80", QUrl thinks "foo" is the protocol and "80" is the path.
  // However, be careful not to do that for valid hostless URLs, e.g. file:///foo!
  if (!protocol.isEmpty() && url.host().isEmpty() && !url.path().isEmpty()
      && cmd.contains(':') && !KProtocolInfo::protocols().contains(protocol)) {
    protocol.clear();
  }

  //kDebug(7023) << "url=" << url << "cmd=" << cmd << "isMalformed=" << isMalformed;

  if (!isMalformed &&
      (protocol.length() == 4) &&
      (protocol != QLatin1String("http")) &&
      (protocol[0]=='h') &&
      (protocol[1]==protocol[2]) &&
      (protocol[3]=='p'))
  {
     // Handle "encrypted" URLs like: h++p://www.kde.org
     url.setScheme( QLatin1String("http"));
     setFilteredUri( data, url);
     setUriType( data, KUriFilterData::NetProtocol );
     return true;
  }

  // TODO: Make this a bit more intelligent for Minicli! There
  // is no need to make comparisons if the supplied data is a local
  // executable and only the argument part, if any, changed! (Dawit)
  // You mean caching the last filtering, to try and reuse it, to save stat()s? (David)

  // Detect UNC style (aka windows SMB) URLs
  if ( cmd.startsWith( QLatin1String( "\\\\") ) )
  {
    // make sure path is unix style
    cmd.replace('\\', '/');
    cmd.prepend( QLatin1String( "smb:" ) );
    setFilteredUri( data, KUrl( cmd ));
    setUriType( data, KUriFilterData::NetProtocol );
    return true;
  }

  bool expanded = false;

  // Expanding shortcut to HOME URL...
  QString path;
  QString ref;
  QString query;
  QString nameFilter;

  if (KUrl::isRelativeUrl(cmd) && QDir::isRelativePath(cmd)) {
     path = cmd;
     //kDebug(7023) << "path=cmd=" << path;
  } else {
    if (url.isLocalFile())
    {
      //kDebug(7023) << "hasFragment=" << url.hasFragment();
      // Split path from ref/query
      // but not for "/tmp/a#b", if "a#b" is an existing file,
      // or for "/tmp/a?b" (#58990)
      if( ( url.hasFragment() || !url.query().isEmpty() )
           && !url.path().endsWith(QL1S("/")) ) // /tmp/?foo is a namefilter, not a query
      {
        path = url.path();
        ref = url.fragment();
        //kDebug(7023) << "isLocalFile set path to " << stringDetails( path );
        //kDebug(7023) << "isLocalFile set ref to " << stringDetails( ref );
        query = url.query();
        if (path.isEmpty() && url.hasHost())
          path = '/';
      }
      else
      {
        path = cmd;
        //kDebug(7023) << "(2) path=cmd=" << path;
      }
    }
  }

  if( path[0] == '~' )
  {
    int slashPos = path.indexOf('/');
    if( slashPos == -1 )
      slashPos = path.length();
    if( slashPos == 1 )   // ~/
    {
      path.replace ( 0, 1, QDir::homePath() );
    }
    else // ~username/
    {
      const QString userName (path.mid( 1, slashPos-1 ));
      KUser user (userName);
      if( user.isValid() && !user.homeDir().isEmpty())
      {
        path.replace (0, slashPos, user.homeDir());
      }
      else
      {
        if (user.isValid()) {
          setErrorMsg(data, i18n("<qt><b>%1</b> does not have a home folder.</qt>", userName));
        } else {
          setErrorMsg(data, i18n("<qt>There is no user called <b>%1</b>.</qt>", userName));
        }
        setUriType( data, KUriFilterData::Error );
        // Always return true for error conditions so
        // that other filters will not be invoked !!
        return true;
      }
    }
    expanded = true;
  }
  else if ( path[0] == '$' ) {
    // Environment variable expansion.
    if ( sEnvVarExp.indexIn( path ) == 0 )
    {
      QByteArray exp = qgetenv( path.mid( 1, sEnvVarExp.matchedLength() - 1 ).toLocal8Bit().data() );
      if(! exp.isNull())
      {
        path.replace( 0, sEnvVarExp.matchedLength(), QString::fromLocal8Bit(exp.constData()) );
        expanded = true;
      }
    }
  }

  if ( expanded || cmd.startsWith( '/' ) )
  {
    // Look for #ref again, after $ and ~ expansion
    // Can't use KUrl here, setPath would escape it...
    const int pos = path.indexOf('#');
    if ( pos > -1 )
    {
      const QString newPath = path.left( pos );
      if ( QFile::exists( newPath ) )
      {
        ref = path.mid( pos + 1 );
        path = newPath;
        //kDebug(7023) << "Extracted ref: path=" << path << " ref=" << ref;
      }
    }
  }


  bool isLocalFullPath = (!path.isEmpty() && path[0] == '/');

  // Checking for local resource match...
  // Determine if "uri" is an absolute path to a local resource  OR
  // A local resource with a supplied absolute path in KUriFilterData
  const QString abs_path = data.absolutePath();

  const bool canBeAbsolute = (protocol.isEmpty() && !abs_path.isEmpty());
  const bool canBeLocalAbsolute = (canBeAbsolute && abs_path[0] =='/' && !isMalformed);
  bool exists = false;

  /*kDebug(7023) << "abs_path=" << abs_path
               << "protocol=" << protocol
               << "canBeAbsolute=" << canBeAbsolute
               << "canBeLocalAbsolute=" << canBeLocalAbsolute
               << "isLocalFullPath=" << isLocalFullPath;*/

  KDE_struct_stat buff;
  if ( canBeLocalAbsolute )
  {
    QString abs = QDir::cleanPath( abs_path );
    // combine absolute path (abs_path) and relative path (cmd) into abs_path
    int len = path.length();
    if( (len==1 && path[0]=='.') || (len==2 && path[0]=='.' && path[1]=='.') )
        path += '/';
    //kDebug(7023) << "adding " << abs << " and " << path;
    abs = QDir::cleanPath(abs + '/' + path);
    //kDebug(7023) << "checking whether " << abs << " exists.";
    // Check if it exists
    if( KDE::stat( abs, &buff ) == 0 )
    {
      path = abs; // yes -> store as the new cmd
      exists = true;
      isLocalFullPath = true;
    }
  }

  if (isLocalFullPath && !exists && !isMalformed) {
    exists = ( KDE::stat( path, &buff ) == 0 );

    if ( !exists ) {
      // Support for name filter (/foo/*.txt)
      // If the app using this filter doesn't support it, well, it'll simply error out itself
      int lastSlash = path.lastIndexOf( '/' );
      if ( lastSlash > -1 && path.indexOf( ' ', lastSlash ) == -1 ) // no space after last slash, otherwise it's more likely command-line arguments
      {
        QString fileName = path.mid( lastSlash + 1 );
        QString testPath = path.left( lastSlash + 1 );
        if ( ( fileName.indexOf( '*' ) != -1 || fileName.indexOf( '[' ) != -1 || fileName.indexOf( '?' ) != -1 )
           && KDE::stat( testPath, &buff ) == 0 )
        {
          nameFilter = fileName;
          //kDebug(7023) << "Setting nameFilter to " << nameFilter;
          path = testPath;
          exists = true;
        }
      }
    }
  }

  //kDebug(7023) << "path =" << path << " isLocalFullPath=" << isLocalFullPath << " exists=" << exists;
  if( exists )
  {
    KUrl u;
    u.setPath(path);
    //kDebug(7023) << "ref=" << stringDetails(ref) << " query=" << stringDetails(query);
    u.setFragment(ref);
    u.setQuery(query);

    // Can be abs path to file or directory, or to executable with args
    bool isDir = S_ISDIR( buff.st_mode );
    if( !isDir && access ( QFile::encodeName(path).data(), X_OK) == 0 )
    {
      //kDebug(7023) << "Abs path to EXECUTABLE";
      setFilteredUri( data, u );
      setUriType( data, KUriFilterData::Executable );
      return true;
    }

    // Open "uri" as file:/xxx if it is a non-executable local resource.
    if( isDir || S_ISREG( buff.st_mode ) )
    {
      //kDebug(7023) << "Abs path as local file or directory";
      if ( !nameFilter.isEmpty() )
        u.setFileName( nameFilter );
      setFilteredUri( data, u );
      setUriType( data, ( isDir ) ? KUriFilterData::LocalDir : KUriFilterData::LocalFile );
      return true;
    }

    // Should we return LOCAL_FILE for non-regular files too?
    kDebug(7023) << "File found, but not a regular file nor dir... socket?";
  }

  if( data.checkForExecutables())
  {
    // Let us deal with possible relative URLs to see
    // if it is executable under the user's $PATH variable.
    // We try hard to avoid parsing any possible command
    // line arguments or options that might have been supplied.
    QString exe = removeArgs( cmd );
    //kDebug(7023) << "findExe with" << exe;

    if (!KStandardDirs::findExe( exe ).isNull() )
    {
      //kDebug(7023) << "EXECUTABLE  exe=" << exe;
      setFilteredUri( data, KUrl::fromPath( exe ));
      // check if we have command line arguments
      if( exe != cmd )
          setArguments(data, cmd.right(cmd.length() - exe.length()));
      setUriType( data, KUriFilterData::Executable );
      return true;
    }
  }

  // Process URLs of known and supported protocols so we don't have
  // to resort to the pattern matching scheme below which can possibly
  // slow things down...
  if ( !isMalformed && !isLocalFullPath && !protocol.isEmpty() )
  {
    //kDebug(7023) << "looking for protocol " << protocol;
    if ( KProtocolInfo::isKnownProtocol( protocol ) )
    {
      setFilteredUri( data, url );
      setUriType( data, KUriFilterData::NetProtocol );
      return true;
    }
  }

  // Short url matches
  if ( !cmd.contains( ' ' ) )
  {
    // Okay this is the code that allows users to supply custom matches for
    // specific URLs using Qt's regexp class. This is hard-coded for now.
    // TODO: Make configurable at some point...
    Q_FOREACH(const URLHint& hint, m_urlHints)
    {
      if (hint.regexp.indexIn(cmd) == 0)
      {
        //kDebug(7023) << "match - prepending" << hint.prepend;
        const QString cmdStr = hint.prepend + cmd;
        KUrl url(cmdStr);
        if (KProtocolInfo::isKnownProtocol(url))
        {
          setFilteredUri( data, url );
          setUriType( data, hint.type );
          return true;
        }
      }
    }

    // NOTE: match for mailto:foo@bar.baz
    Q_FOREACH(const URLHint& hint, m_urlHints)
    {
      if (hint.regexp.indexIn(cmd2) == 0)
      {
        //kDebug(7023) << "match - prepending" << hint.prepend;
        const QString cmdStr = hint.prepend + cmd2;
        KUrl url(cmdStr);
        if (KProtocolInfo::isKnownProtocol(url))
        {
          setFilteredUri( data, url );
          setUriType( data, hint.type );
          return true;
        }
      }
    }

    // No protocol and not malformed means a valid short URL such as kde.org or
    // user@192.168.0.1. However, it might also be valid only because it lacks
    // the scheme component, e.g. www.kde,org (illegal ',' before 'org'). The
    // check below properly deciphers the difference between the two and sends
    // back the proper result.
    if (protocol.isEmpty() && isPotentialShortURL(cmd))
    {
      QString urlStr = data.defaultUrlScheme();
      if (urlStr.isEmpty())
          urlStr = m_strDefaultUrlScheme;

      const int index = urlStr.indexOf(QL1C(':'));
      if (index == -1 || !KProtocolInfo::isKnownProtocol(urlStr.left(index)))
        urlStr += QL1S("://");
      urlStr += cmd;

      KUrl url (urlStr);
      if (url.isValid())
      {
        setFilteredUri(data, url);
        setUriType(data, KUriFilterData::NetProtocol);
      }
      else if (KProtocolInfo::isKnownProtocol(url.protocol()))
      {
        setFilteredUri(data, data.uri());
        setUriType(data, KUriFilterData::Error);
      }
      return true;
    }
  }

  // If we previously determined that the URL might be a file,
  // and if it doesn't exist, then error
  if( isLocalFullPath && !exists )
  {
    KUrl u;
    u.setPath(path);
    u.setFragment(ref);

    //kDebug(7023) << "fileNotFound -> ERROR";
    setErrorMsg( data, i18n( "<qt>The file or folder <b>%1</b> does not exist.</qt>", data.uri().prettyUrl() ) );
    setUriType( data, KUriFilterData::Error );
    return true;
  }

  // If we reach this point, we cannot filter this thing so simply return false
  // so that other filters, if present, can take a crack at it.
  return false;
}

KCModule* KShortUriFilter::configModule( QWidget*, const char* ) const
{
    return 0; //new KShortUriOptions( parent, name );
}

QString KShortUriFilter::configName() const
{
//    return i18n("&ShortURLs"); we don't have a configModule so no need for a configName that confuses translators
    return KUriFilterPlugin::configName();
}

void KShortUriFilter::configure()
{
  KConfig config( objectName() + QL1S( "rc"), KConfig::NoGlobals );
  KConfigGroup cg( config.group("") );

  m_strDefaultUrlScheme = cg.readEntry( "DefaultProtocol", QString("http://") );
  const EntryMap patterns = config.entryMap( QL1S("Pattern") );
  const EntryMap protocols = config.entryMap( QL1S("Protocol") );
  KConfigGroup typeGroup(&config, "Type");

  for( EntryMap::ConstIterator it = patterns.begin(); it != patterns.end(); ++it )
  {
    QString protocol = protocols[it.key()];
    if (!protocol.isEmpty())
    {
      int type = typeGroup.readEntry(it.key(), -1);
      if (type > -1 && type <= KUriFilterData::Unknown)
        m_urlHints.append( URLHint(it.value(), protocol, static_cast<KUriFilterData::UriTypes>(type) ) );
      else
        m_urlHints.append( URLHint(it.value(), protocol) );
    }
  }
}

K_PLUGIN_FACTORY(KShortUriFilterFactory, registerPlugin<KShortUriFilter>();)
K_EXPORT_PLUGIN(KShortUriFilterFactory("kcmkurifilt"))

#include "moc_kshorturifilter.cpp"
