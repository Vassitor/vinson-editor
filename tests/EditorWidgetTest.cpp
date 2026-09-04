#include "editor/EditorWidget.h"
#include "largefile/LargeFilePolicy.h"

#include <QSignalSpy>
#include <QWheelEvent>
#include <QtTest>

class EditorWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void storesUtf8Text();
    void usesDevicePixelAlignedRendering();
    void defaultsToWrappedTextWithoutHorizontalScrolling();
    void acceptsKeyboardInput();
    void togglesViewOptions();
    void createsLargeDocumentBeforeLoading();
    void readsBoundedTextRanges();
    void searchesAcrossResponsiveSliceBoundary();
    void requestsFontSizeAdjustmentFromControlWheel();
    void exposesAndRestoresEditHistory();
};

void EditorWidgetTest::usesDevicePixelAlignedRendering()
{
    vinson::EditorWidget editor;

    QCOMPARE(editor.scaleTechnique(),
             static_cast<sptr_t>(Scintilla::ScaleTechnique::PixelAligned));
    QCOMPARE(editor.viewport()->property("ScintillaScale").toDouble(),
             editor.devicePixelRatioF());
}

void EditorWidgetTest::defaultsToWrappedTextWithoutHorizontalScrolling()
{
    vinson::EditorWidget editor;

    QVERIFY(editor.isWordWrapEnabled());
    QVERIFY(!editor.hScrollBar());

    editor.setWordWrapEnabled(false);
    QVERIFY(!editor.isWordWrapEnabled());
    QVERIFY(editor.hScrollBar());
}

void EditorWidgetTest::storesUtf8Text()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8(QByteArray("hello, \xE4\xB8\x96\xE7\x95\x8C"));

    QCOMPARE(editor.textUtf8(), QByteArray("hello, \xE4\xB8\x96\xE7\x95\x8C"));
}

void EditorWidgetTest::acceptsKeyboardInput()
{
    vinson::EditorWidget editor;
    QSignalSpy modifiedSpy(&editor, &vinson::EditorWidget::documentModified);
    editor.resize(480, 240);
    editor.show();
    editor.QWidget::setFocus();

    QTest::keyClicks(&editor, QStringLiteral("hello"));

    QTRY_COMPARE(editor.textUtf8(), QByteArray("hello"));
    QVERIFY(!modifiedSpy.isEmpty());
    QCOMPARE(modifiedSpy.last().at(0).toBool(), true);
}

void EditorWidgetTest::togglesViewOptions()
{
    vinson::EditorWidget editor;

    editor.setWordWrapEnabled(true);
    QVERIFY(editor.isWordWrapEnabled());
    editor.setLineNumbersVisible(false);
    QVERIFY(!editor.areLineNumbersVisible());
}

void EditorWidgetTest::createsLargeDocumentBeforeLoading()
{
    vinson::EditorWidget editor;

    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::VeryLarge));
    QCOMPARE(editor.largeFileMode(), vinson::LargeFileMode::VeryLarge);
    QCOMPARE(editor.documentOptionFlags(), 0x101);
    editor.appendTextUtf8("large document text");
    editor.completeFileLoad(vinson::LineEnding::Lf);
    QCOMPARE(editor.textUtf8(), QByteArray("large document text"));

    QVERIFY(editor.resetDocument());
    QCOMPARE(editor.largeFileMode(), vinson::LargeFileMode::Normal);
    QCOMPARE(editor.documentOptionFlags(), 0);
    QVERIFY(editor.isEmpty());
}

void EditorWidgetTest::readsBoundedTextRanges()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("0123456789");

    QCOMPARE(editor.textRangeUtf8(3, 4), QByteArray("3456"));
    QCOMPARE(editor.textRangeUtf8(8, 20), QByteArray("89"));
    QVERIFY(editor.textRangeUtf8(10, 1).isEmpty());
}

void EditorWidgetTest::searchesAcrossResponsiveSliceBoundary()
{
    vinson::EditorWidget editor;
    QByteArray text(vinson::LargeFilePolicy::responsiveSearchSlice + 32, 'x');
    const QByteArray query("needle");
    const qint64 queryPosition =
        vinson::LargeFilePolicy::responsiveSearchSlice - 2;
    text.replace(queryPosition, query.size(), query);

    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::Large, text.size()));
    editor.appendTextUtf8(text);
    editor.completeFileLoad(vinson::LineEnding::None);

    const vinson::SearchRange forward = editor.findTextUtf8Responsive(
        query, 0, editor.documentLength(), {});
    QCOMPARE(forward.start, queryPosition);
    QCOMPARE(forward.end, queryPosition + query.size());
    const vinson::SearchRange backward = editor.findTextUtf8Responsive(
        query, editor.documentLength(), 0, {});
    QCOMPARE(backward.start, forward.start);
    QCOMPARE(backward.end, forward.end);
}

void EditorWidgetTest::requestsFontSizeAdjustmentFromControlWheel()
{
    vinson::EditorWidget editor;
    QSignalSpy adjustmentSpy(
        &editor, &vinson::EditorWidget::fontSizeAdjustmentRequested);

    QWheelEvent partialUp(
        QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 60),
        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor.viewport(), &partialUp);
    QCOMPARE(adjustmentSpy.count(), 0);

    QWheelEvent secondPartialUp(
        QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 60),
        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor.viewport(), &secondPartialUp);
    QCOMPARE(adjustmentSpy.count(), 1);
    QCOMPARE(adjustmentSpy.takeFirst().at(0).toInt(), 1);

    QWheelEvent down(
        QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -240),
        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor.viewport(), &down);
    QCOMPARE(adjustmentSpy.count(), 1);
    QCOMPARE(adjustmentSpy.takeFirst().at(0).toInt(), -2);
}

void EditorWidgetTest::exposesAndRestoresEditHistory()
{
    vinson::EditorWidget editor;
    editor.resize(480, 240);
    editor.show();
    editor.QWidget::setFocus();

    QTest::keyClicks(&editor, QStringLiteral("abc"));
    QTest::keyClick(&editor, Qt::Key_Left);
    QTest::keyClicks(&editor, QStringLiteral("X"));

    const QVector<vinson::EditHistoryEntry> history = editor.editHistory();
    QCOMPARE(history.size(), 2);
    QCOMPARE(history.at(0).kind, vinson::EditHistoryKind::Insert);
    QCOMPARE(history.at(0).preview, QStringLiteral("abc"));
    QCOMPARE(history.at(1).preview, QStringLiteral("X"));
    QCOMPARE(editor.textUtf8(), QByteArray("abXc"));

    QVERIFY(editor.restoreEditHistoryPosition(history.at(0).undoPosition));
    QCOMPARE(editor.textUtf8(), QByteArray("abc"));
    QCOMPARE(editor.currentEditHistoryPosition(),
             history.at(0).undoPosition);

    QVERIFY(editor.restoreEditHistoryPosition(history.at(1).undoPosition));
    QCOMPARE(editor.textUtf8(), QByteArray("abXc"));
    QVERIFY(editor.restoreEditHistoryPosition(0));
    QVERIFY(editor.textUtf8().isEmpty());
}

QTEST_MAIN(EditorWidgetTest)
#include "EditorWidgetTest.moc"
