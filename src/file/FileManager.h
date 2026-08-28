#pragma once

#include "file/FileTypes.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>

class QThread;

namespace vinson {

class FileLoader;
class FileSaver;

class FileManager final : public QObject
{
    Q_OBJECT

public:
    enum class Operation {
        None,
        Loading,
        Saving,
    };
    Q_ENUM(Operation)

    explicit FileManager(QObject* parent = nullptr);
    ~FileManager() override;

    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] Operation operation() const noexcept;

    bool openFile(const QString& path);
    bool saveFile(const QString& path, QByteArray utf8Data,
                  TextEncoding encoding);
    bool saveFileStreaming(const QString& path, TextEncoding encoding,
                           qint64 totalUtf8Bytes);
    void provideSaveChunk(QByteArray utf8Data, bool atEnd);
    void cancelCurrentOperation();

signals:
    void operationChanged(vinson::FileManager::Operation operation);
    void loadPrepared(vinson::FileLoadInfo info);
    void loadChunk(QByteArray utf8Data, qint64 bytesRead, qint64 totalBytes);
    void loadCompleted(vinson::FileLoadInfo info);
    void saveChunkRequested(qint64 offset, qint64 maximumBytes);
    void saveProgress(qint64 bytesWritten, qint64 totalBytes);
    void saveCompleted(vinson::FileSaveResult result);
    void operationFailed(QString message);
    void operationCanceled();

private:
    void handleLoadChunk(QByteArray data, qint64 bytesRead, qint64 totalBytes);
    bool startSaver(FileSaver* saver);
    void finishOperation();
    void stopWorkerThread();

    Operation operation_ = Operation::None;
    QPointer<QThread> workerThread_;
    QPointer<FileLoader> loader_;
    QPointer<FileSaver> saver_;
};

} // namespace vinson
