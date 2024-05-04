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

#include "kdirshareimpl.h"
#include "kdirshare.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QBuffer>
#include <QPixmap>
#include <QHostInfo>
#include <kicon.h>
#include <klocale.h>
#include <kmimetype.h>
#include <krandom.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kdebug.h>

static const QDir::SortFlags s_dirsortflags = (QDir::Name | QDir::DirsFirst);
// AVAHI_LABEL_MAX - 3 (for dots) - 1 (for null terminator)
static const int s_sharenamelimit = 60;

static quint16 getPort(const quint16 portmin, const quint16 portmax)
{
    if (portmin == portmax) {
        return portmax;
    }
    quint16 portnumber = 0;
    while (portnumber < portmin) {
        portnumber = quint16(KRandom::randomMax(portmax));
        Q_ASSERT(portnumber <= portmax);
    }
    return portnumber;
}

static QString getFileMIME(const QString &filepath)
{
    const KMimeType::Ptr kmimetypeptr = KMimeType::findByUrl(KUrl(filepath));
    if (!kmimetypeptr.isNull()) {
        return kmimetypeptr->name();
    }
    return QString::fromLatin1("application/octet-stream");
}

static QString getTitle(const QString &dirpath)
{
    const QString sharename = QDir(QDir(dirpath).absolutePath()).dirName();
    QString title = i18n("KDirShare@%1 (%2)", QHostInfo::localHostName(), sharename);
    if (title.size() > s_sharenamelimit) {
        title = title.left(s_sharenamelimit);
        title.append(QLatin1String("..."));
    }
    return title;
}

// for reference:
// https://www.w3schools.com/css/css_link.asp
static QByteArray sheetForContent()
{
    QByteArray stylesheet;
    const QPalette palette = KGlobalSettings::createApplicationPalette();
    const QByteArray foregroundcolor = palette.color(QPalette::Active, QPalette::Foreground).name().toLatin1();
    const QByteArray backgroundcolor = palette.color(QPalette::Active, QPalette::Background).name().toLatin1();
    stylesheet.append("  <style>\n");
    stylesheet.append("body {\n");
    stylesheet.append("  color: " + foregroundcolor + ";\n");
    stylesheet.append("  background-color: " + backgroundcolor + ";\n");
    stylesheet.append("}\n");
    const QByteArray linkcolor = palette.color(QPalette::Active, QPalette::Link).name().toLatin1();
    stylesheet.append("a:link {\n");
    stylesheet.append("  color: " + linkcolor + ";\n");
    stylesheet.append("}\n");
    const QByteArray visitedlinkcolor = palette.color(QPalette::Active, QPalette::LinkVisited).name().toLatin1();
    stylesheet.append("a:visited {\n");
    stylesheet.append("  color: " + visitedlinkcolor + ";\n");
    stylesheet.append("}\n");
    stylesheet.append("  </style>\n");
    return stylesheet;
}

static QByteArray footerForContent()
{
    QByteArray data;
    data.append("    <footer>\n");
    data.append("      <hr>\n");
    data.append("      <center>Copyright (C) 2022 Ivailo Monev <a href=\"mailto:xakepa10@gmail.com\">xakepa10@gmail.com</a></center>\n");
    data.append("    </footer>\n");
    return data;
}

static QByteArray contentForError(const QString &path, const ushort status)
{
    const QString pathtitle = getTitle(path);

    QByteArray data;
    data.append("<html>\n");
    data.append(sheetForContent());
    data.append("  <body>\n");
    data.append("    <title>");
    data.append(pathtitle.toUtf8());
    data.append("</title>\n");
    data.append("    <hr>\n");
    switch (status) {
        case 404: {
            data.append("    <center>404 Not Found</center>\n");
            break;
        }
        case 500: {
            data.append("    <center>500 Internal Server Error</center>\n");
            break;
        }
        default: {
            Q_ASSERT(false);
            break;
        }
    }
    data.append(footerForContent());
    data.append("  </body>\n");
    data.append("</html>");
    return data;
}

