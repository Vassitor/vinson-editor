#include "editor/EditorWidget.h"
#include "settings/ThemeManager.h"
#include "window/GlobalShortcut.h"
#include "window/MainWindow.h"
#include "window/WindowController.h"

#include <QAction>
#include <QFile>
#include <QGuiApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>

class MainWindowPersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void usesWrappedTextByDefault();
    void migratesLegacyHorizontalScrollingDefault();
    void restoresPersistedApplicationState();
    void framelessShortcutRemainsAvailableWithHiddenMenuBar();
    void backgroundOpacityShortcutsArePersistent();
    void controlWheelFontSizeIsPersistent();
    void focusRequestTargetsEditor();
    void restoresAndClearsRecentFiles();
    void opensFilesInTabsAndReusesPristinePage();
    void switchesTabsWithControlAltArrows();
    void reordersTabsWithinTopBar();
    void showsTabBarOnlyInDefaultMode();
    void restoresOpenTabsWhenEnabled();
    void titleBarMenusUseNativeShadows();
    void exposesEditHistoryPanel();
    void closeToTrayPreservesUnsavedDocument();
    void explicitQuitIsSeparateFromCloseToTray();
    void savesChangedApplicationState();
    void relocatesGeometryFromMissingDisplay();

private:
    static void useSettingsDirectory(const QString& path);
};

void MainWindowPersistenceTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("VinsonEditorTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PersistenceTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

void MainWindowPersistenceTest::useSettingsDirectory(const QString& path)
{
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
    QSettings settings;
    settings.clear();
    settings.sync();
}

void MainWindowPersistenceTest::usesWrappedTextByDefault()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor != nullptr);
    QVERIFY(editor->isWordWrapEnabled());
    QVERIFY(!editor->hScrollBar());
}

void MainWindowPersistenceTest::migratesLegacyHorizontalScrollingDefault()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    {
        QSettings settings;
        settings.setValue(QStringLiteral("schema/version"), 1);
        settings.setValue(QStringLiteral("view/wordWrap"), false);
        settings.sync();
    }

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor != nullptr);
    QVERIFY(editor->isWordWrapEnabled());
    QVERIFY(!editor->hScrollBar());
}

void MainWindowPersistenceTest::restoresPersistedApplicationState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const vinson::Appearance defaults = vinson::ThemeManager::defaultAppearance();
    {
        QSettings settings;
        settings.setValue(QStringLiteral("appearance/fontFamily"),
                          defaults.font.family());
        settings.setValue(QStringLiteral("appearance/fontSize"), 19.5);
        settings.setValue(QStringLiteral("appearance/textColor"),
                          QStringLiteral("#3f010203"));
        settings.setValue(QStringLiteral("appearance/backgroundColor"),
                          QStringLiteral("#11040506"));
        settings.setValue(QStringLiteral("appearance/cursorColor"),
                          QStringLiteral("#ff070809"));
        settings.setValue(QStringLiteral("appearance/selectionTextColor"),
                          QStringLiteral("#ff0a0b0c"));
        settings.setValue(QStringLiteral("view/wordWrap"), true);
        settings.setValue(QStringLiteral("view/lineNumbers"), false);
        settings.setValue(QStringLiteral("window/alwaysOnTop"), true);
        settings.setValue(QStringLiteral("window/frameless"), true);
        settings.setValue(QStringLiteral("input/bossKey"),
                          QStringLiteral("Ctrl+Shift+F12"));
        settings.setValue(QStringLiteral("input/focusShortcut"),
                          QStringLiteral("Ctrl+Alt+F11"));
        settings.setValue(QStringLiteral("files/lastDirectory"), directory.path());
        settings.sync();
    }

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* theme = window.findChild<vinson::ThemeManager*>();
    auto* controller = window.findChild<vinson::WindowController*>();
    auto* wrap = window.findChild<QAction*>(QStringLiteral("wordWrapAction"));
    auto* lines = window.findChild<QAction*>(QStringLiteral("lineNumbersAction"));
    QVERIFY(editor != nullptr);
    QVERIFY(theme != nullptr);
    QVERIFY(controller != nullptr);
    QVERIFY(wrap != nullptr);
    QVERIFY(lines != nullptr);

    QCOMPARE(theme->appearance().font.pointSizeF(), 19.5);
    QCOMPARE(theme->appearance().textColor, QColor(1, 2, 3, 63));
    QCOMPARE(theme->appearance().backgroundColor, QColor(4, 5, 6, 17));
    QVERIFY(editor->isWordWrapEnabled());
    QVERIFY(!editor->areLineNumbersVisible());
    QVERIFY(wrap->isChecked());
    QVERIFY(!lines->isChecked());
    QVERIFY(controller->isAlwaysOnTop());
    QVERIFY(controller->isFrameless());
    QCOMPARE(window.bossKey(),
             QKeySequence(QStringLiteral("Ctrl+Shift+F12")));
    QCOMPARE(window.focusShortcut(),
             QKeySequence(QStringLiteral("Ctrl+Alt+F11")));
    QVERIFY(window.menuBar()->isHidden());
    QVERIFY(window.statusBar()->isHidden());
    controller->setFrameless(false);
    QVERIFY(!window.menuBar()->isHidden());
    QVERIFY(!window.statusBar()->isHidden());
}

