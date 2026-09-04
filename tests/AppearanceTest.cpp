#include "editor/EditorWidget.h"
#include "settings/ThemeManager.h"
#include "ui/SettingsDialog.h"
#include "window/NativeWindowAppearance.h"

#include <QImage>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QMainWindow>
#include <QKeySequenceEdit>
#include <QMenuBar>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSlider>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtTest>

#include <algorithm>
#include <cstdlib>

class AppearanceTest final : public QObject
{
    Q_OBJECT

private slots:
    void appliesAppearanceWithoutChangingDocument();
    void appliesReadableWindowChromePalette();
    void resizeDoesNotExposeDarkScrollBarSeam();
    void transparentBackgroundKeepsTextVisible();
    void transparentScrollClearsPreviousText();
    void transparentFontRendersPartialAlpha();
    void nativeFramedBackgroundAlphaIsAvailable();
    void dialogPreviewsBackgroundAlpha();
    void colorSwatchesKeepTheirColorOnHover();
};

void AppearanceTest::appliesAppearanceWithoutChangingDocument()
{
    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto* editor = new vinson::EditorWidget(&window);
    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor);
    vinson::ThemeManager manager(editor, &window);

    editor->setTextUtf8("appearance keeps this text");
    QSignalSpy modifiedSpy(editor, &vinson::EditorWidget::documentModified);
    vinson::Appearance appearance = manager.appearance();
    appearance.font.setPointSizeF(18.5);
    appearance.textColor = QColor(10, 20, 30, 10);
    appearance.backgroundColor = QColor(40, 50, 60, 127);
    appearance.cursorColor = QColor(70, 80, 90, 20);
    appearance.selectionTextColor = QColor(100, 110, 120, 30);

    manager.applyAppearance(appearance);

    QCOMPARE(editor->textUtf8(), QByteArray("appearance keeps this text"));
    QVERIFY(!editor->modify());
    QVERIFY(modifiedSpy.isEmpty());
    QCOMPARE(editor->editorFont().pointSizeF(), 18.5);
    QCOMPARE(editor->textColor(), QColor(10, 20, 30, 10));
    QCOMPARE(manager.appearance().backgroundColor, QColor(40, 50, 60, 127));
    QCOMPARE(editor->backgroundColor(), QColor(40, 50, 60, 255));
    QCOMPARE(editor->cursorColor(), QColor(70, 80, 90, 255));
    QCOMPARE(editor->selectionTextColor(), QColor(100, 110, 120, 255));
    QCOMPARE(window.palette().color(QPalette::Window), QColor(40, 50, 60, 255));
    QCOMPARE(window.windowOpacity(), 1.0);
    QVERIFY(editor->bufferedDraw());

    manager.setFramelessMode(true);
    QCOMPARE(editor->backgroundColor(), QColor(40, 50, 60, 127));
    QCOMPARE(window.palette().color(QPalette::Window), QColor(40, 50, 60, 127));
    QCOMPARE(window.windowOpacity(), 1.0);
    QVERIFY(!editor->bufferedDraw());
    QVERIFY(editor->styleSheet().contains(QStringLiteral("#7f28323c")));

    manager.setFramelessMode(false);
    QCOMPARE(manager.appearance().backgroundColor, QColor(40, 50, 60, 127));
    QCOMPARE(editor->backgroundColor(), QColor(40, 50, 60, 255));
    QCOMPARE(window.palette().color(QPalette::Window), QColor(40, 50, 60, 255));
    QCOMPARE(window.windowOpacity(), 1.0);
    QVERIFY(editor->bufferedDraw());

    appearance.backgroundColor.setAlpha(255);
    manager.applyAppearance(appearance);
    QVERIFY(editor->bufferedDraw());
    QCOMPARE(editor->textUtf8(), QByteArray("appearance keeps this text"));
    QVERIFY(modifiedSpy.isEmpty());
}

