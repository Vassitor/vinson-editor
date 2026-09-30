#include "session/RecoveryManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>

namespace vinson {
namespace {

QString dataPath(const QString& directory, const QString& id)
{
    return QDir(directory).filePath(id + QStringLiteral(".data"));
}

QString metadataPath(const QString& directory, const QString& id)
{
    return QDir(directory).filePath(id + QStringLiteral(".json"));
}

} // namespace

RecoveryManager::RecoveryManager(QObject* parent)
    : RecoveryManager(defaultDirectoryPath(), parent)
{
}

RecoveryManager::RecoveryManager(const QString& directoryPath, QObject* parent)
    : QObject(parent)
    , directoryPath_(QDir::cleanPath(directoryPath))
    , workerThread_(new QThread(this))
    , worker_(new QObject)
{
    worker_->moveToThread(workerThread_);
    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_->start();
}

RecoveryManager::~RecoveryManager()
{
    flush();
    workerThread_->quit();
    workerThread_->wait();
}

QVector<RecoveryEntry> RecoveryManager::entries() const
{
    flush();
    QVector<RecoveryEntry> result;
    const QDir directory(directoryPath_);
    for (const QString& fileName : directory.entryList(
             {QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        QFile metadata(directory.filePath(fileName));
        if (!metadata.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument document = QJsonDocument::fromJson(metadata.readAll());
        const QJsonObject object = document.object();
        RecoveryEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        if (!isValidId(entry.id)) {
            continue;
        }
        const QFileInfo data(dataPath(directoryPath_, entry.id));
        if (!data.isFile() || data.size() < 0
            || data.size() > maximumSnapshotBytes) {
            continue;
        }
        entry.originalPath = object.value(
            QStringLiteral("originalPath")).toString();
        entry.displayName = object.value(QStringLiteral("displayName")).toString();
        entry.encoding = static_cast<TextEncoding>(object.value(
            QStringLiteral("encoding")).toInt(static_cast<int>(TextEncoding::Utf8)));
        entry.lineEnding = static_cast<LineEnding>(object.value(
            QStringLiteral("lineEnding")).toInt(static_cast<int>(LineEnding::None)));
        entry.originalFileSize = object.value(
            QStringLiteral("originalFileSize")).toInteger();
        entry.contentSize = data.size();
        entry.updatedAt = QDateTime::fromString(
            object.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
        result.append(std::move(entry));
    }
    std::sort(result.begin(), result.end(),
              [](const RecoveryEntry& first, const RecoveryEntry& second) {
                  return first.updatedAt < second.updatedAt;
              });
    return result;
}

QByteArray RecoveryManager::loadContent(const QString& id) const
{
    if (!isValidId(id)) {
        return {};
    }
    flush();
    QFile file(dataPath(directoryPath_, id));
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumSnapshotBytes) {
        return {};
    }
    return file.readAll();
}

QString RecoveryManager::directoryPath() const
{
    return directoryPath_;
}

void RecoveryManager::queueSnapshot(RecoverySnapshot snapshot)
{
    if (!isValidId(snapshot.entry.id)
        || snapshot.content.size() > maximumSnapshotBytes) {
        return;
    }
    snapshot.entry.contentSize = snapshot.content.size();
    snapshot.entry.updatedAt = QDateTime::currentDateTimeUtc();
    const QString directory = directoryPath_;
    QMetaObject::invokeMethod(
        worker_, [directory, snapshot = std::move(snapshot)] {
            writeSnapshot(directory, snapshot);
        }, Qt::QueuedConnection);
}

void RecoveryManager::removeSnapshot(const QString& id)
{
    if (!isValidId(id)) {
        return;
    }
    const QString directory = directoryPath_;
    QMetaObject::invokeMethod(
        worker_, [directory, id] { removeSnapshotFiles(directory, id); },
        Qt::QueuedConnection);
}

void RecoveryManager::clearAll()
{
    const QString directory = directoryPath_;
    QMetaObject::invokeMethod(worker_, [directory] {
        const QDir recoveryDirectory(directory);
        for (const QString& name : recoveryDirectory.entryList(
                 {QStringLiteral("*.json"), QStringLiteral("*.data")},
                 QDir::Files)) {
            QFile::remove(recoveryDirectory.filePath(name));
        }
    }, Qt::QueuedConnection);
}

void RecoveryManager::flush() const
{
    if (!workerThread_->isRunning()
        || QThread::currentThread() == workerThread_) {
        return;
    }
    QMetaObject::invokeMethod(worker_, [] {}, Qt::BlockingQueuedConnection);
}

QString RecoveryManager::defaultDirectoryPath()
{
    return QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("recovery"));
}

bool RecoveryManager::isValidId(const QString& id)
{
    static const QRegularExpression valid(
        QStringLiteral("^[A-Za-z0-9_-]{1,80}$"));
    return valid.match(id).hasMatch();
}

void RecoveryManager::writeSnapshot(const QString& directory,
                                    const RecoverySnapshot& snapshot)
{
    if (!QDir().mkpath(directory)) {
        return;
    }
    QSaveFile data(dataPath(directory, snapshot.entry.id));
    if (!data.open(QIODevice::WriteOnly)
        || data.write(snapshot.content) != snapshot.content.size()
        || !data.commit()) {
        data.cancelWriting();
        return;
    }

    QJsonObject object;
    object.insert(QStringLiteral("id"), snapshot.entry.id);
    object.insert(QStringLiteral("originalPath"), snapshot.entry.originalPath);
    object.insert(QStringLiteral("displayName"), snapshot.entry.displayName);
    object.insert(QStringLiteral("encoding"),
                  static_cast<int>(snapshot.entry.encoding));
    object.insert(QStringLiteral("lineEnding"),
                  static_cast<int>(snapshot.entry.lineEnding));
    object.insert(QStringLiteral("originalFileSize"),
                  snapshot.entry.originalFileSize);
    object.insert(QStringLiteral("contentSize"), snapshot.content.size());
    object.insert(QStringLiteral("updatedAt"),
                  snapshot.entry.updatedAt.toString(Qt::ISODateWithMs));
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QSaveFile metadata(metadataPath(directory, snapshot.entry.id));
    if (!metadata.open(QIODevice::WriteOnly)
        || metadata.write(json) != json.size() || !metadata.commit()) {
        metadata.cancelWriting();
    }
}

void RecoveryManager::removeSnapshotFiles(const QString& directory,
                                          const QString& id)
{
    QFile::remove(dataPath(directory, id));
    QFile::remove(metadataPath(directory, id));
}

} // namespace vinson
