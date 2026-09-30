#include "window/NativeTitleBar.h"
#include "window/NativeWindowAppearance.h"

#include <QAbstractButton>
#include <QEvent>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QOperatingSystemVersion>
#include <QTabBar>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

namespace vinson {
namespace {
constexpr int headerHeight = 44;

#if defined(Q_OS_WIN)
RECT nativeCaptionRect(HWND handle)
{
    RECT buttons{}, frame{};
    if (FAILED(DwmGetWindowAttribute(handle, DWMWA_CAPTION_BUTTON_BOUNDS,
                                    &buttons, sizeof(buttons)))
        || !GetWindowRect(handle, &frame))
        return {};
    OffsetRect(&buttons, frame.left, frame.top);
    return buttons;
}

int resizeBorder(HWND handle)
{
    const UINT dpi = GetDpiForWindow(handle);
    return GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi)
        + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}
#endif
} // namespace

NativeTitleBar::NativeTitleBar(QMainWindow* window, QTabBar* tabs)
    : QObject(window), window_(window), tabs_(tabs)
{
    setObjectName(QStringLiteral("nativeTitleBar"));
#if defined(Q_OS_WIN)
    supported_ = QGuiApplication::platformName() == QStringLiteral("windows")
        && QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10;
#endif
    if (!supported_)
        return;
    header_ = new QWidget(window_);
    header_->setObjectName(QStringLiteral("documentTitleBar"));
    header_->setContextMenuPolicy(Qt::PreventContextMenu);
    window_->menuBar()->setCornerWidget(nullptr, Qt::TopRightCorner);
    tabs_->setParent(header_);
    tabs_->setProperty("inTitleBar", true);
    window_->installEventFilter(this);
    refresh();
}

bool NativeTitleBar::isActive() const
{
    return supported_ && !window_->windowFlags().testFlag(Qt::FramelessWindowHint)
        && !window_->isFullScreen();
}

QRect NativeTitleBar::captionButtonRect() const
{
#if defined(Q_OS_WIN)
    const HWND handle = reinterpret_cast<HWND>(window_->internalWinId());
    if (!isActive() || !handle)
        return {};
    const RECT buttons = nativeCaptionRect(handle);
    if (IsRectEmpty(&buttons))
        return {};
    POINT origin{buttons.left, buttons.top};
    ScreenToClient(handle, &origin);
    const qreal ratio = window_->devicePixelRatioF();
    const int left = static_cast<int>(std::floor(origin.x / ratio));
    const int top = static_cast<int>(std::floor(origin.y / ratio));
    const int right = static_cast<int>(std::ceil((origin.x + buttons.right - buttons.left) / ratio));
    const int bottom = static_cast<int>(std::ceil((origin.y + buttons.bottom - buttons.top) / ratio));
    return QRect(left, top, right - left, bottom - top);
#else
    return {};
#endif
}

int NativeTitleBar::maximizedTopInset() const
{
#if defined(Q_OS_WIN)
    const HWND handle = reinterpret_cast<HWND>(window_->internalWinId());
    if (handle && IsZoomed(handle))
        return qCeil(resizeBorder(handle) / window_->devicePixelRatioF());
#endif
    return 0;
}

QRect NativeTitleBar::captionPaintRect() const
{
    if (!isActive())
        return {};
    // Caption bounds describe hit targets, not the whole DWM drawing surface.
    // Maximizing changes the invisible frame offset; keep the complete corner
    // transparent so neither the glyphs nor hover backgrounds can be clipped
    // by our client-area fill while Windows updates those bounds.
    const QRect buttons = captionButtonRect();
    const int left = std::clamp(buttons.isEmpty() ? window_->width() - 150
                                                : buttons.left(), 0, window_->width());
    return QRect(left, 0, window_->width() - left, headerHeight + maximizedTopInset());
}

void NativeTitleBar::layoutHeader()
{
    if (!header_)
        return;
    const bool active = isActive();
    window_->setProperty("nativeTitleBarActive", active);
    const int topInset = active ? maximizedTopInset() : 0;
    window_->setContentsMargins(0, active ? headerHeight + topInset : 0, 0, 0);
    header_->setVisible(active);
    if (!active)
        return;
    window_->setAutoFillBackground(false);
    // The maximized resize border lies beyond the monitor's work area. Keep
    // that inset inside our client layout, not in WM_NCCALCSIZE: DWM stops
    // highlighting native caption buttons if any top nonclient inset remains.
    header_->setGeometry(0, topInset, window_->width(), headerHeight);
    const QRect buttons = captionButtonRect();
    const int buttonLeft = buttons.isEmpty() ? window_->width() - 150 : buttons.left();
    // Let the tab strip use the complete caption area up to the system
    // controls. Empty space inside the QTabBar remains draggable because the
    // native hit test only treats actual tabs and buttons as interactive.
    const int available = std::max(0, buttonLeft - 12);
    tabs_->setFixedWidth(available);
    tabs_->setGeometry(12, 8, available, headerHeight - 8);
    tabs_->show();
    header_->raise();
}

