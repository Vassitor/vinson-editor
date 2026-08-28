#include "file/FileSaver.h"

#include "file/EncodingDetector.h"

#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QStringConverter>

#include <algorithm>

namespace vinson {
namespace {

constexpr qsizetype chunkSize = 256 * 1024;

QStringConverter::Encoding converterEncoding(TextEncoding encoding)
{
    return encoding == TextEncoding::Utf16Be ? QStringConverter::Utf16BE
                                             : QStringConverter::Utf16LE;
}

} // namespace

FileSaver::FileSaver(QString path, QByteArray utf8Data, TextEncoding encoding,
                     QObject* parent)
    : QObject(parent)
    , path_(std::move(path))
    , utf8Data_(std::move(utf8Data))
    , encoding_(encoding)
{
}

FileSaver::FileSaver(QString path, TextEncoding encoding, qint64 totalUtf8Bytes,
                     QObject* parent)
    : QObject(parent)
    , path_(std::move(path))
    , encoding_(encoding)
    , streamTotalBytes_(std::max<qint64>(0, totalUtf8Bytes))
    , streaming_(true)
{
}

void FileSaver::requestCancel() noexcept
{
    cancelRequested_.store(true, std::memory_order_relaxed);
    streamReady_.wakeAll();
}

void FileSaver::provideChunk(QByteArray utf8Data, bool atEnd)
{
    QMutexLocker lock(&streamMutex_);
    if (!streaming_ || chunkReady_
        || cancelRequested_.load(std::memory_order_relaxed)) {
        return;
    }
    pendingChunk_ = {std::move(utf8Data), atEnd};
    chunkReady_ = true;
    streamReady_.wakeOne();
}

void FileSaver::save()
{
    if (streaming_) {
        saveStream();
        return;
    }

    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        emit failed(tr("Cannot save %1: %2").arg(path_, file.errorString()));
        return;
    }

    TextEncoding actualEncoding = encoding_;
    if (actualEncoding == TextEncoding::Ascii
        && !EncodingDetector::isAscii(utf8Data_)) {
        // ASCII cannot represent new Unicode input. UTF-8 is the lossless,
        // compatible upgrade because every ASCII byte retains its value.
        actualEncoding = TextEncoding::Utf8;
    }
    if (actualEncoding == TextEncoding::Ascii
        || actualEncoding == TextEncoding::Utf8
        || actualEncoding == TextEncoding::Utf8Bom) {
        progressTotal_ = utf8Data_.size()
            + (actualEncoding == TextEncoding::Utf8Bom ? 3 : 0);
    }

    qint64 bytesWritten = 0;
    bool success = true;
    if (actualEncoding == TextEncoding::Utf8Bom) {
        static constexpr char bom[] = {'\xEF', '\xBB', '\xBF'};
        success = writeBytes(file, QByteArrayView(bom, 3), bytesWritten);
    }

    if (success && (actualEncoding == TextEncoding::Ascii
                    || actualEncoding == TextEncoding::Utf8
                    || actualEncoding == TextEncoding::Utf8Bom)) {
        success = writeBytes(file, utf8Data_, bytesWritten);
    } else if (success) {
        QStringDecoder decoder(QStringConverter::Utf8);
        QStringEncoder encoder(converterEncoding(actualEncoding),
                               QStringConverter::Flag::WriteBom);
        for (qsizetype offset = 0; offset < utf8Data_.size(); offset += chunkSize) {
            if (cancelRequested_.load(std::memory_order_relaxed)) {
                emit canceled();
                return;
            }
            const qsizetype length = std::min(chunkSize, utf8Data_.size() - offset);
            const QString decoded = decoder(
                QByteArrayView(utf8Data_.constData() + offset, length));
            if (decoder.hasError()) {
                emit failed(tr("The editor contains invalid UTF-8 data."));
                return;
            }
            const QByteArray encoded = encoder(QStringView(decoded));
            if (!writeBytes(file, encoded, bytesWritten)) {
                success = false;
                break;
            }
        }
        if (success) {
            const QString tail = decoder(QByteArrayView());
            const QByteArray encodedTail = encoder(QStringView(tail));
            success = !decoder.hasError()
                && writeBytes(file, encodedTail, bytesWritten);
        }
        if (success && utf8Data_.isEmpty()) {
            const QByteArray bom = encoder(QStringView());
            success = writeBytes(file, bom, bytesWritten);
        }
    }

    if (!success) {
        if (cancelRequested_.load(std::memory_order_relaxed)) {
            return;
        }
        emit failed(tr("Cannot save %1: %2").arg(path_, file.errorString()));
        return;
    }
    if (cancelRequested_.load(std::memory_order_relaxed)) {
        emit canceled();
        return;
    }
    if (!file.commit()) {
        emit failed(tr("Cannot commit %1 safely: %2").arg(path_, file.errorString()));
        return;
    }

    emit completed({path_, actualEncoding, QFileInfo(path_).size()});
}