static QByteArray contentForFile(const QString &basedir, const QFileInfo &fileinfo)
{
    QByteArray data;

    const QString fullpath = fileinfo.absoluteFilePath();

    // chromium does weird stuff if the link starts with two slashes - removes, the host and
    // port part of the link (or rather does not prepend them) and converts the first directory
    // to lower-case
    QString cleanpath = QDir::cleanPath(fullpath.mid(basedir.size()));
    if (fileinfo.isDir()) {
        cleanpath.append(QDir::separator());
    }
    // qDebug() << Q_FUNC_INFO << fullpath << basedir << cleanpath;

    data.append("      <tr>\n");

    const bool isdotdot = (fileinfo.fileName() == QLatin1String(".."));
    if (isdotdot) {
        const QString fileicon = QString::fromLatin1("<img src=\"/kdirshareicons/go-previous\" width=\"20\" height=\"20\">");
        data.append("        <td>");
        data.append(fileicon.toUtf8());
        data.append("</td>\n");
    } else {
        const QString fileicon = QString::fromLatin1("<img src=\"/kdirshareicons/%1\" width=\"20\" height=\"20\">").arg(KMimeType::iconNameForUrl(KUrl(fullpath)));
        data.append("        <td>");
        data.append(fileicon.toUtf8());
        data.append("</td>\n");
    }

    data.append("        <td><a href=\"");
    // NOTE: browsers produce bogus URLs for mixed (non-percentage and percentage-encoded) paths,
    // also browsers decode percentage URLs and pass it back which for local files may be invalid
    data.append(cleanpath.toUtf8());
    data.append("\">");
    data.append(fileinfo.fileName().toUtf8());
    data.append("</a></td>\n");

    data.append("        <td>");
    if (!isdotdot) {
        const QString filemime = getFileMIME(fullpath);
        data.append(filemime.toUtf8());
    }
    data.append("</td>\n");

    data.append("        <td>");
    if (fileinfo.isFile()) {
        const QString filesize = KGlobal::locale()->formatByteSize(fileinfo.size(), 1);
        data.append(filesize.toUtf8());
    }
    data.append("</td>\n");

    data.append("      </tr>\n");

    return data;
}

static QByteArray contentForMatch(const QString &path, const QString &match)
{
    const QString pathtitle = getTitle(path);

    QByteArray data;
    data.append("<html>\n");
    data.append(sheetForContent());
    data.append("  <body>\n");
    data.append("    <title>");
    data.append(pathtitle.toUtf8());
    data.append("</title>\n");
    data.append("    <hr>\n");
    data.append("    <table>\n");
    data.append("      <tr>\n");
    data.append("        <th></th>\n"); // icon
    data.append("        <th>Filename</th>\n");
    data.append("        <th>MIME</th>\n");
    data.append("        <th>Size</th>\n");
    data.append("      </tr>\n");
    const QDir::Filters dirfilters = (QDir::Files | QDir::NoDotAndDotDot);
    QDirIterator diriterator(path, QStringList() << match, dirfilters, QDirIterator::Subdirectories);
    while (diriterator.hasNext()) {
        (void)diriterator.next();
        const QFileInfo dirinfo = diriterator.fileInfo();
        if (dirinfo.isDir()) {
            // sub-directory
            continue;
        }
        data.append(contentForFile(path, dirinfo));
    }
    data.append("    </table>\n");
    data.append(footerForContent());
    data.append("  </body>\n");
    data.append("</html>");
    return data;
}

static QByteArray contentForDirectory(const QString &path, const QString &basedir)
{
    const QString pathtitle = getTitle(path);

    QByteArray data;
    data.append("<html>\n");
    data.append(sheetForContent());
    data.append("  <form action=\"/kdirsharesearch.html\">\n");
    data.append("    <label for=\"match\">Search for:</label>\n");
    data.append("    <input type=\"text\" name=\"match\" value=\"\">\n");
    data.append("    <input type=\"submit\" value=\"Search\">\n");
    data.append("  </form>\n");
    data.append("  <body>\n");
    data.append("    <title>");
    data.append(pathtitle.toUtf8());
    data.append("</title>\n");
    data.append("    <hr>\n");
    data.append("    <table>\n");
    data.append("      <tr>\n");
    data.append("        <th></th>\n"); // icon
    data.append("        <th>Filename</th>\n");
    data.append("        <th>MIME</th>\n");
    data.append("        <th>Size</th>\n");
    data.append("      </tr>\n");
    QDir::Filters dirfilters = (QDir::Files | QDir::AllDirs | QDir::NoDot);
    if (QDir::cleanPath(path) == QDir::cleanPath(basedir)) {
        dirfilters = (QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);
    }
    QDir dir(path);
    foreach (const QFileInfo &fileinfo, dir.entryInfoList(dirfilters, s_dirsortflags)) {
        data.append(contentForFile(basedir, fileinfo));
    }
    data.append("    </table>\n");
    data.append(footerForContent());
    data.append("  </body>\n");
    data.append("</html>");
    return data;
}

KDirServer::KDirServer(QObject *parent)
    : KHTTP(parent)
{
}

bool KDirServer::setDirectory(const QString &directory)
{
    if (!QDir(m_directory).exists()) {
        return false;
    }
    m_directory = directory;
    return true;
}

