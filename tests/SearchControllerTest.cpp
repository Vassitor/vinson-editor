#include "editor/EditorWidget.h"
#include "search/SearchController.h"
#include "ui/FindReplaceWidget.h"

#include <QLineEdit>
#include <QSignalSpy>
#include <QtTest>

class SearchControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void findsForwardAndWraps();
    void findsBackwardAndWraps();
    void respectsCaseAndWholeWord();
    void replacesCurrentMatch();
    void replacesAllAsOneUndoAction();
    void findsUnicodeText();
    void goesToLine();
    void handlesEmptyQuery();
    void findWidgetSwitchesModeAndSubmits();
};

void SearchControllerTest::findsForwardAndWraps()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("alpha beta alpha");
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("alpha"));

    QCOMPARE(controller.findNext(), vinson::SearchResult::Found);
    QCOMPARE(editor.selectedTextUtf8(), QByteArray("alpha"));
    QCOMPARE(editor.selectionStartPosition(), 0);
    QCOMPARE(controller.findNext(), vinson::SearchResult::Found);
    QCOMPARE(editor.selectionStartPosition(), 11);
    QCOMPARE(controller.findNext(), vinson::SearchResult::Wrapped);
    QCOMPARE(editor.selectionStartPosition(), 0);
}

void SearchControllerTest::findsBackwardAndWraps()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("one two one");
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("one"));

    QCOMPARE(controller.findPrevious(), vinson::SearchResult::Wrapped);
    QCOMPARE(editor.selectionStartPosition(), 8);
    QCOMPARE(controller.findPrevious(), vinson::SearchResult::Found);
    QCOMPARE(editor.selectionStartPosition(), 0);
}

void SearchControllerTest::respectsCaseAndWholeWord()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("Cat cat scatter cat");
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("cat"));
    controller.setOptions({true, true, true});

    QCOMPARE(controller.findNext(), vinson::SearchResult::Found);
    QCOMPARE(editor.selectionStartPosition(), 4);
    QCOMPARE(controller.findNext(), vinson::SearchResult::Found);
    QCOMPARE(editor.selectionStartPosition(), 16);
}

void SearchControllerTest::replacesCurrentMatch()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("one two one");
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("one"));
    controller.setReplacementText(QStringLiteral("1"));

    QVERIFY(controller.replaceCurrent());
    QCOMPARE(editor.textUtf8(), QByteArray("one two one"));
    QVERIFY(controller.replaceCurrent());
    QCOMPARE(editor.textUtf8(), QByteArray("1 two one"));
    QCOMPARE(editor.selectedTextUtf8(), QByteArray("one"));
}

void SearchControllerTest::replacesAllAsOneUndoAction()
{
    vinson::EditorWidget editor;
    const QByteArray original("\xE7\x8C\xAB dog \xE7\x8C\xAB");
    editor.setTextUtf8(original);
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("猫"));
    controller.setReplacementText(QStringLiteral("cat"));

    QCOMPARE(controller.replaceAll(), 2);
    QCOMPARE(editor.textUtf8(), QByteArray("cat dog cat"));
    editor.undo();
    QCOMPARE(editor.textUtf8(), original);
}

void SearchControllerTest::findsUnicodeText()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8(QByteArray("hello \xE4\xB8\x96\xE7\x95\x8C world"));
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("世界"));

    QCOMPARE(controller.findNext(), vinson::SearchResult::Found);
    QCOMPARE(editor.selectedTextUtf8(), QByteArray("\xE4\xB8\x96\xE7\x95\x8C"));
}

void SearchControllerTest::goesToLine()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("first\nsecond\nthird");
    vinson::SearchController controller(&editor);
    QVERIFY(controller.goToLine(3));
    QCOMPARE(editor.currentOneBasedLine(), 3);
    QVERIFY(!controller.goToLine(4));
}

void SearchControllerTest::handlesEmptyQuery()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("text");
    vinson::SearchController controller(&editor);

    QCOMPARE(controller.findNext(), vinson::SearchResult::EmptyQuery);
    QCOMPARE(controller.replaceAll(), 0);
}

void SearchControllerTest::findWidgetSwitchesModeAndSubmits()
{
    vinson::FindReplaceWidget widget;
    QSignalSpy nextSpy(&widget, &vinson::FindReplaceWidget::findNextRequested);

    widget.open(false, QStringLiteral("needle"));
    QVERIFY(!widget.isReplaceMode());
    QCOMPARE(widget.findText(), QStringLiteral("needle"));
    auto* findEdit = widget.findChild<QLineEdit*>(QStringLiteral("findText"));
    QVERIFY(findEdit != nullptr);
    QTest::keyClick(findEdit, Qt::Key_Return);
    QCOMPARE(nextSpy.count(), 1);

    widget.open(true);
    QVERIFY(widget.isReplaceMode());
}

QTEST_MAIN(SearchControllerTest)
#include "SearchControllerTest.moc"
