#include "editor/EditorWidget.h"
#include "file/FileManager.h"
#include "largefile/LargeFilePolicy.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <array>
#include <limits>

#if defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace {

constexpr qint64 mebibyte = 1024 * 1024;
constexpr qint64 ioBlockSize = mebibyte;
constexpr auto targetText = "VINSON_BENCHMARK_TARGET_NEAR_END";

qint64 residentMemoryKiB()
{
#if defined(Q_OS_LINUX)
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (!statm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }
    const QList<QByteArray> fields = statm.readLine().simplified().split(' ');
    if (fields.size() >= 2) {
        bool ok = false;
        const qint64 residentPages = fields.at(1).toLongLong(&ok);
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (ok && pageSize > 0) {
            return residentPages * pageSize / 1024;
        }
    }
#endif
    return -1;
}

class OperationProbe final
{
public:
    OperationProbe()
    {
        timer_.setInterval(10);
        QObject::connect(&timer_, &QTimer::timeout, [&] {
            const qint64 gap = interval_.restart();
            maxEventGapMs_ = std::max(maxEventGapMs_, gap);
            ++eventTicks_;
            sampleMemory();
        });
    }

    void start()
    {
        baselineRssKiB_ = residentMemoryKiB();
        peakRssKiB_ = baselineRssKiB_;
        maxEventGapMs_ = 0;
        eventTicks_ = 0;
        interval_.start();
        timer_.start();
    }

    void stop()
    {
        maxEventGapMs_ = std::max(maxEventGapMs_, interval_.elapsed());
        sampleMemory();
        timer_.stop();
    }

    [[nodiscard]] qint64 maxEventGapMs() const noexcept
    {
        return maxEventGapMs_;
    }

    [[nodiscard]] qint64 eventTicks() const noexcept
    {
        return eventTicks_;
    }

    [[nodiscard]] qint64 baselineRssKiB() const noexcept
    {
        return baselineRssKiB_;
    }

    [[nodiscard]] qint64 peakRssKiB() const noexcept
    {
        return peakRssKiB_;
    }

private:
    void sampleMemory()
    {
        peakRssKiB_ = std::max(peakRssKiB_, residentMemoryKiB());
    }

    QTimer timer_;
    QElapsedTimer interval_;
    qint64 baselineRssKiB_ = -1;
    qint64 peakRssKiB_ = -1;
    qint64 maxEventGapMs_ = 0;
    qint64 eventTicks_ = 0;
};

QByteArray generatorBlock()
{
    static constexpr auto line =
        "0123456789 abcdefghijklmnopqrstuvwxyz Vinson benchmark payload line\n";
    QByteArray block;
    block.reserve(ioBlockSize);
    while (block.size() < ioBlockSize) {
        block.append(line);
    }
    block.truncate(ioBlockSize);
    return block;
}

