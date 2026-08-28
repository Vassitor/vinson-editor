#include "window/WindowController.h"

#include "editor/EditorWidget.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace vinson {
namespace {

constexpr int resizeBorderWidth = 6;

Qt::CursorShape cursorForEdges(Qt::Edges edges)
{
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
    frameless_ = enabled;
    clearResizeCursor();
    applyWindowFlag(Qt::FramelessWindowHint, enabled);
    emit framelessChanged(enabled);
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
            window_->menuBar()->isVisible(),
            window_->statusBar()->isVisible(),
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
        window_->menuBar()->hide();
        window_->statusBar()->hide();
        editor_->setLineNumbersVisible(false);
        editor_->setHScrollBar(false);
        editor_->setVScrollBar(false);
        setFrameless(true);
        refreshMinimalMinimumSize();
    } else {
        minimalMode_ = false;
        window_->setMinimumSize(minimalUiState_.windowMinimumSize);
        editor_->setMinimumSize(minimalUiState_.editorMinimumSize);
        editor_->setLineNumbersVisible(minimalUiState_.lineNumbersVisible);
        editor_->setHScrollBar(minimalUiState_.horizontalScrollBarVisible);
        editor_->setVScrollBar(minimalUiState_.verticalScrollBarVisible);
        window_->menuBar()->setVisible(minimalUiState_.menuBarVisible);
        window_->statusBar()->setVisible(minimalUiState_.statusBarVisible);
        if (transientPanel_ != nullptr) {
            transientPanel_->setVisible(minimalUiState_.transientPanelVisible);
        }
        if (!minimalUiState_.windowGeometry.isEmpty()) {
            window_->restoreGeometry(minimalUiState_.windowGeometry);
        }
        setFrameless(minimalUiState_.frameless);
    }
    emit minimalModeChanged(enabled);
}

void WindowController::toggleMinimalMode()
{
    setMinimalMode(!minimalMode_);
}

void WindowController::refreshMinimalMinimumSize()
{
    if (!minimalMode_ || editor_ == nullptr) {
        return;
    }
    const QFontMetricsF metrics(editor_->editorFont());
    const int lineHeight = static_cast<int>(std::ceil(metrics.height())) + 4;
    const int minimumWidth = std::max(80,
        static_cast<int>(std::ceil(metrics.horizontalAdvance(
            QStringLiteral("MMMM")))) + 8);
    editor_->setMinimumSize(0, lineHeight);
    window_->setMinimumSize(minimumWidth, lineHeight);
}

bool WindowController::eventFilter(QObject* watched, QEvent* event)
{
    if (!belongsToManagedWindow(watched)) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers minimalShortcut =
            Qt::ControlModifier | Qt::ShiftModifier;
        if (editor_ != nullptr && keyEvent->key() == Qt::Key_M
            && keyEvent->modifiers().testFlags(minimalShortcut)) {
            toggleMinimalMode();
            return true;
        }
        if (minimalMode_ && keyEvent->key() == Qt::Key_Escape) {
            setMinimalMode(false);
            return true;
        }
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

    window_->setWindowState(state);
    window_->show();
    if (state == Qt::WindowNoState) {
        window_->setGeometry(geometry);
    }
    if (focusedWidget != nullptr && focusedWidget->window() == window_) {
        focusedWidget->setFocus(Qt::OtherFocusReason);
    }
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
