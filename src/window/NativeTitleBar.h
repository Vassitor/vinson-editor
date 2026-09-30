#pragma once

#include <QObject>
#include <QRect>

class QMainWindow;
class QTabBar;
class QWidget;

namespace vinson {

// Extends the client area without replacing Windows caption buttons or
// removing the standard resize, system-menu and maximize window styles.
class NativeTitleBar final : public QObject
{
public:
    NativeTitleBar(QMainWindow* window, QTabBar* tabs);
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] QRect captionButtonRect() const;
    [[nodiscard]] QRect captionPaintRect() const;
    bool nativeEvent(void* message, qintptr* result);
    void refresh();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] int maximizedTopInset() const;
    void scheduleRefresh();
    void layoutHeader();

    QMainWindow* window_;
    QTabBar* tabs_;
    QWidget* header_ = nullptr;
    bool supported_ = false;
    bool refreshPending_ = false;
    quintptr configuredHandle_ = 0;
    bool configuredActive_ = false;
};

} // namespace vinson