void AppearanceTest::appliesReadableWindowChromePalette()
{
    QMainWindow window;
    auto* editor = new vinson::EditorWidget(&window);
    window.setCentralWidget(editor);
    window.menuBar()->addMenu(QStringLiteral("File"));
    window.statusBar()->showMessage(QStringLiteral("Ready"));
    vinson::ThemeManager manager(editor, &window);

    vinson::Appearance appearance = manager.appearance();
    appearance.backgroundColor = QColor(240, 230, 220);
    appearance.textColor = QColor(20, 30, 40);
    manager.applyAppearance(appearance);

    QCOMPARE(window.palette().color(QPalette::Window),
             appearance.backgroundColor);
    QCOMPARE(window.palette().color(QPalette::WindowText),
             QColor(20, 30, 40, 255));
    QCOMPARE(window.menuBar()->palette().color(QPalette::ButtonText),
             appearance.textColor);
    QCOMPARE(window.statusBar()->palette().color(QPalette::WindowText),
             appearance.textColor);
    QVERIFY(window.menuBar()->autoFillBackground());
    QVERIFY(window.statusBar()->autoFillBackground());
    QVERIFY(editor->styleSheet().contains(QStringLiteral("QScrollBar:vertical")));
    QVERIFY(editor->styleSheet().contains(QStringLiteral("QScrollBar:horizontal")));
    QVERIFY(editor->styleSheet().contains(
        QStringLiteral("QWidget#qt_scrollarea_vcontainer")));
    QVERIFY(editor->styleSheet().contains(
        QStringLiteral("QAbstractScrollArea::corner")));
    QVERIFY(editor->styleSheet().contains(QStringLiteral("background: #f0e6dc")));
    QVERIFY(editor->styleSheet().contains(QStringLiteral("border-radius: 4px")));
    QVERIFY(editor->styleSheet().contains(
        QStringLiteral("QScrollBar::sub-page { background: #f0e6dc; }")));
}

void AppearanceTest::resizeDoesNotExposeDarkScrollBarSeam()
{
    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto* editor = new vinson::EditorWidget(&window);
    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor);
    vinson::ThemeManager manager(editor, &window);

    editor->setTextUtf8(QByteArray("line\n").repeated(200));
    window.resize(280, 160);
    window.show();
    QCoreApplication::processEvents();
    window.resize(420, 240);
    QCoreApplication::processEvents();

    QVERIFY(editor->verticalScrollBar()->isVisible());
    const int scrollBarLeft = editor->verticalScrollBar()
                                  ->mapTo(editor, QPoint(0, 0)).x();
    QVERIFY(scrollBarLeft > 0);
    QImage image(editor->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    editor->render(&painter);
    painter.end();

    const QColor expected = editor->backgroundColor();
    for (const int x : {scrollBarLeft - 1, scrollBarLeft}) {
        const QColor actual = image.pixelColor(x, editor->height() / 2);
        QCOMPARE(actual, expected);
    }
}

void AppearanceTest::transparentBackgroundKeepsTextVisible()
{
    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto* editor = new vinson::EditorWidget(&window);
    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor);
    vinson::ThemeManager manager(editor, &window);

    editor->setLineNumbersVisible(true);
    editor->setHScrollBar(false);
    editor->setVScrollBar(false);
    editor->setTextUtf8("Visible");
    vinson::Appearance appearance = manager.appearance();
    appearance.textColor = QColor(255, 255, 255);
    appearance.backgroundColor = QColor(12, 34, 56, 0);
    manager.setFramelessMode(true);
    manager.applyAppearance(appearance);

    window.resize(320, 120);
    window.show();
    QCoreApplication::processEvents();
    QImage image(editor->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    editor->viewport()->render(&painter);
    painter.end();

    int minimumAlpha = 255;
    int maximumAlpha = 0;
    int opaquePixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            minimumAlpha = std::min(minimumAlpha, qAlpha(line[x]));
            maximumAlpha = std::max(maximumAlpha, qAlpha(line[x]));
            opaquePixels += qAlpha(line[x]) == 255 ? 1 : 0;
        }
    }
    QCOMPARE(minimumAlpha, 0);
    QCOMPARE(maximumAlpha, 255);
    QVERIFY(opaquePixels > 30);
    QCOMPARE(qAlpha(image.pixel(image.width() - 20, image.height() - 20)), 0);
    QCOMPARE(qAlpha(image.pixel(image.width() - 20,
                                static_cast<int>(editor->textHeightF(0) / 2))), 0);
    QCOMPARE(qAlpha(image.pixel(4, image.height() - 20)), 0);
    QCOMPARE(editor->textColor().alpha(), 255);
}

