/*
 *   Copyright 2007 Glenn Ergeerts <glenn.ergeerts@telenet.be>
 *   Copyright 2012 Glenn Ergeerts <marco.gulino@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "bookmarksrunner.h"
#include "browser.h"

#include <QList>
#include <QStack>
#include <QDir>

#include <KMimeType>
#include <KMimeTypeTrader>
#include <KToolInvocation>
#include <KUrl>
#include <KStandardDirs>
#include <KDebug>
#include "bookmarkmatch.h"
#include "browserfactory.h"

BookmarksRunner::BookmarksRunner( QObject* parent, const QVariantList &args )
    : Plasma::AbstractRunner(parent, args),
    m_browser(nullptr),
    m_browserFactory(new BrowserFactory(this))
{
    kDebug() << "Creating BookmarksRunner";
    setObjectName( QLatin1String("Bookmarks" ));
    addSyntax(
        Plasma::RunnerSyntax(i18nc("list of all web browser bookmarks", "bookmarks"),
        i18n("List all web browser bookmarks"))
    );
    addSyntax(Plasma::RunnerSyntax(":q:", i18n("Finds web browser bookmarks matching :q:.")));
}

BookmarksRunner::~BookmarksRunner()
{
    m_browser->teardown();
}

void BookmarksRunner::init()
{
    Plasma::AbstractRunner::init();

    m_browser = m_browserFactory->find(findBrowserName(), this);
    m_browser->prepare();
}

void BookmarksRunner::match(Plasma::RunnerContext &context)
{
    if(! m_browser) return;
    const QString term = context.query();
    if (term.length() < 3) {
        return;
    }

    bool allBookmarks = (term.compare(i18nc("list of all konqueror bookmarks", "bookmarks"), Qt::CaseInsensitive) == 0);
    if (!allBookmarks) {
        // how about untranslated match?
        allBookmarks = (term.compare(QLatin1String("bookmarks"), Qt::CaseInsensitive) == 0);
    }
                                     
    QList<BookmarkMatch> matches = m_browser->match(term, allBookmarks);
    foreach(BookmarkMatch match, matches) {
        if(!context.isValid())
            return;
        context.addMatch(match.asQueryMatch(this));
    }
}

QString BookmarksRunner::findBrowserName()
{
    //HACK find the default browser
    KConfigGroup config(KSharedConfig::openConfig("kdeglobals"), QLatin1String("General") );
    QString exec = config.readPathEntry(QLatin1String("BrowserApplication"), QString());
    kDebug() << "Found exec string" << exec;
    if (exec.isEmpty()) {
        KService::Ptr service = KMimeTypeTrader::self()->preferredService("text/html");
        if (service) {
            exec = service->exec();
        }
    }

    kDebug() << "Found executable" << exec << "as default browser";
    return exec;

}

void BookmarksRunner::run(const Plasma::QueryMatch &action)
{
    const QString term = action.data().toString();
    // transforms URLs like "kde.org" to "http://kde.org"
    const KUrl url = KUrl::fromUserInput(term);
    KToolInvocation::self()->invokeBrowser(url.url());
}

QMimeData* BookmarksRunner::mimeDataForMatch(const Plasma::QueryMatch &match)
{
    QMimeData * result = new QMimeData();
    QList<QUrl> urls;
    urls << QUrl(match.data().toString());
    result->setUrls(urls);

    result->setText(match.data().toString());

    return result;
}

#include "moc_bookmarksrunner.cpp"
