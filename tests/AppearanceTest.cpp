#include "editor/EditorWidget.h"
#include "settings/ThemeManager.h"
#include "ui/SettingsDialog.h"

#include <QImage>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QSignalSpy>
#include <QSlider>
#include <QVBoxLayout>
#include <QtTest>

#include <algorithm>

class AppearanceTest final : public QObject
{
    Q_OBJECT

private slots:
    void appliesAppearanceWithoutChangingDocument();
    void transparentBackgroundKeepsTextVisible();
    void dialogPreviewsBackgroundAlpha();
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
    QCOMPARE(editor->textColor(), QColor(10, 20, 30, 255));
    QCOMPARE(editor->backgroundColor(), QColor(40, 50, 60, 127));
    QCOMPARE(editor->cursorColor(), QColor(70, 80, 90, 255));
    QCOMPARE(editor->selectionTextColor(), QColor(100, 110, 120, 255));
    QCOMPARE(window.palette().color(QPalette::Window), QColor(40, 50, 60, 127));
    QCOMPARE(window.windowOpacity(), 1.0);
    QVERIFY(!editor->bufferedDraw());

    appearance.backgroundColor.setAlpha(255);
    manager.applyAppearance(appearance);
    QVERIFY(editor->bufferedDraw());
    QCOMPARE(editor->textUtf8(), QByteArray("appearance keeps this text"));
    QVERIFY(modifiedSpy.isEmpty());
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

    editor->setLineNumbersVisible(false);
    editor->setHScrollBar(false);
    editor->setVScrollBar(false);
    editor->setTextUtf8("Visible");
    vinson::Appearance appearance = manager.appearance();
    appearance.textColor = QColor(255, 255, 255);
    appearance.backgroundColor = QColor(12, 34, 56, 0);
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
    QCOMPARE(editor->textColor().alpha(), 255);
}

void AppearanceTest::dialogPreviewsBackgroundAlpha()
{
    vinson::Appearance appearance = vinson::ThemeManager::defaultAppearance();
    vinson::SettingsDialog dialog(appearance);
    QSignalSpy previewSpy(&dialog, &vinson::SettingsDialog::previewChanged);
    auto* fontSize = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("fontSize"));
    auto* alpha = dialog.findChild<QSlider*>(QStringLiteral("backgroundAlpha"));
    QVERIFY(fontSize != nullptr);
    QVERIFY(alpha != nullptr);

    fontSize->setValue(20.0);
    QCOMPARE(dialog.appearance().font.pointSizeF(), 20.0);
    QCOMPARE(previewSpy.count(), 1);
    previewSpy.clear();
    alpha->setValue(0);

    QCOMPARE(dialog.appearance().backgroundColor.alpha(), 0);
    QCOMPARE(previewSpy.count(), 1);
}

QTEST_MAIN(AppearanceTest)
#include "AppearanceTest.moc"
