#include "window/WindowController.h"

#include "editor/EditorWidget.h"
#include "window/NativeWindowAppearance.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLayout>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace vinson {
namespace {

constexpr int resizeBorderWidth = 6;

Qt::CursorShape cursorForEdges(Qt::Edges edges)
{
    if (edges == Qt::TopEdge) {
        return Qt::SizeAllCursor;
    }
    if (edges == (Qt::LeftEdge | Qt::TopEdge)
        || edges == (Qt::RightEdge | Qt::BottomEdge)) {
        return Qt::SizeFDiagCursor;
    }
    if (edges == (Qt::RightEdge | Qt::TopEdge)
        || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
        return Qt::SizeBDiagCursor;
    }
    if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge)) {
        return Qt::SizeHorCursor;
    }
    return Qt::SizeVerCursor;
}

} // namespace

WindowController::WindowController(QMainWindow* window, QObject* parent)
    : QObject(parent)
    , window_(window)
    , frameless_(window != nullptr
          && window->windowFlags().testFlag(Qt::FramelessWindowHint))
    , alwaysOnTop_(window != nullptr
          && window->windowFlags().testFlag(Qt::WindowStaysOnTopHint))
{
    Q_ASSERT(window_ != nullptr);
    window_->setMouseTracking(true);
    qApp->installEventFilter(this);
}

WindowController::~WindowController()
{
    if (qApp != nullptr) {
        qApp->removeEventFilter(this);
    }
}

bool WindowController::isFrameless() const noexcept
{
    return frameless_;
}

bool WindowController::isAlwaysOnTop() const noexcept
{
    return alwaysOnTop_;
}

bool WindowController::isMinimalMode() const noexcept
{
    return minimalMode_;
}

bool WindowController::isTaskbarVisible() const noexcept
{
    return taskbarVisible_;
}

bool WindowController::persistableFrameless() const noexcept
{
    return minimalMode_ ? minimalUiState_.frameless : frameless_;
}

QByteArray WindowController::persistableGeometry() const
{
    return minimalMode_ ? minimalUiState_.windowGeometry
                        : window_->saveGeometry();
}

void WindowController::configureMinimalMode(EditorWidget* editor,
                                            QWidget* transientPanel)
{
    Q_ASSERT(editor != nullptr);
    editor_ = editor;
    transientPanel_ = transientPanel;
}

void WindowController::setFrameless(bool enabled)
{
    if (minimalMode_ && !enabled) {
        return;
    }
    if (frameless_ == enabled) {
        return;
    }

    bool restoreChrome = false;
    bool menuBarVisible = false;
    bool statusBarVisible = false;
    if (enabled) {
        framelessUiState_ = {
            window_->minimumSize(),
            editor_ != nullptr ? editor_->minimumSize() : QSize{},
            !window_->menuBar()->isHidden(),
            !window_->statusBar()->isHidden(),
            true,
        };
        window_->menuBar()->hide();
        window_->statusBar()->hide();
    } else if (!minimalMode_ && framelessUiState_.valid) {
        restoreChrome = true;
        menuBarVisible = framelessUiState_.menuBarVisible;
        statusBarVisible = framelessUiState_.statusBarVisible;
        window_->setMinimumSize(framelessUiState_.windowMinimumSize);
        if (editor_ != nullptr) {
            editor_->setMinimumSize(framelessUiState_.editorMinimumSize);
        }

        // Restore child visibility before Qt recreates and shows the native
        // framed window. QMainWindow then includes both bars in its first
        // layout pass instead of leaving the status bar below the client area.
        window_->menuBar()->setVisible(menuBarVisible);
        window_->statusBar()->setVisible(statusBarVisible);
        if (window_->layout() != nullptr) {
            window_->layout()->invalidate();
            window_->layout()->activate();
        }
    }
    frameless_ = enabled;
    clearResizeCursor();
    applyWindowFlag(Qt::FramelessWindowHint, enabled);
    updateTaskbarVisibility();
    if (restoreChrome) {
        window_->menuBar()->setVisible(menuBarVisible);
        window_->statusBar()->setVisible(statusBarVisible);
        window_->statusBar()->updateGeometry();
        if (window_->layout() != nullptr) {
            window_->layout()->invalidate();
            window_->layout()->activate();
        }
        framelessUiState_.valid = false;

        // Windows can deliver one more layout pass after the native frame is
        // recreated. Reapply the captured chrome state after that pass so the
        // status bar cannot remain collapsed at the bottom of the window.
        QTimer::singleShot(0, this,
            [this, menuBarVisible, statusBarVisible] {
                if (frameless_ || minimalMode_) {
                    return;
                }
                window_->menuBar()->setVisible(menuBarVisible);
                if (statusBarVisible) {
                    // A plain setVisible(true) is a no-op when the native
                    // frame transition left the status bar logically visible
                    // but outside QMainWindow's active layout. Cycling its
                    // visibility emits the layout requests needed to dock it
                    // back into the bottom of the client area.
                    window_->statusBar()->hide();
                    window_->statusBar()->show();
                    window_->statusBar()->raise();
                } else {
                    window_->statusBar()->hide();
                }

                window_->statusBar()->updateGeometry();
                if (window_->layout() != nullptr) {
                    window_->layout()->invalidate();
                    window_->layout()->activate();
                }

                // On Windows, adding the native frame can reduce the client
                // area without changing QWidget::size(). Qt consequently
                // sends no resize event and QMainWindow keeps the status bar
                // at the old, now-clipped client bottom. A one-pixel resize
                // makes Qt recalculate the real framed client area; restore
                // the requested outer size on the next event-loop turn.
                if (!window_->isMaximized() && !window_->isFullScreen()) {
                    const QSize framedSize = window_->size();
                    window_->resize(framedSize.width(),
                                    framedSize.height() + 1);
                    QTimer::singleShot(0, this, [this, framedSize] {
                        if (!frameless_ && !minimalMode_) {
                            window_->resize(framedSize);
                        }
                    });
                }
            });
    }
    emit framelessChanged(enabled);
    refreshMinimumSize();
}

