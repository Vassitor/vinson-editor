#pragma once

#include "file/FileTypes.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class QThread;

namespace vinson {

struct RecoveryEntry
{
    QString id;
    QString originalPath;
    QString displayName;
    TextEncoding encoding = TextEncoding::Utf8;
    LineEnding lineEnding = LineEnding::None;
    qint64 originalFileSize = 0;
    qint64 contentSize = 0;
    QDateTime updatedAt;
};

struct RecoverySnapshot
{
    RecoveryEntry entry;
    QByteArray content;
};

class RecoveryManager final : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 maximumSnapshotBytes = 8 * 1024 * 1024;

    explicit RecoveryManager(QObject* parent = nullptr);
    explicit RecoveryManager(const QString& directoryPath,
                             QObject* parent = nullptr);
    ~RecoveryManager() override;

    [[nodiscard]] QVector<RecoveryEntry> entries() const;
    [[nodiscard]] QByteArray loadContent(const QString& id) const;
    [[nodiscard]] QString directoryPath() const;

    void queueSnapshot(RecoverySnapshot snapshot);
    void removeSnapshot(const QString& id);
    void clearAll();
    void flush() const;

private:
    [[nodiscard]] static QString defaultDirectoryPath();
    [[nodiscard]] static bool isValidId(const QString& id);
    static void writeSnapshot(const QString& directory,
                              const RecoverySnapshot& snapshot);
    static void removeSnapshotFiles(const QString& directory,
                                    const QString& id);

    QString directoryPath_;
    QThread* workerThread_ = nullptr;
    QObject* worker_ = nullptr;
};

} // namespace vinson