void MainWindowPersistenceTest::framelessShortcutRemainsAvailableWithHiddenMenuBar()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* controller = window.findChild<vinson::WindowController*>();
    auto* action = window.findChild<QAction*>(QStringLiteral("framelessAction"));
    QVERIFY(controller != nullptr);
    QVERIFY(action != nullptr);
    QVERIFY(window.actions().contains(action));

    window.show();
    QCoreApplication::processEvents();
    QTest::keyClick(&window, Qt::Key_F11);
    QTRY_VERIFY(controller->isFrameless());
    QVERIFY(window.menuBar()->isHidden());

    QTest::keyClick(&window, Qt::Key_F11);
    QTRY_VERIFY(!controller->isFrameless());
    QVERIFY(!window.menuBar()->isHidden());
    QTRY_VERIFY(window.statusBar()->isVisible());
    QVERIFY(window.statusBar()->height() > 0);
    QVERIFY(window.rect().intersects(window.statusBar()->geometry()));
    QVERIFY(window.statusBar()->geometry().bottom() <= window.rect().bottom());
}

void MainWindowPersistenceTest::backgroundOpacityShortcutsArePersistent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* theme = window.findChild<vinson::ThemeManager*>();
    auto* controller = window.findChild<vinson::WindowController*>();
    auto* increase = window.findChild<QAction*>(
        QStringLiteral("increaseBackgroundAlphaAction"));
    auto* decrease = window.findChild<QAction*>(
        QStringLiteral("decreaseBackgroundAlphaAction"));
    QVERIFY(editor != nullptr);
    QVERIFY(theme != nullptr);
    QVERIFY(controller != nullptr);
    QVERIFY(increase != nullptr);
    QVERIFY(decrease != nullptr);
    QCOMPARE(increase->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Up));
    QCOMPARE(decrease->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Down));
    QVERIFY(window.actions().contains(increase));
    QVERIFY(window.actions().contains(decrease));

    editor->setTextUtf8("shortcut keeps document text");
    window.show();
    editor->QWidget::setFocus();
    QCoreApplication::processEvents();

    QTest::keyClick(editor, Qt::Key_Down, Qt::ControlModifier);
    QTRY_COMPARE(theme->appearance().backgroundColor.alpha(), 250);
    QCOMPARE(editor->backgroundColor().alpha(), 255);
    QCOMPARE(window.palette().color(QPalette::Window).alpha(), 255);
    QVERIFY(editor->bufferedDraw());
    QCOMPARE(editor->textUtf8(), QByteArray("shortcut keeps document text"));
    QVERIFY(!editor->modify());
    {
        QSettings settings;
        QCOMPARE(settings.value(QStringLiteral("appearance/backgroundColor"))
                     .toString().left(3),
                 QStringLiteral("#fa"));
    }

    controller->setFrameless(true);
    QCOMPARE(editor->backgroundColor().alpha(), 250);
    QCOMPARE(window.palette().color(QPalette::Window).alpha(), 250);
    QVERIFY(!editor->bufferedDraw());
    controller->setFrameless(false);
    QCOMPARE(editor->backgroundColor().alpha(), 255);
    QCOMPARE(window.palette().color(QPalette::Window).alpha(), 255);
    QVERIFY(editor->bufferedDraw());

    controller->setMinimalMode(true);
    QCOMPARE(editor->backgroundColor().alpha(), 250);
    QCOMPARE(window.palette().color(QPalette::Window).alpha(), 250);
    controller->setMinimalMode(false);
    QCOMPARE(editor->backgroundColor().alpha(), 255);
    QCOMPARE(window.palette().color(QPalette::Window).alpha(), 255);

    QTest::keyClick(editor, Qt::Key_Up, Qt::ControlModifier);
    QTRY_COMPARE(theme->appearance().backgroundColor.alpha(), 255);
    QCOMPARE(editor->textUtf8(), QByteArray("shortcut keeps document text"));
}