void WindowController::toggleFrameless()
{
    setFrameless(!frameless_);
}

void WindowController::setAlwaysOnTop(bool enabled)
{
    if (alwaysOnTop_ == enabled) {
        return;
    }
    alwaysOnTop_ = enabled;
    applyWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    emit alwaysOnTopChanged(enabled);
}

void WindowController::toggleAlwaysOnTop()
{
    setAlwaysOnTop(!alwaysOnTop_);
}

void WindowController::setMinimalMode(bool enabled)
{
    if (minimalMode_ == enabled || editor_ == nullptr) {
        return;
    }

    if (enabled) {
        minimalUiState_ = {
            window_->minimumSize(),
            editor_->minimumSize(),
            !window_->menuBar()->isHidden(),
            !window_->statusBar()->isHidden(),
            transientPanel_ != nullptr && transientPanel_->isVisible(),
            editor_->areLineNumbersVisible(),
            editor_->hScrollBar(),
            editor_->vScrollBar(),
            frameless_,
            window_->saveGeometry(),
        };
        minimalMode_ = true;
        if (transientPanel_ != nullptr) {
            transientPanel_->hide();
        }
        setFrameless(true);
        editor_->setLineNumbersVisible(false);
        editor_->setHScrollBar(false);
        editor_->setVScrollBar(false);
        refreshMinimumSize();
    } else {
        minimalMode_ = false;
        window_->setMinimumSize(minimalUiState_.windowMinimumSize);
        editor_->setMinimumSize(minimalUiState_.editorMinimumSize);
        editor_->setLineNumbersVisible(minimalUiState_.lineNumbersVisible);
        editor_->setHScrollBar(minimalUiState_.horizontalScrollBarVisible);
        editor_->setVScrollBar(minimalUiState_.verticalScrollBarVisible);
        setFrameless(minimalUiState_.frameless);
        window_->menuBar()->setVisible(minimalUiState_.menuBarVisible);
        window_->statusBar()->setVisible(minimalUiState_.statusBarVisible);
        if (transientPanel_ != nullptr) {
            transientPanel_->setVisible(minimalUiState_.transientPanelVisible);
        }
        if (!minimalUiState_.windowGeometry.isEmpty()) {
            window_->restoreGeometry(minimalUiState_.windowGeometry);
        }
        refreshMinimumSize();
    }
    emit minimalModeChanged(enabled);
}

void WindowController::toggleMinimalMode()
{
    setMinimalMode(!minimalMode_);
}

void WindowController::refreshMinimumSize()
{
    if (!frameless_ || editor_ == nullptr) {
        return;
    }
    const QFontMetricsF metrics(editor_->editorFont());
    const int lineHeight = std::max(
        1, static_cast<int>(std::ceil(editor_->textHeightF(0))));
    const int minimumWidth = std::max(80,
        static_cast<int>(std::ceil(metrics.horizontalAdvance(
            QStringLiteral("MMMM")))) + 8);
    if (minimalMode_) {
        editor_->setMinimumSize(0, lineHeight);
        window_->setMinimumSize(minimumWidth, lineHeight);
    } else {
        // Override the editor's padded minimumSizeHint with its actual line
        // height. Include any visible panels in the window's layout minimum.
        editor_->setMinimumHeight(lineHeight);
        for (QWidget* parent = editor_->parentWidget(); parent != nullptr;
             parent = parent->parentWidget()) {
            if (parent->layout() != nullptr) {
                parent->layout()->invalidate();
                parent->layout()->activate();
            }
            if (parent == window_) {
                break;
            }
        }
        const int layoutHeight = window_->layout() != nullptr
            ? window_->layout()->totalMinimumSize().height() : 0;
        window_->setMinimumHeight(std::max(lineHeight, layoutHeight));
    }
}

