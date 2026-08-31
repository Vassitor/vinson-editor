#include "window/GlobalShortcut.h"

#include <QtTest>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

class GlobalShortcutTest final : public QObject
{
    Q_OBJECT

private slots:
    void validatesBossKeySequences();
    void registersAndDispatchesNativeHotkey();
};

void GlobalShortcutTest::validatesBossKeySequences()
{
    QVERIFY(vinson::GlobalShortcut::isSupportedShortcut(
        vinson::GlobalShortcut::defaultShortcut()));
    QVERIFY(vinson::GlobalShortcut::isSupportedShortcut(
        vinson::GlobalShortcut::defaultFocusShortcut()));
    QVERIFY(vinson::GlobalShortcut::defaultShortcut()
            != vinson::GlobalShortcut::defaultFocusShortcut());
    QVERIFY(vinson::GlobalShortcut::isSupportedShortcut(QKeySequence()));
    QVERIFY(vinson::GlobalShortcut::isSupportedShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+F12"))));
    QVERIFY(!vinson::GlobalShortcut::isSupportedShortcut(
        QKeySequence(QStringLiteral("A"))));
    QVERIFY(!vinson::GlobalShortcut::isSupportedShortcut(
        QKeySequence(QStringLiteral("Ctrl+A, Ctrl+B"))));
}

void GlobalShortcutTest::registersAndDispatchesNativeHotkey()
{
#if defined(Q_OS_WIN)
    vinson::GlobalShortcut shortcut;
    QString error;
    QVERIFY2(shortcut.setShortcut(
                 QKeySequence(QStringLiteral("Ctrl+Alt+F24")), &error),
             qPrintable(error));
    QVERIFY(shortcut.isRegistered());
    QSignalSpy activatedSpy(&shortcut, &vinson::GlobalShortcut::activated);

    MSG message{};
    message.message = WM_HOTKEY;
    message.wParam = static_cast<WPARAM>(shortcut.nativeHotkeyId());
    qintptr result = 0;
    QVERIFY(shortcut.nativeEventFilter(
        QByteArrayLiteral("windows_generic_MSG"), &message, &result));
    QCOMPARE(activatedSpy.count(), 1);

    vinson::GlobalShortcut secondShortcut;
    QVERIFY(secondShortcut.nativeHotkeyId() != shortcut.nativeHotkeyId());
    QVERIFY2(secondShortcut.setShortcut(
                 QKeySequence(QStringLiteral("Ctrl+Alt+F23")), &error),
             qPrintable(error));
    QVERIFY(secondShortcut.isRegistered());
    QSignalSpy secondActivatedSpy(
        &secondShortcut, &vinson::GlobalShortcut::activated);
    message.wParam = static_cast<WPARAM>(secondShortcut.nativeHotkeyId());
    QVERIFY(!shortcut.nativeEventFilter(
        QByteArrayLiteral("windows_generic_MSG"), &message, &result));
    QVERIFY(secondShortcut.nativeEventFilter(
        QByteArrayLiteral("windows_generic_MSG"), &message, &result));
    QCOMPARE(secondActivatedSpy.count(), 1);
#else
    QSKIP("Native global-shortcut registration is Windows-specific.");
#endif
}

QTEST_MAIN(GlobalShortcutTest)
#include "GlobalShortcutTest.moc"
