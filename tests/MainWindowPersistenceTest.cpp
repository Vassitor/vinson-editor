#include "editor/EditorWidget.h"
#include "settings/ThemeManager.h"
#include "window/MainWindow.h"
#include "window/WindowController.h"

#include <QAction>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
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
    void titleBarMenusUseSoftShadows();
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
                          QStringLiteral("#ff010203"));
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
    QCOMPARE(theme->appearance().textColor, QColor(1, 2, 3, 255));
    QCOMPARE(theme->appearance().backgroundColor, QColor(4, 5, 6, 17));
    QVERIFY(editor->isWordWrapEnabled());
    QVERIFY(!editor->areLineNumbersVisible());
    QVERIFY(wrap->isChecked());
    QVERIFY(!lines->isChecked());
    QVERIFY(controller->isAlwaysOnTop());
    QVERIFY(controller->isFrameless());
    QCOMPARE(window.bossKey(),
             QKeySequence(QStringLiteral("Ctrl+Shift+F12")));
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
    auto* increase = window.findChild<QAction*>(
        QStringLiteral("increaseBackgroundAlphaAction"));
    auto* decrease = window.findChild<QAction*>(
        QStringLiteral("decreaseBackgroundAlphaAction"));
    QVERIFY(editor != nullptr);
    QVERIFY(theme != nullptr);
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
    QCOMPARE(editor->textUtf8(), QByteArray("shortcut keeps document text"));
    QVERIFY(!editor->modify());
    {
        QSettings settings;
        QCOMPARE(settings.value(QStringLiteral("appearance/backgroundColor"))
                     .toString().left(3),
                 QStringLiteral("#fa"));
    }

    QTest::keyClick(editor, Qt::Key_Up, Qt::ControlModifier);
    QTRY_COMPARE(theme->appearance().backgroundColor.alpha(), 255);
    QCOMPARE(editor->textUtf8(), QByteArray("shortcut keeps document text"));
}

void MainWindowPersistenceTest::titleBarMenusUseSoftShadows()
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
        auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(
            menu->graphicsEffect());
        QVERIFY(shadow != nullptr);
        QVERIFY(shadow->blurRadius() >= 20.0);
        QVERIFY(shadow->color().alpha() > 0);
    }
    QCOMPARE(menuCount, 5);
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
