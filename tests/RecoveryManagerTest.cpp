#include "session/RecoveryManager.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace vinson;

class RecoveryManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesLoadsAndRemovesSnapshot();
    void rejectsInvalidIdsAndOversizedContent();
    void clearsAllSnapshots();
};

namespace {

RecoverySnapshot snapshot(const QString& id, const QByteArray& content)
{
    RecoverySnapshot result;
    result.entry.id = id;
    result.entry.originalPath = QStringLiteral("C:/notes/example.txt");
    result.entry.displayName = QStringLiteral("example.txt");
    result.entry.encoding = TextEncoding::Utf8Bom;
    result.entry.lineEnding = LineEnding::CrLf;
    result.entry.originalFileSize = 42;
    result.content = content;
    return result;
}

} // namespace

void RecoveryManagerTest::writesLoadsAndRemovesSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RecoveryManager manager(directory.path());
    manager.queueSnapshot(snapshot(QStringLiteral("document-1"), "unsaved"));
    manager.flush();

    const QVector<RecoveryEntry> entries = manager.entries();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.front().id, QStringLiteral("document-1"));
    QCOMPARE(entries.front().displayName, QStringLiteral("example.txt"));
    QCOMPARE(entries.front().encoding, TextEncoding::Utf8Bom);
    QCOMPARE(entries.front().lineEnding, LineEnding::CrLf);
    QCOMPARE(entries.front().originalFileSize, qint64(42));
    QCOMPARE(entries.front().contentSize, qint64(7));
    QVERIFY(entries.front().updatedAt.isValid());
    QCOMPARE(manager.loadContent(QStringLiteral("document-1")),
             QByteArray("unsaved"));

    manager.removeSnapshot(QStringLiteral("document-1"));
    manager.flush();
    QVERIFY(manager.entries().isEmpty());
}

void RecoveryManagerTest::rejectsInvalidIdsAndOversizedContent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RecoveryManager manager(directory.path());
    manager.queueSnapshot(snapshot(QStringLiteral("../escape"), "invalid"));
    manager.queueSnapshot(snapshot(
        QStringLiteral("too-large"),
        QByteArray(RecoveryManager::maximumSnapshotBytes + 1, 'x')));
    manager.flush();

    QVERIFY(manager.entries().isEmpty());
    QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("escape.data"))));
}

void RecoveryManagerTest::clearsAllSnapshots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RecoveryManager manager(directory.path());
    manager.queueSnapshot(snapshot(QStringLiteral("one"), "1"));
    manager.queueSnapshot(snapshot(QStringLiteral("two"), "2"));
    manager.flush();
    QCOMPARE(manager.entries().size(), 2);

    manager.clearAll();
    manager.flush();
    QVERIFY(manager.entries().isEmpty());
}

QTEST_GUILESS_MAIN(RecoveryManagerTest)

#include "RecoveryManagerTest.moc"
