#pragma once

#include <QObject>
#include <QKeySequence>

class QAction;
class QMainWindow;
class QMenu;
class QSystemTrayIcon;

namespace vinson {

class GlobalShortcut;

class TrayController final : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QMainWindow* window, QObject* parent = nullptr);

    [[nodiscard]] bool isAvailable() const noexcept;
    [[nodiscard]] QKeySequence bossKey() const;
    [[nodiscard]] QKeySequence focusShortcut() const;
    void show();

public slots:
    void toggleWindowVisibility();
    void showWindow();
    void hideWindow();
    void focusWindow();
    bool setBossKey(const QKeySequence& shortcut);
    bool setFocusShortcut(const QKeySequence& shortcut);

signals:
    void quitRequested();
    void settingsRequested();
    void editorFocusRequested();
    void bossKeyRegistrationFailed(const QKeySequence& activeShortcut,
                                   const QString& message);
    void focusShortcutRegistrationFailed(
        const QKeySequence& activeShortcut, const QString& message);

private:
    void updateToggleAction();

    QMainWindow* window_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* toggleAction_ = nullptr;
    QAction* settingsAction_ = nullptr;
    QAction* bossKeyAction_ = nullptr;
    QAction* focusShortcutAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    GlobalShortcut* bossKeyShortcut_ = nullptr;
    GlobalShortcut* focusShortcut_ = nullptr;
    bool available_ = false;
};

} // namespace vinson
