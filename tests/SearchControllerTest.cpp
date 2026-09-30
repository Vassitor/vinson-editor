#include "editor/EditorWidget.h"
#include "search/SearchController.h"
#include "ui/FindReplaceWidget.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

namespace {

QByteArray largeSearchText(qsizetype bytes)
{
    QByteArray text(bytes, 'x');
    for (qsizetype position = 63; position < bytes; position += 64) {
        text[position] = '\n';
    }
    return text;
}

} // namespace

class SearchControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void findsForwardAndWraps();
    void findsBackwardAndWraps();
    void doesNotWrapWhenDisabled();
    void respectsCaseAndWholeWord();
    void replacesCurrentMatch();
    void replacesAllAsOneUndoAction();
    void findsUnicodeText();
    void goesToLine();
    void handlesEmptyQuery();
    void findWidgetSwitchesModeAndSubmits();
    void findsLargeFileBoundaryMatch_data();
    void findsLargeFileBoundaryMatch();
    void cancelsBetweenSlicesWithoutChangingDocument();
    void cancelsBeforeFirstSliceAndRestarts();
    void invalidatesPendingSearch_data();
    void invalidatesPendingSearch();
    void finishesMissingLargeFileSearch_data();
    void finishesMissingLargeFileSearch();
    void findWidgetKeepsCancellationAvailable();
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

void SearchControllerTest::doesNotWrapWhenDisabled()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("only match");
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("only"));
    controller.setOptions({false, false, false});

    QCOMPARE(controller.findNext(), vinson::SearchResult::Found);
    QCOMPARE(controller.findNext(), vinson::SearchResult::NotFound);
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
    QSignalSpy optionsSpy(&widget, &vinson::FindReplaceWidget::optionsChanged);

    widget.open(false, QStringLiteral("needle"));
    QVERIFY(!widget.isReplaceMode());
    QCOMPARE(widget.findText(), QStringLiteral("needle"));
    auto* findEdit = widget.findChild<QLineEdit*>(QStringLiteral("findText"));
    QVERIFY(findEdit != nullptr);
    QTest::keyClick(findEdit, Qt::Key_Return);
    QCOMPARE(nextSpy.count(), 1);

    auto* wrapAround = widget.findChild<QCheckBox*>(
        QStringLiteral("wrapAround"));
    QVERIFY(wrapAround != nullptr);
    QVERIFY(widget.options().wrapAround);
    wrapAround->setChecked(false);
    QVERIFY(!widget.options().wrapAround);
    QVERIFY(!optionsSpy.isEmpty());
    QVERIFY(!qvariant_cast<vinson::SearchOptions>(
                 optionsSpy.last().at(0)).wrapAround);

    widget.open(true);
    QVERIFY(widget.isReplaceMode());
}

void SearchControllerTest::findsLargeFileBoundaryMatch_data()
{
    QTest::addColumn<bool>("backward");
    QTest::addColumn<bool>("wrapped");
    QTest::newRow("forward") << false << false;
    QTest::newRow("backward") << true << false;
    QTest::newRow("forward-wrapped") << false << true;
    QTest::newRow("backward-wrapped") << true << true;
}

