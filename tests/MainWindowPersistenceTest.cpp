#include "editor/EditorWidget.h"
#include "search/SearchController.h"
#include "session/RecoveryManager.h"
#include "settings/ThemeManager.h"
#include "window/GlobalShortcut.h"
#include "window/MainWindow.h"
#include "window/WindowController.h"
#include "ui/SettingsDialog.h"
#include "ui/FindReplaceWidget.h"
#include "ui/MenuAppearance.h"
#include "window/NativeTitleBar.h"

#include <QAction>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QDockWidget>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionTab>
#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QTabBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <QtTest>


#include <algorithm>
#include <cmath>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <dwmapi.h>
#endif

class MainWindowPersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void savesSelectedEncodingAndConvertedLineEndings_data();
    void savesSelectedEncodingAndConvertedLineEndings();
    void initTestCase();
    void usesWrappedTextByDefault();
    void migratesLegacyHorizontalScrollingDefault();
    void restoresPersistedApplicationState();
    void restoresAndUsesCustomApplicationShortcuts();
    void framelessShortcutRemainsAvailableWithHiddenMenuBar();
    void framelessWindowShrinksToOneLineAndTracksAppearance();
    void backgroundOpacityShortcutsArePersistent();
    void controlWheelFontSizeIsPersistent();
    void switchesPersistedCustomStyles();
    void focusRequestTargetsEditor();
    void restoresAndClearsRecentFiles();
    void opensFilesInTabsAndReusesPristinePage();
    void switchesTabsWithControlAltArrows();
    void reordersTabsWithinTopBar();
    void titleBarPreservesNativeWindowControls();
    void rendersTabsAtAnimatedPositions();
    void tabIndicatorsFollowDocumentAndHoverState();
    void closingLastTabKeepsDocumentIdentityAndClearsSession();
    void reopensMostRecentlyClosedFileTab();
    void reloadsUnmodifiedFileAfterExternalChange();
    void preservesDirtyEditorAfterExternalConflict();
    void detectsDiskChangesEvenBeforeWatcherNotification();
    void findPrefillHandlesUnicodeAndLargeSelections();
    void cancelsLargeFileSearchFromWindow_data();
    void cancelsLargeFileSearchFromWindow();
    void queuesExternalOpenWhileSearching();
    void bookmarkActionsNavigateAndKeepTabsIndependent();
    void restoresCustomBookmarkShortcuts();
    void restoresDefaultApplicationShortcutsFromSettings();
    void tabBarFillsRemainingSpaceAndScrollsWithoutSwitching();
    void tabContextMenuTargetsClickedDocument();
    void tabContextMenuClosesRangesAndStopsOnCancel();
    void showsTabBarOnlyInDefaultMode();
    void restoresOpenTabsWhenEnabled();
    void titleBarMenusUseNativeFrames();
    void nativeMenusPreserveInteractionAndReopening();
    void exposesEditHistoryPanel();
    void closeToTrayPreservesUnsavedDocument();
    void explicitQuitIsSeparateFromCloseToTray();
    void dismissingRecoveryPromptKeepsSnapshots();
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

void MainWindowPersistenceTest::cancelsLargeFileSearchFromWindow_data()
{
    QTest::addColumn<int>("cancelMethod");
    QTest::newRow("panel-button") << 0;
    QTest::newRow("escape") << 1;
    QTest::newRow("minimal-escape") << 2;
    QTest::newRow("status-button") << 3;
}

void MainWindowPersistenceTest::savesSelectedEncodingAndConvertedLineEndings_data()
{
    QTest::addColumn<int>("encoding");
    QTest::addColumn<QByteArray>("prefix");
    QTest::newRow("utf8") << int(vinson::TextEncoding::Utf8) << QByteArray::fromHex("610d0a620d0ae4b8adf09f98800d0a");
    QTest::newRow("bom") << int(vinson::TextEncoding::Utf8Bom) << QByteArray::fromHex("efbbbf610d0a620d0ae4b8adf09f98800d0a");
    QTest::newRow("le") << int(vinson::TextEncoding::Utf16Le) << QByteArray::fromHex("fffe61000d000a0062000d000a002d4e3dd800de0d000a00");
    QTest::newRow("be") << int(vinson::TextEncoding::Utf16Be) << QByteArray::fromHex("feff0061000d000a0062000d000a4e2dd83dde00000d000a");
}

void MainWindowPersistenceTest::savesSelectedEncodingAndConvertedLineEndings()
{
    QFETCH(int, encoding);
    QFETCH(QByteArray, prefix);
    QTemporaryDir directory;
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("formats.txt"));
    const QByteArray original = QByteArray::fromHex("610a620d0ae4b8adf09f98800d");
    { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(original), qint64(original.size())); }
    vinson::MainWindow window(nullptr, false);
    window.show();
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* tabs = window.findChild<QTabBar*>();
    window.openFiles({path});
    QTRY_COMPARE(editor->textUtf8(), original);
    QTRY_VERIFY(editor->isEnabled());
    auto* encodingAction = window.findChild<QAction*>(QStringLiteral("saveEncoding%1").arg(encoding));
    auto* crlfAction = window.findChild<QAction*>(QStringLiteral("convertLineEnding%1").arg(int(vinson::LineEnding::CrLf)));
    QVERIFY(encodingAction && crlfAction);
    auto* initialSelection = encoding == int(vinson::TextEncoding::Utf8)
        ? window.findChild<QAction*>(QStringLiteral("saveEncoding%1").arg(int(vinson::TextEncoding::Utf8Bom)))
        : encodingAction;
    QVERIFY(initialSelection);
    initialSelection->trigger();
    QVERIFY(initialSelection->isChecked());
    QCOMPARE(editor->textUtf8(), original);
    QVERIFY(!editor->modify());
    QVERIFY(window.windowTitle().contains('*'));
    editor->setSel(editor->documentLength(), editor->documentLength());
    editor->replaceSelectionUtf8("x");
    editor->undo();
    QVERIFY(window.windowTitle().contains('*'));
    encodingAction->trigger();
    QCOMPARE(window.windowTitle().contains('*'), encoding != int(vinson::TextEncoding::Utf8));
    window.handleExternalOpenRequest({});
    QCOMPARE(tabs->count(), 2);
    tabs->setCurrentIndex(0);
    QVERIFY(encodingAction->isChecked());
    crlfAction->trigger();
    QCOMPARE(editor->textUtf8(), QByteArray::fromHex("610d0a620d0ae4b8adf09f98800d0a"));
    const auto newlineMode = editor->eOLMode();
    tabs->setCurrentIndex(1);
    tabs->setCurrentIndex(0);
    QCOMPARE(editor->eOLMode(), newlineMode);
    QAction* save = nullptr;
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("shortcutId") == QStringLiteral("save")) save = action;
    }
    QVERIFY(save);
    save->trigger();
    QTRY_VERIFY(!window.windowTitle().contains('*'));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), prefix);
    QVERIFY(encodingAction->isChecked());
    QVERIFY(!editor->modify());
}