bool WindowController::eventFilter(QObject* watched, QEvent* event)
{
    if (!belongsToManagedWindow(watched)) {
        return QObject::eventFilter(watched, event);
    }

    if (!frameless_) {
        return QObject::eventFilter(watched, event);
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    if (event->type() == QEvent::MouseMove) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->buttons() == Qt::NoButton) {
            const Qt::Edges edges = resizeEdgesAt(
                mouseEvent->globalPosition().toPoint());
            updateResizeCursor(widget, edges);
            if (edges != Qt::Edges{}) {
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (mouseEvent->modifiers().testFlag(Qt::AltModifier)) {
                return beginSystemMove();
            }
            const Qt::Edges edges = resizeEdgesAt(
                mouseEvent->globalPosition().toPoint());
            if (edges == Qt::TopEdge) {
                return beginSystemMove();
            }
            if (edges != Qt::Edges{}) {
                return beginSystemResize(edges);
            }
        }
    } else if (event->type() == QEvent::Leave && resizeCursorWidget_ == widget) {
        clearResizeCursor();
    }
    return QObject::eventFilter(watched, event);
}

bool WindowController::belongsToManagedWindow(QObject* watched) const
{
    const auto* widget = qobject_cast<const QWidget*>(watched);
    return widget != nullptr && widget->window() == window_;
}

Qt::Edges WindowController::resizeEdgesAt(const QPoint& globalPosition) const
{
    if (window_->isMaximized() || window_->isFullScreen()) {
        return {};
    }
    const QPoint position = window_->mapFromGlobal(globalPosition);
    const QRect area = window_->rect();
    if (!area.contains(position)) {
        return {};
    }

    Qt::Edges edges;
    if (position.x() < resizeBorderWidth) {
        edges |= Qt::LeftEdge;
    } else if (position.x() >= area.width() - resizeBorderWidth) {
        edges |= Qt::RightEdge;
    }
    if (position.y() < resizeBorderWidth) {
        edges |= Qt::TopEdge;
    } else if (position.y() >= area.height() - resizeBorderWidth) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

bool WindowController::beginSystemMove()
{
    QWindow* handle = window_->windowHandle();
    return handle != nullptr && handle->startSystemMove();
}

bool WindowController::beginSystemResize(Qt::Edges edges)
{
    QWindow* handle = window_->windowHandle();
    return handle != nullptr && handle->startSystemResize(edges);
}

void WindowController::applyWindowFlag(Qt::WindowType flag, bool enabled)
{
    const bool wasVisible = window_->isVisible();
    const QRect geometry = window_->geometry();
    const Qt::WindowStates state = window_->windowState();
    QPointer<QWidget> focusedWidget = QApplication::focusWidget();

    window_->setWindowFlag(flag, enabled);
    if (!wasVisible) {
        return;
    }

    // setWindowFlag may recreate the HWND. Prepare per-pixel composition
    // before showing the replacement framed window.
    (void)window_->winId();
    (void)setNativeBackgroundAlphaEnabled(window_, true);
    window_->setWindowState(state);
    window_->show();
    if (state == Qt::WindowNoState) {
        window_->setGeometry(geometry);
    }
    if (focusedWidget != nullptr && focusedWidget->window() == window_) {
        focusedWidget->setFocus(Qt::OtherFocusReason);
    }
    refreshEditorScrollBars();
}

void WindowController::refreshEditorScrollBars()
{
    if (editor_ == nullptr) {
        return;
    }

    const auto refresh = [editor = QPointer<EditorWidget>(editor_)] {
        if (editor == nullptr) {
            return;
        }
        editor->refreshScrollBarLayout();
    };
    refresh();
    QTimer::singleShot(0, editor_, refresh);
}

void WindowController::updateTaskbarVisibility()
{
    taskbarVisible_ = !frameless_ && !minimalMode_;
    setNativeTaskbarVisible(window_, taskbarVisible_);

    // setWindowFlag() can recreate the HWND. Reapply after queued native
    // events so the replacement handle gets the same taskbar policy.
    QTimer::singleShot(0, this, [this] {
        setNativeTaskbarVisible(window_, !frameless_ && !minimalMode_);
    });
}

void WindowController::updateResizeCursor(QWidget* widget, Qt::Edges edges)
{
    if (edges == Qt::Edges{}) {
        clearResizeCursor();
        return;
    }
    if (resizeCursorWidget_ != nullptr && resizeCursorWidget_ != widget) {
        resizeCursorWidget_->unsetCursor();
    }
    resizeCursorWidget_ = widget;
    resizeCursorWidget_->setCursor(cursorForEdges(edges));
}

void WindowController::clearResizeCursor()
{
    if (resizeCursorWidget_ != nullptr) {
        resizeCursorWidget_->unsetCursor();
    }
    resizeCursorWidget_.clear();
}

} // namespace vinson
