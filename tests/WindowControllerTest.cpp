#include "editor/EditorWidget.h"
#include "window/WindowController.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtTest>

#include <cmath>

class WindowControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void togglesWindowFlagsWithoutLosingSize();
    void framelessModeHidesChromeAndRestoresScrollBars();
    void framelessModeKeepsEffectiveEditorHeight_data();
    void framelessModeKeepsEffectiveEditorHeight();
    void framelessEdgesExposeMoveAndResizeCursors();
    void framelessModePreservesTextSelection();
    void framelessModeShrinksToOneLineAndRestoresMinimumSize();
    void minimalModeIsReversibleAndTracksFont();
    void minimalModeDoesNotChangeText();
};

void WindowControllerTest::togglesWindowFlagsWithoutLosingSize()
{
    QMainWindow window;
    window.resize(640, 360);
    window.show();
    QCoreApplication::processEvents();
    const QSize originalSize = window.size();
    vinson::WindowController controller(&window);
    QSignalSpy framelessSpy(&controller,
                            &vinson::WindowController::framelessChanged);
    QSignalSpy topmostSpy(&controller,
                          &vinson::WindowController::alwaysOnTopChanged);
    QVERIFY(controller.isTaskbarVisible());

    controller.setFrameless(true);
    QVERIFY(controller.isFrameless());
    QVERIFY(!controller.isTaskbarVisible());
    QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
    QCOMPARE(window.size(), originalSize);
    QCOMPARE(framelessSpy.count(), 1);

    controller.setAlwaysOnTop(true);
    QVERIFY(controller.isAlwaysOnTop());
    QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
    QCOMPARE(window.size(), originalSize);
    QCOMPARE(topmostSpy.count(), 1);

    controller.toggleFrameless();
    QVERIFY(!controller.isFrameless());
    QVERIFY(controller.isTaskbarVisible());
    QVERIFY(!window.windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QCOMPARE(framelessSpy.count(), 2);
}

void WindowControllerTest::framelessModeHidesChromeAndRestoresScrollBars()
{
    QMainWindow window;
    auto* editor = new vinson::EditorWidget(&window);
    QByteArray document;
    for (int line = 0; line < 200; ++line) {
        document += QByteArray(300, 'x') + '\n';
    }
    editor->setTextUtf8(document);
    window.setCentralWidget(editor);
    window.menuBar()->addMenu(QStringLiteral("Menu"));
    window.statusBar()->showMessage(QStringLiteral("Status"));
    window.resize(480, 200);
    window.show();
    QCoreApplication::processEvents();
    vinson::WindowController controller(&window);
    controller.configureMinimalMode(editor, nullptr);

    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(window.statusBar()->isVisible());
    QVERIFY(editor->verticalScrollBar()->isVisible());

    controller.setFrameless(true);
    QCoreApplication::processEvents();
    QVERIFY(!window.menuBar()->isVisible());
    QVERIFY(!window.statusBar()->isVisible());

    controller.setFrameless(false);
    QTRY_VERIFY(window.menuBar()->isVisible());
    QTRY_VERIFY(window.statusBar()->isVisible());
    QVERIFY(window.statusBar()->height() > 0);
    QVERIFY(window.rect().intersects(window.statusBar()->geometry()));
    QVERIFY(window.statusBar()->geometry().bottom() <= window.rect().bottom());
    QVERIFY(!editor->hScrollBar());
    QVERIFY(editor->vScrollBar());
    QCOMPARE(editor->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QCOMPARE(editor->verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
    QVERIFY(editor->verticalScrollBar()->isVisible());
}

void WindowControllerTest::framelessModeKeepsEffectiveEditorHeight_data()
{
    QTest::addColumn<bool>("panelVisible");
    QTest::addColumn<int>("titleHeight");
    QTest::newRow("editor-only") << false << 0;
    QTest::newRow("visible-find-panel") << true << 0;
    QTest::newRow("custom-title-bar") << false << 44;
    QTest::newRow("title-bar-and-find-panel") << true << 44;
}

void WindowControllerTest::framelessModeKeepsEffectiveEditorHeight()
{
    QFETCH(bool, panelVisible);
    QFETCH(int, titleHeight);
    QMainWindow window;
    auto* central = new QWidget(&window);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* panel = new QWidget(central);
    panel->setFixedHeight(28);
    auto* editor = new vinson::EditorWidget(central);
    editor->setTextUtf8("first line\nsecond line");
    layout->addWidget(panel);
    layout->addWidget(editor, 1);
    panel->setVisible(panelVisible);
    window.setCentralWidget(central);
    window.menuBar()->addMenu(QStringLiteral("Menu"));
    window.statusBar()->showMessage(QStringLiteral("Status"));
    window.setContentsMargins(0, titleHeight, 0, 0);
    window.resize(640, 360);
    window.show();
    QCoreApplication::processEvents();
    vinson::WindowController controller(&window);
    controller.configureMinimalMode(editor, panel);
    connect(&controller, &vinson::WindowController::framelessChanged,
            &window, [&window, titleHeight](bool enabled) {
                window.setContentsMargins(0, enabled ? 0 : titleHeight, 0, 0);
            });
    const QSize framedSize = window.size();
    const int editorHeight = editor->viewport()->height();
    QVERIFY(editorHeight < framedSize.height());
    for (int cycle = 0; cycle < 3; ++cycle) {
        controller.setFrameless(true);
        QTRY_COMPARE(window.height(), editorHeight + (panelVisible ? panel->height() : 0));
        QTRY_COMPARE(editor->viewport()->height(), editorHeight);
        QCOMPARE(window.width(), framedSize.width());
        QCOMPARE(panel->isVisible(), panelVisible);
        controller.setFrameless(false);
        QTRY_COMPARE(editor->viewport()->height(), editorHeight);
        QTRY_COMPARE(window.size(), framedSize);
    }
    QCOMPARE(editor->textUtf8(), QByteArray("first line\nsecond line"));
    QVERIFY(!editor->modify());
}

void WindowControllerTest::framelessEdgesExposeMoveAndResizeCursors()
{
    QMainWindow window;
    auto* editor = new vinson::EditorWidget(&window);
    window.setCentralWidget(editor);
    window.resize(640, 240);
    window.show();
    QCoreApplication::processEvents();
    vinson::WindowController controller(&window);
    controller.setFrameless(true);
    QCoreApplication::processEvents();

    QTest::mouseMove(editor->viewport(),
                     QPoint(1, editor->viewport()->height() / 2));
    QCOMPARE(editor->viewport()->cursor().shape(), Qt::SizeHorCursor);

    QTest::mouseMove(editor->viewport(),
                     QPoint(editor->viewport()->width() / 2, 1));
    QCOMPARE(editor->viewport()->cursor().shape(), Qt::SizeAllCursor);

    QTest::mouseMove(editor->viewport(), QPoint(1, 1));
    QCOMPARE(editor->viewport()->cursor().shape(), Qt::SizeFDiagCursor);

    QTest::mouseMove(editor->viewport(), editor->viewport()->rect().center());
    QVERIFY(editor->viewport()->cursor().shape() != Qt::SizeHorCursor);
    QVERIFY(editor->viewport()->cursor().shape() != Qt::SizeAllCursor);
}

void WindowControllerTest::framelessModePreservesTextSelection()
{
    QMainWindow window;
    auto* editor = new vinson::EditorWidget(&window);
    editor->setLineNumbersVisible(false);
    editor->setTextUtf8("select this text normally");
    window.setCentralWidget(editor);
    window.resize(640, 240);
    window.show();
    QCoreApplication::processEvents();
    vinson::WindowController controller(&window);
    controller.setFrameless(true);
    QCoreApplication::processEvents();

    const QPoint start(
        static_cast<int>(editor->pointXFromPosition(1)),
        static_cast<int>(editor->pointYFromPosition(1)) + 8);
    const QPoint end(
        static_cast<int>(editor->pointXFromPosition(11)),
        static_cast<int>(editor->pointYFromPosition(11)) + 8);
    QTest::mousePress(editor->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(editor->viewport(), end, 10);
    QTest::mouseRelease(editor->viewport(), Qt::LeftButton, Qt::NoModifier, end);

    QVERIFY(editor->selectionEndPosition() > editor->selectionStartPosition());
    QVERIFY(!editor->selectedTextUtf8().isEmpty());
}

void WindowControllerTest::framelessModeShrinksToOneLineAndRestoresMinimumSize()
{
    QMainWindow window;
    auto* central = new QWidget(&window);
    auto* editor = new vinson::EditorWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(editor);
    window.setCentralWidget(central);
    window.menuBar()->addMenu(QStringLiteral("Menu"));
    window.statusBar()->showMessage(QStringLiteral("Status"));
    window.setMinimumSize(240, 180);
    editor->setMinimumSize(100, 80);
    const QByteArray text("first line\nsecond line");
    editor->setTextUtf8(text);
    window.resize(640, 360);
    window.show();
    QCoreApplication::processEvents();
    vinson::WindowController controller(&window);
    controller.configureMinimalMode(editor, nullptr);
    const QSize originalWindowMinimum = window.minimumSize();
    const QSize originalEditorMinimum = editor->minimumSize();
    window.resize(100, window.height());
    QCOMPARE(window.width(), 250);
    window.resize(640, 360);

    controller.setFrameless(true);
    QCoreApplication::processEvents();
    const int lineHeight = static_cast<int>(std::ceil(editor->textHeightF(0)));
    QCOMPARE(window.minimumHeight(), lineHeight);
    QCOMPARE(editor->minimumHeight(), lineHeight);
    window.resize(100, lineHeight);
    QCoreApplication::processEvents();
    QCOMPARE(window.width(), 100);
    QCOMPARE(window.height(), lineHeight);
    QCOMPARE(editor->viewport()->height(), lineHeight);
    QVERIFY(editor->pointYFromPosition(text.indexOf('\n') + 1)
            >= editor->viewport()->height());
    QVERIFY(editor->areLineNumbersVisible());
    QVERIFY(editor->vScrollBar());

    QFont font = editor->editorFont();
    font.setPointSizeF(24.0);
    editor->setEditorFont(font);
    controller.refreshMinimumSize();
    const int largerLineHeight = static_cast<int>(std::ceil(editor->textHeightF(0)));
    QVERIFY(largerLineHeight > lineHeight);
    QCOMPARE(window.minimumHeight(), largerLineHeight);
    window.resize(320, largerLineHeight);
    QCoreApplication::processEvents();
    QCOMPARE(editor->viewport()->height(), largerLineHeight);

    controller.setMinimalMode(true);
    font.setPointSizeF(28.0);
    editor->setEditorFont(font);
    controller.refreshMinimumSize();
    controller.setMinimalMode(false);
    QVERIFY(controller.isFrameless());
    QCOMPARE(window.minimumHeight(),
             static_cast<int>(std::ceil(editor->textHeightF(0))));
    controller.setFrameless(false);
    QCoreApplication::processEvents();
    QCOMPARE(window.minimumSize(), originalWindowMinimum);
    window.resize(100, window.height());
    QCOMPARE(window.width(), 250);
    QCOMPARE(editor->minimumSize(), originalEditorMinimum);
    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(window.statusBar()->isVisible());
    QCOMPARE(editor->textUtf8(), text);
    QVERIFY(!editor->modify());
}

void WindowControllerTest::minimalModeIsReversibleAndTracksFont()
{
    QMainWindow window;
    auto* central = new QWidget(&window);
    auto* panel = new QWidget(central);
    panel->setFixedHeight(28);
    auto* editor = new vinson::EditorWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(panel);
    layout->addWidget(editor, 1);
    window.setCentralWidget(central);
    window.menuBar()->addMenu(QStringLiteral("Menu"));
    window.statusBar()->showMessage(QStringLiteral("Status"));
    window.setMinimumSize(240, 180);
    editor->setMinimumSize(100, 80);
    const QByteArray minimalText(
        "minimal mode keeps the document\nsecond line stays out of view");
    editor->setTextUtf8(minimalText);
    window.resize(640, 360);
    window.show();
    QCoreApplication::processEvents();

    vinson::WindowController controller(&window);
    controller.configureMinimalMode(editor, panel);
    const QByteArray originalGeometry = window.saveGeometry();
    const QSize originalWindowSize = window.size();
    const QSize originalWindowMinimum = window.minimumSize();
    const QSize originalEditorMinimum = editor->minimumSize();
    controller.setAlwaysOnTop(true);
    controller.setMinimalMode(true);

    QVERIFY(controller.isMinimalMode());
    QVERIFY(controller.isFrameless());
    QVERIFY(!controller.isTaskbarVisible());
    QVERIFY(controller.isAlwaysOnTop());
    QVERIFY(!window.menuBar()->isVisible());
    QVERIFY(!window.statusBar()->isVisible());
    QVERIFY(!panel->isVisible());
    QVERIFY(!editor->areLineNumbersVisible());
    QVERIFY(!editor->hScrollBar());
    QVERIFY(!editor->vScrollBar());
    QCOMPARE(controller.persistableGeometry(), originalGeometry);
    QVERIFY(!controller.persistableFrameless());
    controller.setFrameless(false);
    QVERIFY(controller.isFrameless());
    const int initialLineHeight = static_cast<int>(
        std::ceil(editor->textHeightF(0)));
    QCOMPARE(window.minimumHeight(), initialLineHeight);
    QCOMPARE(editor->minimumHeight(), initialLineHeight);
    window.resize(100, window.minimumHeight());
    QCoreApplication::processEvents();
    QCOMPARE(window.width(), 100);
    QCOMPARE(window.height(), initialLineHeight);
    QCOMPARE(editor->viewport()->height(), initialLineHeight);
    const qsizetype secondLinePosition = minimalText.indexOf('\n') + 1;
    QVERIFY(editor->pointYFromPosition(secondLinePosition)
            >= editor->viewport()->height());

    QFont largerFont = editor->editorFont();
    largerFont.setPointSizeF(24.0);
    editor->setEditorFont(largerFont);
    controller.refreshMinimumSize();
    QVERIFY(window.minimumHeight() > initialLineHeight);

    controller.setMinimalMode(false);
    QVERIFY(!controller.isMinimalMode());
    QVERIFY(!controller.isFrameless());
    QVERIFY(controller.isTaskbarVisible());
    QVERIFY(controller.isAlwaysOnTop());
    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(window.statusBar()->isVisible());
    QVERIFY(panel->isVisible());
    QVERIFY(editor->areLineNumbersVisible());
    QVERIFY(!editor->hScrollBar());
    QVERIFY(editor->vScrollBar());
    QCOMPARE(window.minimumSize(), originalWindowMinimum);
    QCOMPARE(editor->minimumSize(), originalEditorMinimum);
    QCOMPARE(window.size(), originalWindowSize);
    QCOMPARE(editor->textUtf8(), minimalText);
    QVERIFY(!editor->modify());

    controller.setFrameless(true);
    controller.setMinimalMode(true);
    QVERIFY(controller.persistableFrameless());
    controller.setMinimalMode(false);
    QVERIFY(controller.isFrameless());
}

void WindowControllerTest::minimalModeDoesNotChangeText()
{
    QMainWindow window;
    auto* editor = new vinson::EditorWidget(&window);
    editor->setTextUtf8("unsaved text remains safe");
    window.setCentralWidget(editor);
    window.resize(480, 200);
    window.show();
    editor->QWidget::setFocus();
    QCoreApplication::processEvents();
    vinson::WindowController controller(&window);
    controller.configureMinimalMode(editor, nullptr);
    QSignalSpy minimalSpy(&controller,
                          &vinson::WindowController::minimalModeChanged);

    controller.setMinimalMode(true);
    QVERIFY(controller.isMinimalMode());
    editor->setSel(editor->documentLength(), editor->documentLength());
    QTest::keyClicks(editor, QStringLiteral("!"));
    QTRY_COMPARE(editor->textUtf8(), QByteArray("unsaved text remains safe!"));
    QVERIFY(editor->modify());
    controller.setMinimalMode(false);
    QVERIFY(!controller.isMinimalMode());

    QCOMPARE(minimalSpy.count(), 2);
    QCOMPARE(editor->textUtf8(), QByteArray("unsaved text remains safe!"));
    QVERIFY(editor->modify());
}

QTEST_MAIN(WindowControllerTest)
#include "WindowControllerTest.moc"