void MainWindowPersistenceTest::cancelsLargeFileSearchFromWindow()
{
    QFETCH(int, cancelMethod);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window(nullptr, false);
    window.show();
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* search = window.findChild<vinson::SearchController*>();
    auto* panel = window.findChild<vinson::FindReplaceWidget*>();
    auto* tabs = window.findChild<QTabBar*>();
    auto* modes = window.findChild<vinson::WindowController*>();
    QVERIFY(editor && search && panel && tabs && modes);
    const qint64 length = 2 * vinson::LargeFilePolicy::responsiveSearchSlice;
    editor->setWordWrapEnabled(false);
    QVERIFY(editor->beginFileLoad(vinson::LargeFileMode::Large, length));
    QByteArray text(length, 'x');
    for (qsizetype position = 63; position < text.size(); position += 64) {
        text[position] = '\n';
    }
    editor->appendTextUtf8(text);
    editor->completeFileLoad(vinson::LineEnding::Lf);
    editor->setSel(2, 5);
    panel->open(true, QStringLiteral("missing"));
    if (cancelMethod == 2) {
        modes->setMinimalMode(true);
    }
    auto* cancel = panel->findChild<QPushButton*>(QStringLiteral("cancelSearchButton"));
    QVERIFY(cancel);
    bool cancelledAfterScan = false;
    connect(search, &vinson::SearchController::progressChanged,
            &window, [&](int percent) {
                if (percent <= 0) {
                    return;
                }
                cancelledAfterScan = true;
                if (cancelMethod == 0) {
                    QTest::mouseClick(cancel, Qt::LeftButton);
                } else if (cancelMethod == 3) {
                    auto* statusCancel = window.statusBar()->findChild<QPushButton*>();
                    QVERIFY(statusCancel);
                    QVERIFY(statusCancel->isVisible());
                    QTest::mouseClick(statusCancel, Qt::LeftButton);
                } else {
                    QTest::keyClick(cancelMethod == 2
                                        ? static_cast<QWidget*>(&window)
                                        : static_cast<QWidget*>(cancel), Qt::Key_Escape);
                }
            });

    QCOMPARE(search->findNext(), vinson::SearchResult::Searching);
    QVERIFY(!editor->isEnabled());
    QVERIFY(!tabs->isEnabled());
    QVERIFY(panel->isEnabled());
    QVERIFY(cancel->isEnabled());
    for (const auto* action : window.findChildren<QAction*>()) {
        const QString id = action->property("shortcutId").toString();
        if (id == QLatin1String("undo") || id == QLatin1String("toggleBookmark")
            || id == QLatin1String("nextBookmark")
            || id == QLatin1String("previousBookmark")
            || id == QLatin1String("clearBookmarks")) {
            QVERIFY(!action->isEnabled());
        }
    }
    QTRY_VERIFY(!search->isSearching());
    QVERIFY(cancelledAfterScan);
    QVERIFY(editor->isEnabled());
    QVERIFY(tabs->isEnabled());
    QCOMPARE(editor->selectionStartPosition(), 2);
    QCOMPARE(editor->selectionEndPosition(), 5);
    QCOMPARE(editor->documentLength(), length);
    QCOMPARE(modes->isMinimalMode(), cancelMethod == 2);
    if (cancelMethod == 2) {
        modes->setMinimalMode(false);
    }
    auto* find = panel->findChild<QLineEdit*>(QStringLiteral("findText"));
    QVERIFY(find && find->isEnabled());
    QTest::keyClicks(editor, QStringLiteral("ok"));
    QCOMPARE(editor->textRangeUtf8(2, 2), QByteArray("ok"));
}

void MainWindowPersistenceTest::queuesExternalOpenWhileSearching()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("queued.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("opened"), qint64(6));
    file.close();
    vinson::MainWindow window(nullptr, false);
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* search = window.findChild<vinson::SearchController*>();
    auto* tabs = window.findChild<QTabBar*>();
    QVERIFY(editor && search && tabs);
    QVERIFY(editor->beginFileLoad(vinson::LargeFileMode::Large));
    editor->appendTextUtf8("first document");
    editor->completeFileLoad(vinson::LineEnding::None);
    search->setSearchText(QStringLiteral("missing"));
    QCOMPARE(search->findNext(), vinson::SearchResult::Searching);
    window.handleExternalOpenRequest({path});
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(editor->textUtf8(), QByteArray("first document"));
    search->cancelSearch();
    QTRY_COMPARE(editor->textUtf8(), QByteArray("opened"));
    QCOMPARE(tabs->count(), 2);
    QVERIFY(editor->isEnabled());
}

void MainWindowPersistenceTest::bookmarkActionsNavigateAndKeepTabsIndependent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.txt"));
    for (const QString& path : {firstPath, secondPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("first\nsecond\nthird"), qint64(18));
    }
    vinson::MainWindow window(nullptr, false);
    window.show();
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* tabs = window.findChild<QTabBar*>();
    auto* modes = window.findChild<vinson::WindowController*>();
    auto* toggle = window.findChild<QAction*>(QStringLiteral("toggleBookmarkAction"));
    auto* next = window.findChild<QAction*>(QStringLiteral("nextBookmarkAction"));
    auto* previous = window.findChild<QAction*>(QStringLiteral("previousBookmarkAction"));
    auto* clear = window.findChild<QAction*>(QStringLiteral("clearBookmarksAction"));
    QVERIFY(editor && tabs && modes && toggle && next && previous && clear);
    QCOMPARE(toggle->shortcut(), QKeySequence(QStringLiteral("Ctrl+F2")));
    QCOMPARE(next->shortcut(), QKeySequence(QStringLiteral("F2")));
    QCOMPARE(previous->shortcut(), QKeySequence(QStringLiteral("Shift+F2")));
    QCOMPARE(clear->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+F2")));
    window.openFiles({firstPath, secondPath});
    QTRY_COMPARE(tabs->count(), 2);
    QTRY_VERIFY(editor->isEnabled());
    tabs->setCurrentIndex(0);
    QVERIFY(editor->goToOneBasedLine(2));
    QTest::keyClick(editor, Qt::Key_F2, Qt::ControlModifier);
    QVERIFY(editor->hasBookmarkAtLine(2));
    QVERIFY(toggle->isChecked());
    QVERIFY(editor->goToOneBasedLine(3));
    toggle->trigger();
    QVERIFY(editor->hasBookmarkAtLine(3));
    QTest::keyClick(editor, Qt::Key_F2);
    QCOMPARE(editor->currentOneBasedLine(), 2);
    QTest::keyClick(editor, Qt::Key_F2, Qt::ShiftModifier);
    QCOMPARE(editor->currentOneBasedLine(), 3);
    QVERIFY(!editor->modify());

    tabs->setCurrentIndex(1);
    QVERIFY(!editor->hasBookmarkAtLine(2));
    QVERIFY(!editor->hasBookmarkAtLine(3));
    QVERIFY(editor->goToOneBasedLine(1));
    toggle->trigger();
    QVERIFY(editor->hasBookmarkAtLine(1));
    tabs->setCurrentIndex(0);
    QVERIFY(editor->hasBookmarkAtLine(2));
    QVERIFY(editor->hasBookmarkAtLine(3));
    QTRY_VERIFY(toggle->isChecked());
    QTest::keyClick(editor, Qt::Key_F2, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(!editor->hasBookmarkAtLine(2));
    QVERIFY(!editor->hasBookmarkAtLine(3));
    tabs->setCurrentIndex(1);
    QVERIFY(editor->hasBookmarkAtLine(1));

    modes->setMinimalMode(true);
    QCOMPARE(editor->marginWidthN(1), 0);
    QVERIFY(editor->goToOneBasedLine(3));
    QTest::keyClick(editor, Qt::Key_F2);
    QCOMPARE(editor->currentOneBasedLine(), 1);
    QVERIFY(modes->isMinimalMode());
    modes->setMinimalMode(false);
    QVERIFY(editor->marginWidthN(1) > 0);
    QVERIFY(editor->hasBookmarkAtLine(1));

    QAction* reload = nullptr;
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("shortcutId") == QStringLiteral("reload")) {
            reload = action;
        }
    }
    QVERIFY(reload);
    reload->trigger();
    QTRY_VERIFY(!editor->hasBookmarkAtLine(1));
    QTRY_VERIFY(editor->isEnabled());
    QCOMPARE(editor->textUtf8(), QByteArray("first\nsecond\nthird"));
    QVERIFY(!editor->modify());
}