void MainWindowPersistenceTest::controlWheelFontSizeIsPersistent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* theme = window.findChild<vinson::ThemeManager*>();
    QVERIFY(editor != nullptr);
    QVERIFY(theme != nullptr);
    const qreal originalSize = theme->appearance().font.pointSizeF();

    QWheelEvent event(
        QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 120),
        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor->viewport(), &event);

    QCOMPARE(theme->appearance().font.pointSizeF(), originalSize + 1.0);
    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("appearance/fontSize")).toDouble(),
             originalSize + 1.0);
}

void MainWindowPersistenceTest::focusRequestTargetsEditor()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor != nullptr);
    window.show();
    QCoreApplication::processEvents();
    editor->clearFocus();

    window.focusEditor();

    QTRY_VERIFY(editor->hasFocus());
}

void MainWindowPersistenceTest::restoresAndClearsRecentFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.txt"));
    {
        QSettings settings;
        settings.setValue(QStringLiteral("files/recentFiles"),
                          QStringList{firstPath, secondPath});
        settings.sync();
    }

    vinson::MainWindow window;
    auto* recentMenu = window.findChild<QMenu*>(
        QStringLiteral("recentFilesMenu"));
    QVERIFY(recentMenu != nullptr);
    QVERIFY(recentMenu->isEnabled());
    QCOMPARE(recentMenu->actions().size(), 4);
    QCOMPARE(recentMenu->actions().at(0)->data().toString(), firstPath);
    QCOMPARE(recentMenu->actions().at(1)->data().toString(), secondPath);
    QVERIFY(recentMenu->actions().at(2)->isSeparator());

    auto* clearAction = window.findChild<QAction*>(
        QStringLiteral("clearRecentFilesAction"));
    QVERIFY(clearAction != nullptr);
    clearAction->trigger();

    QVERIFY(!recentMenu->isEnabled());
    QCOMPARE(recentMenu->actions().size(), 1);
    QVERIFY(!recentMenu->actions().first()->isEnabled());
    QSettings settings;
    QVERIFY(settings.value(QStringLiteral("files/recentFiles"))
                .toStringList().isEmpty());
}

void MainWindowPersistenceTest::opensFilesInTabsAndReusesPristinePage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.txt"));
    {
        QFile first(firstPath);
        QVERIFY(first.open(QIODevice::WriteOnly));
        QCOMPARE(first.write("first tab"), qint64(9));
        QFile second(secondPath);
        QVERIFY(second.open(QIODevice::WriteOnly));
        QCOMPARE(second.write("second tab"), qint64(10));
    }

    vinson::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(tabs != nullptr);
    QVERIFY(editor != nullptr);
    QCOMPARE(tabs->count(), 1);

    window.openFiles({firstPath});
    QTRY_COMPARE_WITH_TIMEOUT(editor->textUtf8(), QByteArray("first tab"), 5000);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabText(0), QStringLiteral("first.txt"));

    window.openFiles({secondPath});
    QTRY_COMPARE_WITH_TIMEOUT(editor->textUtf8(), QByteArray("second tab"), 5000);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(1), QStringLiteral("second.txt"));

    tabs->setCurrentIndex(0);
    QCOMPARE(editor->textUtf8(), QByteArray("first tab"));
}

void MainWindowPersistenceTest::switchesTabsWithControlAltArrows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* next = window.findChild<QAction*>(QStringLiteral("nextTabAction"));
    auto* previous = window.findChild<QAction*>(
        QStringLiteral("previousTabAction"));
    QVERIFY(tabs != nullptr);
    QVERIFY(editor != nullptr);
    QVERIFY(next != nullptr);
    QVERIFY(previous != nullptr);
    QCOMPARE(next->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+Right")));
    QCOMPARE(previous->shortcut(),
             QKeySequence(QStringLiteral("Ctrl+Alt+Left")));

    editor->setTextUtf8("first");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("second");
    QCOMPARE(tabs->count(), 2);

    previous->trigger();
    QCOMPARE(editor->textUtf8(), QByteArray("first"));
    next->trigger();
    QCOMPARE(editor->textUtf8(), QByteArray("second"));
}