void SearchControllerTest::findsLargeFileBoundaryMatch()
{
    QFETCH(bool, backward);
    QFETCH(bool, wrapped);
    const qint64 slice = vinson::LargeFilePolicy::responsiveSearchSlice;
    const QByteArray needle = QStringLiteral("世界").toUtf8();
    const qint64 position = slice - 3;
    QByteArray text = largeSearchText(2 * slice);
    text.replace(position, needle.size(), needle);
    text[position - 1] = ' ';
    text[position + needle.size()] = ' ';
    vinson::EditorWidget editor;
    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::Large, text.size()));
    editor.appendTextUtf8(text);
    editor.completeFileLoad(vinson::LineEnding::None);
    const qint64 origin = backward != wrapped ? text.size() : 0;
    editor.setSel(origin, origin);
    vinson::SearchController controller(&editor);
    controller.setSearchText(QString::fromUtf8(needle));
    controller.setOptions({true, true, wrapped});
    QSignalSpy results(&controller, &vinson::SearchController::resultChanged);

    QCOMPARE(backward ? controller.findPrevious() : controller.findNext(),
             vinson::SearchResult::Searching);
    QVERIFY(controller.isSearching());
    QTRY_VERIFY(!controller.isSearching());
    QCOMPARE(qvariant_cast<vinson::SearchResult>(results.last().at(0)),
             wrapped ? vinson::SearchResult::Wrapped : vinson::SearchResult::Found);
    QCOMPARE(editor.selectionStartPosition(), position);
    QCOMPARE(editor.selectedTextUtf8(), needle);
}

void SearchControllerTest::cancelsBetweenSlicesWithoutChangingDocument()
{
    const qint64 slice = vinson::LargeFilePolicy::responsiveSearchSlice;
    QByteArray text = largeSearchText(3 * slice);
    text.replace(text.size() - 6, 6, "needle");
    vinson::EditorWidget editor;
    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::Large, text.size()));
    editor.appendTextUtf8(text);
    editor.completeFileLoad(vinson::LineEnding::None);
    editor.setSel(2, 5);
    const int undoPosition = editor.currentEditHistoryPosition();
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("needle"));
    controller.setReplacementText(QStringLiteral("changed"));
    QSignalSpy results(&controller, &vinson::SearchController::resultChanged);
    bool cancelledAfterScan = false;
    connect(&controller, &vinson::SearchController::progressChanged,
            &controller, [&](int percent) {
                if (percent > 0) {
                    // Repeated commands cannot mutate a pending scan.
                    QCOMPARE(controller.findPrevious(), vinson::SearchResult::Searching);
                    QVERIFY(!controller.replaceCurrent());
                    QCOMPARE(controller.replaceAll(), 0);
                    QVERIFY(!controller.goToLine(1));
                    cancelledAfterScan = true;
                    controller.cancelSearch();
                }
            });

    QCOMPARE(controller.findNext(), vinson::SearchResult::Searching);
    QTRY_VERIFY(!controller.isSearching());
    QVERIFY(cancelledAfterScan);
    QCOMPARE(qvariant_cast<vinson::SearchResult>(results.last().at(0)),
             vinson::SearchResult::Cancelled);
    QCOMPARE(editor.selectionStartPosition(), 2);
    QCOMPARE(editor.selectionEndPosition(), 5);
    QCOMPARE(editor.currentEditHistoryPosition(), undoPosition);
    QCOMPARE(editor.textUtf8(), text);
    QVERIFY(!editor.modify());
    QTest::qWait(30);
    QCOMPARE(results.count(), 2); // No stale completion after cancellation.
}

void SearchControllerTest::cancelsBeforeFirstSliceAndRestarts()
{
    vinson::EditorWidget editor;
    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::Large));
    editor.appendTextUtf8("first needle last");
    editor.completeFileLoad(vinson::LineEnding::None);
    editor.setSel(0, 0);
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("absent"));
    QSignalSpy results(&controller, &vinson::SearchController::resultChanged);

    QCOMPARE(controller.findNext(), vinson::SearchResult::Searching);
    controller.cancelSearch();
    QVERIFY(!controller.isSearching());
    controller.setSearchText(QStringLiteral("needle"));
    QCOMPARE(controller.findNext(), vinson::SearchResult::Searching);
    QTRY_VERIFY(!controller.isSearching());
    QCOMPARE(editor.selectedTextUtf8(), QByteArray("needle"));
    QCOMPARE(qvariant_cast<vinson::SearchResult>(results.last().at(0)),
             vinson::SearchResult::Found);
    QTest::qWait(30);
    QCOMPARE(results.count(), 4);
}

