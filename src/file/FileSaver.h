#pragma once

#include "file/FileTypes.h"

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QWaitCondition>

#include <atomic>

namespace vinson {

class FileSaver final : public QObject
{
    Q_OBJECT

public:
    FileSaver(QString path, QByteArray utf8Data, TextEncoding encoding,
              QObject* parent = nullptr);
    FileSaver(QString path, TextEncoding encoding, qint64 totalUtf8Bytes,
              QObject* parent = nullptr);

    void requestCancel() noexcept;
    void provideChunk(QByteArray utf8Data, bool atEnd);

public slots:
    void save();

signals:
    void chunkRequested(qint64 offset, qint64 maximumBytes);
    void progress(qint64 bytesWritten, qint64 totalBytes);
    void completed(vinson::FileSaveResult result);
    void failed(QString message);
    void canceled();

private:
    struct StreamChunk {
        QByteArray data;
        bool atEnd = false;
    };

    void saveStream();
    [[nodiscard]] bool takeStreamChunk(qint64 offset, StreamChunk& chunk);
    [[nodiscard]] bool writeBytes(QIODevice& device, QByteArrayView data,
                                  qint64& bytesWritten);

    QString path_;
    QByteArray utf8Data_;
    TextEncoding encoding_;
    qint64 progressTotal_ = 0;
    qint64 streamTotalBytes_ = 0;
    bool streaming_ = false;
    std::atomic_bool cancelRequested_ = false;
    QMutex streamMutex_;
    QWaitCondition streamReady_;
    StreamChunk pendingChunk_;
    bool chunkReady_ = false;
};

} // namespace vinson