// for reference:
// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
void KDirServer::respond(const QByteArray &url, QByteArray *outdata,
                         ushort *outhttpstatus, KHTTPHeaders *outheaders,
                         QString *outfilepath)
{
    // qDebug() << Q_FUNC_INFO << url;
    const QString normalizedpath = QUrl::fromPercentEncoding(url);
    QFileInfo pathinfo(m_directory + QLatin1Char('/') + normalizedpath);
    // qDebug() << Q_FUNC_INFO << normalizedpath << pathinfo.filePath();
    if (normalizedpath == QLatin1String("/favicon.ico")
        || normalizedpath.startsWith(QLatin1String("/kdirshareicons/"))) {
        const bool isfavicon = (normalizedpath == QLatin1String("/favicon.ico"));
        QPixmap iconpixmap;
        QByteArray iconformat;
        QByteArray iconmime;
        if (isfavicon) {
            iconpixmap = KIcon("folder-html").pixmap(32);
            iconformat = "ICO";
            iconmime = "image/vnd.microsoft.icon";
        } else {
            iconpixmap = KIcon(normalizedpath.mid(16)).pixmap(20);
            iconformat = "PNG";
            iconmime = "image/PNG";
        }
        QBuffer iconbuffer;
        iconbuffer.open(QBuffer::WriteOnly);
        if (!iconpixmap.save(&iconbuffer, iconformat)) {
            kWarning() << "Could not save image";
            outdata->append(contentForError(normalizedpath, 500));
            *outhttpstatus = 500;
            outheaders->insert("Content-Type", "text/html; charset=UTF-8");
        } else {
            outdata->append(iconbuffer.data());
            *outhttpstatus = 200;
            outheaders->insert("Content-Type", iconmime);
        }
    } else if (normalizedpath.startsWith(QLatin1String("/kdirsharesearch.html"))) {
        const QString match = QUrl::fromEncoded(url).queryItemValue("match");
        *outhttpstatus = 200;
        outheaders->insert("Content-Type", "text/html; charset=UTF-8");
        outdata->append(contentForMatch(m_directory, match));
    } else if (pathinfo.isDir()) {
        *outhttpstatus = 200;
        outheaders->insert("Content-Type", "text/html; charset=UTF-8");
        outdata->append(contentForDirectory(pathinfo.filePath(), m_directory));
    } else if (pathinfo.isFile()) {
        const QString filemime = getFileMIME(pathinfo.filePath());
        *outhttpstatus = 200;
        outheaders->insert("Content-Type", filemime.toAscii());
        outfilepath->append(pathinfo.filePath());
    } else {
        outdata->append(contentForError(normalizedpath, 404));
        *outhttpstatus = 404;
        outheaders->insert("Content-Type", "text/html; charset=UTF-8");
    }
}


KDirShareImpl::KDirShareImpl(QObject *parent)
    : QThread(parent),
    m_directory(QDir::currentPath()),
    m_portmin(s_kdirshareportmin),
    m_portmax(s_kdirshareportmax),
    m_starting(false),
    m_kdirserver(nullptr)
{
    connect(
        this, SIGNAL(unblock()),
        this, SLOT(slotUnblock())
    );
    connect(
        this, SIGNAL(serveError(QString)),
        this, SLOT(slotServeError(QString))
    );
}

KDirShareImpl::~KDirShareImpl()
{
    m_kdnssd.unpublishService();
    delete m_kdirserver;
}

QString KDirShareImpl::serve(const QString &dirpath,
                             const quint16 portmin, const quint16 portmax,
                             const QString &username, const QString &password)
{
    // qDebug() << Q_FUNC_INFO << dirpath << portmin << portmax << username << password;
    m_directory = dirpath;
    m_portmin = portmin;
    m_portmax = portmax;
    m_user = username;
    m_password = password;
    m_error.clear();
    m_starting = true;
    m_error.clear();
    start();
    while (m_starting) {
        QCoreApplication::processEvents();
    }
    return m_error;
}
QString KDirShareImpl::directory() const
{
    return m_directory;
}

quint16 KDirShareImpl::portMin() const
{
    return m_portmin;
}

quint16 KDirShareImpl::portMax() const
{
    return m_portmax;
}

QString KDirShareImpl::user() const
{
    return m_user;
}

QString KDirShareImpl::password() const
{
    return m_password;
}

QString KDirShareImpl::address() const
{
    if (!m_kdirserver) {
        return QString();
    }
    return m_kdirserver->address();
}

void KDirShareImpl::run()
{
    m_kdirserver = new KDirServer();
    m_kdirserver->setServerID(QString::fromLatin1("KDirShare"));

    if (!m_kdirserver->setDirectory(m_directory)) {
        emit serveError(i18n("Directory does not exist: %1", m_directory));
        emit unblock();
        return;
    }
    if (!m_user.isEmpty() && !m_password.isEmpty()) {
        if (!m_kdirserver->setAuthenticate(m_user.toUtf8(), m_password.toUtf8())) {
            emit serveError(i18n("Could not set authentication: %1", m_kdirserver->errorString()));
            emit unblock();
            return;
        }
    }
    const quint16 port = getPort(m_portmin, m_portmax);
    if (!m_kdirserver->start(QHostAddress(QHostAddress::Any), port)) {
        emit serveError(i18n("Could not serve: %1", m_kdirserver->errorString()));
        emit unblock();
        return;
    }
    if (!m_kdnssd.publishService("_http._tcp", port, getTitle(m_directory))) {
        m_kdirserver->stop();
        emit serveError(i18n("Could not publish service: %1", m_kdnssd.errorString()));
        emit unblock();
        return;
    }
    emit unblock();

    exec();
}

void KDirShareImpl::slotUnblock()
{
    m_starting = false;
}

void KDirShareImpl::slotServeError(const QString &error)
{
    m_error = error;
}

#include "moc_kdirshareimpl.cpp"
