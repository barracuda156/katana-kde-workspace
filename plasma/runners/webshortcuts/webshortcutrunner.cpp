/*
 *   Copyright (C) 2007 Teemu Rytilahti <tpr@iki.fi>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
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

#include "webshortcutrunner.h"

#include <KDebug>
#include <KLocale>
#include <KMimeType>
#include <KServiceTypeTrader>
#include <KStandardDirs>
#include <KSycoca>
#include <KToolInvocation>
#include <KUrl>
#include <KUriFilter>

#include <QtDBus/QtDBus>

WebshortcutRunner::WebshortcutRunner(QObject *parent, const QVariantList& args)
    : Plasma::AbstractRunner(parent, args)
{
    setObjectName( QLatin1String("Web Shortcut" ));
    setIgnoredTypes(Plasma::RunnerContext::Directory | Plasma::RunnerContext::File | Plasma::RunnerContext::Executable);

    // Listen for KUriFilter plugin config changes and update state...
    QDBusConnection sessionDbus = QDBusConnection::sessionBus();
    sessionDbus.connect(QString(), "/", "org.kde.KUriFilterPlugin",
                        "configure", this, SLOT(readFiltersConfig()));

    readFiltersConfig();
}

void WebshortcutRunner::readFiltersConfig()
{
    KUriFilterData filterData (QLatin1String(":q"));
    filterData.setSearchFilteringOptions(KUriFilterData::RetrieveAvailableSearchProvidersOnly);
    if (KUriFilter::self()->filterSearchUri(filterData, KUriFilter::NormalTextFilter)) {
        m_delimiter = filterData.searchTermSeparator();
    }

    // kDebug() << "keyword delimiter:" << m_delimiter;
    // kDebug() << "search providers:" << filterData.preferredSearchProviders();

    QList<Plasma::RunnerSyntax> syns;
    Q_FOREACH (const QString &provider, filterData.preferredSearchProviders()) {
        // kDebug() << "checking out" << provider;
        Plasma::RunnerSyntax s(filterData.queryForPreferredSearchProvider(provider), /*":q:",*/
                              i18n("Opens \"%1\" in a web browser with the query :q:.", provider));
        syns << s;
    }

    setSyntaxes(syns);
}

void WebshortcutRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();

    if (term.length() < 3 || !term.contains(m_delimiter)) {
        return;
    }

    // kDebug() << "checking term" << term;

    const int delimIndex = term.indexOf(m_delimiter);
    if (delimIndex == term.length() - 1) {
        return;
    }

    if (!context.isValid()) {
        kDebug() << "invalid context";
        return;
    }

    KUriFilterData filterData(term);
    if (!KUriFilter::self()->filterSearchUri(filterData, KUriFilter::WebShortcutFilter)) {
        return;
    }

    Plasma::QueryMatch match(this);
    match.setRelevance(0.9);
    match.setData(filterData.uri().url());
    match.setId("WebShortcut:" + term.left(delimIndex));
    match.setIcon(KIcon(filterData.iconName()));
    match.setText(i18n("Search %1 for %2", filterData.searchProvider(), filterData.searchTerm()));
    context.addMatch(match);
}

void WebshortcutRunner::run(const Plasma::QueryMatch &match)
{
    QString location = match.data().toString();

    // kDebug() << location;
    if (!location.isEmpty()) {
        KToolInvocation::self()->invokeBrowser(location);
    }
}

#include "moc_webshortcutrunner.cpp"
