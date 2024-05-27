/***************************************************************************
 *   Copyright (C) 2011 by Peter Penz <peter.penz19@gmail.com>             *
 *   Copyright (C) 2011 by Frank Reininghaus <frank78ac@googlemail.com>    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include <qtest_kde.h>

#include <QtCore/qmimedata.h>

#include <KDirLister>
#include <kio/job.h>

#include "kitemviews/kfileitemmodel.h"
#include "kitemviews/private/kfileitemmodeldirlister.h"
#include "testdir.h"

void myMessageOutput(QtMsgType type, const char* msg)
{
    switch (type) {
    case QtDebugMsg:
        break;
    case QtWarningMsg:
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s\n", msg);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s\n", msg);
        abort();
    default:
       break;
    }
}

namespace {
    const int DefaultTimeout = 5000;
};

Q_DECLARE_METATYPE(KItemRange)
Q_DECLARE_METATYPE(KItemRangeList)
Q_DECLARE_METATYPE(QList<int>)

class KFileItemModelTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testDefaultRoles();
    void testDefaultSortRole();
    void testDefaultGroupedSorting();
    void testNewItems();
    void testRemoveItems();
    void testDirLoadingCompleted();
    void testSetData();
    void testChangeSortRole();
    void testResortAfterChangingName();
    void testModelConsistencyWhenInsertingItems();
    void testItemRangeConsistencyWhenInsertingItems();
    void testIndexForKeyboardSearch();
    void testNameFilter();
    void testEmptyPath();
    void testRemoveHiddenItems();
    void testNameRoleGroups();
    void testChangeRolesForFilteredItems();
    void testChangeSortRoleWhileFiltering();
    void testRefreshFilteredItems();
    void testDeleteFileMoreThanOnce();

private:
    QStringList itemsInModel() const;

private:
    KFileItemModel* m_model;
    TestDir* m_testDir;
};

void KFileItemModelTest::init()
{
    // The item-model tests result in a huge number of debugging
    // output from kdelibs. Only show critical and fatal messages.
    qInstallMsgHandler(myMessageOutput);

    qRegisterMetaType<KItemRange>("KItemRange");
    qRegisterMetaType<KItemRangeList>("KItemRangeList");
    qRegisterMetaType<KFileItemList>("KFileItemList");

    m_testDir = new TestDir();
    m_model = new KFileItemModel();
    m_model->m_dirLister->setAutoUpdate(false);

    // Reduce the timer interval to make the test run faster.
    m_model->m_resortAllItemsTimer->setInterval(0);
}

void KFileItemModelTest::cleanup()
{
    delete m_model;
    m_model = 0;

    delete m_testDir;
    m_testDir = 0;
}

void KFileItemModelTest::testDefaultRoles()
{
    const QSet<QByteArray> roles = m_model->roles();
    QCOMPARE(roles.count(), 3);
    QVERIFY(roles.contains("text"));
    QVERIFY(roles.contains("isDir"));
    QVERIFY(roles.contains("isLink"));
}

void KFileItemModelTest::testDefaultSortRole()
{
    QCOMPARE(m_model->sortRole(), QByteArray("text"));

    QStringList files;
    files << "c.txt" << "a.txt" << "b.txt";

    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    QCOMPARE(m_model->count(), 3);
    QCOMPARE(m_model->data(0)["text"].toString(), QString("a.txt"));
    QCOMPARE(m_model->data(1)["text"].toString(), QString("b.txt"));
    QCOMPARE(m_model->data(2)["text"].toString(), QString("c.txt"));
}

void KFileItemModelTest::testDefaultGroupedSorting()
{
    QCOMPARE(m_model->groupedSorting(), false);
}

void KFileItemModelTest::testNewItems()
{
    QStringList files;
    files << "a.txt" << "b.txt" << "c.txt";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    QCOMPARE(m_model->count(), 3);

    QVERIFY(m_model->isConsistent());
}

void KFileItemModelTest::testRemoveItems()
{
    m_testDir->createFile("a.txt");
    m_testDir->createFile("b.txt");
    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(m_model->count(), 2);
    QVERIFY(m_model->isConsistent());

    m_testDir->removeFile("a.txt");
    m_model->m_dirLister->updateDirectory();
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsRemoved(KItemRangeList)), DefaultTimeout));
    QCOMPARE(m_model->count(), 1);
    QVERIFY(m_model->isConsistent());
}

void KFileItemModelTest::testDirLoadingCompleted()
{
    QSignalSpy loadingCompletedSpy(m_model, SIGNAL(directoryLoadingCompleted()));
    QSignalSpy itemsInsertedSpy(m_model, SIGNAL(itemsInserted(KItemRangeList)));
    QSignalSpy itemsRemovedSpy(m_model, SIGNAL(itemsRemoved(KItemRangeList)));

    m_testDir->createFiles(QStringList() << "a.txt" << "b.txt" << "c.txt");

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(directoryLoadingCompleted()), DefaultTimeout));
    QCOMPARE(loadingCompletedSpy.count(), 1);
    QCOMPARE(itemsInsertedSpy.count(), 1);
    QCOMPARE(itemsRemovedSpy.count(), 0);
    QCOMPARE(m_model->count(), 3);

    m_testDir->createFiles(QStringList() << "d.txt" << "e.txt");
    m_model->m_dirLister->updateDirectory();
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(directoryLoadingCompleted()), DefaultTimeout));
    QCOMPARE(loadingCompletedSpy.count(), 2);
    QCOMPARE(itemsInsertedSpy.count(), 2);
    QCOMPARE(itemsRemovedSpy.count(), 0);
    QCOMPARE(m_model->count(), 5);

    m_testDir->removeFile("a.txt");
    m_testDir->createFile("f.txt");
    m_model->m_dirLister->updateDirectory();
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(directoryLoadingCompleted()), DefaultTimeout));
    QCOMPARE(loadingCompletedSpy.count(), 3);
    QCOMPARE(itemsInsertedSpy.count(), 3);
    QCOMPARE(itemsRemovedSpy.count(), 1);
    QCOMPARE(m_model->count(), 5);

    m_testDir->removeFile("b.txt");
    m_model->m_dirLister->updateDirectory();
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsRemoved(KItemRangeList)), DefaultTimeout));
    QCOMPARE(loadingCompletedSpy.count(), 4);
    QCOMPARE(itemsInsertedSpy.count(), 3);
    QCOMPARE(itemsRemovedSpy.count(), 2);
    QCOMPARE(m_model->count(), 4);

    QVERIFY(m_model->isConsistent());
}

void KFileItemModelTest::testSetData()
{
    m_testDir->createFile("a.txt");

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    QHash<QByteArray, QVariant> values;
    values.insert("customRole1", "Test1");
    values.insert("customRole2", "Test2");

    QSignalSpy itemsChangedSpy(m_model, SIGNAL(itemsChanged(KItemRangeList,QSet<QByteArray>)));
    m_model->setData(0, values);
    QCOMPARE(itemsChangedSpy.count(), 1);

    values = m_model->data(0);
    QCOMPARE(values.value("customRole1").toString(), QString("Test1"));
    QCOMPARE(values.value("customRole2").toString(), QString("Test2"));
    QVERIFY(m_model->isConsistent());
}

void KFileItemModelTest::testChangeSortRole()
{
    QCOMPARE(m_model->sortRole(), QByteArray("text"));

    QStringList files;
    files << "a.txt" << "b.jpg" << "c.txt";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.jpg" << "c.txt");

    // Now: sort by type.
    QSignalSpy spyItemsMoved(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)));
    m_model->setSortRole("type");
    QCOMPARE(m_model->sortRole(), QByteArray("type"));
    QVERIFY(!spyItemsMoved.isEmpty());

    // The actual order of the files might depend on the translation of the
    // result of KFileItem::mimeComment() in the user's language.
    QStringList version1;
    version1 << "b.jpg" << "a.txt" << "c.txt";

    QStringList version2;
    version2 << "a.txt" << "c.txt" << "b.jpg";

    const bool ok1 = (itemsInModel() == version1);
    const bool ok2 = (itemsInModel() == version2);

    QVERIFY(ok1 || ok2);
}

void KFileItemModelTest::testResortAfterChangingName()
{
    // We sort by size in a directory where all files have the same size.
    // Therefore, the files are sorted by their names.
    m_model->setSortRole("size");

    QStringList files;
    files << "a.txt" << "b.txt" << "c.txt";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.txt");

    // We rename a.txt to d.txt. Even though the size has not changed at all,
    // the model must re-sort the items.
    QHash<QByteArray, QVariant> data;
    data.insert("text", "d.txt");
    m_model->setData(0, data);

    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "b.txt" << "c.txt" << "d.txt");

    // We rename d.txt back to a.txt using the dir lister's refreshItems() signal.
    const KFileItem fileItemD = m_model->fileItem(2);
    KFileItem fileItemA = fileItemD;
    KUrl urlA = fileItemA.url();
    urlA.setFileName("a.txt");
    fileItemA.setUrl(urlA);

    m_model->slotRefreshItems(QList<QPair<KFileItem, KFileItem> >() << qMakePair(fileItemD, fileItemA));

    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.txt");
}

void KFileItemModelTest::testModelConsistencyWhenInsertingItems()
{
    //QSKIP("Temporary disabled", SkipSingle);

    // KFileItemModel prevents that inserting a punch of items sequentially
    // results in an itemsInserted()-signal for each item. Instead internally
    // a timeout is given that collects such operations and results in only
    // one itemsInserted()-signal. However in this test we want to stress
    // KFileItemModel to do a lot of insert operation and hence decrease
    // the timeout to 1 millisecond.
    m_testDir->createFile("1");
    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(m_model->count(), 1);

    // Insert 10 items for 20 times. After each insert operation the model consistency
    // is checked.
    QSet<int> insertedItems;
    for (int i = 0; i < 20; ++i) {
        QSignalSpy spy(m_model, SIGNAL(itemsInserted(KItemRangeList)));

        for (int j = 0; j < 10; ++j) {
            int itemName = qrand();
            while (insertedItems.contains(itemName)) {
                itemName = qrand();
            }
            insertedItems.insert(itemName);

            m_testDir->createFile(QString::number(itemName));
        }

        m_model->m_dirLister->updateDirectory();
        if (spy.count() == 0) {
            QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
        }

        QVERIFY(m_model->isConsistent());
    }

    QCOMPARE(m_model->count(), 201);
}

void KFileItemModelTest::testItemRangeConsistencyWhenInsertingItems()
{
    QStringList files;
    files << "B" << "E" << "G";
    m_testDir->createFiles(files);

    // Due to inserting the 3 items one item-range with index == 0 and
    // count == 3 must be given
    QSignalSpy spy1(m_model, SIGNAL(itemsInserted(KItemRangeList)));
    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    QCOMPARE(spy1.count(), 1);
    QList<QVariant> arguments = spy1.takeFirst();
    KItemRangeList itemRangeList = arguments[0].value<KItemRangeList>();
    QCOMPARE(itemRangeList, KItemRangeList() << KItemRange(0, 3));

    // The indexes of the item-ranges must always be related to the model before
    // the items have been inserted. Having:
    //   0 1 2
    //   B E G
    // and inserting A, C, D, F the resulting model will be:
    //   0 1 2 3 4 5 6
    //   A B C D E F G
    // and the item-ranges must be:
    //   index: 0, count: 1 for A
    //   index: 1, count: 2 for B, C
    //   index: 2, count: 1 for G

    files.clear();
    files << "A" << "C" << "D" << "F";
    m_testDir->createFiles(files);

    QSignalSpy spy2(m_model, SIGNAL(itemsInserted(KItemRangeList)));
    m_model->m_dirLister->updateDirectory();
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    QCOMPARE(spy2.count(), 1);
    arguments = spy2.takeFirst();
    itemRangeList = arguments[0].value<KItemRangeList>();
    QCOMPARE(itemRangeList, KItemRangeList() << KItemRange(0, 1) << KItemRange(1, 2) << KItemRange(2, 1));
}

void KFileItemModelTest::testIndexForKeyboardSearch()
{
    QStringList files;
    files << "a" << "aa" << "Image.jpg" << "Image.png" << "Text" << "Text1" << "Text2" << "Text11";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    // Search from index 0
    QCOMPARE(m_model->indexForKeyboardSearch("a", 0), 0);
    QCOMPARE(m_model->indexForKeyboardSearch("aa", 0), 1);
    QCOMPARE(m_model->indexForKeyboardSearch("i", 0), 2);
    QCOMPARE(m_model->indexForKeyboardSearch("image", 0), 2);
    QCOMPARE(m_model->indexForKeyboardSearch("image.jpg", 0), 2);
    QCOMPARE(m_model->indexForKeyboardSearch("image.png", 0), 3);
    QCOMPARE(m_model->indexForKeyboardSearch("t", 0), 4);
    QCOMPARE(m_model->indexForKeyboardSearch("text", 0), 4);
    QCOMPARE(m_model->indexForKeyboardSearch("text1", 0), 5);
    QCOMPARE(m_model->indexForKeyboardSearch("text2", 0), 6);
    QCOMPARE(m_model->indexForKeyboardSearch("text11", 0), 7);

    // Start a search somewhere in the middle
    QCOMPARE(m_model->indexForKeyboardSearch("a", 1), 1);
    QCOMPARE(m_model->indexForKeyboardSearch("i", 3), 3);
    QCOMPARE(m_model->indexForKeyboardSearch("t", 5), 5);
    QCOMPARE(m_model->indexForKeyboardSearch("text1", 6), 7);

    // Test searches that go past the last item back to index 0
    QCOMPARE(m_model->indexForKeyboardSearch("a", 2), 0);
    QCOMPARE(m_model->indexForKeyboardSearch("i", 7), 2);
    QCOMPARE(m_model->indexForKeyboardSearch("image.jpg", 3), 2);
    QCOMPARE(m_model->indexForKeyboardSearch("text2", 7), 6);

    // Test searches that yield no result
    QCOMPARE(m_model->indexForKeyboardSearch("aaa", 0), -1);
    QCOMPARE(m_model->indexForKeyboardSearch("b", 0), -1);
    QCOMPARE(m_model->indexForKeyboardSearch("image.svg", 0), -1);
    QCOMPARE(m_model->indexForKeyboardSearch("text3", 0), -1);
    QCOMPARE(m_model->indexForKeyboardSearch("text3", 5), -1);

    // Test upper case searches (note that search is case insensitive)
    QCOMPARE(m_model->indexForKeyboardSearch("A", 0), 0);
    QCOMPARE(m_model->indexForKeyboardSearch("aA", 0), 1);
    QCOMPARE(m_model->indexForKeyboardSearch("TexT", 5), 5);
    QCOMPARE(m_model->indexForKeyboardSearch("IMAGE", 4), 2);

    // TODO: Maybe we should also test keyboard searches in directories which are not sorted by Name?
}

void KFileItemModelTest::testNameFilter()
{
    QStringList files;
    files << "A1" << "A2" << "Abc" << "Bcd" << "Cde";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));

    m_model->setNameFilter("A"); // Shows A1, A2 and Abc
    QCOMPARE(m_model->count(), 3);

    m_model->setNameFilter("A2"); // Shows only A2
    QCOMPARE(m_model->count(), 1);

    m_model->setNameFilter("A2"); // Shows only A1
    QCOMPARE(m_model->count(), 1);

    m_model->setNameFilter("Bc"); // Shows "Abc" and "Bcd"
    QCOMPARE(m_model->count(), 2);

    m_model->setNameFilter("bC"); // Shows "Abc" and "Bcd"
    QCOMPARE(m_model->count(), 2);

    m_model->setNameFilter(QString()); // Shows again all items
    QCOMPARE(m_model->count(), 5);
}

/**
 * Verifies that we do not crash when adding a KFileItem with an empty path.
 */
