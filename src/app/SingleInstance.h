#pragma once

#include <QObject>
#include <QStringList>

class QLockFile;
class QTimer;

namespace vinson {

class SingleInstance final : public QObject
{
    Q_OBJECT

public:
    enum class StartResult {
        Primary,
        Forwarded,
        Error,
    };

    explicit SingleInstance(QObject* parent = nullptr);
    ~SingleInstance() override;
    [[nodiscard]] StartResult start(const QStringList& paths);
    [[nodiscard]] QString errorString() const;

signals:
    void openRequested(const QStringList& paths);

private:
    [[nodiscard]] bool forward(const QStringList& paths);
    void processRequests();

    QLockFile* lockFile_ = nullptr;
    QTimer* requestTimer_ = nullptr;
    QString requestDirectory_;
    QString readyFilePath_;
    QString errorString_;
    bool primary_ = false;
};

} // namespace vinson
