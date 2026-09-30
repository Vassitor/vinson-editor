#include "file/FileChangeMonitor.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace vinson;

class FileChangeMonitorTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsModificationRemovalAndRecreation();
    void suppressesChangesWhileSuspended();
};

namespace {

void writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), static_cast<qint64>(contents.size()));
    file.close();
}

bool spyContainsChange(const QSignalSpy& spy, FileChangeMonitor::Change change)
{
    for (const QList<QVariant>& arguments : spy) {
        if (arguments.size() == 2
            && arguments.at(1).value<FileChangeMonitor::Change>() == change) {
            return true;
        }
    }
    return false;
}

} // namespace

void FileChangeMonitorTest::reportsModificationRemovalAndRecreation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("watched.txt"));
    writeFile(path, "initial");

    FileChangeMonitor monitor;
    QSignalSpy changed(&monitor, &FileChangeMonitor::fileChangedExternally);
    monitor.watchFile(path);
    QVERIFY(monitor.isWatching(path));

    writeFile(path, "changed");
    QTRY_VERIFY_WITH_TIMEOUT(
        spyContainsChange(changed, FileChangeMonitor::Change::Modified), 3000);
    changed.clear();

    QVERIFY(QFile::remove(path));
    QTRY_VERIFY_WITH_TIMEOUT(
        spyContainsChange(changed, FileChangeMonitor::Change::Removed), 3000);
    changed.clear();

    writeFile(path, "recreated");
    QTRY_VERIFY_WITH_TIMEOUT(
        spyContainsChange(changed, FileChangeMonitor::Change::Modified), 3000);
}

void FileChangeMonitorTest::suppressesChangesWhileSuspended()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("saving.txt"));
    writeFile(path, "initial");

    FileChangeMonitor monitor;
    QSignalSpy changed(&monitor, &FileChangeMonitor::fileChangedExternally);
    monitor.watchFile(path);
    monitor.suspendFile(path);
    writeFile(path, "own save");
    QTest::qWait(150);
    QCOMPARE(changed.count(), 0);

    monitor.resumeFile(path);
    writeFile(path, "external save");
    QTRY_VERIFY_WITH_TIMEOUT(
        spyContainsChange(changed, FileChangeMonitor::Change::Modified), 3000);

    monitor.unwatchFile(path);
    QVERIFY(!monitor.isWatching(path));
}

QTEST_GUILESS_MAIN(FileChangeMonitorTest)

#include "FileChangeMonitorTest.moc"