void KFileItemModelTest::testEmptyPath()
{
    QSet<QByteArray> roles;
    roles.insert("text");
    m_model->setRoles(roles);

    const KUrl emptyUrl;
    QVERIFY(emptyUrl.path().isEmpty());

    const KUrl url("file:///test/");

    KFileItemList items;
    items << KFileItem(emptyUrl, QString(), KFileItem::Unknown) << KFileItem(url, QString(), KFileItem::Unknown);
    m_model->slotItemsAdded(items);
    m_model->slotCompleted();
}

/**
 * Verify that removing hidden files and folders from the model does not
 * result in a crash, see https://bugs.kde.org/show_bug.cgi?id=314046
 */
void KFileItemModelTest::testRemoveHiddenItems()
{
    m_testDir->createDir(".a");
    m_testDir->createDir(".b");
    m_testDir->createDir("c");
    m_testDir->createDir("d");
    m_testDir->createFiles(QStringList() << ".f" << ".g" << "h" << "i");

    QSignalSpy spyItemsInserted(m_model, SIGNAL(itemsInserted(KItemRangeList)));
    QSignalSpy spyItemsRemoved(m_model, SIGNAL(itemsRemoved(KItemRangeList)));

    m_model->setShowHiddenFiles(true);
    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << ".a" << ".b" << "c" << "d" <<".f" << ".g" << "h" << "i");
    QCOMPARE(spyItemsInserted.count(), 1);
    QCOMPARE(spyItemsRemoved.count(), 0);
    KItemRangeList itemRangeList = spyItemsInserted.takeFirst().at(0).value<KItemRangeList>();
    QCOMPARE(itemRangeList, KItemRangeList() << KItemRange(0, 8));

    m_model->setShowHiddenFiles(false);
    QTest::qWait(2000);
    QCOMPARE(itemsInModel(), QStringList() << "c" << "d" << "h" << "i");
    QCOMPARE(spyItemsInserted.count(), 0);
    QCOMPARE(spyItemsRemoved.count(), 1);
    itemRangeList = spyItemsRemoved.takeFirst().at(0).value<KItemRangeList>();
    QCOMPARE(itemRangeList, KItemRangeList() << KItemRange(0, 2) << KItemRange(4, 2));

    m_model->setShowHiddenFiles(true);
    QTest::qWait(2000);
    QCOMPARE(itemsInModel(), QStringList() << ".a" << ".b" << "c" << "d" <<".f" << ".g" << "h" << "i");
    QCOMPARE(spyItemsInserted.count(), 1);
    QCOMPARE(spyItemsRemoved.count(), 0);
    itemRangeList = spyItemsInserted.takeFirst().at(0).value<KItemRangeList>();
    QCOMPARE(itemRangeList, KItemRangeList() << KItemRange(0, 2) << KItemRange(2, 2));

    m_model->clear();
    QCOMPARE(itemsInModel(), QStringList());
    QCOMPARE(spyItemsInserted.count(), 0);
    QCOMPARE(spyItemsRemoved.count(), 1);
    itemRangeList = spyItemsRemoved.takeFirst().at(0).value<KItemRangeList>();
    QCOMPARE(itemRangeList, KItemRangeList() << KItemRange(0, 8));

    // Hiding hidden files makes the dir lister emit its itemsDeleted signal.
    // Verify that this does not make the model crash.
    m_model->setShowHiddenFiles(false);
}

