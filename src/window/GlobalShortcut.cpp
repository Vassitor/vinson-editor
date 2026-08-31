#include "window/GlobalShortcut.h"

#include <QCoreApplication>
#include <QKeyCombination>

#include <atomic>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace vinson {
namespace {

#if defined(Q_OS_WIN)
std::atomic_int nextHotkeyId{0x5645};

UINT windowsVirtualKey(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<UINT>('A' + key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<UINT>('0' + key - Qt::Key_0);
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return static_cast<UINT>(VK_F1 + key - Qt::Key_F1);
    }

    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    default: break;
    }

    if (key >= 0x20 && key <= 0x7e) {
        const SHORT mapped = VkKeyScanW(static_cast<WCHAR>(key));
        if (mapped != -1) {
            return LOBYTE(mapped);
        }
    }
    return 0;
}

UINT windowsModifiers(Qt::KeyboardModifiers modifiers)
{
    UINT nativeModifiers = MOD_NOREPEAT;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        nativeModifiers |= MOD_CONTROL;
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        nativeModifiers |= MOD_ALT;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        nativeModifiers |= MOD_SHIFT;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        nativeModifiers |= MOD_WIN;
    }
    return nativeModifiers;
}
#endif

} // namespace

GlobalShortcut::GlobalShortcut(QObject* parent)
    : QObject(parent)
#if defined(Q_OS_WIN)
    , nativeHotkeyId_(nextHotkeyId.fetch_add(1, std::memory_order_relaxed))
#endif
{
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

GlobalShortcut::~GlobalShortcut()
{
    unregisterShortcut();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

QKeySequence GlobalShortcut::defaultShortcut()
{
    return QKeySequence(QStringLiteral("Ctrl+Alt+Space"));
}

QKeySequence GlobalShortcut::defaultFocusShortcut()
{
    return QKeySequence(QStringLiteral("Ctrl+Alt+F"));
}

bool GlobalShortcut::isSupportedShortcut(
    const QKeySequence& shortcut) noexcept
{
    if (shortcut.isEmpty()) {
        return true;
    }
    if (shortcut.count() != 1) {
        return false;
    }
    const QKeyCombination combination = shortcut[0];
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier
                       | Qt::ShiftModifier | Qt::MetaModifier))) {
        return false;
    }
#if defined(Q_OS_WIN)
    return windowsVirtualKey(combination.key()) != 0;
#else
    return combination.key() != Qt::Key_unknown;
#endif
}

const QKeySequence& GlobalShortcut::shortcut() const noexcept
{
    return shortcut_;
}

bool GlobalShortcut::isRegistered() const noexcept
{
    return registered_;
}

int GlobalShortcut::nativeHotkeyId() const noexcept
{
    return nativeHotkeyId_;
}

bool GlobalShortcut::setShortcut(const QKeySequence& shortcut, QString* error)
{
    if (!isSupportedShortcut(shortcut)) {
        if (error != nullptr) {
            *error = tr("Use one shortcut containing at least one modifier key.");
        }
        return false;
    }
    if (shortcut == shortcut_ && (registered_ || shortcut.isEmpty())) {
        return true;
    }

    unregisterShortcut();
    shortcut_ = QKeySequence();
    if (shortcut.isEmpty()) {
        return true;
    }

#if defined(Q_OS_WIN)
    const QKeyCombination combination = shortcut[0];
    registered_ = RegisterHotKey(
        nullptr, nativeHotkeyId_,
        windowsModifiers(combination.keyboardModifiers()),
        windowsVirtualKey(combination.key()));
    if (!registered_) {
        if (error != nullptr) {
            *error = tr("The shortcut is already used by another application.");
        }
        return false;
    }
    shortcut_ = shortcut;
    return true;
#else
    if (error != nullptr) {
        *error = tr("Global shortcuts are supported on Windows only.");
    }
    return false;
#endif
}

bool GlobalShortcut::nativeEventFilter(const QByteArray& eventType,
                                       void* message, qintptr* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
#if defined(Q_OS_WIN)
    const auto* nativeMessage = static_cast<const MSG*>(message);
    if (nativeMessage != nullptr && nativeMessage->message == WM_HOTKEY
        && nativeMessage->wParam
            == static_cast<WPARAM>(nativeHotkeyId_)) {
        emit activated();
        return true;
    }
#else
    Q_UNUSED(message);
#endif
    return false;
}

void GlobalShortcut::unregisterShortcut()
{
#if defined(Q_OS_WIN)
    if (registered_) {
        UnregisterHotKey(nullptr, nativeHotkeyId_);
    }
#endif
    registered_ = false;
}

} // namespace vinson
