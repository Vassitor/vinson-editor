#include "session/DocumentHistory.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace vinson;

class DocumentHistoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesDeduplicatesAndLimitsRecentFiles();
    void ordersClosedTabsByMostRecent();
    void ignoresEmptyPathsAndZeroCapacity();
};

void DocumentHistoryTest::normalizesDeduplicatesAndLimitsRecentFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = directory.filePath(QStringLiteral("first.txt"));
    const QString second = directory.filePath(QStringLiteral("second.txt"));
    const QString third = directory.filePath(QStringLiteral("third.txt"));

    DocumentHistory history(2, 2);
    history.setRecentFiles({first, second, first, third});
    QCOMPARE(history.recentFiles(),
             QStringList({QDir::cleanPath(QFileInfo(first).absoluteFilePath()),
                          QDir::cleanPath(QFileInfo(second).absoluteFilePath())}));

    history.addRecentFile(third);
    QCOMPARE(history.recentFiles(),
             QStringList({QDir::cleanPath(QFileInfo(third).absoluteFilePath()),
                          QDir::cleanPath(QFileInfo(first).absoluteFilePath())}));
    QVERIFY(history.removeRecentFile(first));
    QVERIFY(!history.removeRecentFile(first));
}

void DocumentHistoryTest::ordersClosedTabsByMostRecent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = directory.filePath(QStringLiteral("first.txt"));
    const QString second = directory.filePath(QStringLiteral("second.txt"));
    const QString third = directory.filePath(QStringLiteral("third.txt"));

    DocumentHistory history(2, 2);
    history.recordClosedTab(first);
    history.recordClosedTab(second);
    history.recordClosedTab(first);
    history.recordClosedTab(third);

    QVERIFY(history.hasClosedTabs());
    QCOMPARE(history.takeLastClosedTab(),
             QDir::cleanPath(QFileInfo(third).absoluteFilePath()));
    QCOMPARE(history.takeLastClosedTab(),
             QDir::cleanPath(QFileInfo(first).absoluteFilePath()));
    QVERIFY(!history.hasClosedTabs());
    QVERIFY(history.takeLastClosedTab().isEmpty());
}

void DocumentHistoryTest::ignoresEmptyPathsAndZeroCapacity()
{
    DocumentHistory history(0, 0);
    history.setRecentFiles({QStringLiteral("one.txt")});
    history.addRecentFile(QStringLiteral("two.txt"));
    history.recordClosedTab(QStringLiteral("three.txt"));
    history.recordClosedTab(QStringLiteral("  "));

    QVERIFY(history.recentFiles().isEmpty());
    QVERIFY(!history.hasClosedTabs());
}

QTEST_APPLESS_MAIN(DocumentHistoryTest)

#include "DocumentHistoryTest.moc"