void KFileItemModelTest::testNameRoleGroups()
{
    QStringList files;
    files << "b.txt" << "c.txt" << "d.txt" << "e.txt";

    m_testDir->createFiles(files);

    m_model->setGroupedSorting(true);
    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "b.txt" << "c.txt" << "d.txt" << "e.txt");

    QList<QPair<int, QVariant> > expectedGroups;
    expectedGroups << QPair<int, QVariant>(0, QLatin1String("B"));
    expectedGroups << QPair<int, QVariant>(1, QLatin1String("C"));
    expectedGroups << QPair<int, QVariant>(2, QLatin1String("D"));
    expectedGroups << QPair<int, QVariant>(3, QLatin1String("E"));
    QCOMPARE(m_model->groups(), expectedGroups);

    // Rename d.txt to a.txt.
    QHash<QByteArray, QVariant> data;
    data.insert("text", "a.txt");
    m_model->setData(2, data);
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsMoved(KItemRange,QList<int>)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.txt" << "e.txt");

    expectedGroups.clear();
    expectedGroups << QPair<int, QVariant>(0, QLatin1String("A"));
    expectedGroups << QPair<int, QVariant>(1, QLatin1String("B"));
    expectedGroups << QPair<int, QVariant>(2, QLatin1String("C"));
    expectedGroups << QPair<int, QVariant>(3, QLatin1String("E"));
    QCOMPARE(m_model->groups(), expectedGroups);

    // Rename c.txt to d.txt.
    data.insert("text", "d.txt");
    m_model->setData(2, data);
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(groupsChanged()), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "d.txt" << "e.txt");

    expectedGroups.clear();
    expectedGroups << QPair<int, QVariant>(0, QLatin1String("A"));
    expectedGroups << QPair<int, QVariant>(1, QLatin1String("B"));
    expectedGroups << QPair<int, QVariant>(2, QLatin1String("D"));
    expectedGroups << QPair<int, QVariant>(3, QLatin1String("E"));
    QCOMPARE(m_model->groups(), expectedGroups);

    // Change d.txt back to c.txt, but this time using the dir lister's refreshItems() signal.
    const KFileItem fileItemD = m_model->fileItem(2);
    KFileItem fileItemC = fileItemD;
    KUrl urlC = fileItemC.url();
    urlC.setFileName("c.txt");
    fileItemC.setUrl(urlC);

    m_model->slotRefreshItems(QList<QPair<KFileItem, KFileItem> >() << qMakePair(fileItemD, fileItemC));
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(groupsChanged()), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.txt" << "e.txt");

    expectedGroups.clear();
    expectedGroups << QPair<int, QVariant>(0, QLatin1String("A"));
    expectedGroups << QPair<int, QVariant>(1, QLatin1String("B"));
    expectedGroups << QPair<int, QVariant>(2, QLatin1String("C"));
    expectedGroups << QPair<int, QVariant>(3, QLatin1String("E"));
    QCOMPARE(m_model->groups(), expectedGroups);
}