void MainWindowPersistenceTest::reordersTabsWithinTopBar()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(tabs != nullptr);
    QVERIFY(editor != nullptr);
    QVERIFY(tabs->isMovable());
    QCOMPARE(window.menuBar()->cornerWidget(Qt::TopRightCorner), tabs);
    QVERIFY(tabs->property("browserStyle").toBool());

    editor->setTextUtf8("first");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("second");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("third");
    QCOMPARE(tabs->count(), 3);

    tabs->setCurrentIndex(0);
    QCOMPARE(editor->textUtf8(), QByteArray("first"));
    tabs->moveTab(0, 2);
    QCOMPARE(editor->textUtf8(), QByteArray("first"));
    QCOMPARE(tabs->currentIndex(), 2);
    tabs->setCurrentIndex(0);
    QCOMPARE(editor->textUtf8(), QByteArray("second"));
    tabs->setCurrentIndex(1);
    QCOMPARE(editor->textUtf8(), QByteArray("third"));
}

void MainWindowPersistenceTest::showsTabBarOnlyInDefaultMode()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* controller = window.findChild<vinson::WindowController*>();
    QVERIFY(tabs != nullptr);
    QVERIFY(controller != nullptr);
    window.show();
    QTRY_VERIFY(tabs->isVisible());

    controller->setFrameless(true);
    QVERIFY(!tabs->isVisible());
    controller->setFrameless(false);
    QTRY_VERIFY(tabs->isVisible());
    controller->setMinimalMode(true);
    QVERIFY(!tabs->isVisible());
    controller->setMinimalMode(false);
    QTRY_VERIFY(tabs->isVisible());
}

void MainWindowPersistenceTest::restoresOpenTabsWhenEnabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.txt"));
    for (const QString& path : {firstPath, secondPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QFileInfo(path).baseName().toUtf8()) > 0);
    }
    {
        QSettings settings;
        settings.setValue(QStringLiteral("session/restoreTabsOnStartup"), true);
        settings.setValue(QStringLiteral("session/openTabs"),
                          QStringList{firstPath, secondPath});
        settings.sync();
    }

    vinson::MainWindow restored;
    auto* restoredTabs = restored.findChild<QTabBar*>(
        QStringLiteral("documentTabBar"));
    QVERIFY(restoredTabs != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(restoredTabs->count(), 2, 8000);
    QTRY_COMPARE_WITH_TIMEOUT(restoredTabs->tabText(1),
                              QStringLiteral("second.txt"), 8000);

    useSettingsDirectory(directory.path());
    {
        QSettings settings;
        settings.setValue(QStringLiteral("session/restoreTabsOnStartup"), false);
        settings.setValue(QStringLiteral("session/openTabs"),
                          QStringList{firstPath, secondPath});
        settings.sync();
    }
    vinson::MainWindow notRestored;
    auto* freshTabs = notRestored.findChild<QTabBar*>(
        QStringLiteral("documentTabBar"));
    QVERIFY(freshTabs != nullptr);
    QCoreApplication::processEvents();
    QCOMPARE(freshTabs->count(), 1);
    QCOMPARE(freshTabs->tabText(0), QStringLiteral("Untitled"));
}

void MainWindowPersistenceTest::titleBarMenusUseNativeShadows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    int menuCount = 0;
    for (QAction* action : window.menuBar()->actions()) {
        QMenu* menu = action->menu();
        if (menu == nullptr) {
            continue;
        }
        ++menuCount;
        QVERIFY(!menu->windowFlags().testFlag(
            Qt::NoDropShadowWindowHint));
        QVERIFY(!menu->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(menu->graphicsEffect() == nullptr);
    }
    QCOMPARE(menuCount, 5);
}

