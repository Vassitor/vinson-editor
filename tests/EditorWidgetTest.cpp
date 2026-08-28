#include "editor/EditorWidget.h"
#include "largefile/LargeFilePolicy.h"

#include <QSignalSpy>
#include <QtTest>

class EditorWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void storesUtf8Text();
    void acceptsKeyboardInput();
    void togglesViewOptions();
    void createsLargeDocumentBeforeLoading();
    void readsBoundedTextRanges();
    void searchesAcrossResponsiveSliceBoundary();
};

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

QTEST_MAIN(EditorWidgetTest)
#include "EditorWidgetTest.moc"