void KFileItemModelTest::testChangeRolesForFilteredItems()
{
    QSet<QByteArray> modelRoles = m_model->roles();
    modelRoles << "owner";
    m_model->setRoles(modelRoles);

    QStringList files;
    files << "a.txt" << "aa.txt" << "aaa.txt";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "aa.txt" << "aaa.txt");

    for (int index = 0; index < m_model->count(); ++index) {
        // All items should have the "text" and "owner" roles, but not "group".
        QVERIFY(m_model->data(index).contains("text"));
        QVERIFY(m_model->data(index).contains("owner"));
        QVERIFY(!m_model->data(index).contains("group"));
    }

    // Add a filter, such that only "aaa.txt" remains in the model.
    m_model->setNameFilter("aaa");
    QCOMPARE(itemsInModel(), QStringList() << "aaa.txt");

    // Add the "group" role.
    modelRoles << "group";
    m_model->setRoles(modelRoles);

    // Modify the filter, such that "aa.txt" reappears, and verify that all items have the expected roles.
    m_model->setNameFilter("aa");
    QCOMPARE(itemsInModel(), QStringList() << "aa.txt" << "aaa.txt");

    for (int index = 0; index < m_model->count(); ++index) {
        // All items should have the "text", "owner", and "group" roles.
        QVERIFY(m_model->data(index).contains("text"));
        QVERIFY(m_model->data(index).contains("owner"));
        QVERIFY(m_model->data(index).contains("group"));
    }

    // Remove the "owner" role.
    modelRoles.remove("owner");
    m_model->setRoles(modelRoles);

    // Clear the filter, and verify that all items have the expected roles
    m_model->setNameFilter(QString());
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "aa.txt" << "aaa.txt");

    for (int index = 0; index < m_model->count(); ++index) {
        // All items should have the "text" and "group" roles, but now "owner".
        QVERIFY(m_model->data(index).contains("text"));
        QVERIFY(!m_model->data(index).contains("owner"));
        QVERIFY(m_model->data(index).contains("group"));
    }
}

