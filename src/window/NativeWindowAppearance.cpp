#include "window/NativeWindowAppearance.h"

#include <QColor>
#include <QWidget>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <dwmapi.h>
#endif

namespace vinson {

void applyNativeTitleBarColors(QWidget* window, const QColor& background,
                               const QColor& text)
{
#if defined(Q_OS_WIN)
    if (window == nullptr) {
        return;
    }

    // A translucent Qt top-level can otherwise inherit an unreadable black
    // DWM caption. Unsupported attributes are safely ignored by older Windows.
    constexpr DWORD borderColorAttribute = 34;
    constexpr DWORD captionColorAttribute = 35;
    constexpr DWORD textColorAttribute = 36;
    const COLORREF captionColor = RGB(background.red(), background.green(),
                                      background.blue());
    const COLORREF captionTextColor = RGB(text.red(), text.green(), text.blue());
    // Only consume an existing native handle; its creation is owned by the
    // caller so this helper cannot disturb widget construction.
    const WId nativeId = window->effectiveWinId();
    if (nativeId == 0) {
        return;
    }
    const HWND handle = reinterpret_cast<HWND>(nativeId);
    DwmSetWindowAttribute(handle, captionColorAttribute, &captionColor,
                          sizeof(captionColor));
    DwmSetWindowAttribute(handle, textColorAttribute, &captionTextColor,
                          sizeof(captionTextColor));
    DwmSetWindowAttribute(handle, borderColorAttribute, &captionColor,
                          sizeof(captionColor));
#else
    Q_UNUSED(window);
    Q_UNUSED(background);
    Q_UNUSED(text);
#endif
}

bool setNativeBackgroundAlphaEnabled(QWidget* window, bool enabled)
{
#if defined(Q_OS_WIN)
    if (window == nullptr) {
        return false;
    }

    // DWMWA_REDIRECTIONBITMAP_ALPHA (Windows 11 build 26100+) makes DWM
    // composite the premultiplied alpha produced by Qt instead of treating the
    // redirection surface as opaque. Unlike QWidget::setWindowOpacity(), this
    // changes only translucent pixels and therefore leaves glyphs fully opaque.
    constexpr DWORD redirectionBitmapAlphaAttribute = 39;
    const BOOL value = enabled ? TRUE : FALSE;
    // Never create a native handle here: callers control whether the widget is
    // fully constructed and ready for native-handle creation.
    const WId nativeId = window->effectiveWinId();
    if (nativeId == 0) {
        return false;
    }
    const HWND handle = reinterpret_cast<HWND>(nativeId);
    const HRESULT result = DwmSetWindowAttribute(
        handle, redirectionBitmapAlphaAttribute, &value, sizeof(value));
    return SUCCEEDED(result);
#else
    Q_UNUSED(window);
    Q_UNUSED(enabled);
    return false;
#endif
}

void setNativeTaskbarVisible(QWidget* window, bool visible)
{
#if defined(Q_OS_WIN)
    if (window == nullptr) {
        return;
    }

    const HWND handle = reinterpret_cast<HWND>(window->winId());
    LONG_PTR extendedStyle = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    if (visible) {
        extendedStyle &= ~static_cast<LONG_PTR>(WS_EX_TOOLWINDOW);
        extendedStyle |= WS_EX_APPWINDOW;
    } else {
        extendedStyle &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
        extendedStyle |= WS_EX_TOOLWINDOW;
    }
    SetWindowLongPtrW(handle, GWL_EXSTYLE, extendedStyle);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE
                     | SWP_NOZORDER | SWP_NOACTIVATE);
#else
    Q_UNUSED(window);
    Q_UNUSED(visible);
#endif
}

} // namespace vinson
