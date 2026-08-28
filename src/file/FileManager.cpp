#include "file/FileManager.h"

#include "file/FileLoader.h"
#include "file/FileSaver.h"

#include <QFileInfo>
#include <QThread>

namespace vinson {

FileManager::FileManager(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<FileLoadInfo>();
    qRegisterMetaType<FileSaveResult>();
}

FileManager::~FileManager()
{
    cancelCurrentOperation();
    stopWorkerThread();
}

bool FileManager::isBusy() const noexcept
{
    return operation_ != Operation::None;
}

FileManager::Operation FileManager::operation() const noexcept
{
    return operation_;
}

bool FileManager::openFile(const QString& path)
{
    if (isBusy() || path.isEmpty()) {
        return false;
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    auto* thread = new QThread(this);
    auto* loader = new FileLoader(absolutePath);
    loader->moveToThread(thread);
    workerThread_ = thread;
    loader_ = loader;
    operation_ = Operation::Loading;

    connect(thread, &QThread::started, loader, &FileLoader::load);
    connect(loader, &FileLoader::prepared, this, &FileManager::loadPrepared);
    connect(loader, &FileLoader::chunkReady,
            this, &FileManager::handleLoadChunk);
    connect(loader, &FileLoader::completed, this,
            [this](const FileLoadInfo& info) {
                finishOperation();
                emit loadCompleted(info);
            });
    connect(loader, &FileLoader::failed, this,
            [this](const QString& message) {
                finishOperation();
                emit operationFailed(message);
            });
    connect(loader, &FileLoader::canceled, this, [this] {
        finishOperation();
        emit operationCanceled();
    });
    connect(loader, &FileLoader::completed, loader, &QObject::deleteLater);
    connect(loader, &FileLoader::failed, loader, &QObject::deleteLater);
    connect(loader, &FileLoader::canceled, loader, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    emit operationChanged(operation_);
    thread->start();
    return true;
}

bool FileManager::saveFile(const QString& path, QByteArray utf8Data,
                           TextEncoding encoding)
{
    if (isBusy() || path.isEmpty()) {
        return false;
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    auto* saver = new FileSaver(absolutePath, std::move(utf8Data), encoding);
    return startSaver(saver);
}

bool FileManager::saveFileStreaming(const QString& path, TextEncoding encoding,
                                    qint64 totalUtf8Bytes)
{
    if (isBusy() || path.isEmpty() || totalUtf8Bytes < 0) {
        return false;
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    auto* saver = new FileSaver(absolutePath, encoding, totalUtf8Bytes);
    return startSaver(saver);
}

void FileManager::provideSaveChunk(QByteArray utf8Data, bool atEnd)
{
    if (saver_) {
        saver_->provideChunk(std::move(utf8Data), atEnd);
    }
}

bool FileManager::startSaver(FileSaver* saver)
{
    auto* thread = new QThread(this);
    saver->moveToThread(thread);
    workerThread_ = thread;
    saver_ = saver;
    operation_ = Operation::Saving;

    connect(thread, &QThread::started, saver, &FileSaver::save);
    connect(saver, &FileSaver::chunkRequested,
            this, &FileManager::saveChunkRequested);
    connect(saver, &FileSaver::progress, this, &FileManager::saveProgress);
    connect(saver, &FileSaver::completed, this,
            [this](const FileSaveResult& result) {
                finishOperation();
                emit saveCompleted(result);
            });
    connect(saver, &FileSaver::failed, this,
            [this](const QString& message) {
                finishOperation();
                emit operationFailed(message);
            });
    connect(saver, &FileSaver::canceled, this, [this] {
        finishOperation();
        emit operationCanceled();
    });
    connect(saver, &FileSaver::completed, saver, &QObject::deleteLater);
    connect(saver, &FileSaver::failed, saver, &QObject::deleteLater);
    connect(saver, &FileSaver::canceled, saver, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    emit operationChanged(operation_);
    thread->start();
    return true;
}

void FileManager::cancelCurrentOperation()
{
    if (loader_) {
        loader_->requestCancel();
    }
    if (saver_) {
        saver_->requestCancel();
    }
}

void FileManager::handleLoadChunk(QByteArray data, qint64 bytesRead,
                                  qint64 totalBytes)
{
    emit loadChunk(std::move(data), bytesRead, totalBytes);
    if (loader_) {
        loader_->acknowledgeChunk();
    }
}

void FileManager::finishOperation()
{
    if (workerThread_) {
        workerThread_->quit();
        // The terminal worker signal is emitted immediately before its slot
        // returns, so this wait is normally only a scheduling hand-off. It
        // guarantees no QThread child remains running during shutdown.
        workerThread_->wait();
    }
    loader_.clear();
    saver_.clear();
    workerThread_.clear();
    operation_ = Operation::None;
    emit operationChanged(operation_);
}

void FileManager::stopWorkerThread()
{
    if (!workerThread_) {
        return;
    }
    workerThread_->quit();
    workerThread_->wait();
}

} // namespace vinson