void AppearanceTest::transparentScrollClearsPreviousText()
{
    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto* editor = new vinson::EditorWidget(&window);
    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor);
    vinson::ThemeManager manager(editor, &window);

    editor->setLineNumbersVisible(false);
    editor->setHScrollBar(false);
    editor->setVScrollBar(false);
    editor->setCaretWidth(0);
    QByteArray document;
    for (int line = 0; line < 40; ++line) {
        document += QByteArray::number(line)
            + (line % 2 == 0 ? " MMMMWWWW" : " iiiillll") + '\n';
    }
    editor->setTextUtf8(document);
    vinson::Appearance appearance = manager.appearance();
    appearance.textColor = QColor(255, 255, 255);
    appearance.backgroundColor = QColor(0, 0, 0, 0);
    manager.setFramelessMode(true);
    manager.applyAppearance(appearance);

    window.resize(300, 120);
    window.show();
    QCoreApplication::processEvents();

    QImage reused(editor->viewport()->size(),
                  QImage::Format_ARGB32_Premultiplied);
    reused.fill(Qt::transparent);
    editor->viewport()->render(&reused, QPoint(), QRegion(),
                               QWidget::RenderFlags{});

    editor->lineScroll(0, 1);
    QCoreApplication::processEvents();

    QImage expected(editor->viewport()->size(),
                    QImage::Format_ARGB32_Premultiplied);
    expected.fill(Qt::transparent);
    editor->viewport()->render(&expected, QPoint(), QRegion(),
                               QWidget::RenderFlags{});

    // Repaint the scrolled state over the previous frame. Translucent
    // background pixels must replace that frame instead of blending with it.
    editor->viewport()->render(&reused, QPoint(), QRegion(),
                               QWidget::RenderFlags{});
    QCOMPARE(reused, expected);
}

void AppearanceTest::nativeFramedBackgroundAlphaIsAvailable()
{
#if defined(Q_OS_WIN)
    if (QGuiApplication::platformName() != QStringLiteral("windows")) {
        QSKIP("Native DWM verification requires the Windows QPA plugin.");
    }

    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    (void)window.winId();
    QVERIFY(vinson::setNativeBackgroundAlphaEnabled(&window, true));
#else
    QSKIP("Native DWM verification is Windows-only.");
#endif
}

void AppearanceTest::transparentFontRendersPartialAlpha()
{
    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto* editor = new vinson::EditorWidget(&window);
    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor);
    vinson::ThemeManager manager(editor, &window);

    editor->setLineNumbersVisible(false);
    editor->setTextUtf8("Opacity");
    vinson::Appearance appearance = manager.appearance();
    appearance.textColor = QColor(255, 255, 255, 96);
    appearance.backgroundColor = QColor(0, 0, 0, 0);
    manager.setFramelessMode(true);
    manager.applyAppearance(appearance);

    window.resize(260, 90);
    window.show();
    QCoreApplication::processEvents();
    QImage image(editor->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    editor->viewport()->render(&painter);
    painter.end();

    int partialAlphaPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            partialAlphaPixels += alpha > 0 && alpha < 255 ? 1 : 0;
        }
    }
    QVERIFY(partialAlphaPixels > 20);
    QCOMPARE(editor->textColor(), QColor(255, 255, 255, 96));
}

