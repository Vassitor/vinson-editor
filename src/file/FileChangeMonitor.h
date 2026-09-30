#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

namespace vinson {

class FileChangeMonitor final : public QObject
{
    Q_OBJECT

public:
    enum class Change {
        Modified,
        Removed,
    };
    Q_ENUM(Change)

    explicit FileChangeMonitor(QObject* parent = nullptr);

    void watchFile(const QString& path);
    void unwatchFile(const QString& path);
    void suspendFile(const QString& path);
    void resumeFile(const QString& path);
    void refreshFile(const QString& path);

    [[nodiscard]] bool isWatching(const QString& path) const;
    [[nodiscard]] QStringList watchedFiles() const;

signals:
    void fileChangedExternally(QString path,
                               vinson::FileChangeMonitor::Change change);

private:
    [[nodiscard]] static QString normalizedPath(const QString& path);
    void ensureFileWatch(const QString& path);
    void ensureDirectoryWatch(const QString& path);
    void rebuildDirectoryWatches();
    void handleFileChanged(const QString& path);
    void handleDirectoryChanged(const QString& directory);

    QFileSystemWatcher watcher_;
    QSet<QString> desiredFiles_;
    QSet<QString> suspendedFiles_;
    QHash<QString, bool> knownExists_;
};

} // namespace vinson

Q_DECLARE_METATYPE(vinson::FileChangeMonitor::Change)
