#include "window/TrayController.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QtTest>

class TrayControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void togglesWindowVisibility();
    void restoresMinimizedWindow();
    void focusesWindowWithoutToggling();
    void exposesContextMenuActions();
};

void TrayControllerTest::togglesWindowVisibility()
{
    QMainWindow window;
    window.show();
    QCoreApplication::processEvents();
    vinson::TrayController controller(&window);
    auto* trayIcon = controller.findChild<QSystemTrayIcon*>(
        QStringLiteral("systemTrayIcon"));
    QVERIFY(trayIcon != nullptr);

    trayIcon->activated(QSystemTrayIcon::Trigger);
    QVERIFY(!window.isVisible());

    trayIcon->activated(QSystemTrayIcon::Trigger);
    QVERIFY(window.isVisible());
}

void TrayControllerTest::restoresMinimizedWindow()
{
    QMainWindow window;
    vinson::TrayController controller(&window);
    window.showMinimized();
    QCoreApplication::processEvents();

    controller.toggleWindowVisibility();
    QVERIFY(window.isVisible());
    QVERIFY(!window.isMinimized());
}

void TrayControllerTest::focusesWindowWithoutToggling()
{
    QMainWindow window;
    vinson::TrayController controller(&window);
    QSignalSpy focusSpy(&controller,
                        &vinson::TrayController::editorFocusRequested);

    window.hide();
    controller.focusWindow();
    QVERIFY(window.isVisible());
    QVERIFY(!window.isMinimized());
    QCOMPARE(focusSpy.count(), 1);

    controller.focusWindow();
    QVERIFY(window.isVisible());
    QVERIFY(!window.isMinimized());
    QCOMPARE(focusSpy.count(), 2);
}

void TrayControllerTest::exposesContextMenuActions()
{
    QMainWindow window;
    vinson::TrayController controller(&window);
    auto* toggle = window.findChild<QAction*>(
        QStringLiteral("trayToggleWindowAction"));
    auto* quit = window.findChild<QAction*>(QStringLiteral("trayQuitAction"));
    auto* settings = window.findChild<QAction*>(
        QStringLiteral("traySettingsAction"));
    auto* bossKey = window.findChild<QAction*>(
        QStringLiteral("trayBossKeyAction"));
    auto* focusShortcut = window.findChild<QAction*>(
        QStringLiteral("trayFocusShortcutAction"));
    QVERIFY(toggle != nullptr);
    QVERIFY(quit != nullptr);
    QVERIFY(settings != nullptr);
    QVERIFY(bossKey != nullptr);
    QVERIFY(focusShortcut != nullptr);
    auto* trayIcon = controller.findChild<QSystemTrayIcon*>(
        QStringLiteral("systemTrayIcon"));
    QVERIFY(trayIcon != nullptr);
    QVERIFY(trayIcon->contextMenu() != nullptr);
    trayIcon->contextMenu()->ensurePolished();
    QVERIFY(trayIcon->contextMenu()->findChild<QObject*>(
        QStringLiteral("nativeMenuAppearance"), Qt::FindDirectChildrenOnly));
    QVERIFY(trayIcon->contextMenu()->graphicsEffect() == nullptr);
    QVERIFY(!trayIcon->contextMenu()->testAttribute(Qt::WA_TranslucentBackground));
    QVERIFY(!trayIcon->contextMenu()->windowFlags().testFlag(Qt::NoDropShadowWindowHint));
    QVERIFY(trayIcon->contextMenu()->actions().contains(toggle));
    QVERIFY(trayIcon->contextMenu()->actions().contains(settings));
    QVERIFY(trayIcon->contextMenu()->actions().contains(bossKey));
    QVERIFY(trayIcon->contextMenu()->actions().contains(focusShortcut));
    QVERIFY(trayIcon->contextMenu()->actions().contains(quit));
    QVERIFY(!bossKey->isEnabled());
    QVERIFY(!focusShortcut->isEnabled());

    QSignalSpy quitSpy(&controller, &vinson::TrayController::quitRequested);
    QSignalSpy settingsSpy(&controller,
                           &vinson::TrayController::settingsRequested);
    settings->trigger();
    QCOMPARE(settingsSpy.count(), 1);
    quit->trigger();
    QCOMPARE(quitSpy.count(), 1);
}

QTEST_MAIN(TrayControllerTest)
#include "TrayControllerTest.moc"
