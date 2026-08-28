#include "editor/EditorDocument.h"
#include "file/EncodingDetector.h"
#include "file/FileLoader.h"
#include "file/FileManager.h"
#include "file/FileSaver.h"
#include "largefile/LargeFilePolicy.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

namespace {

struct LoadedFile {
    QByteArray text;
    vinson::FileLoadInfo info;
    QString error;
    bool completed = false;
};

LoadedFile loadFile(const QString& path)
{
    vinson::FileLoader loader(path);
    LoadedFile result;
    QObject::connect(&loader, &vinson::FileLoader::chunkReady,
                     [&result, &loader](const QByteArray& data, qint64, qint64) {
                         result.text.append(data);
                         loader.acknowledgeChunk();
                     });
    QObject::connect(&loader, &vinson::FileLoader::completed,
                     [&result](const vinson::FileLoadInfo& info) {
                         result.info = info;
                         result.completed = true;
                     });
    QObject::connect(&loader, &vinson::FileLoader::failed,
                     [&result](const QString& error) { result.error = error; });
    loader.load();
    return result;
}

vinson::FileSaveResult saveFile(const QString& path, const QByteArray& text,
                                vinson::TextEncoding encoding, QString* error)
{
    vinson::FileSaver saver(path, text, encoding);
    vinson::FileSaveResult result;
    QObject::connect(&saver, &vinson::FileSaver::completed,
                     [&result](const vinson::FileSaveResult& saved) {
                         result = saved;
                     });
    QObject::connect(&saver, &vinson::FileSaver::failed,
                     [error](const QString& message) { *error = message; });
    saver.save();
    return result;
}

void writeFixture(const QString& path, QByteArrayView data)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write(data.data(), data.size()), data.size());
}

QByteArray readFixture(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class FileCoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void detectsEncodingAndLineEndings();
    void loadsUtf8FromUnicodePath();
    void loadsEmptyFile();
    void reportsMissingFile();
    void rejectsInvalidUtf8();
    void validatesUtf8AcrossChunkBoundary();
    void preservesEncodingOnSave_data();
    void preservesEncodingOnSave();
    void upgradesAsciiWithoutDataLoss();
    void canceledSavePreservesSource();
    void tracksDocumentState();
    void fileManagerLoadsAsynchronously();
    void fileManagerSavesAsynchronously();
    void classifiesLargeFileBoundaries();
    void fileManagerStreamsSaveAsynchronously();
    void canceledStreamSavePreservesSource();
};

void FileCoreTest::detectsEncodingAndLineEndings()
{
    QCOMPARE(vinson::EncodingDetector::detect("plain text").encoding,
             vinson::TextEncoding::Ascii);
    QCOMPARE(vinson::EncodingDetector::detect("\xEF\xBB\xBFhello").encoding,
             vinson::TextEncoding::Utf8Bom);
    QCOMPARE(vinson::EncodingDetector::detect("\xFF\xFEh\0").encoding,
             vinson::TextEncoding::Utf16Le);
    QCOMPARE(vinson::EncodingDetector::detect("\xFE\xFF\0h").encoding,
             vinson::TextEncoding::Utf16Be);
    QCOMPARE(vinson::EncodingDetector::detectLineEnding("a\nb\n"),
             vinson::LineEnding::Lf);
    QCOMPARE(vinson::EncodingDetector::detectLineEnding("a\r\nb\r\n"),
             vinson::LineEnding::CrLf);
    QCOMPARE(vinson::EncodingDetector::detectLineEnding("a\rb\r"),
             vinson::LineEnding::Cr);
    QCOMPARE(vinson::EncodingDetector::detectLineEnding("a\r\nb\n"),
             vinson::LineEnding::Mixed);
}

void FileCoreTest::classifiesLargeFileBoundaries()
{
    using vinson::LargeFileMode;
    using vinson::LargeFilePolicy;

    QCOMPARE(LargeFilePolicy::modeForSize(1 * LargeFilePolicy::mebibyte),
             LargeFileMode::Normal);
    QCOMPARE(LargeFilePolicy::modeForSize(63 * LargeFilePolicy::mebibyte),
             LargeFileMode::Normal);
    QCOMPARE(LargeFilePolicy::modeForSize(
                 LargeFilePolicy::largeFileThreshold - 1),
             LargeFileMode::Normal);
    QCOMPARE(LargeFilePolicy::modeForSize(
                 LargeFilePolicy::largeFileThreshold),
             LargeFileMode::Large);
    QCOMPARE(LargeFilePolicy::modeForSize(
                 LargeFilePolicy::veryLargeFileThreshold - 1),
             LargeFileMode::Large);
    QCOMPARE(LargeFilePolicy::modeForSize(
                 LargeFilePolicy::veryLargeFileThreshold),
             LargeFileMode::VeryLarge);
    QCOMPARE(LargeFilePolicy::modeForSize(1024 * LargeFilePolicy::mebibyte),
             LargeFileMode::VeryLarge);
    QVERIFY(LargeFilePolicy::defaultsWordWrapOff(LargeFileMode::Large));
    QVERIFY(LargeFilePolicy::requiresReplaceAllConfirmation(
        LargeFileMode::VeryLarge));
}