void MainWindowPersistenceTest::restoresCustomBookmarkShortcuts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    {
        QSettings settings;
        settings.setValue(QStringLiteral("shortcuts/toggleBookmark"), QStringLiteral("Alt+F9"));
        settings.setValue(QStringLiteral("shortcuts/nextBookmark"), QStringLiteral("Alt+F10"));
        settings.setValue(QStringLiteral("shortcuts/previousBookmark"), QStringLiteral("Alt+Shift+F10"));
        settings.setValue(QStringLiteral("shortcuts/clearBookmarks"), QStringLiteral("Alt+Shift+F9"));
        settings.sync();
    }
    vinson::MainWindow window;
    window.show();
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor);
    editor->setTextUtf8("first\nsecond\nthird");
    auto* toggle = window.findChild<QAction*>(QStringLiteral("toggleBookmarkAction"));
    QVERIFY(toggle);
    QCOMPARE(toggle->shortcut(), QKeySequence(QStringLiteral("Alt+F9")));
    window.activateWindow();
    editor->QWidget::setFocus();
    QCoreApplication::processEvents();
    QTest::keyClick(editor, Qt::Key_F9, Qt::AltModifier);
    QVERIFY(editor->hasBookmarkAtLine(1));
    QVERIFY(editor->goToOneBasedLine(3));
    QTest::keyClick(editor, Qt::Key_F9, Qt::AltModifier);
    QVERIFY(editor->hasBookmarkAtLine(3));
    QTest::keyClick(editor, Qt::Key_F10, Qt::AltModifier);
    QCOMPARE(editor->currentOneBasedLine(), 1);
    QTest::keyClick(editor, Qt::Key_F10, Qt::AltModifier | Qt::ShiftModifier);
    QCOMPARE(editor->currentOneBasedLine(), 3);
    QTest::keyClick(editor, Qt::Key_F9, Qt::AltModifier | Qt::ShiftModifier);
    QVERIFY(!editor->hasBookmarkAtLine(1));
    QVERIFY(!editor->hasBookmarkAtLine(3));
    QVERIFY(!editor->modify());
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

void MainWindowPersistenceTest::framelessWindowShrinksToOneLineAndTracksAppearance()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* controller = window.findChild<vinson::WindowController*>();
    auto* theme = window.findChild<vinson::ThemeManager*>();
    QVERIFY(editor != nullptr);
    QVERIFY(controller != nullptr);
    QVERIFY(theme != nullptr);
    window.show();
    QCoreApplication::processEvents();
    controller->setFrameless(true);
    QCoreApplication::processEvents();
    int lineHeight = static_cast<int>(std::ceil(editor->textHeightF(0)));
    QCOMPARE(window.minimumHeight(), lineHeight);
    window.resize(400, lineHeight);
    QCoreApplication::processEvents();
    QCOMPARE(window.height(), lineHeight);
    QCOMPARE(editor->viewport()->height(), lineHeight);

    auto appearance = theme->appearance();
    appearance.font.setPointSizeF(24.0);
    appearance.lineSpacing = 8;
    theme->applyAppearance(appearance);
    lineHeight = static_cast<int>(std::ceil(editor->textHeightF(0)));
    QCOMPARE(window.minimumHeight(), lineHeight);
    window.resize(400, lineHeight);
    QCoreApplication::processEvents();
    QCOMPARE(window.height(), lineHeight);
    QCOMPARE(editor->viewport()->height(), lineHeight);

    appearance.font.setPointSizeF(10.0);
    appearance.lineSpacing = 0;
    theme->applyAppearance(appearance);
    lineHeight = static_cast<int>(std::ceil(editor->textHeightF(0)));
    QCOMPARE(window.minimumHeight(), lineHeight);
    window.resize(400, lineHeight);
    QCoreApplication::processEvents();
    QCOMPARE(editor->viewport()->height(), lineHeight);

    controller->setFrameless(false);
    QTRY_VERIFY(window.menuBar()->isVisible());
    QTRY_VERIFY(window.statusBar()->isVisible());
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
    if (tabs->property("inTitleBar").toBool())
        QCOMPARE(tabs->parentWidget()->objectName(), QStringLiteral("documentTitleBar"));
    else
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

void MainWindowPersistenceTest::titleBarPreservesNativeWindowControls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    // Keep the restore geometry inside the desktop even with extra Qt scaling;
    // Windows intentionally clamps oversized saved windows to the work area.
    window.resize(640, 360);
    window.show();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* title = static_cast<vinson::NativeTitleBar*>(
        window.findChild<QObject*>(QStringLiteral("nativeTitleBar")));
    QVERIFY(tabs && title);
    if (!title->isActive()) {
        QCOMPARE(window.menuBar()->cornerWidget(Qt::TopRightCorner), tabs);
        QVERIFY(title->captionButtonRect().isEmpty());
        return;
    }