void MainWindowPersistenceTest::exposesEditHistoryPanel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* action = window.findChild<QAction*>(
        QStringLiteral("editHistoryAction"));
    auto* dock = window.findChild<QDockWidget*>(
        QStringLiteral("editHistoryDock"));
    auto* historyWidget = window.findChild<QWidget*>(
        QStringLiteral("editHistoryWidget"));
    auto* historyViewport = window.findChild<QWidget*>(
        QStringLiteral("editHistoryListViewport"));
    auto* findReplaceWidget = window.findChild<QWidget*>(
        QStringLiteral("findReplaceWidget"));
    auto* restoreButton = window.findChild<QPushButton*>(
        QStringLiteral("editHistoryRestoreButton"));
    auto* settingsAction = window.findChild<QAction*>(
        QStringLiteral("appearanceAndShortcutsAction"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* theme = window.findChild<vinson::ThemeManager*>();
    QVERIFY(action != nullptr);
    QVERIFY(dock != nullptr);
    QVERIFY(historyWidget != nullptr);
    QVERIFY(historyViewport != nullptr);
    QVERIFY(findReplaceWidget != nullptr);
    QVERIFY(restoreButton != nullptr);
    QVERIFY(settingsAction != nullptr);
    QVERIFY(editor != nullptr);
    QVERIFY(theme != nullptr);
    QCOMPARE(action->shortcut(),
             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    QCOMPARE(settingsAction->text(),
             QStringLiteral("&Appearance and Shortcuts…"));
    QVERIFY(!dock->isVisible());

    window.show();
    action->setChecked(true);
    QTRY_VERIFY(dock->isVisible());
    QVERIFY(action->isChecked());

    vinson::Appearance appearance = theme->appearance();
    appearance.backgroundColor = QColor(30, 40, 50, 48);
    appearance.textColor = QColor(220, 230, 240, 32);
    theme->applyAppearance(appearance);

    const QColor dockBorder =
        dock->property("historyBorderColor").value<QColor>();
    const QColor panelBorder =
        historyWidget->property("historyBorderColor").value<QColor>();
    const QColor separator =
        window.property("historySeparatorColor").value<QColor>();
    QVERIFY(dockBorder.isValid());
    QVERIFY(panelBorder.isValid());
    QVERIFY(separator.isValid());
    QCOMPARE(dockBorder.alpha(), 255);
    QCOMPARE(panelBorder.alpha(), 255);
    QCOMPARE(separator.alpha(), 255);
    QCOMPARE(historyWidget->palette().color(QPalette::Window).alpha(), 255);
    QCOMPARE(historyViewport->palette().color(QPalette::Window).alpha(), 255);
    QCOMPARE(findReplaceWidget->palette().color(QPalette::Window),
             historyWidget->palette().color(QPalette::Window));
    QCOMPARE(findReplaceWidget->property("findPanelBackgroundColor")
                 .value<QColor>(),
             historyWidget->palette().color(QPalette::Window));
    QVERIFY(findReplaceWidget->testAttribute(Qt::WA_StyledBackground));
    QVERIFY(findReplaceWidget->styleSheet().contains(
        QStringLiteral("background: #1e2832")));
    QVERIFY(historyWidget->testAttribute(Qt::WA_StyledBackground));
    QVERIFY(historyWidget->testAttribute(Qt::WA_OpaquePaintEvent));
    QVERIFY(historyViewport->testAttribute(Qt::WA_StyledBackground));
    QVERIFY(restoreButton->minimumHeight() >= 42);
    QVERIFY(!dock->testAttribute(Qt::WA_TranslucentBackground));
    QVERIFY(dock->styleSheet().contains(
        QStringLiteral("border: 1px solid %1").arg(
            dockBorder.name(QColor::HexRgb))));
    QVERIFY(historyWidget->styleSheet().contains(
        QStringLiteral("QListWidget#editHistoryList::item:selected")));
    QVERIFY(historyWidget->styleSheet().contains(
        QStringLiteral("margin: 1px 2px")));
    QVERIFY(historyWidget->styleSheet().contains(
        QStringLiteral("border-radius: 4px")));
    QVERIFY(historyWidget->styleSheet().contains(
        QStringLiteral("background: #1e2832")));
    QVERIFY(historyWidget->styleSheet().contains(
        QStringLiteral("QWidget#editHistoryListViewport")));
    QVERIFY(window.styleSheet().contains(
        QStringLiteral("QMainWindow#vinsonMainWindow::separator")));
    QVERIFY(dock->titleBarWidget() == nullptr);
    QVERIFY(dock->styleSheet().contains(
        QStringLiteral("QDockWidget#editHistoryDock::close-button")));
    QVERIFY(dock->styleSheet().contains(
        QStringLiteral("QDockWidget#editHistoryDock::float-button")));

    QVERIFY(editor->styleSheet().contains(
        QStringLiteral("QWidget#qt_scrollarea_vcontainer")));
    QVERIFY(editor->styleSheet().contains(
        QStringLiteral("margin: 1px 1px 1px 0")));

    dock->setFloating(true);
    QTRY_VERIFY(dock->isFloating());
    QCOMPARE(dock->property("historyFloating").toBool(), true);
    QVERIFY(dock->styleSheet().contains(QStringLiteral("border: none")));
    QVERIFY(dock->styleSheet().contains(QStringLiteral("background: #1e2832")));
}

void MainWindowPersistenceTest::closeToTrayPreservesUnsavedDocument()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor != nullptr);
    window.setCloseToTrayEnabled(true);
    window.show();
    editor->QWidget::setFocus();
    QTest::keyClicks(editor, QStringLiteral("unsaved tray draft"));
    QTRY_VERIFY(editor->modify());

    window.close();
    QVERIFY(!window.isVisible());
    QCOMPARE(editor->textUtf8(), QByteArray("unsaved tray draft"));
    QVERIFY(editor->modify());
}

