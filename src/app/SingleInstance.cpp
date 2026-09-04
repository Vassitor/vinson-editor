#include "app/SingleInstance.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QThread>
#include <QTimer>
#include <QUuid>

namespace vinson {

namespace {

QString applicationIdentityHash()
{
    QByteArray applicationIdentity = qgetenv("VINSON_SINGLE_INSTANCE_ID");
    if (applicationIdentity.isEmpty()) {
        applicationIdentity = QCoreApplication::organizationName().toUtf8()
            + '\0' + QCoreApplication::applicationName().toUtf8();
    }
    return QString::fromLatin1(QCryptographicHash::hash(
        applicationIdentity,
        QCryptographicHash::Sha256).toHex().left(16));
}

void configureStream(QDataStream& stream)
{
    stream.setVersion(QDataStream::Qt_6_5);
}

} // namespace

SingleInstance::SingleInstance(QObject* parent)
    : QObject(parent)
{
    const QString identity = applicationIdentityHash();
    requestDirectory_ = QDir::temp().filePath(
        QStringLiteral("VinsonEditor-%1-requests").arg(identity));
    readyFilePath_ = QDir(requestDirectory_).filePath(
        QStringLiteral("primary.ready"));
    lockFile_ = new QLockFile(QDir::temp().filePath(
        QStringLiteral("VinsonEditor-%1.lock").arg(identity)));
    lockFile_->setStaleLockTime(0);

    requestTimer_ = new QTimer(this);
    requestTimer_->setInterval(40);
    connect(requestTimer_, &QTimer::timeout,
            this, &SingleInstance::processRequests);
}

SingleInstance::~SingleInstance()
{
    if (primary_) {
        QFile::remove(readyFilePath_);
    }
    delete lockFile_;
}

SingleInstance::StartResult SingleInstance::start(const QStringList& paths)
{
    if (lockFile_->tryLock()) {
        QDir requestDirectory;
        if (!requestDirectory.mkpath(requestDirectory_)) {
            lockFile_->unlock();
            errorString_ = tr("Could not create the single-instance request directory.");
            return StartResult::Error;
        }
        QFile::remove(readyFilePath_);
        const QDir directory(requestDirectory_);
        for (const QString& fileName : directory.entryList(
                 {QStringLiteral("*.request")}, QDir::Files)) {
            QFile::remove(directory.filePath(fileName));
        }
        QSaveFile readyFile(readyFilePath_);
        if (!readyFile.open(QIODevice::WriteOnly)
            || readyFile.write(QByteArray::number(
                   QCoreApplication::applicationPid())) < 0
            || !readyFile.commit()) {
            lockFile_->unlock();
            errorString_ = readyFile.errorString();
            return StartResult::Error;
        }
        primary_ = true;
        requestTimer_->start();
        errorString_.clear();
        return StartResult::Primary;
    }

    return forward(paths) ? StartResult::Forwarded : StartResult::Error;
}

QString SingleInstance::errorString() const
{
    return errorString_;
}

bool SingleInstance::forward(const QStringList& paths)
{
    if (!QDir().mkpath(requestDirectory_)) {
        errorString_ = tr("Could not create the single-instance request directory.");
        return false;
    }

    qint64 primaryPid = 0;
    QString primaryHost;
    QString primaryApplication;
    if (!lockFile_->getLockInfo(
            &primaryPid, &primaryHost, &primaryApplication)) {
        errorString_ = tr("Could not identify the running instance.");
        return false;
    }
    QElapsedTimer readinessTimer;
    readinessTimer.start();
    bool primaryIsReady = false;
    while (readinessTimer.elapsed() < 1500) {
        QFile readyFile(readyFilePath_);
        if (readyFile.open(QIODevice::ReadOnly)
            && readyFile.readAll().trimmed()
                == QByteArray::number(primaryPid)) {
            primaryIsReady = true;
            break;
        }
        QThread::msleep(10);
    }
    if (!primaryIsReady) {
        errorString_ = tr("The running instance is not ready.");
        return false;
    }

    const QString requestPath = QDir(requestDirectory_).filePath(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
        + QStringLiteral(".request"));
    QSaveFile request(requestPath);
    if (!request.open(QIODevice::WriteOnly)) {
        errorString_ = request.errorString();
        return false;
    }
    QDataStream stream(&request);
    configureStream(stream);
    stream << paths;
    if (stream.status() != QDataStream::Ok || !request.commit()) {
        errorString_ = request.errorString();
        return false;
    }

    QElapsedTimer acknowledgementTimer;
    acknowledgementTimer.start();
    while (QFileInfo::exists(requestPath)
           && acknowledgementTimer.elapsed() < 3000) {
        QThread::msleep(10);
    }
    if (!QFileInfo::exists(requestPath)) {
        errorString_.clear();
        return true;
    }

    QFile::remove(requestPath);
    errorString_ = tr("The running instance did not acknowledge the request.");
    return false;
}

void SingleInstance::processRequests()
{
    const QDir directory(requestDirectory_);
    const QStringList requestFiles = directory.entryList(
        {QStringLiteral("*.request")}, QDir::Files, QDir::Name);
    for (const QString& fileName : requestFiles) {
        const QString requestPath = directory.filePath(fileName);
        QFile request(requestPath);
        if (!request.open(QIODevice::ReadOnly)) {
            continue;
        }
        if (request.size() > 1024 * 1024) {
            request.close();
            QFile::remove(requestPath);
            continue;
        }
        QDataStream stream(&request);
        configureStream(stream);
        QStringList paths;
        stream >> paths;
        request.close();
        if (stream.status() == QDataStream::Ok) {
            emit openRequested(paths);
        }
        QFile::remove(requestPath);
    }
}

} // namespace vinson