#if defined(Q_OS_WIN)
    QTRY_VERIFY(window.property("nativeTitleBarActive").toBool());
    QVERIFY(!window.windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(!window.menuBar()->cornerWidget(Qt::TopRightCorner));
    QCOMPARE(tabs->parentWidget()->objectName(), QStringLiteral("documentTitleBar"));
    QVERIFY(window.menuBar()->y() >= tabs->parentWidget()->height());
    const auto checkCaption = [&] {
        const HWND handle = reinterpret_cast<HWND>(window.winId());
        const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);
        constexpr LONG_PTR required = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU
            | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        QCOMPARE(style & required, required);
        RECT buttons{}, frame{};
        QVERIFY(SUCCEEDED(DwmGetWindowAttribute(handle, DWMWA_CAPTION_BUTTON_BOUNDS,
                                               &buttons, sizeof(buttons))));
        QVERIFY(GetWindowRect(handle, &frame));
        const int width = (buttons.right - buttons.left) / 3;
        const int y = frame.top + (buttons.top + buttons.bottom) / 2;
        const int expected[] = {HTMINBUTTON, HTMAXBUTTON, HTCLOSE};
        for (int i = 0; i < 3; ++i) {
            const int x = frame.left + buttons.left + width * i + width / 2;
            QCOMPARE(SendMessageW(handle, WM_NCHITTEST, 0, MAKELPARAM(x, y)),
                     static_cast<LRESULT>(expected[i]));
            // DWM itself must recognize the buttons, including when maximized.
            // Our hit-test fallback alone enables clicks but cannot restore the
            // system hover animation when a top nonclient inset blocks DWM.
            // DWM updates geometry asynchronously after resizing or showing.
            const auto dwmRecognizesButton = [&] {
                RECT liveButtons{}, liveFrame{};
                if (FAILED(DwmGetWindowAttribute(handle, DWMWA_CAPTION_BUTTON_BOUNDS,
                                                &liveButtons, sizeof(liveButtons)))
                    || !GetWindowRect(handle, &liveFrame))
                    return false;
                const int buttonWidth = (liveButtons.right - liveButtons.left) / 3;
                const int hitX = liveFrame.left + liveButtons.left + buttonWidth * i + buttonWidth / 2;
                const int hitY = liveFrame.top + (liveButtons.top + liveButtons.bottom) / 2;
                LRESULT dwmHit = HTNOWHERE;
                return DwmDefWindowProc(handle, WM_NCHITTEST, 0, MAKELPARAM(hitX, hitY), &dwmHit)
                    && dwmHit == expected[i];
            };
            QTRY_VERIFY(dwmRecognizesButton());
        }
        const int tabStripRight = tabs->mapTo(
            &window, QPoint(tabs->width(), 0)).x();
        QTRY_COMPARE(tabStripRight, title->captionButtonRect().left());
        QVERIFY2(tabs->geometry().right() < title->captionButtonRect().left(),
            qPrintable(QStringLiteral("tabs right=%1, caption left=%2, maximized=%3, size=%4x%5")
                .arg(tabs->geometry().right()).arg(title->captionButtonRect().left())
                .arg(window.isMaximized()).arg(window.width()).arg(window.height())));
        const QImage image = window.grab().toImage();
        const QPoint center = title->captionButtonRect().center();
        const QPoint pixel(qRound(center.x() * image.devicePixelRatio()),
                           qRound(center.y() * image.devicePixelRatio()));
        QCOMPARE(image.pixelColor(pixel).alpha(), 0);
        // The entire reserved corner stays transparent, including below the
        // reported hit bounds when maximized frame metrics change.
        const QRect corner = title->captionPaintRect();
        const QPoint lowerPixel(qRound(corner.center().x() * image.devicePixelRatio()),
                                qRound((corner.bottom() - 1) * image.devicePixelRatio()));
        QCOMPARE(image.pixelColor(lowerPixel).alpha(), 0);
    };
    checkCaption();
    const QSize normalSize = window.size();
    const auto hitAt = [&](const QPoint& point) {
        const HWND handle = reinterpret_cast<HWND>(window.winId());
        POINT nativePoint{qRound(point.x() * window.devicePixelRatioF()),
                          qRound(point.y() * window.devicePixelRatioF())};
        ClientToScreen(handle, &nativePoint);
        return SendMessageW(handle, WM_NCHITTEST, 0, MAKELPARAM(nativePoint.x, nativePoint.y));
    };
    QCOMPARE(hitAt(tabs->mapTo(&window, tabs->tabRect(0).center())), LRESULT(HTCLIENT));
    QCOMPARE(hitAt(QPoint(title->captionButtonRect().left() - 32, 20)), LRESULT(HTCAPTION));
    QCOMPARE(hitAt(QPoint(window.width() / 2, 1)), LRESULT(HTTOP));
    window.resize(480, normalSize.height());
    checkCaption();
    window.resize(normalSize);
    window.showMinimized();
    QTRY_VERIFY(window.isMinimized());
    window.showNormal();
    QTRY_VERIFY(!window.isMinimized());
    checkCaption();
    window.showMaximized();
    QTRY_VERIFY(window.isMaximized());
    QTest::qWait(100);
    checkCaption();
    SendMessageW(reinterpret_cast<HWND>(window.winId()), WM_SYSCOMMAND, SC_RESTORE, 0);
    QTRY_VERIFY(!window.isMaximized());
    QTest::qWait(100);
    checkCaption();
    auto* controller = window.findChild<vinson::WindowController*>();
    controller->setFrameless(true);
    QTRY_VERIFY(!title->isActive());
    QVERIFY(!tabs->isVisible());
    QCOMPARE(window.contentsMargins().top(), 0);
    controller->setFrameless(false);
    QTRY_VERIFY(title->isActive() && tabs->isVisible());
    QTest::qWait(150);
    checkCaption();
    controller->setMinimalMode(true);
    QTRY_VERIFY(!title->isActive());
    controller->setMinimalMode(false);
    QTRY_VERIFY(title->isActive() && tabs->isVisible());
    QTest::qWait(150);
    checkCaption();
    QCOMPARE(window.size(), normalSize);
#endif
}

void MainWindowPersistenceTest::rendersTabsAtAnimatedPositions()
{
    QTemporaryDir directory;
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    window.resize(1100, 650);
    window.show();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    editor->setTextUtf8("first");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("second");
    QCoreApplication::processEvents();
    QCOMPARE(window.menuBar()->contextMenuPolicy(), Qt::PreventContextMenu);
    QVERIFY(!tabs->autoFillBackground());

    // Both displaced neighbours and the dragged tab must render in the
    // supplied rectangle, independently of the tab's layout position.
    for (bool selected : {false, true}) {
        const auto render = [&](int offset, bool focused = false) {
            QImage image(240, 48, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.translate(-offset, 0);
            QStyleOptionTab option;
            option.initFrom(tabs);
            option.rect = QRect(offset, 0, 200, 36);
            option.tabIndex = 1;
            option.position = QStyleOptionTab::Middle;
            option.text = QStringLiteral("Moving document");
            option.rightButtonSize = QSize(20, 20);
            option.state.setFlag(QStyle::State_Selected, selected);
            option.state.setFlag(QStyle::State_HasFocus, focused);
            tabs->style()->drawControl(QStyle::CE_TabBarTab, &option, &painter, tabs);
            return image;
        };
        const auto still = render(0);
        QVERIFY(still.pixelColor(15, 10).alpha() > 0);
        QCOMPARE(render(47), still);
        QCOMPARE(render(-39), still);
        QCOMPARE(render(0, true), still);
    }

    tabs->setCurrentIndex(0);
    const auto handle = tabs->tabData(0);
    auto* close = tabs->tabButton(0, QTabBar::RightSide);
    QVERIFY(close);
    const QPoint start = tabs->tabRect(0).center();
    const QPoint closeStart = close->pos();
    QTest::mousePress(tabs, Qt::LeftButton, Qt::NoModifier, start);
    const QPoint shift(30, 0);
    QMouseEvent move(QEvent::MouseMove, start + shift,
                     tabs->mapToGlobal(start + shift), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tabs, &move);
    QCOMPARE(close->pos(), closeStart + shift);
    const QPoint destination = tabs->tabRect(1).center();
    QMouseEvent reorder(QEvent::MouseMove, destination,
                        tabs->mapToGlobal(destination), Qt::NoButton,
                        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tabs, &reorder);
    QTest::mouseRelease(tabs, Qt::LeftButton, Qt::NoModifier, destination);
    QTRY_COMPARE(tabs->tabData(1), handle);
    QTRY_VERIFY(tabs->tabRect(1).contains(close->geometry()));
    QCOMPARE(editor->textUtf8(), QByteArray("first"));
}

void MainWindowPersistenceTest::tabIndicatorsFollowDocumentAndHoverState()
{
    QTemporaryDir directory;
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    window.resize(1000, 500);
    window.show();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    editor->setTextUtf8("first");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("second");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("third");
    const auto hover = [&](int index) {
        if (index < 0) {
            QEvent leave(QEvent::HoverLeave);
            QApplication::sendEvent(tabs, &leave);
        } else {
            const QPoint point = tabs->tabRect(index).center();
            QHoverEvent move(QEvent::HoverMove, point, tabs->mapToGlobal(point), QPointF(-1, -1));
            QApplication::sendEvent(tabs, &move);
        }
    };
    const auto indicator = [&](int index) {
        QImage image(20, 20, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        tabs->tabButton(index, QTabBar::RightSide)->render(
            &painter, QPoint(), QRegion(), QWidget::DrawChildren);
        return image;
    };
    hover(-1);
    QImage empty(20, 20, QImage::Format_ARGB32_Premultiplied);
    empty.fill(Qt::transparent);
    QCOMPARE(indicator(0), empty); // Saved, inactive: no close icon or dot.
    const QImage cross = indicator(2);
    QVERIFY(cross != empty); // Saved, selected: close icon.
    const QRect labelBefore = tabs->tabRect(2);
    const QRect buttonBefore = tabs->tabButton(2, QTabBar::RightSide)->geometry();
    editor->appendTextUtf8(" edited");
    const QImage dot = indicator(2);
    QVERIFY(dot != empty && dot != cross);
    QVERIFY(dot.pixelColor(9, 9).alpha() > 0);
    QVERIFY(!tabs->tabText(2).startsWith('*'));
    hover(2);
    QCOMPARE(indicator(2), cross); // Hover replaces the dirty dot with close.
    hover(-1);
    QCOMPARE(indicator(2), dot);
    QCOMPARE(tabs->tabRect(2), labelBefore);
    QCOMPARE(tabs->tabButton(2, QTabBar::RightSide)->geometry(), buttonBefore);
    tabs->setCurrentIndex(0);
    QCOMPARE(indicator(2), dot); // Dirty marker follows the inactive document.
    QCOMPARE(indicator(0), cross);
    tabs->moveTab(2, 1);
    QCOMPARE(indicator(1), dot);
    tabs->setCurrentIndex(1);
    editor->undo();
    QCOMPARE(indicator(1), cross); // Returning to the save point clears the dot.

    tabs->setCurrentIndex(2);
    const auto renderFirst = [&] {
        QImage image(180, 36, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QStyleOptionTab option;
        option.initFrom(tabs);
        option.rect = image.rect();
        option.tabIndex = 0;
        option.state.setFlag(QStyle::State_Selected, false);
        option.state.setFlag(QStyle::State_MouseOver, false);
        option.state.setFlag(QStyle::State_HasFocus, false);
        tabs->style()->drawControl(QStyle::CE_TabBarTab, &option, &painter, tabs);
        return image;
    };
    hover(-1);
    const QImage separated = renderFirst();
    hover(1);
    const QImage adjacentHovered = renderFirst();
    QVERIFY(separated.pixelColor(178, 20).alpha() > 0);
    QCOMPARE(adjacentHovered.pixelColor(178, 20).alpha(), 0);
    hover(0);
    const QVariant remainingDocument = tabs->tabData(1);
    QTest::mouseClick(tabs->tabButton(0, QTabBar::RightSide), Qt::LeftButton);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabData(0), remainingDocument);
}

void MainWindowPersistenceTest::closingLastTabKeepsDocumentIdentityAndClearsSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("last.txt"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("saved"), qint64(5));
    }
    vinson::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(tabs && editor);
    window.openFiles({path});
    QTRY_COMPARE(editor->textUtf8(), QByteArray("saved"));
    tabs->tabCloseRequested(0);
    QVERIFY(editor->isEmpty());
    QCOMPARE(tabs->tabData(0).value<qintptr>(), static_cast<qintptr>(editor->docPointer()));
    QSettings saved;
    QVERIFY(saved.value(QStringLiteral("session/openTabs")).toStringList().isEmpty());
    editor->setTextUtf8("first");
    window.handleExternalOpenRequest({});
    editor->setTextUtf8("second");
    tabs->moveTab(0, 1);
    tabs->setCurrentIndex(1);
    QCOMPARE(editor->textUtf8(), QByteArray("first"));
    tabs->setCurrentIndex(0);
    QCOMPARE(editor->textUtf8(), QByteArray("second"));
}