void MainWindowPersistenceTest::explicitQuitIsSeparateFromCloseToTray()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    vinson::MainWindow window;
    window.setCloseToTrayEnabled(true);
    QSignalSpy quitSpy(&window, &vinson::MainWindow::applicationQuitAccepted);
    window.requestApplicationQuit();
    QCOMPARE(quitSpy.count(), 1);
}

void MainWindowPersistenceTest::savesChangedApplicationState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    {
        vinson::MainWindow window;
        auto* theme = window.findChild<vinson::ThemeManager*>();
        auto* controller = window.findChild<vinson::WindowController*>();
        auto* wrap = window.findChild<QAction*>(QStringLiteral("wordWrapAction"));
        auto* lines = window.findChild<QAction*>(QStringLiteral("lineNumbersAction"));
        QVERIFY(theme != nullptr);
        QVERIFY(controller != nullptr);
        QVERIFY(wrap != nullptr);
        QVERIFY(lines != nullptr);

        vinson::Appearance appearance = theme->appearance();
        appearance.font.setPointSizeF(21.0);
        appearance.backgroundColor = QColor(20, 30, 40, 55);
        theme->applyAppearance(appearance);
        wrap->setChecked(true);
        lines->setChecked(false);
        controller->setAlwaysOnTop(true);
        controller->setFrameless(true);
        window.resize(777, 444);
        window.show();
        QCoreApplication::processEvents();
        window.close();
    }

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("appearance/fontSize")).toDouble(),
             21.0);
    QCOMPARE(settings.value(QStringLiteral("appearance/backgroundColor")).toString(),
             QStringLiteral("#37141e28"));
    QCOMPARE(settings.value(QStringLiteral("view/wordWrap")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("view/lineNumbers")).toBool(), false);
    QCOMPARE(settings.value(QStringLiteral("window/alwaysOnTop")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("window/frameless")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("input/focusShortcut")).toString(),
             vinson::GlobalShortcut::defaultFocusShortcut().toString(
                 QKeySequence::PortableText));
    QVERIFY(!settings.value(QStringLiteral("window/geometry")).toByteArray().isEmpty());
    const QStringList keys = settings.allKeys();
    QVERIFY(std::none_of(keys.cbegin(), keys.cend(), [](const QString& key) {
        return key.contains(QStringLiteral("document"), Qt::CaseInsensitive)
            || key.contains(QStringLiteral("content"), Qt::CaseInsensitive);
    }));
}

void MainWindowPersistenceTest::relocatesGeometryFromMissingDisplay()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    QMainWindow offscreenWindow;
    offscreenWindow.resize(600, 400);
    offscreenWindow.move(1000000, 1000000);
    {
        QSettings settings;
        settings.setValue(QStringLiteral("window/geometry"),
                          offscreenWindow.saveGeometry());
        settings.sync();
    }

    vinson::MainWindow restored;
    const QRect frame = restored.frameGeometry();
    const bool visiblyIntersects = std::any_of(
        QGuiApplication::screens().cbegin(), QGuiApplication::screens().cend(),
        [&frame](const QScreen* screen) {
            const QRect intersection = screen->availableGeometry().intersected(frame);
            return intersection.width() >= 64 && intersection.height() >= 32;
        });
    QVERIFY(visiblyIntersects);
}

QTEST_MAIN(MainWindowPersistenceTest)
#include "MainWindowPersistenceTest.moc"
