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
    message.wParam = 0x5645;
    qintptr result = 0;
    QVERIFY(shortcut.nativeEventFilter(
        QByteArrayLiteral("windows_generic_MSG"), &message, &result));
    QCOMPARE(activatedSpy.count(), 1);
#else
    QSKIP("Native boss-key registration is Windows-specific.");
#endif
}

QTEST_MAIN(GlobalShortcutTest)
#include "GlobalShortcutTest.moc"