void MainWindowPersistenceTest::reopensMostRecentlyClosedFileTab()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("reopen.txt"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("reopened"), qint64(8));
    }

    vinson::MainWindow window;
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* reopen = window.findChild<QAction*>(
        QStringLiteral("reopenClosedTabAction"));
    QVERIFY(tabs && editor && reopen);
    QVERIFY(!reopen->isEnabled());

    window.openFiles({path});
    QTRY_COMPARE(editor->textUtf8(), QByteArray("reopened"));
    tabs->tabCloseRequested(0);
    QVERIFY(editor->isEmpty());
    QVERIFY(reopen->isEnabled());

    reopen->trigger();
    QTRY_COMPARE(editor->textUtf8(), QByteArray("reopened"));
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabText(0), QStringLiteral("reopen.txt"));
    QVERIFY(!reopen->isEnabled());
}

void MainWindowPersistenceTest::reloadsUnmodifiedFileAfterExternalChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("external.txt"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("before"), qint64(6));
    }

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor);
    window.openFiles({path});
    QTRY_COMPARE_WITH_TIMEOUT(editor->textUtf8(), QByteArray("before"), 5000);
    QVERIFY(!editor->modify());

    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("after"), qint64(5));
    }
    QTRY_COMPARE_WITH_TIMEOUT(editor->textUtf8(), QByteArray("after"), 5000);
    QVERIFY(!editor->modify());
}

void MainWindowPersistenceTest::preservesDirtyEditorAfterExternalConflict()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("conflict.txt"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("disk"), qint64(4));
    }

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    QVERIFY(editor);
    window.openFiles({path});
    QTRY_COMPARE_WITH_TIMEOUT(editor->textUtf8(), QByteArray("disk"), 5000);
    editor->appendTextUtf8("-editor");
    QTRY_VERIFY(editor->modify());

    bool keptEditorChanges = false;
    QTimer promptChooser;
    connect(&promptChooser, &QTimer::timeout, this, [&] {
        auto* message = qobject_cast<QMessageBox*>(
            QApplication::activeModalWidget());
        if (!message) {
            return;
        }
        for (QAbstractButton* button : message->buttons()) {
            if (button->text() == QStringLiteral("Keep Editor Changes")) {
                keptEditorChanges = true;
                promptChooser.stop();
                button->click();
                return;
            }
        }
    });
    promptChooser.start(10);
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("external"), qint64(8));
    }

    QTRY_VERIFY_WITH_TIMEOUT(keptEditorChanges, 5000);
    QCOMPARE(editor->textUtf8(), QByteArray("disk-editor"));
    QVERIFY(editor->modify());

    QAction* saveAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->property("shortcutId").toString() == QStringLiteral("save")) {
            saveAction = action;
            break;
        }
    }
    QVERIFY(saveAction);

    bool canceledOverwrite = false;
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (message && message->windowTitle() == QStringLiteral("File changed on disk")) {
            canceledOverwrite = true;
            message->reject();
        }
    });
    saveAction->trigger();
    QVERIFY(canceledOverwrite);
    QFile unchanged(path);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), QByteArray("external"));
    unchanged.close();
    QVERIFY(editor->modify());

    bool confirmedOverwrite = false;
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!message) {
            return;
        }
        for (QAbstractButton* button : message->buttons()) {
            if (button->text() == QStringLiteral("Overwrite File")) {
                confirmedOverwrite = true;
                button->click();
                return;
            }
        }
    });
    saveAction->trigger();
    QVERIFY(confirmedOverwrite);
    QTRY_VERIFY(!editor->modify());
    QFile overwritten(path);
    QVERIFY(overwritten.open(QIODevice::ReadOnly));
    QCOMPARE(overwritten.readAll(), QByteArray("disk-editor"));
}

void MainWindowPersistenceTest::detectsDiskChangesEvenBeforeWatcherNotification()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const QString path = directory.filePath(QStringLiteral("early-change.txt"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("original"), qint64(8));
    }

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* monitor = window.findChild<vinson::FileChangeMonitor*>();
    QVERIFY(editor && monitor);
    window.openFiles({path});
    QTRY_COMPARE(editor->textUtf8(), QByteArray("original"));
    editor->appendTextUtf8("-draft");
    QVERIFY(editor->modify());

    const QDateTime originalModifiedAt = QFileInfo(path).lastModified();
    QVERIFY(originalModifiedAt.isValid());
    monitor->suspendFile(path);
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("revised!"), qint64(8));
        QVERIFY(file.flush());
        QVERIFY(file.setFileTime(originalModifiedAt,
                                 QFileDevice::FileModificationTime));
    }
    QCOMPARE(QFileInfo(path).size(), qint64(8));
    QCOMPARE(QFileInfo(path).lastModified(), originalModifiedAt);

    QAction* saveAction = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->property("shortcutId").toString() == QStringLiteral("save")) {
            saveAction = action;
            break;
        }
    }
    QVERIFY(saveAction);
    bool warned = false;
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (message && message->windowTitle() == QStringLiteral("File changed on disk")) {
            warned = true;
            message->reject();
        }
    });
    saveAction->trigger();
    QVERIFY(warned);
    QVERIFY(editor->modify());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("revised!"));
}

