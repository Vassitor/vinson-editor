#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

namespace vinson {

class GlobalShortcut final : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalShortcut(QObject* parent = nullptr);
    ~GlobalShortcut() override;

    [[nodiscard]] static QKeySequence defaultShortcut();
    [[nodiscard]] static bool isSupportedShortcut(
        const QKeySequence& shortcut) noexcept;
    [[nodiscard]] const QKeySequence& shortcut() const noexcept;
    [[nodiscard]] bool isRegistered() const noexcept;
    bool setShortcut(const QKeySequence& shortcut, QString* error = nullptr);

    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

signals:
    void activated();

private:
    void unregisterShortcut();

    QKeySequence shortcut_;
    bool registered_ = false;
};

} // namespace vinson
