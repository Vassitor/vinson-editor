#pragma once

#include "file/FileTypes.h"

#include <QObject>
#include <QSemaphore>

#include <atomic>

namespace vinson {

class FileLoader final : public QObject
{
    Q_OBJECT

public:
    explicit FileLoader(QString path, QObject* parent = nullptr);

    // These methods are intentionally thread-safe: the worker's event loop is
    // occupied while reading, so queued cancellation/acknowledgement would not run.
    void requestCancel() noexcept;
    void acknowledgeChunk();

public slots:
    void load();

signals:
    void prepared(vinson::FileLoadInfo info);
    void chunkReady(QByteArray utf8Data, qint64 bytesRead, qint64 totalBytes);
    void completed(vinson::FileLoadInfo info);
    void failed(QString message);
    void canceled();

private:
    [[nodiscard]] bool acquireChunkCredit();

    QString path_;
    std::atomic_bool cancelRequested_ = false;
    QSemaphore chunkCredits_{4};
};

} // namespace vinson