void MainWindowPersistenceTest::findPrefillHandlesUnicodeAndLargeSelections()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* findText = window.findChild<QLineEdit*>(QStringLiteral("findText"));
    QAction* findAction = nullptr;
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("shortcutId").toString() == QStringLiteral("find")) {
            findAction = action;
        }
    }
    QVERIFY(editor && findText && findAction);
    const QString query(256, QChar(0x4e2d));
    editor->setTextUtf8(query.toUtf8());
    editor->selectAll();
    findAction->trigger();
    QCOMPARE(findText->text(), query);
    for (const QByteArray& selection : {
             QByteArray("x").repeated(2 * 1024 * 1024), QByteArray("first\nsecond")}) {
        editor->setTextUtf8(selection);
        editor->selectAll();
        findAction->trigger();
        QCOMPARE(findText->text(), query);
    }
}

void MainWindowPersistenceTest::restoresDefaultApplicationShortcutsFromSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    {
        QSettings settings;
        settings.setValue(QStringLiteral("shortcuts/new"), QStringLiteral("Ctrl+Alt+N"));
        settings.setValue(QStringLiteral("session/restoreTabsOnStartup"), false);
        settings.sync();
    }
    vinson::MainWindow window;
    bool restored = false;
    QTimer::singleShot(0, &window, [&] {
        auto* dialog = window.findChild<vinson::SettingsDialog*>();
        if (!dialog) {
            return;
        }
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        restored = true;
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    window.showSettings();
    QVERIFY(restored);
    auto* action = window.findChild<QAction*>(QStringLiteral("newDocumentAction"));
    QVERIFY(action != nullptr);
    QCOMPARE(action->shortcut(), QKeySequence(QKeySequence::New));
    QSettings saved;
    QCOMPARE(saved.value(QStringLiteral("shortcuts/new")).toString(),
             QKeySequence(QKeySequence::New).toString(QKeySequence::PortableText));
    QVERIFY(saved.value(QStringLiteral("session/restoreTabsOnStartup")).toBool());
}

void MainWindowPersistenceTest::tabBarFillsRemainingSpaceAndScrollsWithoutSwitching()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    window.show();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    QVERIFY(tabs != nullptr);
    auto* menu = window.menuBar();
    QTRY_VERIFY(tabs->width() > 400);
    // The native caption bounds arrive after the first show/frame transition.
    if (tabs->property("inTitleBar").toBool())
        QTest::qWait(100);
    const int originalWidth = tabs->width();
    window.resize(window.width() + 200, window.height());
    QTRY_COMPARE(tabs->width(), originalWidth + 200);
    const QRect lastMenu = menu->actionGeometry(menu->actions().last());
    if (tabs->property("inTitleBar").toBool()) {
        QVERIFY(menu->y() >= tabs->parentWidget()->height());
    } else {
        QVERIFY(tabs->geometry().left() >= lastMenu.right());
        QTRY_VERIFY(tabs->geometry().left() - lastMenu.right() < 12);
        QTRY_VERIFY(menu->width() - tabs->geometry().right() < 12);
    }
    QTRY_VERIFY(tabs->tabRect(0).left() < 12);

    auto* editor = window.findChild<vinson::EditorWidget*>();
    const int compressibleCount = tabs->width() / 154 + 1;
    for (int index = 1; index < compressibleCount; ++index) {
        editor->setTextUtf8("document");
        window.handleExternalOpenRequest({});
    }
    for (int index = 0; index < tabs->count(); ++index) {
        tabs->setTabText(index,
            QStringLiteral("very-long-document-title-%1.txt").arg(index));
    }
    tabs->setCurrentIndex(0);
    QCoreApplication::processEvents();
    auto* overflow = menu->findChild<QToolButton*>(QStringLiteral("qt_menubar_ext_button"));
    QVERIFY(overflow != nullptr);
    QTRY_VERIFY(!overflow->isVisible());
    QCOMPARE(tabs->width(), originalWidth + 200);
    QCOMPARE(tabs->elideMode(), Qt::ElideNone);
    QTRY_VERIFY(tabs->tabRect(tabs->count() - 1).right() < tabs->width());
    for (int index = 0; index < tabs->count(); ++index) {
        const QRect tabRect = tabs->tabRect(index);
        auto* close = tabs->tabButton(index, QTabBar::RightSide);
        QVERIFY(close != nullptr);
        QVERIFY(tabRect.width() < 234);
        QVERIFY(tabRect.contains(close->geometry()));
        QVERIFY(tabRect.width() >= close->width()
            + tabs->fontMetrics().horizontalAdvance(tabs->tabText(index).left(1)));
        QVERIFY(tabs->fontMetrics().horizontalAdvance(tabs->tabText(index))
            > tabRect.width() - close->width());
    }

    const int minimumTabWidth = 12
        + tabs->fontMetrics().horizontalAdvance(tabs->tabText(0).left(1))
        + 8 + tabs->tabButton(0, QTabBar::RightSide)->width() + 14;
    const int scrollingCount = tabs->width() / minimumTabWidth + 3;
    while (tabs->count() < scrollingCount) {
        editor->setTextUtf8("document");
        window.handleExternalOpenRequest({});
    }
    tabs->setCurrentIndex(0);
    QCoreApplication::processEvents();
    if (!tabs->property("inTitleBar").toBool())
        QVERIFY(tabs->geometry().left() >= lastMenu.right());
    const int initialPosition = tabs->tabRect(0).left();
    const QPointF position(tabs->rect().center());
    QWheelEvent forward(position, tabs->mapToGlobal(position.toPoint()),
                        QPoint(), QPoint(0, -120), Qt::NoButton,
                        Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(tabs, &forward);
    QVERIFY(tabs->tabRect(0).left() < initialPosition);
    QCOMPARE(tabs->currentIndex(), 0);
    QWheelEvent backward(position, tabs->mapToGlobal(position.toPoint()),
                         QPoint(), QPoint(120, 0), Qt::NoButton,
                         Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(tabs, &backward);
    QCOMPARE(tabs->tabRect(0).left(), initialPosition);
    QCOMPARE(tabs->currentIndex(), 0);
    window.resize(5000, window.height());
    QTRY_VERIFY(!overflow->isVisible());
    QTRY_VERIFY(tabs->tabRect(0).left() < 12);
    QTRY_VERIFY(tabs->tabRect(tabs->count() - 1).right() < tabs->width());
}

void MainWindowPersistenceTest::tabContextMenuTargetsClickedDocument()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    QStringList paths;
    for (const auto& name : {"first.txt", "second.txt", "third.txt"}) {
        QFile file(directory.filePath(QString::fromLatin1(name)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(name);
        paths.append(file.fileName());
    }
    vinson::MainWindow window;
    window.resize(1400, 600);
    window.show();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    window.openFiles(paths);
    QTRY_COMPARE(tabs->count(), 3);
    QTRY_COMPARE(editor->textUtf8(), QByteArray("third.txt"));
    QTRY_VERIFY(tabs->isEnabled());
    tabs->customContextMenuRequested(tabs->tabRect(0).center());
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    QVERIFY(menu);
    QCOMPARE(menu->objectName(), QStringLiteral("tabContextMenu"));
    QVERIFY(menu->findChild<QObject*>(QStringLiteral("nativeMenuAppearance")));
    QCOMPARE(tabs->currentIndex(), 2);
    auto* copy = menu->findChild<QAction*>(QStringLiteral("tabCopyPathAction"));
    QVERIFY(copy && copy->isEnabled());
    copy->trigger();
    QCOMPARE(QGuiApplication::clipboard()->text(), QDir::toNativeSeparators(paths.first()));
    menu->hide();
    tabs->setCurrentIndex(0);
    QTest::keyClicks(editor, QStringLiteral("updated-"));
    QVERIFY(editor->modify());
    tabs->setCurrentIndex(2);
    tabs->customContextMenuRequested(tabs->tabRect(0).center());
    menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    QVERIFY(menu);
    auto* save = menu->findChild<QAction*>(QStringLiteral("tabSaveAction"));
    QVERIFY(save);
    menu->hide();
    save->trigger();
    QTRY_COMPARE(tabs->currentIndex(), 0);
    QTRY_VERIFY(!editor->modify());
    QFile saved(paths.first());
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QVERIFY(saved.readAll().contains("updated-"));
    saved.close();

    tabs->customContextMenuRequested(tabs->tabRect(0).center());
    menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    QVERIFY(menu);
    auto* close = menu->findChild<QAction*>(QStringLiteral("tabCloseAction"));
    QVERIFY(close);
    tabs->moveTab(0, 2);
    menu->hide();
    close->trigger();
    QTRY_COMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(0), QStringLiteral("second.txt"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("third.txt"));

    // The empty strip offers new-tab creation, not the main-window dock menu.
    tabs->customContextMenuRequested(QPoint(tabs->width() - 3, tabs->height() / 2));
    menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    QVERIFY(menu);
    QCOMPARE(menu->actions().size(), 1);
    auto* create = menu->findChild<QAction*>(QStringLiteral("tabNewAction"));
    QVERIFY(create);
    menu->hide();
    create->trigger();
    QCOMPARE(tabs->count(), 3);
    tabs->customContextMenuRequested(tabs->tabRect(2).center());
    menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    QVERIFY(menu);
    QVERIFY(!menu->findChild<QAction*>(QStringLiteral("tabCopyPathAction"))->isEnabled());
    QVERIFY(!menu->findChild<QAction*>(QStringLiteral("tabCloseRightAction"))->isEnabled());
    menu->hide();
}

void MainWindowPersistenceTest::tabContextMenuClosesRangesAndStopsOnCancel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    window.resize(1400, 600);
    window.show();
    auto* tabs = window.findChild<QTabBar*>(QStringLiteral("documentTabBar"));
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* create = window.findChild<QAction*>(QStringLiteral("newDocumentAction"));
    const auto runAction = [&](int index, const QString& name) {
        tabs->customContextMenuRequested(tabs->tabRect(index).center());
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        QVERIFY(menu);
        auto* action = menu->findChild<QAction*>(name);
        QVERIFY(action && action->isEnabled());
        menu->hide();
        action->trigger();
    };
    create->trigger();
    create->trigger();
    const qintptr first = tabs->tabData(0).value<qintptr>();
    runAction(0, QStringLiteral("tabCloseRightAction"));
    QTRY_COMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabData(0).value<qintptr>(), first);
    create->trigger();
    const qintptr middle = tabs->tabData(1).value<qintptr>();
    create->trigger();
    QTest::keyClicks(editor, QStringLiteral("unsaved text"));
    QVERIFY(editor->modify());
    bool canceled = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            canceled = true;
            box->button(QMessageBox::Cancel)->click();
        }
    });
    runAction(0, QStringLiteral("tabCloseAllAction"));
    QVERIFY(canceled);
    QCoreApplication::processEvents();
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(editor->textUtf8(), QByteArray("unsaved text"));
    bool discarded = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            discarded = true;
            box->button(QMessageBox::Discard)->click();
        }
    });
    runAction(1, QStringLiteral("tabCloseOthersAction"));
    QVERIFY(discarded);
    QTRY_COMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabData(0).value<qintptr>(), middle);
    runAction(0, QStringLiteral("tabCloseAllAction"));
    QTRY_VERIFY(editor->isEmpty());
    QCOMPARE(tabs->count(), 1);
    QVERIFY(window.isVisible());
    QCOMPARE(tabs->tabData(0).value<qintptr>(), static_cast<qintptr>(editor->docPointer()));
    const QString savedPath = directory.filePath(QStringLiteral("save-before-close.txt"));
    {
        QFile file(savedPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("before");
    }
    window.openFiles({savedPath});
    QTRY_COMPARE(editor->textUtf8(), QByteArray("before"));
    QTRY_VERIFY(tabs->isEnabled());
    QTest::keyClicks(editor, QStringLiteral("changed-"));
    QVERIFY(editor->modify());
    create->trigger();
    const qintptr kept = tabs->tabData(1).value<qintptr>();
    bool saved = false;
    QTimer::singleShot(0, &window, [&] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            saved = true;
            box->button(QMessageBox::Save)->click();
        }
    });
    runAction(1, QStringLiteral("tabCloseOthersAction"));
    QVERIFY(saved);
    QTRY_COMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabData(0).value<qintptr>(), kept);
    QFile file(savedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("changed-"));
}