void AppearanceTest::dialogPreviewsBackgroundAlpha()
{
    vinson::Appearance appearance = vinson::ThemeManager::defaultAppearance();
    const QKeySequence bossKey(QStringLiteral("Ctrl+Alt+Space"));
    const QKeySequence focusShortcut(QStringLiteral("Ctrl+Alt+F"));
    vinson::SettingsDialog dialog(appearance, bossKey, focusShortcut, true);
    QSignalSpy previewSpy(&dialog, &vinson::SettingsDialog::previewChanged);
    auto* fontSize = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("fontSize"));
    auto* alpha = dialog.findChild<QSlider*>(QStringLiteral("backgroundAlpha"));
    auto* textAlpha = dialog.findChild<QSlider*>(QStringLiteral("textAlpha"));
    auto* categories = dialog.findChild<QTabWidget*>(
        QStringLiteral("settingsCategories"));
    QVERIFY(fontSize != nullptr);
    QVERIFY(alpha != nullptr);
    QVERIFY(textAlpha != nullptr);
    QVERIFY(categories != nullptr);
    QCOMPARE(categories->count(), 3);
    QVERIFY(dialog.findChild<QGroupBox*>(
        QStringLiteral("typographyGroup")) != nullptr);
    QVERIFY(dialog.findChild<QGroupBox*>(
        QStringLiteral("colorsGroup")) != nullptr);
    QVERIFY(dialog.findChild<QGroupBox*>(
        QStringLiteral("transparencyGroup")) != nullptr);
    QVERIFY(dialog.findChild<QGroupBox*>(
        QStringLiteral("shortcutsGroup")) != nullptr);
    QVERIFY(dialog.findChild<QGroupBox*>(
        QStringLiteral("sessionGroup")) != nullptr);
    QVERIFY(dialog.restoreTabsOnStartup());
    const QColor categoryText =
        dialog.property("categoryTextColor").value<QColor>();
    const QColor categoryBorder =
        dialog.property("categoryBorderColor").value<QColor>();
    QVERIFY(categoryText.isValid());
    QVERIFY(categoryBorder.isValid());
    const QColor dialogBackground = dialog.palette().color(QPalette::Window);
    const QColor dialogText = dialog.palette().color(QPalette::WindowText);
    auto colorDistance = [](const QColor& first, const QColor& second) {
        return std::abs(first.red() - second.red())
            + std::abs(first.green() - second.green())
            + std::abs(first.blue() - second.blue());
    };
    QVERIFY(colorDistance(dialogBackground, categoryText)
            < colorDistance(dialogBackground, dialogText));
    QVERIFY(colorDistance(dialogBackground, categoryBorder)
            < colorDistance(dialogBackground, categoryText));
    auto* bossKeyEdit = dialog.findChild<QKeySequenceEdit*>(
        QStringLiteral("bossKey"));
    QVERIFY(bossKeyEdit != nullptr);
    QCOMPARE(dialog.bossKey(), bossKey);
    QCOMPARE(dialog.focusShortcut(), focusShortcut);

    fontSize->setValue(20.0);
    QCOMPARE(dialog.appearance().font.pointSizeF(), 20.0);
    QCOMPARE(previewSpy.count(), 1);
    previewSpy.clear();
    textAlpha->setValue(80);
    QCOMPARE(dialog.appearance().textColor.alpha(), 80);
    QCOMPARE(previewSpy.count(), 1);
    previewSpy.clear();
    alpha->setValue(0);

    QCOMPARE(dialog.appearance().backgroundColor.alpha(), 0);
    QCOMPARE(previewSpy.count(), 1);

    bossKeyEdit->setKeySequence(
        QKeySequence(QStringLiteral("Ctrl+Shift+F12")));
    QCOMPARE(dialog.bossKey(),
             QKeySequence(QStringLiteral("Ctrl+Shift+F12")));

    auto* focusShortcutEdit = dialog.findChild<QKeySequenceEdit*>(
        QStringLiteral("focusShortcut"));
    QVERIFY(focusShortcutEdit != nullptr);
    focusShortcutEdit->setKeySequence(
        QKeySequence(QStringLiteral("Ctrl+Alt+F11")));
    QCOMPARE(dialog.focusShortcut(),
             QKeySequence(QStringLiteral("Ctrl+Alt+F11")));
}

void AppearanceTest::colorSwatchesKeepTheirColorOnHover()
{
    const vinson::Appearance appearance =
        vinson::ThemeManager::defaultAppearance();
    vinson::SettingsDialog dialog(
        appearance, QKeySequence(QStringLiteral("Ctrl+Alt+Space")),
        QKeySequence(QStringLiteral("Ctrl+Alt+F")), true);
    auto* button = dialog.findChild<QPushButton*>(
        QStringLiteral("textColor"));
    QVERIFY(button != nullptr);
    QCOMPARE(button->property("swatchColor").toString(),
             appearance.textColor.name(QColor::HexRgb));
    QVERIFY(button->styleSheet().contains(
        QStringLiteral("QPushButton:hover,")));
    QVERIFY(button->styleSheet().contains(
        QStringLiteral("background-color: %1")
            .arg(appearance.textColor.name(QColor::HexRgb))));

    dialog.show();
    QCoreApplication::processEvents();
    const QPoint samplePoint(8, button->height() / 2);
    QImage normal(button->size(), QImage::Format_ARGB32_Premultiplied);
    normal.fill(Qt::transparent);
    button->render(&normal);

    QTest::mouseMove(button, button->rect().center());
    QCoreApplication::processEvents();
    QImage hovered(button->size(), QImage::Format_ARGB32_Premultiplied);
    hovered.fill(Qt::transparent);
    button->render(&hovered);

    QCOMPARE(hovered.pixelColor(samplePoint), normal.pixelColor(samplePoint));
    QCOMPARE(hovered.pixelColor(samplePoint),
             QColor(appearance.textColor.red(), appearance.textColor.green(),
                    appearance.textColor.blue()));
}

QTEST_MAIN(AppearanceTest)
#include "AppearanceTest.moc"
