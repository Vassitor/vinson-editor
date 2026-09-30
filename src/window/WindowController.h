#pragma once

#include <QObject>
#include <QByteArray>
#include <QPoint>
#include <QPointer>
#include <QSize>

class QMainWindow;
class QWidget;

namespace vinson {

class EditorWidget;

class WindowController final : public QObject
{
    Q_OBJECT

public:
    explicit WindowController(QMainWindow* window, QObject* parent = nullptr);
    ~WindowController() override;

    [[nodiscard]] bool isFrameless() const noexcept;
    [[nodiscard]] bool isAlwaysOnTop() const noexcept;
    [[nodiscard]] bool isMinimalMode() const noexcept;
    [[nodiscard]] bool isTaskbarVisible() const noexcept;
    [[nodiscard]] bool persistableFrameless() const noexcept;
    [[nodiscard]] QByteArray persistableGeometry() const;
    void configureMinimalMode(EditorWidget* editor, QWidget* transientPanel);

public slots:
    void setFrameless(bool enabled);
    void toggleFrameless();
    void setAlwaysOnTop(bool enabled);
    void toggleAlwaysOnTop();
    void setMinimalMode(bool enabled);
    void toggleMinimalMode();
    void refreshMinimumSize();

signals:
    void framelessChanged(bool enabled);
    void alwaysOnTopChanged(bool enabled);
    void minimalModeChanged(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] bool belongsToManagedWindow(QObject* watched) const;
    [[nodiscard]] Qt::Edges resizeEdgesAt(const QPoint& globalPosition) const;
    [[nodiscard]] bool beginSystemMove();
    [[nodiscard]] bool beginSystemResize(Qt::Edges edges);
    void applyWindowFlag(Qt::WindowType flag, bool enabled);
    void refreshEditorScrollBars();
    void updateTaskbarVisibility();
    void updateResizeCursor(QWidget* widget, Qt::Edges edges);
    void clearResizeCursor();

    struct FramelessUiState
    {
        QSize windowMinimumSize;
        QSize editorMinimumSize;
        bool menuBarVisible = true;
        bool statusBarVisible = true;
        bool valid = false;
    };

    struct MinimalUiState
    {
        QSize windowMinimumSize;
        QSize editorMinimumSize;
        bool menuBarVisible = true;
        bool statusBarVisible = true;
        bool transientPanelVisible = false;
        bool lineNumbersVisible = true;
        bool horizontalScrollBarVisible = true;
        bool verticalScrollBarVisible = true;
        bool frameless = false;
        QByteArray windowGeometry;
    };

    QMainWindow* window_ = nullptr;
    EditorWidget* editor_ = nullptr;
    QWidget* transientPanel_ = nullptr;
    QPointer<QWidget> resizeCursorWidget_;
    FramelessUiState framelessUiState_;
    MinimalUiState minimalUiState_;
    bool frameless_ = false;
    bool alwaysOnTop_ = false;
    bool minimalMode_ = false;
    bool taskbarVisible_ = true;
};

} // namespace vinson