void KFileItemModelTest::testChangeSortRoleWhileFiltering()
{
    KFileItemList items;

    KIO::UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, 0100000);    // S_IFREG might not be defined on non-Unix platforms.
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 07777);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 0);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0);
    entry.insert(KIO::UDSEntry::UDS_GROUP, "group");
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, 0);

    entry.insert(KIO::UDSEntry::UDS_NAME, "a.txt");
    entry.insert(KIO::UDSEntry::UDS_USER, "user-b");
    items.append(KFileItem(entry, m_testDir->url()));

    entry.insert(KIO::UDSEntry::UDS_NAME, "b.txt");
    entry.insert(KIO::UDSEntry::UDS_USER, "user-c");
    items.append(KFileItem(entry, m_testDir->url()));

    entry.insert(KIO::UDSEntry::UDS_NAME, "c.txt");
    entry.insert(KIO::UDSEntry::UDS_USER, "user-a");
    items.append(KFileItem(entry, m_testDir->url()));

    m_model->slotItemsAdded(items);
    m_model->slotCompleted();

    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.txt");

    // Add a filter.
    m_model->setNameFilter("a");
    QCOMPARE(itemsInModel(), QStringList() << "a.txt");

    // Sort by "owner".
    m_model->setSortRole("owner");

    // Clear the filter, and verify that the items are sorted correctly.
    m_model->setNameFilter(QString());
    QCOMPARE(itemsInModel(), QStringList() << "c.txt" << "a.txt" << "b.txt");
}

