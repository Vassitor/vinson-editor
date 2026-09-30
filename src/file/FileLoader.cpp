#include "file/FileLoader.h"

#include "file/EncodingDetector.h"

#include <QFile>
#include <QCryptographicHash>
#include <QStringConverter>

namespace vinson {
namespace {

constexpr qsizetype chunkSize = 256 * 1024;

class StreamingUtf8Validator final
{
public:
    [[nodiscard]] bool consume(QByteArrayView data) noexcept
    {
        for (const char value : data) {
            const auto byte = static_cast<unsigned char>(value);
            if (continuations_ > 0) {
                if ((byte & 0xC0) != 0x80) {
                    return false;
                }
                codePoint_ = (codePoint_ << 6) | (byte & 0x3F);
                --continuations_;
                if (continuations_ == 0
                    && (codePoint_ < minimumCodePoint_
                        || codePoint_ > 0x10FFFF
                        || (codePoint_ >= 0xD800 && codePoint_ <= 0xDFFF))) {
                    return false;
                }
                continue;
            }

            if (byte < 0x80) {
                continue;
            }
            if (byte >= 0xC2 && byte <= 0xDF) {
                beginSequence(byte & 0x1F, 1, 0x80);
            } else if (byte >= 0xE0 && byte <= 0xEF) {
                beginSequence(byte & 0x0F, 2, 0x800);
            } else if (byte >= 0xF0 && byte <= 0xF4) {
                beginSequence(byte & 0x07, 3, 0x10000);
            } else {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool complete() const noexcept
    {
        return continuations_ == 0;
    }

private:
    void beginSequence(quint32 codePoint, int continuations,
                       quint32 minimumCodePoint) noexcept
    {
        codePoint_ = codePoint;
        continuations_ = continuations;
        minimumCodePoint_ = minimumCodePoint;
    }

    quint32 codePoint_ = 0;
    quint32 minimumCodePoint_ = 0;
    int continuations_ = 0;
};

QStringConverter::Encoding converterEncoding(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf16Le:
        return QStringConverter::Utf16LE;
    case TextEncoding::Utf16Be:
        return QStringConverter::Utf16BE;
    case TextEncoding::Ascii:
    case TextEncoding::Utf8:
    case TextEncoding::Utf8Bom:
        return QStringConverter::Utf8;
    }
    return QStringConverter::Utf8;
}

} // namespace

FileLoader::FileLoader(QString path, QObject* parent)
    : QObject(parent)
    , path_(std::move(path))
{
}

void FileLoader::requestCancel() noexcept
{
    cancelRequested_.store(true, std::memory_order_relaxed);
    // Wake a producer that is waiting for the GUI to consume a queued chunk.
    chunkCredits_.release(4);
}

void FileLoader::acknowledgeChunk()
{
    chunkCredits_.release();
}

void FileLoader::load()
{
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(tr("Cannot open %1: %2").arg(path_, file.errorString()));
        return;
    }

    const qint64 totalBytes = file.size();
    const QDateTime sourceModifiedAt = file.fileTime(
        QFileDevice::FileModificationTime);
    bool hashContent = totalBytes <= maximumContentFingerprintBytes;
    QCryptographicHash contentHash(QCryptographicHash::Sha256);
    QByteArray firstChunk = file.read(chunkSize);
    if (firstChunk.isNull() && file.error() != QFileDevice::NoError) {
        emit failed(tr("Cannot read %1: %2").arg(path_, file.errorString()));
        return;
    }
    if (hashContent) {
        contentHash.addData(firstChunk);
    }

    const EncodingDetection detection = EncodingDetector::detect(firstChunk);
    FileLoadInfo info{path_, detection.encoding, LineEnding::None, totalBytes};
    info.modifiedAt = sourceModifiedAt;

    const bool sourceIsUtf16 = detection.encoding == TextEncoding::Utf16Le
        || detection.encoding == TextEncoding::Utf16Be;
    QStringDecoder decoder(converterEncoding(detection.encoding));
    StreamingUtf8Validator utf8Validator;
    LineEndingDetector lineEndings;
    bool allAscii = true;
    qint64 bytesRead = firstChunk.size();