void FileCoreTest::loadsUtf8FromUnicodePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("中文 file.txt"));
    const QByteArray source("first\r\n\xE4\xB8\x96\xE7\x95\x8C\r\n");
    writeFixture(path, source);

    const LoadedFile loaded = loadFile(path);

    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QVERIFY(loaded.completed);
    QCOMPARE(loaded.text, source);
    QCOMPARE(loaded.info.encoding, vinson::TextEncoding::Utf8);
    QCOMPARE(loaded.info.lineEnding, vinson::LineEnding::CrLf);
    QCOMPARE(loaded.info.path, path);
}

void FileCoreTest::loadsEmptyFile()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("empty.txt"));
    writeFixture(path, {});

    const LoadedFile loaded = loadFile(path);

    QVERIFY(loaded.completed);
    QVERIFY(loaded.text.isEmpty());
    QCOMPARE(loaded.info.encoding, vinson::TextEncoding::Ascii);
    QCOMPARE(loaded.info.lineEnding, vinson::LineEnding::None);
}

void FileCoreTest::reportsMissingFile()
{
    QTemporaryDir directory;
    const LoadedFile loaded = loadFile(
        directory.filePath(QStringLiteral("does-not-exist.txt")));

    QVERIFY(!loaded.completed);
    QVERIFY(!loaded.error.isEmpty());
}

void FileCoreTest::rejectsInvalidUtf8()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("invalid.txt"));
    writeFixture(path, QByteArray("\xC3\x28", 2));

    const LoadedFile loaded = loadFile(path);

    QVERIFY(!loaded.completed);
    QVERIFY(!loaded.error.isEmpty());
}

void FileCoreTest::validatesUtf8AcrossChunkBoundary()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("split-utf8.txt"));
    QByteArray source(256 * 1024 - 1, 'x');
    source.append(QByteArray("\xE4\xB8\x96", 3));
    source.append("\nfinished\n");
    writeFixture(path, source);

    const LoadedFile loaded = loadFile(path);

    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QVERIFY(loaded.completed);
    QCOMPARE(loaded.info.encoding, vinson::TextEncoding::Utf8);
    QCOMPARE(loaded.text, source);
}

void FileCoreTest::preservesEncodingOnSave_data()
{
    QTest::addColumn<vinson::TextEncoding>("encoding");
    QTest::addColumn<QByteArray>("bom");
    QTest::newRow("utf8") << vinson::TextEncoding::Utf8 << QByteArray();
    QTest::newRow("utf8-bom") << vinson::TextEncoding::Utf8Bom
                              << QByteArray("\xEF\xBB\xBF", 3);
    QTest::newRow("utf16-le") << vinson::TextEncoding::Utf16Le
                              << QByteArray("\xFF\xFE", 2);
    QTest::newRow("utf16-be") << vinson::TextEncoding::Utf16Be
                              << QByteArray("\xFE\xFF", 2);
}

void FileCoreTest::preservesEncodingOnSave()
{
    QFETCH(vinson::TextEncoding, encoding);
    QFETCH(QByteArray, bom);
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("roundtrip.txt"));
    const QByteArray source("hello \xE4\xB8\x96\xE7\x95\x8C\r\n");
    QString error;

    const auto saved = saveFile(path, source, encoding, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(saved.encoding, encoding);
    QVERIFY(readFixture(path).startsWith(bom));

    const LoadedFile loaded = loadFile(path);
    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QCOMPARE(loaded.text, source);
    QCOMPARE(loaded.info.encoding, encoding);
    QCOMPARE(loaded.info.lineEnding, vinson::LineEnding::CrLf);
}

void FileCoreTest::upgradesAsciiWithoutDataLoss()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("ascii.txt"));
    const QByteArray source("hello \xE4\xB8\x96\xE7\x95\x8C");
    QString error;

    const auto saved = saveFile(path, source, vinson::TextEncoding::Ascii, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(saved.encoding, vinson::TextEncoding::Utf8);
    QCOMPARE(readFixture(path), source);
}

void FileCoreTest::canceledSavePreservesSource()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("important.txt"));
    const QByteArray original("original data");
    writeFixture(path, original);

    vinson::FileSaver saver(path, QByteArray("replacement"),
                            vinson::TextEncoding::Utf8);
    bool canceled = false;
    QObject::connect(&saver, &vinson::FileSaver::canceled,
                     [&canceled] { canceled = true; });
    saver.requestCancel();
    saver.save();

    QVERIFY(canceled);
    QCOMPARE(readFixture(path), original);
}