void KFileItemModelTest::testRefreshFilteredItems()
{
    QStringList files;
    files << "a.txt" << "b.txt" << "c.jpg" << "d.jpg";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.jpg" << "d.jpg");

    const KFileItem fileItemC = m_model->fileItem(2);

    // Show only the .txt files.
    m_model->setNameFilter(".txt");
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt");

    // Rename one of the .jpg files.
    KFileItem fileItemE = fileItemC;
    KUrl urlE = fileItemE.url();
    urlE.setFileName("e.jpg");
    fileItemE.setUrl(urlE);

    m_model->slotRefreshItems(QList<QPair<KFileItem, KFileItem> >() << qMakePair(fileItemC, fileItemE));

    // Show all files again, and verify that the model has updated the file name.
    m_model->setNameFilter(QString());
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "d.jpg" << "e.jpg");
}

void KFileItemModelTest::testDeleteFileMoreThanOnce()
{
    QStringList files;
    files << "a.txt" << "b.txt" << "c.txt" << "d.txt";
    m_testDir->createFiles(files);

    m_model->loadDirectory(m_testDir->url());
    QVERIFY(QTest::kWaitForSignal(m_model, SIGNAL(itemsInserted(KItemRangeList)), DefaultTimeout));
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "b.txt" << "c.txt" << "d.txt");

    const KFileItem fileItemB = m_model->fileItem(1);

    // Tell the model that a list of items has been deleted, where "b.txt" appears twice in the list.
    KFileItemList list;
    list << fileItemB << fileItemB;
    m_model->slotItemsDeleted(list);

    QVERIFY(m_model->isConsistent());
    QCOMPARE(itemsInModel(), QStringList() << "a.txt" << "c.txt" << "d.txt");
}

QStringList KFileItemModelTest::itemsInModel() const
{
    QStringList items;
    for (int i = 0; i < m_model->count(); i++) {
        items << m_model->fileItem(i).text();
    }
    return items;
}

QTEST_KDEMAIN(KFileItemModelTest, NoGUI)

#include "kfileitemmodeltest.moc"