void NativeTitleBar::refresh()
{
    if (!supported_)
        return;
    layoutHeader();
#if defined(Q_OS_WIN)
    const WId id = window_->internalWinId();
    if (!id)
        return;
    const bool active = isActive();
    const HWND handle = reinterpret_cast<HWND>(id);
    if (active)
        applyNativeTitleBarColors(window_, titleBarBackground(window_->palette().color(QPalette::Window)),
                                  window_->palette().color(QPalette::WindowText));
    const bool changed = configuredHandle_ != id || configuredActive_ != active;
    configuredHandle_ = id;
    configuredActive_ = active;
    const MARGINS margins{0, 0, active ? qRound((headerHeight + maximizedTopInset())
                                              * window_->devicePixelRatioF()) : 0, 0};
    DwmExtendFrameIntoClientArea(handle, &margins);
    if (changed) {
        SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    layoutHeader();
#endif
    window_->update();
}

void NativeTitleBar::scheduleRefresh()
{
    if (refreshPending_)
        return;
    refreshPending_ = true;
    QTimer::singleShot(0, this, [this] {
        refreshPending_ = false;
        refresh();
    });
}

bool NativeTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == window_) {
        switch (event->type()) {
        case QEvent::Resize:
            layoutHeader();
            break;
        case QEvent::Show:
        case QEvent::WinIdChange:
        case QEvent::WindowStateChange:
        case QEvent::DevicePixelRatioChange:
        case QEvent::PaletteChange:
            scheduleRefresh();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool NativeTitleBar::nativeEvent(void* nativeMessage, qintptr* result)
{
#if defined(Q_OS_WIN)
    if (!isActive())
        return false;
    const auto* message = static_cast<MSG*>(nativeMessage);
    const HWND handle = message->hwnd;
    if (message->message == WM_NCCALCSIZE && message->wParam) {
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(message->lParam);
        const LONG top = params->rgrc[0].top;
        // Windows still computes the left/right/bottom frame and maximized
        // work area; only the caption is made available to the client.
        DefWindowProcW(handle, message->message, message->wParam, message->lParam);
        params->rgrc[0].top = top;
        *result = 0;
        return true;
    }
    if (message->message == WM_NCMOUSEMOVE) {
        // Qt handles nonclient mouse movement itself. Track leaving this area
        // explicitly so DWM receives the matching leave and clears highlights.
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE | TME_NONCLIENT, handle, 0};
        TrackMouseEvent(&tracking);
    }
    LRESULT dwmResult = 0;
    if (DwmDefWindowProc(handle, message->message, message->wParam,
                         message->lParam, &dwmResult)) {
        *result = dwmResult;
        return true;
    }
    switch (message->message) {
    case WM_NCHITTEST: {
        const POINT screenPoint{GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam)};
        const RECT buttons = nativeCaptionRect(handle);
        // DWM can decline hit testing after maximization. Preserve native
        // button commands instead of treating the visible buttons as a drag area.
        if (!IsRectEmpty(&buttons) && PtInRect(&buttons, screenPoint)) {
            const int width = (buttons.right - buttons.left) / 3;
            *result = screenPoint.x >= buttons.right - width ? HTCLOSE
                : screenPoint.x >= buttons.right - 2 * width ? HTMAXBUTTON : HTMINBUTTON;
            return true;
        }
        const LRESULT native = DefWindowProcW(handle, message->message,
                                              message->wParam, message->lParam);
        if (native != HTCLIENT) {
            *result = native;
            return true;
        }
        POINT clientPoint = screenPoint;
        ScreenToClient(handle, &clientPoint);
        if (!IsZoomed(handle) && clientPoint.y < resizeBorder(handle)) {
            *result = HTTOP;
            return true;
        }
        const qreal ratio = window_->devicePixelRatioF();
        const QPoint point(qFloor(clientPoint.x / ratio), qFloor(clientPoint.y / ratio));
        if (point.y() >= 0 && point.y() < headerHeight + maximizedTopInset()) {
            const QPoint tabPoint = tabs_->mapFrom(window_, point);
            QWidget* child = tabs_->childAt(tabPoint);
            const bool interactive = tabs_->rect().contains(tabPoint)
                && (tabs_->tabAt(tabPoint) >= 0 || qobject_cast<QAbstractButton*>(child));
            *result = interactive ? HTCLIENT : HTCAPTION;
            return true;
        }
        break;
    }
    case WM_NCMOUSEMOVE:
    case WM_NCMOUSEHOVER:
    case WM_NCMOUSELEAVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONUP:
        // Qt consumes some nonclient mouse messages; hand these to Windows so
        // hover, resize, drag-to-restore and system commands stay native even
        // when DwmDefWindowProc declines a message after maximizing.
        *result = DefWindowProcW(handle, message->message, message->wParam, message->lParam);
        return true;
    case WM_SIZE:
    case WM_DPICHANGED:
    case WM_DWMCOMPOSITIONCHANGED:
        scheduleRefresh();
        break;
    default:
        break;
    }
#else
    Q_UNUSED(nativeMessage);
    Q_UNUSED(result);
#endif
    return false;
}

} // namespace vinson