void SearchControllerTest::invalidatesPendingSearch_data()
{
    QTest::addColumn<int>("change");
    QTest::newRow("text") << 0;
    QTest::newRow("document") << 1;
    QTest::newRow("query") << 2;
    QTest::newRow("options") << 3;
}

void SearchControllerTest::invalidatesPendingSearch()
{
    QFETCH(int, change);
    vinson::EditorWidget editor;
    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::Large));
    editor.appendTextUtf8("needle");
    editor.completeFileLoad(vinson::LineEnding::None);
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("needle"));
    QSignalSpy results(&controller, &vinson::SearchController::resultChanged);
    QCOMPARE(controller.findNext(), vinson::SearchResult::Searching);
    switch (change) {
    case 0:
        editor.appendTextUtf8(" edit");
        break;
    case 1:
        QVERIFY(editor.resetDocument());
        editor.setTextUtf8("second"); // Same byte length as the first document.
        break;
    case 2:
        controller.setSearchText(QStringLiteral("other"));
        break;
    default:
        controller.setOptions({true, false, false});
        break;
    }
    QTRY_VERIFY(!controller.isSearching());
    QCOMPARE(qvariant_cast<vinson::SearchResult>(results.last().at(0)),
             vinson::SearchResult::Cancelled);
    QCOMPARE(results.count(), 2);
}

void SearchControllerTest::finishesMissingLargeFileSearch_data()
{
    findsLargeFileBoundaryMatch_data();
}

void SearchControllerTest::finishesMissingLargeFileSearch()
{
    QFETCH(bool, backward);
    QFETCH(bool, wrapped);
    const qint64 slice = vinson::LargeFilePolicy::responsiveSearchSlice;
    vinson::EditorWidget editor;
    QVERIFY(editor.beginFileLoad(vinson::LargeFileMode::VeryLarge, 2 * slice));
    editor.appendTextUtf8(largeSearchText(2 * slice));
    editor.completeFileLoad(vinson::LineEnding::None);
    editor.setSel(slice, slice);
    vinson::SearchController controller(&editor);
    controller.setSearchText(QStringLiteral("absent"));
    controller.setOptions({false, false, wrapped});
    QSignalSpy results(&controller, &vinson::SearchController::resultChanged);
    QSignalSpy progress(&controller, &vinson::SearchController::progressChanged);

    QCOMPARE(backward ? controller.findPrevious() : controller.findNext(),
             vinson::SearchResult::Searching);
    QTRY_VERIFY(!controller.isSearching());
    QCOMPARE(qvariant_cast<vinson::SearchResult>(results.last().at(0)),
             vinson::SearchResult::NotFound);
    QCOMPARE(editor.currentPosition(), slice);
    int previous = 0;
    for (const auto& update : progress) {
        const int percent = update.at(0).toInt();
        QVERIFY(percent >= previous && percent <= 100);
        previous = percent;
    }
}

void SearchControllerTest::findWidgetKeepsCancellationAvailable()
{
    vinson::FindReplaceWidget widget;
    widget.open(true, QStringLiteral("needle"));
    auto* cancel = widget.findChild<QPushButton*>(QStringLiteral("cancelSearchButton"));
    auto* find = widget.findChild<QLineEdit*>(QStringLiteral("findText"));
    QVERIFY(cancel);
    QVERIFY(find);
    QSignalSpy cancelled(&widget, &vinson::FindReplaceWidget::cancelSearchRequested);
    widget.setSearching(true);
    QVERIFY(cancel->isVisible());
    QVERIFY(cancel->isEnabled());
    QVERIFY(!find->isEnabled());
    QTest::mouseClick(cancel, Qt::LeftButton);
    QCOMPARE(cancelled.count(), 1);
    widget.setSearching(false);
    QVERIFY(cancel->isHidden());
    QVERIFY(find->isEnabled());
    QCOMPARE(find->text(), QStringLiteral("needle"));
}

QTEST_MAIN(SearchControllerTest)
#include "SearchControllerTest.moc"