    auto emitDecodedChunk = [&](QByteArray utf8) -> bool {
        if (utf8.isEmpty()) {
            return true;
        }
        lineEndings.consume(utf8);
        if (!acquireChunkCredit()) {
            emit canceled();
            return false;
        }
        emit chunkReady(std::move(utf8), bytesRead, totalBytes);
        return true;
    };

    auto decodeUtf16Chunk = [&](QByteArrayView source) -> bool {
        allAscii = allAscii && EncodingDetector::isAscii(source);
        const QString decoded = decoder(source);
        if (decoder.hasError()) {
            emit failed(tr("%1 contains invalid %2 data.")
                            .arg(path_, encodingName(detection.encoding)));
            return false;
        }
        return emitDecodedChunk(decoded.toUtf8());
    };

    const QByteArrayView initialData(firstChunk.constData() + detection.bomLength,
                                     firstChunk.size() - detection.bomLength);
    allAscii = EncodingDetector::isAscii(initialData);
    QByteArray initialUtf8;
    bool decoderFlushed = false;
    if (sourceIsUtf16) {
        const QString initialDecoded = decoder(initialData);
        if (decoder.hasError()) {
            emit failed(tr("%1 contains invalid %2 data.")
                            .arg(path_, encodingName(detection.encoding)));
            return;
        }
        initialUtf8 = initialDecoded.toUtf8();
        if (file.atEnd()) {
            initialUtf8.append(QString(decoder(QByteArrayView())).toUtf8());
            decoderFlushed = true;
            if (decoder.hasError()) {
                emit failed(tr("%1 ends with an incomplete %2 sequence.")
                                .arg(path_, encodingName(detection.encoding)));
                return;
            }
        }
    } else {
        if (!utf8Validator.consume(initialData)) {
            emit failed(tr("%1 contains invalid UTF-8 data.").arg(path_));
            return;
        }
        initialUtf8 = QByteArray(initialData.data(), initialData.size());
    }
    if (cancelRequested_.load(std::memory_order_relaxed)) {
        emit canceled();
        return;
    }
    emit prepared(info);
    if (!emitDecodedChunk(initialUtf8)) {
        return;
    }

    while (!file.atEnd()) {
        if (cancelRequested_.load(std::memory_order_relaxed)) {
            emit canceled();
            return;
        }
        QByteArray chunk = file.read(chunkSize);
        if (chunk.isNull() && file.error() != QFileDevice::NoError) {
            emit failed(tr("Cannot read %1: %2").arg(path_, file.errorString()));
            return;
        }
        bytesRead += chunk.size();
        if (hashContent && bytesRead <= maximumContentFingerprintBytes) {
            contentHash.addData(chunk);
        } else {
            hashContent = false;
        }
        if (sourceIsUtf16) {
            if (!decodeUtf16Chunk(chunk)) {
                return;
            }
        } else {
            allAscii = allAscii && EncodingDetector::isAscii(chunk);
            if (!utf8Validator.consume(chunk)) {
                emit failed(tr("%1 contains invalid UTF-8 data.").arg(path_));
                return;
            }
            if (!emitDecodedChunk(std::move(chunk))) {
                return;
            }
        }
    }

    if (sourceIsUtf16) {
        const QString decoderTail = decoderFlushed
            ? QString() : QString(decoder(QByteArrayView()));
        if (decoder.hasError()) {
            emit failed(tr("%1 ends with an incomplete %2 sequence.")
                            .arg(path_, encodingName(detection.encoding)));
            return;
        }
        const QByteArray utf8Tail = decoderTail.toUtf8();
        if (!utf8Tail.isEmpty() && !emitDecodedChunk(utf8Tail)) {
            return;
        }
    } else if (!utf8Validator.complete()) {
        emit failed(tr("%1 ends with an incomplete UTF-8 sequence.").arg(path_));
        return;
    }

    info.lineEnding = lineEndings.result();
    if (info.encoding == TextEncoding::Ascii && !allAscii) {
        info.encoding = TextEncoding::Utf8;
    }
    if (hashContent && bytesRead == totalBytes) {
        info.contentHash = contentHash.result();
    }
    emit completed(info);
}

bool FileLoader::acquireChunkCredit()
{
    while (!cancelRequested_.load(std::memory_order_relaxed)) {
        if (chunkCredits_.tryAcquire(1, 100)) {
            return !cancelRequested_.load(std::memory_order_relaxed);
        }
    }
    return false;
}

} // namespace vinson