void FileCoreTest::tracksDocumentState()
{
    vinson::EditorDocument document;
    QVERIFY(document.isUntitled());
    document.adoptLoadedFile({QStringLiteral("/tmp/example.txt"),
                              vinson::TextEncoding::Utf8Bom,
                              vinson::LineEnding::Lf, 42});
    QCOMPARE(document.displayName(), QStringLiteral("example.txt"));
    QCOMPARE(document.fileSize(), 42);
    QVERIFY(!document.isModified());
    document.setModified(true);
    QVERIFY(document.isModified());
}

void FileCoreTest::fileManagerLoadsAsynchronously()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("async.txt"));
    QByteArray source;
    source.reserve(2 * 1024 * 1024);
    while (source.size() < 2 * 1024 * 1024) {
        source.append("a line of text\n");
    }
    writeFixture(path, source);

    vinson::FileManager manager;
    QSignalSpy chunkSpy(&manager, &vinson::FileManager::loadChunk);
    QSignalSpy completedSpy(&manager, &vinson::FileManager::loadCompleted);
    QSignalSpy preparedSpy(&manager, &vinson::FileManager::loadPrepared);
    QSignalSpy failedSpy(&manager, &vinson::FileManager::operationFailed);
    QByteArray loadedText;
    vinson::FileLoadInfo loadedInfo;
    QString error;
    QObject::connect(&manager, &vinson::FileManager::loadChunk,
                     [&loadedText](const QByteArray& chunk, qint64, qint64) {
                         loadedText.append(chunk);
                     });
    QObject::connect(&manager, &vinson::FileManager::loadCompleted,
                     [&loadedInfo](const vinson::FileLoadInfo& info) {
                         loadedInfo = info;
                     });
    QObject::connect(&manager, &vinson::FileManager::operationFailed,
                     [&error](const QString& message) { error = message; });

    QVERIFY(manager.openFile(path));
    QVERIFY(manager.isBusy());
    QTRY_COMPARE_WITH_TIMEOUT(preparedSpy.count(), 1, 5000);
    QCOMPARE(failedSpy.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(chunkSpy.count() > 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 5000);
    QVERIFY(!manager.isBusy());

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loadedText, source);
    QCOMPARE(loadedInfo.lineEnding, vinson::LineEnding::Lf);
}

void FileCoreTest::fileManagerSavesAsynchronously()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("async-save.txt"));
    QByteArray source(2 * 1024 * 1024, 'x');
    source.append("\nfinished\n");

    vinson::FileManager manager;
    QSignalSpy completedSpy(&manager, &vinson::FileManager::saveCompleted);
    QSignalSpy failedSpy(&manager, &vinson::FileManager::operationFailed);

    QVERIFY(manager.saveFile(path, source, vinson::TextEncoding::Utf8));
    QVERIFY(manager.isBusy());
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 5000);

    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(!manager.isBusy());
    QCOMPARE(readFixture(path), source);
}

void FileCoreTest::fileManagerStreamsSaveAsynchronously()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("stream-save.txt"));
    QByteArray source(700 * 1024, 'x');
    source.replace(256 * 1024 - 2, 4,
                   QByteArray("\xE4\xB8\x96\xE7\x95\x8C", 6));
    source.append("\nfinished\n");

    vinson::FileManager manager;
    QSignalSpy requestSpy(&manager, &vinson::FileManager::saveChunkRequested);
    QSignalSpy completedSpy(&manager, &vinson::FileManager::saveCompleted);
    QSignalSpy failedSpy(&manager, &vinson::FileManager::operationFailed);
    QObject::connect(
        &manager, &vinson::FileManager::saveChunkRequested,
        [&manager, &source](qint64 offset, qint64 maximumBytes) {
            const qint64 length = std::min<qint64>(
                maximumBytes, source.size() - offset);
            const QByteArray chunk = source.mid(offset, length);
            manager.provideSaveChunk(chunk,
                                     offset + length >= source.size());
        });

    QVERIFY(manager.saveFileStreaming(path, vinson::TextEncoding::Utf16Le,
                                      source.size()));
    QVERIFY(manager.isBusy());
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 1, 5000);

    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(requestSpy.count() >= 3);
    QVERIFY(!manager.isBusy());
    const LoadedFile loaded = loadFile(path);
    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QCOMPARE(loaded.info.encoding, vinson::TextEncoding::Utf16Le);
    QCOMPARE(loaded.text, source);
}

void FileCoreTest::canceledStreamSavePreservesSource()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("stream-cancel.txt"));
    const QByteArray original("original data");
    writeFixture(path, original);

    vinson::FileSaver saver(path, vinson::TextEncoding::Utf8, 1024);
    bool canceled = false;
    QObject::connect(&saver, &vinson::FileSaver::chunkRequested,
                     [&saver](qint64, qint64) { saver.requestCancel(); });
    QObject::connect(&saver, &vinson::FileSaver::canceled,
                     [&canceled] { canceled = true; });
    saver.save();

    QVERIFY(canceled);
    QCOMPARE(readFixture(path), original);
}

QTEST_GUILESS_MAIN(FileCoreTest)
#include "FileCoreTest.moc"
