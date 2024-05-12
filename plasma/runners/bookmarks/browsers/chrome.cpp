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


#include <QFileInfo>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <KDebug>

#include "chrome.h"
#include "browsers/findprofile.h"
#include "favicon.h"

class ProfileBookmarks {
public:
    ProfileBookmarks(const Profile &profile) : m_profile(profile) {}
    inline QList<QVariantMap> bookmarks() { return m_bookmarks; }
    inline Profile profile() { return m_profile; }
    void tearDown() { m_profile.favicon()->teardown(); m_bookmarks.clear(); }
    void add(const QVariantMap &bookmarkEntry) { m_bookmarks << bookmarkEntry; }
private:
    Profile m_profile;
    QList<QVariantMap> m_bookmarks;
};

Chrome::Chrome( FindProfile* findProfile, QObject* parent )
    : QObject(parent)
{
    foreach (const Profile &profile, findProfile->find()) {
        m_profileBookmarks << new ProfileBookmarks(profile);
    }
}

Chrome::~Chrome()
{
    foreach (ProfileBookmarks *profileBookmark, m_profileBookmarks) {
        delete profileBookmark;
    }
}

QList<BookmarkMatch> Chrome::match(const QString &term, bool addEveryThing)
{
    QList<BookmarkMatch> results;
    foreach (ProfileBookmarks *profileBookmarks, m_profileBookmarks) {
        results << match(term, addEveryThing, profileBookmarks);
    }
    return results;
}

QList<BookmarkMatch> Chrome::match(const QString &term, bool addEveryThing, ProfileBookmarks *profileBookmarks)
{
    QList<BookmarkMatch> results;
    foreach(const QVariantMap &bookmark, profileBookmarks->bookmarks()) {
        const QString url = bookmark.value("url").toString();
        const QString name = bookmark.value("name").toString();
        kDebug() << "Match" << name << url;
        BookmarkMatch bookmarkMatch(profileBookmarks->profile().favicon(), term, name, url);
        bookmarkMatch.addTo(results, addEveryThing);
    }
    return results;
}

void Chrome::prepare()
{
    foreach (ProfileBookmarks *profileBookmarks, m_profileBookmarks) {
        Profile profile = profileBookmarks->profile();
        QFile bookmarksFile(profile.path());
        if (!bookmarksFile.open(QFile::ReadOnly)) {
            continue;
        }

        QJsonDocument jsondoc = QJsonDocument::fromJson(bookmarksFile.readAll());
        if (jsondoc.isNull()) {
            kDebug() << "Null profile document" << jsondoc.errorString();
            continue;
        }
        const QVariantMap root = jsondoc.toVariant().toMap();
        if (!root.contains("roots")) {
            kDebug() << "No roots in" << profile.path();
            continue;
        }
        kDebug() << "Filling entries from" << profile.path();
        const QVariantMap roots = root.value("roots").toMap();
        foreach (const QVariant &folder, roots.values()) {
            parseFolder(folder.toMap(), profileBookmarks);
        }
        profile.favicon()->prepare();
    }
}

void Chrome::teardown()
{
    foreach(ProfileBookmarks *profileBookmarks, m_profileBookmarks) {
        profileBookmarks->tearDown();
    }
}

void Chrome::parseFolder(const QVariantMap &entry, ProfileBookmarks *profile)
{
    QVariantList children = entry.value("children").toList();
    foreach (const QVariant &child, children) {
        QVariantMap entry = child.toMap();
        if(entry.value("type").toString() == "folder") {
            parseFolder(entry, profile);
        } else {
            // kDebug() << "Adding entry" << entry;
            profile->add(entry);
        }
    }
}
