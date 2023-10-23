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

#include "favicontest.h"

#include <qtest_kde.h>
#include <kmimetype.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <knetworkmanager.h>

#include <QImageReader>
#include <QElapsedTimer>
#include <QEventLoop>

QTEST_KDEMAIN(FavIconTest, NoGUI)

static const char s_icoPath[] = KDESRCDIR "designer.ico";
static const int s_waitTime = 20000; // in ms

static bool checkICOReadable()
{
    QFile icofile(s_icoPath);
    icofile.open(QFile::ReadOnly);
    QImageReader icoimagereader(&icofile);
    if (!icoimagereader.canRead()) {
        qWarning("%s", qPrintable(icoimagereader.errorString()));
        return false;
    }
    qDebug("ICO is readable");
    return true;
}

static void cleanCache()
{
    QDir faviconsdir(KGlobal::dirs()->saveLocation("cache", QString::fromLatin1("favicons/")));
    // qDebug() << Q_FUNC_INFO << faviconsdir.absolutePath();
    foreach (const QFileInfo &favinfo, faviconsdir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        qDebug() << "Removing cached icon" << favinfo.filePath();
        QFile::remove(favinfo.filePath());
    }
}

FavIconTest::FavIconTest()
    : QObject(),
    m_iconChanged(false),
    m_favIconModule("org.kde.kded", "/modules/favicons", QDBusConnection::sessionBus())
{
    connect(
        &m_favIconModule, SIGNAL(iconChanged(QString,QString)),
        this, SLOT(slotIconChanged(QString,QString))
    );
}

void FavIconTest::initTestCase()
{
    KNetworkManager knetworkmanager;
    if (knetworkmanager.status() != KNetworkManager::ConnectedStatus) {
        QSKIP("Network status is not connected", SkipAll);
    }

    if (!checkICOReadable()) {
        QSKIP("ICO not readable", SkipAll);
    }
}

void FavIconTest::testIconForURL_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("icon");

    QTest::newRow("https://www.google.com")
        << QString::fromLatin1("https://www.google.com") << QString::fromLatin1("favicons/www.google.com");
    // NOTE: 256x256
    QTest::newRow("https://ci.appveyor.com/foo?bar=baz")
        << QString::fromLatin1("https://ci.appveyor.com/foo?bar=baz") << QString::fromLatin1("favicons/ci.appveyor.com");
     // NOTE: favicon.png
    QTest::newRow("https://www.wpoven.com/")
        << QString::fromLatin1("https://www.wpoven.com/") << QString::fromLatin1("favicons/www.wpoven.com");
#if 0
    // handshake failure (IP addresses and certificates do not get along)
    QTest::newRow("https://140.82.121.3/")
        << QString::fromLatin1("https://140.82.121.3/") << QString::fromLatin1("favicons/140.82.121.3");
#endif
}

void FavIconTest::testIconForURL()
{
    QFETCH(QString, url);
    QFETCH(QString, icon);

    cleanCache();

    const KUrl favUrl(url);
    QString favicon = KMimeType::favIconForUrl(favUrl);
    QCOMPARE(favicon, QString());

    m_iconChanged = false;
    m_favIconModule.downloadUrlIcon(url);
    qDebug("called downloadUrlIcon, waiting..");
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();
    while (!m_iconChanged && elapsedTimer.elapsed() < s_waitTime) {
        QTest::qWait(400);
    }
    QVERIFY(m_iconChanged);

    favicon = KMimeType::favIconForUrl(favUrl);
    QCOMPARE(favicon, icon);
}

void FavIconTest::slotIconChanged(const QString &url, const QString &iconName)
{
    // qDebug() << Q_FUNC_INFO << url << iconName;
    m_iconChanged = true;
    m_url = url;
    m_iconName = iconName;
}

#include "moc_favicontest.cpp"
