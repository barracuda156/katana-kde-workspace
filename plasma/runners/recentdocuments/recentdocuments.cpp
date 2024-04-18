/*
 *   Copyright 2008 Sebastian Kügler <sebas@kde.org>
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

#include "recentdocuments.h"

#include <QMimeData>

#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KDirWatch>
#include <KIcon>
#include <KRun>
#include <KRecentDocument>

RecentDocuments::RecentDocuments(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    setObjectName(QLatin1String("Recent Documents"));
    addSyntax(
        Plasma::RunnerSyntax(
            ":q:",
            i18n("Looks for documents recently used with names matching :q:.")
        )
    );

    loadRecentDocuments();
    // listen for changes to the list of recent documents
    KDirWatch *recentDocWatch = new KDirWatch(this);
    recentDocWatch->addDir(KRecentDocument::recentDocumentDirectory());
    connect(
        recentDocWatch, SIGNAL(dirty(QString)),
        this, SLOT(loadRecentDocuments())
    );
}

RecentDocuments::~RecentDocuments()
{
}

void RecentDocuments::loadRecentDocuments()
{
    //kDebug() << "Refreshing recent documents.";
    m_recentdocuments = KRecentDocument::recentDocuments();
}


void RecentDocuments::match(Plasma::RunnerContext &context)
{
    if (m_recentdocuments.isEmpty()) {
        return;
    }

    const QString term = context.query();
    if (term.length() < 3) {
        return;
    }

    foreach (const QString &document, m_recentdocuments) {
        if (!context.isValid()) {
            return;
        }

        if (document.contains(term, Qt::CaseInsensitive)) {
            KConfig _config(document, KConfig::SimpleConfig);
            KConfigGroup config(&_config, "Desktop Entry" );
            Plasma::QueryMatch match(this);
            match.setRelevance(1.0);
            match.setIcon(KIcon(config.readEntry("Icon")));
            match.setData(document); // TODO: Read URL[$e], or can we just pass the path to the .desktop file?
            match.setText(config.readEntry("Name"));
            match.setSubtext(i18n("Recent Document"));
            context.addMatch(match);
        }
    }
}

void RecentDocuments::run(const Plasma::QueryMatch &match)
{
    const QString url = match.data().toString();
    kDebug() << "Opening Recent Document" << url;
    new KRun(url, 0);
}

QMimeData* RecentDocuments::mimeDataForMatch(const Plasma::QueryMatch *match)
{
    QMimeData* result = new QMimeData();
    const QString url = match->data().toString();
    QList<QUrl> urls;
    urls << QUrl(url);
    result->setUrls(urls);
    result->setText(url);
    return result;
}

#include "moc_recentdocuments.cpp"
