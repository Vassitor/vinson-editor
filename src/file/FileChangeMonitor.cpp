#include "file/FileChangeMonitor.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include <algorithm>

namespace vinson {

FileChangeMonitor::FileChangeMonitor(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Change>();
    connect(&watcher_, &QFileSystemWatcher::fileChanged,
            this, &FileChangeMonitor::handleFileChanged);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged,
            this, &FileChangeMonitor::handleDirectoryChanged);
}

void FileChangeMonitor::watchFile(const QString& path)
{
    const QString normalized = normalizedPath(path);
    if (normalized.isEmpty()) {
        return;
    }
    desiredFiles_.insert(normalized);
    suspendedFiles_.remove(normalized);
    knownExists_.insert(normalized, QFileInfo::exists(normalized));
    ensureDirectoryWatch(normalized);
    ensureFileWatch(normalized);
}

void FileChangeMonitor::unwatchFile(const QString& path)
{
    const QString normalized = normalizedPath(path);
    desiredFiles_.remove(normalized);
    suspendedFiles_.remove(normalized);
    knownExists_.remove(normalized);
    if (watcher_.files().contains(normalized)) {
        watcher_.removePath(normalized);
    }
    rebuildDirectoryWatches();
}

void FileChangeMonitor::suspendFile(const QString& path)
{
    const QString normalized = normalizedPath(path);
    if (!desiredFiles_.contains(normalized)) {
        return;
    }
    suspendedFiles_.insert(normalized);
    if (watcher_.files().contains(normalized)) {
        watcher_.removePath(normalized);
    }
}

void FileChangeMonitor::resumeFile(const QString& path)
{
    const QString normalized = normalizedPath(path);
    if (!desiredFiles_.contains(normalized)) {
        return;
    }
    suspendedFiles_.remove(normalized);
    knownExists_.insert(normalized, QFileInfo::exists(normalized));
    ensureDirectoryWatch(normalized);
    ensureFileWatch(normalized);
}

void FileChangeMonitor::refreshFile(const QString& path)
{
    const QString normalized = normalizedPath(path);
    if (!desiredFiles_.contains(normalized)) {
        return;
    }
    if (watcher_.files().contains(normalized)) {
        watcher_.removePath(normalized);
    }
    knownExists_.insert(normalized, QFileInfo::exists(normalized));
    ensureDirectoryWatch(normalized);
    ensureFileWatch(normalized);
}

bool FileChangeMonitor::isWatching(const QString& path) const
{
    return desiredFiles_.contains(normalizedPath(path));
}

QStringList FileChangeMonitor::watchedFiles() const
{
    QStringList files(desiredFiles_.cbegin(), desiredFiles_.cend());
    files.sort();
    return files;
}

QString FileChangeMonitor::normalizedPath(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

void FileChangeMonitor::ensureFileWatch(const QString& path)
{
    if (!desiredFiles_.contains(path) || suspendedFiles_.contains(path)
        || !QFileInfo::exists(path) || watcher_.files().contains(path)) {
        return;
    }
    watcher_.addPath(path);
}

void FileChangeMonitor::ensureDirectoryWatch(const QString& path)
{
    const QString directory = QFileInfo(path).absolutePath();
    if (!directory.isEmpty() && QDir(directory).exists()
        && !watcher_.directories().contains(directory)) {
        watcher_.addPath(directory);
    }
}

void FileChangeMonitor::rebuildDirectoryWatches()
{
    QSet<QString> requiredDirectories;
    for (const QString& path : std::as_const(desiredFiles_)) {
        requiredDirectories.insert(QFileInfo(path).absolutePath());
    }
    for (const QString& directory : watcher_.directories()) {
        if (!requiredDirectories.contains(directory)) {
            watcher_.removePath(directory);
        }
    }
    for (const QString& path : std::as_const(desiredFiles_)) {
        ensureDirectoryWatch(path);
    }
}

void FileChangeMonitor::handleFileChanged(const QString& path)
{
    const QString normalized = normalizedPath(path);
    if (!desiredFiles_.contains(normalized)
        || suspendedFiles_.contains(normalized)) {
        return;
    }
    const bool exists = QFileInfo::exists(normalized);
    knownExists_.insert(normalized, exists);
    emit fileChangedExternally(
        normalized, exists ? Change::Modified : Change::Removed);
    if (exists) {
        QTimer::singleShot(0, this,
                           [this, normalized] { ensureFileWatch(normalized); });
    }
}

void FileChangeMonitor::handleDirectoryChanged(const QString& directory)
{
    QTimer::singleShot(0, this, [this, directory] {
        for (const QString& path : std::as_const(desiredFiles_)) {
            if (suspendedFiles_.contains(path)
                || QFileInfo(path).absolutePath() != directory) {
                continue;
            }
            const bool exists = QFileInfo::exists(path);
            const bool previouslyExisted = knownExists_.value(path, exists);
            knownExists_.insert(path, exists);
            if (exists != previouslyExisted) {
                emit fileChangedExternally(
                    path, exists ? Change::Modified : Change::Removed);
            }
            if (exists) {
                ensureFileWatch(path);
            }
        }
    });
}

} // namespace vinson