bool generateFile(const QString& path, qint64 sizeBytes, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = file.errorString();
        return false;
    }

    const QByteArray block = generatorBlock();
    const QByteArray marker = QByteArray("\n") + targetText + "\n";
    qint64 remainingPayload = sizeBytes - marker.size();
    while (remainingPayload > 0) {
        const qint64 length = std::min<qint64>(remainingPayload, block.size());
        if (file.write(block.constData(), length) != length) {
            error = file.errorString();
            return false;
        }
        remainingPayload -= length;
    }
    if (file.write(marker) != marker.size()) {
        error = file.errorString();
        return false;
    }
    if (!file.flush()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool generateFixtures(const QString& directoryPath)
{
    QDir directory;
    if (!directory.mkpath(directoryPath)) {
        QTextStream(stderr) << "Cannot create " << directoryPath << '\n';
        return false;
    }

    static constexpr std::array<qint64, 4> sizesMiB{10, 100, 500, 1024};
    for (const qint64 sizeMiB : sizesMiB) {
        const QString path = QDir(directoryPath).filePath(
            QStringLiteral("benchmark-%1MiB.txt").arg(sizeMiB));
        QString error;
        if (!generateFile(path, sizeMiB * mebibyte, error)) {
            QTextStream(stderr) << "Cannot generate " << path << ": "
                                << error << '\n';
            return false;
        }
        QTextStream(stdout) << path << " (" << sizeMiB << " MiB)\n";
    }
    return true;
}

struct BenchmarkResult {
    qint64 fileBytes = 0;
    vinson::LargeFileMode mode = vinson::LargeFileMode::Normal;
    qint64 loadMs = -1;
    qint64 loadBaselineRssKiB = -1;
    qint64 loadPeakRssKiB = -1;
    qint64 loadFinalRssKiB = -1;
    qint64 loadMaxEventGapMs = -1;
    qint64 loadEventTicks = 0;
    qint64 searchMs = -1;
    qint64 searchMaxEventGapMs = -1;
    qint64 searchEventTicks = 0;
    qint64 afterSearchRssKiB = -1;
    qint64 scrollTotalMs = -1;
    qint64 scrollMaxStepMs = -1;
    qint64 afterScrollRssKiB = -1;
    qint64 editMs = -1;
    qint64 afterEditRssKiB = -1;
    qint64 undoMs = -1;
    qint64 afterUndoRssKiB = -1;
    qint64 saveMs = -1;
    qint64 savePeakRssKiB = -1;
    qint64 saveBaselineRssKiB = -1;
    qint64 saveMaxEventGapMs = -1;
    qint64 saveEventTicks = 0;
};

class BenchmarkRunner final
{
public:
    BenchmarkRunner()
    {
        editor_.resize(900, 600);
        editor_.show();
    }

    bool run(const QString& sourcePath, BenchmarkResult& result, QString& error)
    {
        if (!load(sourcePath, result, error)) {
            return false;
        }
        measureSearch(result, error);
        if (!error.isEmpty()) {
            return false;
        }
        measureScroll(result);
        measureEdit(result);
        return save(sourcePath, result, error);
    }

private:
    bool load(const QString& path, BenchmarkResult& result, QString& error)
    {
        QEventLoop loop;
        OperationProbe probe;
        QElapsedTimer elapsed;
        bool prepared = false;
        bool completed = false;

        QObject::connect(&manager_, &vinson::FileManager::loadPrepared, &loop,
                         [&](const vinson::FileLoadInfo& info) {
            result.fileBytes = info.fileSize;
            result.mode = vinson::LargeFilePolicy::modeForSize(info.fileSize);
            prepared = editor_.beginFileLoad(result.mode, info.fileSize);
            if (!prepared) {
                error = QStringLiteral("Scintilla document creation failed");
                manager_.cancelCurrentOperation();
            }
        });
        QObject::connect(&manager_, &vinson::FileManager::loadChunk, &loop,
                         [&](const QByteArray& chunk, qint64, qint64) {
            if (prepared) {
                editor_.appendTextUtf8(chunk);
            }
        });
        QObject::connect(&manager_, &vinson::FileManager::loadCompleted, &loop,
                         [&](const vinson::FileLoadInfo& info) {
            editor_.completeFileLoad(info.lineEnding);
            completed = true;
            loop.quit();
        });
        QObject::connect(&manager_, &vinson::FileManager::operationFailed, &loop,
                         [&](const QString& message) {
            error = message;
            loop.quit();
        });
        QObject::connect(&manager_, &vinson::FileManager::operationCanceled, &loop,
                         [&] {
            if (error.isEmpty()) {
                error = QStringLiteral("Load canceled");
            }
            loop.quit();
        });

        probe.start();
        elapsed.start();
        if (!manager_.openFile(path)) {
            error = QStringLiteral("FileManager rejected the load");
            return false;
        }
        loop.exec();
        result.loadMs = elapsed.elapsed();
        probe.stop();
        result.loadBaselineRssKiB = probe.baselineRssKiB();
        result.loadPeakRssKiB = probe.peakRssKiB();
        result.loadFinalRssKiB = residentMemoryKiB();
        result.loadMaxEventGapMs = probe.maxEventGapMs();
        result.loadEventTicks = probe.eventTicks();
        return completed && error.isEmpty();
    }

    void measureSearch(BenchmarkResult& result, QString& error)
    {
        OperationProbe probe;
        QElapsedTimer elapsed;
        probe.start();
        elapsed.start();
        const vinson::SearchRange match = editor_.findTextUtf8Responsive(
            targetText, 0, editor_.documentLength(), {});
        result.searchMs = elapsed.elapsed();
        probe.stop();
        result.searchMaxEventGapMs = probe.maxEventGapMs();
        result.searchEventTicks = probe.eventTicks();
        result.afterSearchRssKiB = residentMemoryKiB();
        if (!match.isValid()) {
            error = QStringLiteral("Search marker was not found");
        }
    }

    void measureScroll(BenchmarkResult& result)
    {
        constexpr qint64 steps = 100;
        const qint64 lines = std::max<qint64>(1, editor_.editorLineCount());
        QElapsedTimer total;
        total.start();
        qint64 maxStep = 0;
        for (qint64 step = 0; step < steps; ++step) {
            QElapsedTimer oneStep;
            oneStep.start();
            const qint64 line = 1 + ((lines - 1) * step / (steps - 1));
            editor_.goToOneBasedLine(line);
            QApplication::processEvents(QEventLoop::AllEvents, 5);
            maxStep = std::max(maxStep, oneStep.elapsed());
        }
        result.scrollTotalMs = total.elapsed();
        result.scrollMaxStepMs = maxStep;
        result.afterScrollRssKiB = residentMemoryKiB();
    }

    void measureEdit(BenchmarkResult& result)
    {
        const qint64 originalLength = editor_.documentLength();
        const qint64 middle = originalLength / 2;
        editor_.selectSearchRange({middle, middle});

        QElapsedTimer elapsed;
        elapsed.start();
        editor_.replaceSelectionUtf8("BENCH_EDIT");
        result.editMs = elapsed.elapsed();
        result.afterEditRssKiB = residentMemoryKiB();
        elapsed.restart();
        editor_.undo();
        result.undoMs = elapsed.elapsed();
        result.afterUndoRssKiB = residentMemoryKiB();
    }

    bool save(const QString& sourcePath, BenchmarkResult& result, QString& error)
    {
        const QString outputPath = sourcePath + QStringLiteral(".save.tmp");
        QFile::remove(outputPath);
        QEventLoop loop;
        OperationProbe probe;
        QElapsedTimer elapsed;
        bool completed = false;

        QObject::connect(&manager_, &vinson::FileManager::saveChunkRequested,
                         &loop, [&](qint64 offset, qint64 maximumBytes) {
            const QByteArray chunk = editor_.textRangeUtf8(offset, maximumBytes);
            manager_.provideSaveChunk(
                chunk, offset + chunk.size() >= editor_.documentLength());
        });
        QObject::connect(&manager_, &vinson::FileManager::saveCompleted, &loop,
                         [&](const vinson::FileSaveResult&) {
            completed = true;
            loop.quit();
        });
        QObject::connect(&manager_, &vinson::FileManager::operationFailed, &loop,
                         [&](const QString& message) {
            error = message;
            loop.quit();
        });
        QObject::connect(&manager_, &vinson::FileManager::operationCanceled, &loop,
                         [&] {
            error = QStringLiteral("Save canceled");
            loop.quit();
        });

        probe.start();
        elapsed.start();
        const bool started = vinson::LargeFilePolicy::usesLargeDocument(result.mode)
            ? manager_.saveFileStreaming(outputPath, vinson::TextEncoding::Ascii,
                                         editor_.documentLength())
            : manager_.saveFile(outputPath, editor_.textUtf8(),
                                vinson::TextEncoding::Ascii);
        if (!started) {
            error = QStringLiteral("FileManager rejected the save");
            return false;
        }
        loop.exec();
        result.saveMs = elapsed.elapsed();
        probe.stop();
        result.saveBaselineRssKiB = probe.baselineRssKiB();
        result.savePeakRssKiB = probe.peakRssKiB();
        result.saveMaxEventGapMs = probe.maxEventGapMs();
        result.saveEventTicks = probe.eventTicks();

        if (completed && QFileInfo(outputPath).size() != result.fileBytes) {
            error = QStringLiteral("Saved file size differs from source");
            completed = false;
        }
        QFile::remove(outputPath);
        return completed && error.isEmpty();
    }

    vinson::EditorWidget editor_;
    vinson::FileManager manager_;
};

QJsonObject resultJson(const QString& path, const BenchmarkResult& result)
{
    QJsonObject json;
    json.insert(QStringLiteral("path"), path);
    json.insert(QStringLiteral("file_bytes"), result.fileBytes);
    json.insert(QStringLiteral("file_mib"),
                static_cast<double>(result.fileBytes) / mebibyte);
    json.insert(QStringLiteral("mode"),
                vinson::LargeFilePolicy::displayName(result.mode).isEmpty()
                    ? QStringLiteral("Normal")
                    : vinson::LargeFilePolicy::displayName(result.mode));
    json.insert(QStringLiteral("load_ms"), result.loadMs);
    json.insert(QStringLiteral("load_baseline_rss_kib"),
                result.loadBaselineRssKiB);
    json.insert(QStringLiteral("load_peak_rss_kib"), result.loadPeakRssKiB);
    json.insert(QStringLiteral("load_final_rss_kib"), result.loadFinalRssKiB);
    json.insert(QStringLiteral("load_max_event_gap_ms"),
                result.loadMaxEventGapMs);
    json.insert(QStringLiteral("load_event_ticks"), result.loadEventTicks);
    json.insert(QStringLiteral("search_ms"), result.searchMs);
    json.insert(QStringLiteral("search_max_event_gap_ms"),
                result.searchMaxEventGapMs);
    json.insert(QStringLiteral("search_event_ticks"), result.searchEventTicks);
    json.insert(QStringLiteral("after_search_rss_kib"),
                result.afterSearchRssKiB);
    json.insert(QStringLiteral("scroll_100_steps_ms"), result.scrollTotalMs);
    json.insert(QStringLiteral("scroll_max_step_ms"), result.scrollMaxStepMs);
    json.insert(QStringLiteral("after_scroll_rss_kib"),
                result.afterScrollRssKiB);
    json.insert(QStringLiteral("middle_edit_ms"), result.editMs);
    json.insert(QStringLiteral("after_edit_rss_kib"), result.afterEditRssKiB);
    json.insert(QStringLiteral("undo_ms"), result.undoMs);
    json.insert(QStringLiteral("after_undo_rss_kib"), result.afterUndoRssKiB);
    json.insert(QStringLiteral("save_ms"), result.saveMs);
    json.insert(QStringLiteral("save_baseline_rss_kib"),
                result.saveBaselineRssKiB);
    json.insert(QStringLiteral("save_peak_rss_kib"), result.savePeakRssKiB);
    json.insert(QStringLiteral("save_max_event_gap_ms"),
                result.saveMaxEventGapMs);
    json.insert(QStringLiteral("save_event_ticks"), result.saveEventTicks);
    return json;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("vinson-large-file-benchmark"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Generate or benchmark Vinson Editor large-file fixtures."));
    parser.addHelpOption();
    const QCommandLineOption generateOption(
        QStringLiteral("generate"),
        QStringLiteral("Generate 10, 100, 500, and 1024 MiB fixtures in DIR."),
        QStringLiteral("DIR"));
    const QCommandLineOption fileOption(
        QStringLiteral("file"), QStringLiteral("Benchmark one fixture."),
        QStringLiteral("PATH"));
    parser.addOption(generateOption);
    parser.addOption(fileOption);
    parser.process(application);

    if (parser.isSet(generateOption)) {
        return generateFixtures(parser.value(generateOption)) ? 0 : 1;
    }
    if (!parser.isSet(fileOption)) {
        parser.showHelp(2);
    }

    const QString path = QFileInfo(parser.value(fileOption)).absoluteFilePath();
    BenchmarkResult result;
    QString error;
    BenchmarkRunner runner;
    if (!runner.run(path, result, error)) {
        QTextStream(stderr) << error << '\n';
        return 1;
    }
    QTextStream(stdout)
        << QJsonDocument(resultJson(path, result)).toJson(QJsonDocument::Compact)
        << '\n';
    return 0;
}