void FileSaver::saveStream()
{
    if (cancelRequested_.load(std::memory_order_relaxed)) {
        emit canceled();
        return;
    }

    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        emit failed(tr("Cannot save %1: %2").arg(path_, file.errorString()));
        return;
    }

    // ASCII and UTF-8 have identical output bytes while the stream remains
    // ASCII, so the final metadata can be selected without a preliminary pass.
    TextEncoding actualEncoding = encoding_;
    const bool writesUtf8 = actualEncoding == TextEncoding::Ascii
        || actualEncoding == TextEncoding::Utf8
        || actualEncoding == TextEncoding::Utf8Bom;
    if (writesUtf8) {
        progressTotal_ = streamTotalBytes_
            + (actualEncoding == TextEncoding::Utf8Bom ? 3 : 0);
    }

    qint64 bytesWritten = 0;
    bool success = true;
    if (actualEncoding == TextEncoding::Utf8Bom) {
        static constexpr char bom[] = {'\xEF', '\xBB', '\xBF'};
        success = writeBytes(file, QByteArrayView(bom, 3), bytesWritten);
    }

    QStringDecoder decoder(QStringConverter::Utf8);
    QStringEncoder encoder(converterEncoding(actualEncoding),
                           QStringConverter::Flag::WriteBom);
    qint64 offset = 0;
    bool reachedEnd = false;
    while (success && !reachedEnd) {
        StreamChunk chunk;
        if (!takeStreamChunk(offset, chunk)) {
            emit canceled();
            return;
        }
        offset += chunk.data.size();
        reachedEnd = chunk.atEnd;
        if (chunk.data.isEmpty() && !reachedEnd) {
            emit failed(tr("The editor could not provide the next save range."));
            return;
        }
        if (offset > streamTotalBytes_) {
            emit failed(tr("The document changed while it was being saved."));
            return;
        }

        if (writesUtf8) {
            if (actualEncoding == TextEncoding::Ascii
                && !EncodingDetector::isAscii(chunk.data)) {
                actualEncoding = TextEncoding::Utf8;
            }
            success = writeBytes(file, chunk.data, bytesWritten);
            continue;
        }

        const QString decoded = decoder(chunk.data);
        if (decoder.hasError()) {
            emit failed(tr("The editor contains invalid UTF-8 data."));
            return;
        }
        const QByteArray encoded = encoder(QStringView(decoded));
        success = writeBytes(file, encoded, bytesWritten);
    }

    if (success && offset != streamTotalBytes_) {
        emit failed(tr("The document changed while it was being saved."));
        return;
    }
    if (success && !writesUtf8) {
        const QString tail = decoder(QByteArrayView());
        if (decoder.hasError()) {
            emit failed(tr("The editor contains invalid UTF-8 data."));
            return;
        }
        const QByteArray encodedTail = encoder(QStringView(tail));
        success = writeBytes(file, encodedTail, bytesWritten);
        if (success && streamTotalBytes_ == 0) {
            const QByteArray bom = encoder(QStringView());
            success = writeBytes(file, bom, bytesWritten);
        }
    }

    if (!success) {
        if (!cancelRequested_.load(std::memory_order_relaxed)) {
            emit failed(tr("Cannot save %1: %2").arg(path_, file.errorString()));
        }
        return;
    }
    if (cancelRequested_.load(std::memory_order_relaxed)) {
        emit canceled();
        return;
    }
    if (!file.commit()) {
        emit failed(tr("Cannot commit %1 safely: %2").arg(path_, file.errorString()));
        return;
    }

    emit completed({path_, actualEncoding, QFileInfo(path_).size()});
}

bool FileSaver::takeStreamChunk(qint64 offset, StreamChunk& chunk)
{
    {
        QMutexLocker lock(&streamMutex_);
        chunkReady_ = false;
    }
    emit chunkRequested(offset, chunkSize);

    QMutexLocker lock(&streamMutex_);
    while (!chunkReady_
           && !cancelRequested_.load(std::memory_order_relaxed)) {
        streamReady_.wait(&streamMutex_, 100);
    }
    if (cancelRequested_.load(std::memory_order_relaxed)) {
        return false;
    }
    chunk = std::move(pendingChunk_);
    chunkReady_ = false;
    return true;
}

bool FileSaver::writeBytes(QIODevice& device, QByteArrayView data,
                           qint64& bytesWritten)
{
    qsizetype offset = 0;
    while (offset < data.size()) {
        if (cancelRequested_.load(std::memory_order_relaxed)) {
            emit canceled();
            return false;
        }
        const qsizetype length = std::min(chunkSize, data.size() - offset);
        const qint64 written = device.write(data.data() + offset, length);
        if (written <= 0) {
            return false;
        }
        offset += written;
        bytesWritten += written;
        emit progress(bytesWritten, progressTotal_);
    }
    return true;
}

} // namespace vinson