void MainWindowPersistenceTest::restoresAndUsesCustomApplicationShortcuts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    {
        QSettings settings;
        settings.setValue(QStringLiteral("shortcuts/lineNumbers"),
                          QStringLiteral("Ctrl+Alt+L"));
        settings.setValue(QStringLiteral("shortcuts/minimalMode"),
                          QStringLiteral("Ctrl+Alt+M"));
        settings.setValue(QStringLiteral("shortcuts/exitMinimalMode"),
                          QStringLiteral("Ctrl+Alt+E"));
        settings.sync();
    }

    vinson::MainWindow window;
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* controller = window.findChild<vinson::WindowController*>();
    auto* lines = window.findChild<QAction*>(QStringLiteral("lineNumbersAction"));
    auto* minimal = window.findChild<QAction*>(QStringLiteral("minimalModeAction"));
    auto* exitMinimal = window.findChild<QAction*>(
        QStringLiteral("exitMinimalModeAction"));
    QVERIFY(editor != nullptr);
    QVERIFY(controller != nullptr);
    QVERIFY(lines != nullptr);
    QVERIFY(minimal != nullptr);
    QVERIFY(exitMinimal != nullptr);
    QCOMPARE(lines->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+L")));
    QCOMPARE(minimal->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+M")));
    QCOMPARE(exitMinimal->shortcut(),
             QKeySequence(QStringLiteral("Ctrl+Alt+E")));

    window.show();
    editor->QWidget::setFocus();
    QCoreApplication::processEvents();
    QVERIFY(editor->areLineNumbersVisible());
    QTest::keyClick(editor, Qt::Key_L,
                    Qt::ControlModifier | Qt::AltModifier);
    QTRY_VERIFY(!editor->areLineNumbersVisible());

    QTest::keyClick(editor, Qt::Key_M,
                    Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(!controller->isMinimalMode());
    QTest::keyClick(editor, Qt::Key_M,
                    Qt::ControlModifier | Qt::AltModifier);
    QTRY_VERIFY(controller->isMinimalMode());
    QTest::keyClick(editor, Qt::Key_E,
                    Qt::ControlModifier | Qt::AltModifier);
    QTRY_VERIFY(!controller->isMinimalMode());

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("shortcuts/lineNumbers")).toString(),
             QStringLiteral("Ctrl+Alt+L"));
    QCOMPARE(settings.value(QStringLiteral("shortcuts/exitMinimalMode")).toString(),
             QStringLiteral("Ctrl+Alt+E"));
}

