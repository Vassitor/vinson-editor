#include "editor/EditorWidget.h"
#include "window/WindowController.h"

#include <QMainWindow>
#include <QFontMetricsF>
#include <QMenuBar>
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
    void framelessEdgesExposeResizeCursor();
    void framelessModePreservesTextSelection();
    void minimalModeIsReversibleAndTracksFont();
    void minimalShortcutsDoNotChangeText();
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

    controller.setFrameless(true);
    QVERIFY(controller.isFrameless());
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
    QVERIFY(!window.windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QCOMPARE(framelessSpy.count(), 2);
}

void WindowControllerTest::framelessEdgesExposeResizeCursor()
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

    QTest::mouseMove(editor->viewport(), editor->viewport()->rect().center());
    QVERIFY(editor->viewport()->cursor().shape() != Qt::SizeHorCursor);
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
    editor->setTextUtf8("minimal mode keeps the document");
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
        std::ceil(QFontMetricsF(editor->editorFont()).height())) + 4;
    QCOMPARE(window.minimumHeight(), initialLineHeight);
    QCOMPARE(editor->minimumHeight(), initialLineHeight);
    window.resize(window.minimumWidth(), window.minimumHeight());
    QCoreApplication::processEvents();
    QCOMPARE(window.height(), initialLineHeight);

    QFont largerFont = editor->editorFont();
    largerFont.setPointSizeF(24.0);
    editor->setEditorFont(largerFont);
    controller.refreshMinimalMinimumSize();
    QVERIFY(window.minimumHeight() > initialLineHeight);

    controller.setMinimalMode(false);
    QVERIFY(!controller.isMinimalMode());
    QVERIFY(!controller.isFrameless());
    QVERIFY(controller.isAlwaysOnTop());
    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(window.statusBar()->isVisible());
    QVERIFY(panel->isVisible());
    QVERIFY(editor->areLineNumbersVisible());
    QVERIFY(editor->hScrollBar());
    QVERIFY(editor->vScrollBar());
    QCOMPARE(window.minimumSize(), originalWindowMinimum);
    QCOMPARE(editor->minimumSize(), originalEditorMinimum);
    QCOMPARE(window.size(), originalWindowSize);
    QCOMPARE(editor->textUtf8(), QByteArray("minimal mode keeps the document"));
    QVERIFY(!editor->modify());

    controller.setFrameless(true);
    controller.setMinimalMode(true);
    QVERIFY(controller.persistableFrameless());
    controller.setMinimalMode(false);
    QVERIFY(controller.isFrameless());
}

void WindowControllerTest::minimalShortcutsDoNotChangeText()
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

    QTest::keyClick(editor, Qt::Key_M,
                    Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(controller.isMinimalMode());
    editor->setSel(editor->documentLength(), editor->documentLength());
    QTest::keyClicks(editor, QStringLiteral("!"));
    QTRY_COMPARE(editor->textUtf8(), QByteArray("unsaved text remains safe!"));
    QVERIFY(editor->modify());
    QTest::keyClick(editor, Qt::Key_Escape);
    QVERIFY(!controller.isMinimalMode());

    QCOMPARE(minimalSpy.count(), 2);
    QCOMPARE(editor->textUtf8(), QByteArray("unsaved text remains safe!"));
    QVERIFY(editor->modify());
}

QTEST_MAIN(WindowControllerTest)
#include "WindowControllerTest.moc"