void MainWindowPersistenceTest::switchesPersistedCustomStyles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    const vinson::Appearance defaults = vinson::ThemeManager::defaultAppearance();
    {
        QSettings settings;
        settings.beginWriteArray(QStringLiteral("appearancePresets"), 2);
        settings.setArrayIndex(0);
        settings.setValue(QStringLiteral("name"), QStringLiteral("Night"));
        settings.setValue(QStringLiteral("shortcut"), QStringLiteral("Ctrl+Alt+1"));
        settings.setValue(QStringLiteral("appearance/fontFamily"), defaults.font.family());
        settings.setValue(QStringLiteral("appearance/fontSize"), 17.0);
        settings.setValue(QStringLiteral("appearance/textColor"), QStringLiteral("#ffe6e6e6"));
        settings.setValue(QStringLiteral("appearance/backgroundColor"), QStringLiteral("#ff101820"));
        settings.setValue(QStringLiteral("appearance/cursorColor"), QStringLiteral("#ffffffff"));
        settings.setValue(QStringLiteral("appearance/selectionTextColor"), QStringLiteral("#ffffffff"));
        settings.setArrayIndex(1);
        settings.setValue(QStringLiteral("name"), QStringLiteral("Paper"));
        settings.setValue(QStringLiteral("appearance/fontFamily"), defaults.font.family());
        settings.setValue(QStringLiteral("appearance/fontSize"), 13.0);
        settings.setValue(QStringLiteral("appearance/textColor"), QStringLiteral("#ff202124"));
        settings.setValue(QStringLiteral("appearance/backgroundColor"), QStringLiteral("#fffff8e7"));
        settings.setValue(QStringLiteral("appearance/cursorColor"), QStringLiteral("#ff202124"));
        settings.setValue(QStringLiteral("appearance/selectionTextColor"), QStringLiteral("#ffffffff"));
        settings.endArray();
        settings.sync();
    }

    vinson::MainWindow window;
    auto* theme = window.findChild<vinson::ThemeManager*>();
    auto* editor = window.findChild<vinson::EditorWidget*>();
    auto* night = window.findChild<QAction*>(QStringLiteral("appearancePresetAction0"));
    auto* paper = window.findChild<QAction*>(QStringLiteral("appearancePresetAction1"));
    auto* next = window.findChild<QAction*>(QStringLiteral("nextAppearancePresetAction"));
    QVERIFY(theme != nullptr);
    QVERIFY(editor != nullptr);
    QVERIFY(night != nullptr);
    QVERIFY(paper != nullptr);
    QVERIFY(next != nullptr);
    QCOMPARE(night->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+1")));
    QVERIFY(paper->shortcut().isEmpty());
    QCOMPARE(next->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+PgDown")));
    QVERIFY(window.actions().contains(night));

    window.show();
    editor->QWidget::setFocus();
    QCoreApplication::processEvents();
    QTest::keyClick(editor, Qt::Key_1,
                    Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(theme->appearance().font.pointSizeF(), 17.0);
    QCOMPARE(theme->appearance().backgroundColor, QColor(16, 24, 32));
    QTest::keyClick(editor, Qt::Key_PageDown,
                    Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(theme->appearance().font.pointSizeF(), 13.0);
    QCOMPARE(theme->appearance().backgroundColor, QColor(255, 248, 231));

    QSettings saved;
    QCOMPARE(saved.value(QStringLiteral("appearance/fontSize")).toDouble(), 13.0);
    QCOMPARE(saved.beginReadArray(QStringLiteral("appearancePresets")), 2);
    saved.endArray();
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

void MainWindowPersistenceTest::titleBarMenusUseNativeFrames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());
    vinson::MainWindow window;
    window.show();
    QCoreApplication::processEvents();
    const auto menus = window.menuBar()->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        menu->ensurePolished();
        QVERIFY(menu->findChild<QObject*>(QStringLiteral("nativeMenuAppearance"),
                                          Qt::FindDirectChildrenOnly));
        QVERIFY(menu->graphicsEffect() == nullptr);
        QVERIFY(!menu->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(!menu->windowFlags().testFlag(Qt::NoDropShadowWindowHint));
        QVERIFY(!menu->findChild<QWidget*>(QStringLiteral("menuShadowOverflow")));
    }
    QCOMPARE(window.menuBar()->actions().size(), 5);
    QVERIFY(menus.size() >= 7);
}

void MainWindowPersistenceTest::nativeMenusPreserveInteractionAndReopening()
{
    QMainWindow owner;
    owner.resize(640, 480);
    owner.show();
    owner.activateWindow();
    QCoreApplication::processEvents();
    QMenu menu(&owner);
    auto* disabled = menu.addAction(QStringLiteral("Unavailable"));
    disabled->setEnabled(false);
    auto* toggle = menu.addAction(QStringLiteral("Always on top\tCtrl+Shift+T"));
    toggle->setCheckable(true);
    menu.addSeparator();
    auto* submenu = menu.addMenu(QStringLiteral("Styles"));
    auto* child = submenu->addAction(QStringLiteral("Default"));
    vinson::applyMenuAppearance(&menu);
    vinson::applyMenuAppearance(submenu);
    auto* style = menu.style();
    vinson::applyMenuAppearance(&menu);
    QCOMPARE(menu.style(), style);
    QSignalSpy toggled(toggle, &QAction::triggered);
    QSignalSpy selected(child, &QAction::triggered);
    for (bool dark : {false, true}) {
        QPalette palette = menu.palette();
        palette.setColor(QPalette::Window, dark ? QColor(30, 30, 30) : Qt::white);
        owner.setPalette(palette);
        style->unpolish(&menu);
        style->polish(&menu);
        menu.popup(owner.mapToGlobal(QPoint(30, 30)));
        QCoreApplication::processEvents();
        QVERIFY(menu.isVisible());
        QCOMPARE(menu.palette().color(QPalette::Window), owner.palette().color(QPalette::Window));
        QCOMPARE(QApplication::activePopupWidget(), &menu);
        QVERIFY(menu.graphicsEffect() == nullptr);
        QVERIFY(!menu.testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(!menu.windowFlags().testFlag(Qt::NoDropShadowWindowHint));
        QTest::keyClick(&menu, Qt::Key_Down);
        QCOMPARE(menu.activeAction(), toggle);
        QTest::keyClick(&menu, Qt::Key_Return);
        QVERIFY(!menu.isVisible());
        QCOMPARE(toggle->isChecked(), !dark);
        menu.popup(owner.mapToGlobal(QPoint(30, 30)));
        menu.setActiveAction(submenu->menuAction());
        QTest::keyClick(&menu, Qt::Key_Right);
        QTRY_VERIFY(submenu->isVisible());
        QTest::keyClick(submenu, Qt::Key_Return);
        QTRY_VERIFY(!menu.isVisible());
        QVERIFY(!submenu->isVisible());
        menu.popup(owner.mapToGlobal(QPoint(30, 30)));
        QTest::keyClick(&menu, Qt::Key_Escape);
        QVERIFY(!menu.isVisible());
    }
    QCOMPARE(toggled.count(), 2);
    QCOMPARE(selected.count(), 2);
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

void MainWindowPersistenceTest::dismissingRecoveryPromptKeepsSnapshots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    useSettingsDirectory(directory.path());

    const QString recoveryPath = QFileInfo(QSettings().fileName())
                                     .absoluteDir().filePath(QStringLiteral("recovery"));
    vinson::RecoveryManager recovery(recoveryPath);
    vinson::RecoverySnapshot snapshot;
    snapshot.entry.id = QStringLiteral("dismissed-recovery");
    snapshot.entry.displayName = QStringLiteral("Untitled");
    snapshot.content = QByteArray("unsaved draft");
    recovery.queueSnapshot(snapshot);
    recovery.flush();
    QCOMPARE(recovery.entries().size(), 1);

    {
        vinson::MainWindow window;
        bool dismissed = false;
        QTimer::singleShot(0, &window, [&] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                auto* message = qobject_cast<QMessageBox*>(widget);
                if (message && message->isVisible()) {
                    message->reject();
                    dismissed = true;
                    break;
                }
            }
        });
        QTRY_VERIFY(dismissed);
    }
    QCOMPARE(recovery.entries().size(), 1);
    QCOMPARE(recovery.loadContent(QStringLiteral("dismissed-recovery")),
             QByteArray("unsaved draft"));
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
