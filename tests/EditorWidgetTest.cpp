#include "editor/EditorWidget.h"
#include "largefile/LargeFilePolicy.h"
#include "ui/EditHistoryWidget.h"

#include <QListWidget>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>
#include <QPushButton>
#include <QSignalSpy>
#include <QWheelEvent>
#include <QtTest>

class EditorWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void convertsLineEndingsAsOneUndoAction_data();
    void convertsLineEndingsAsOneUndoAction();
    void tracksLineEndingBoundaryEdits();
    void storesUtf8Text();
    void marksRecoveredTextAsModifiedWithoutChangingIt();
    void contextMenuUsesSharedAppearanceAndActions();
    void usesDevicePixelAlignedRendering();
    void defaultsToWrappedTextWithoutHorizontalScrolling();
    void acceptsKeyboardInput();
    void togglesViewOptions();
    void adjustsLineNumberMarginForLineCountAndFont();
    void createsLargeDocumentBeforeLoading();
    void readsBoundedTextRanges();
    void searchesAcrossResponsiveSliceBoundary();
    void requestsFontSizeAdjustmentFromControlWheel();
    void exposesAndRestoresEditHistory();
    void defersHiddenEditHistoryRefresh();
    void refreshesEditHistoryAfterDocumentSwitch();
    void bookmarksDoNotChangeTextOrUndoHistory();
    void bookmarkNavigationWrapsAndHandlesEmptyDocuments();
    void bookmarksFollowLineEdits();
    void bookmarksBelongToEachDocument_data();
    void bookmarksBelongToEachDocument();
    void bookmarkMarginTogglesWithoutMovingSelection();
    void hiddenBookmarksDoNotDecorateText();
};

void EditorWidgetTest::convertsLineEndingsAsOneUndoAction_data()
{
    QTest::addColumn<int>("ending");
    QTest::addColumn<QByteArray>("expected");
    QTest::newRow("lf") << int(vinson::LineEnding::Lf) << QByteArray("a\nb\nc\nd\n");
    QTest::newRow("crlf") << int(vinson::LineEnding::CrLf) << QByteArray("a\r\nb\r\nc\r\nd\r\n");
    QTest::newRow("cr") << int(vinson::LineEnding::Cr) << QByteArray("a\rb\rc\rd\r");
}

void EditorWidgetTest::convertsLineEndingsAsOneUndoAction()
{
    QFETCH(int, ending);
    QFETCH(QByteArray, expected);
    vinson::EditorWidget editor;
    const QByteArray source("a\r\nb\nc\rd\n");
    editor.setTextUtf8(source);
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::Mixed);
    QVERIFY(editor.toggleBookmarkAtLine(2));
    editor.convertLineEndings(static_cast<vinson::LineEnding>(ending));
    QCOMPARE(editor.textUtf8(), expected);
    QCOMPARE(editor.detectedLineEnding(), static_cast<vinson::LineEnding>(ending));
    QVERIFY(editor.modify());
    QVERIFY(editor.hasBookmarkAtLine(2));
    editor.undo();
    QCOMPARE(editor.textUtf8(), source);
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::Mixed);
    QVERIFY(!editor.modify());
    editor.redo();
    QCOMPARE(editor.textUtf8(), expected);
    QCOMPARE(editor.detectedLineEnding(), static_cast<vinson::LineEnding>(ending));
}

void EditorWidgetTest::tracksLineEndingBoundaryEdits()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("\r\n");
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::CrLf);
    editor.setSel(1, 1);
    editor.replaceSelectionUtf8("x");
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::Mixed);
    editor.undo();
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::CrLf);
    editor.setSel(0, 1);
    editor.replaceSelectionUtf8("");
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::Lf);
    editor.undo();
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::CrLf);
    const auto first = editor.retainCurrentDocument();
    const auto second = editor.createTabDocument();
    editor.activateTabDocument(second, vinson::LargeFileMode::Normal);
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::None);
    editor.appendTextUtf8("\r");
    editor.appendTextUtf8("\n");
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::CrLf);
    editor.activateTabDocument(first, vinson::LargeFileMode::Normal);
    QCOMPARE(editor.detectedLineEnding(), vinson::LineEnding::CrLf);
    editor.releaseTabDocument(second);
    editor.releaseTabDocument(first);
}

void EditorWidgetTest::bookmarksDoNotChangeTextOrUndoHistory()
{
    vinson::EditorWidget editor;
    const QByteArray original("first\nsecond\nthird\n");
    editor.setTextUtf8(original);
    editor.setSel(editor.documentLength(), editor.documentLength());
    editor.replaceSelectionUtf8("fourth");
    editor.markSaved();
    const int undoPosition = editor.currentEditHistoryPosition();
    QSignalSpy modified(&editor, &vinson::EditorWidget::documentModified);
    QSignalSpy history(&editor, &vinson::EditorWidget::editHistoryChanged);

    QVERIFY(editor.toggleBookmarkAtLine(1));
    QVERIFY(editor.toggleBookmarkAtLine(3));
    QVERIFY(editor.hasBookmarkAtLine(1));
    QVERIFY(editor.hasBookmarkAtLine(3));
    QVERIFY(editor.toggleBookmarkAtLine(3));
    QVERIFY(!editor.hasBookmarkAtLine(3));
    QVERIFY(!editor.toggleBookmarkAtLine(0));
    QVERIFY(!editor.toggleBookmarkAtLine(5));
    QVERIFY(!editor.hasBookmarkAtLine(-1));
    editor.clearAllBookmarks();
    QVERIFY(!editor.hasBookmarkAtLine(1));
    QCOMPARE(editor.textUtf8(), original + "fourth");
    QVERIFY(!editor.modify());
    QCOMPARE(editor.currentEditHistoryPosition(), undoPosition);
    QCOMPARE(modified.count(), 0);
    QCOMPARE(history.count(), 0);
    editor.undo();
    QCOMPARE(editor.textUtf8(), original);
}

void EditorWidgetTest::bookmarkNavigationWrapsAndHandlesEmptyDocuments()
{
    vinson::EditorWidget editor;
    QVERIFY(!editor.goToNextBookmark());
    QVERIFY(!editor.goToPreviousBookmark());
    editor.setTextUtf8("one\ntwo\nthree\nfour\nfive");
    QVERIFY(editor.toggleBookmarkAtLine(1));
    QVERIFY(editor.toggleBookmarkAtLine(3));
    QVERIFY(editor.toggleBookmarkAtLine(5));
    QVERIFY(editor.goToOneBasedLine(1));
    for (const qint64 line : {3, 5, 1}) {
        QVERIFY(editor.goToNextBookmark());
        QCOMPARE(editor.currentOneBasedLine(), line);
    }
    for (const qint64 line : {5, 3, 1}) {
        QVERIFY(editor.goToPreviousBookmark());
        QCOMPARE(editor.currentOneBasedLine(), line);
    }
    editor.clearAllBookmarks();
    editor.setSel(1, 3);
    QVERIFY(!editor.goToNextBookmark());
    QVERIFY(!editor.goToPreviousBookmark());
    QCOMPARE(editor.selectionStartPosition(), 1);
    QCOMPARE(editor.selectionEndPosition(), 3);
    QVERIFY(editor.toggleBookmarkAtLine(1));
    QVERIFY(editor.goToNextBookmark());
    QCOMPARE(editor.currentOneBasedLine(), 1);
    QVERIFY(editor.goToPreviousBookmark());
    QCOMPARE(editor.currentOneBasedLine(), 1);
}

void EditorWidgetTest::bookmarksFollowLineEdits()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("one\ntwo\nthree\nfour");
    QVERIFY(editor.toggleBookmarkAtLine(3));
    editor.insertText(0, "new\n");
    QVERIFY(editor.hasBookmarkAtLine(4));
    QVERIFY(!editor.hasBookmarkAtLine(3));
    editor.undo();
    QVERIFY(editor.hasBookmarkAtLine(3));
    editor.deleteRange(0, 4);
    QVERIFY(editor.hasBookmarkAtLine(2));
    editor.undo();
    QVERIFY(editor.hasBookmarkAtLine(3));
    editor.setTextUtf8("replacement");
    QVERIFY(!editor.hasBookmarkAtLine(1));
}

void EditorWidgetTest::bookmarksBelongToEachDocument_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("normal") << static_cast<int>(vinson::LargeFileMode::Normal);
    QTest::newRow("large") << static_cast<int>(vinson::LargeFileMode::Large);
    QTest::newRow("very-large") << static_cast<int>(vinson::LargeFileMode::VeryLarge);
}

void EditorWidgetTest::bookmarksBelongToEachDocument()
{
    QFETCH(int, mode);
    const auto fileMode = static_cast<vinson::LargeFileMode>(mode);
    vinson::EditorWidget editor;
    QVERIFY(editor.beginFileLoad(fileMode));
    editor.appendTextUtf8("first\nsecond\nthird");
    editor.completeFileLoad(vinson::LineEnding::Lf);
    const int options = editor.documentOptionFlags();
    QVERIFY(editor.toggleBookmarkAtLine(2));
    const sptr_t first = editor.retainCurrentDocument();
    const sptr_t second = editor.createTabDocument();
    QVERIFY(second != 0);
    editor.activateTabDocument(second, vinson::LargeFileMode::Normal);
    editor.setTextUtf8("other\npage\nlast");
    QVERIFY(!editor.hasBookmarkAtLine(2));
    QVERIFY(editor.toggleBookmarkAtLine(3));
    editor.activateTabDocument(first, fileMode);
    QVERIFY(editor.hasBookmarkAtLine(2));
    QVERIFY(!editor.hasBookmarkAtLine(3));
    QCOMPARE(editor.documentOptionFlags(), options);
    editor.activateTabDocument(second, vinson::LargeFileMode::Normal);
    QVERIFY(!editor.hasBookmarkAtLine(2));
    QVERIFY(editor.hasBookmarkAtLine(3));
    QVERIFY(editor.resetDocument());
    QVERIFY(!editor.hasBookmarkAtLine(1));
    editor.releaseTabDocument(first);
    editor.releaseTabDocument(second);
}

void EditorWidgetTest::bookmarkMarginTogglesWithoutMovingSelection()
{
    vinson::EditorWidget editor;
    editor.resize(480, 240);
    editor.setTextUtf8("first\nsecond\nthird");
    editor.show();
    QCoreApplication::processEvents();
    editor.setSel(1, 3);
    const QPoint gutter(static_cast<int>(editor.marginWidthN(0)
                                        + editor.marginWidthN(1) / 2),
                        static_cast<int>(editor.pointYFromPosition(6)
                                         + editor.textHeight(1) / 2));
    QTest::mouseClick(editor.viewport(), Qt::LeftButton, Qt::NoModifier, gutter);
    QVERIFY(editor.hasBookmarkAtLine(2));
    QCOMPARE(editor.selectionStartPosition(), 1);
    QCOMPARE(editor.selectionEndPosition(), 3);
    QTest::mouseClick(editor.viewport(), Qt::LeftButton, Qt::NoModifier, gutter);
    QVERIFY(!editor.hasBookmarkAtLine(2));
    QVERIFY(!editor.modify());
}

void EditorWidgetTest::hiddenBookmarksDoNotDecorateText()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("first\nsecond");
    QVERIFY(editor.toggleBookmarkAtLine(2));
    editor.setLineNumbersVisible(false);
    QCOMPARE(editor.marginWidthN(0), 0);
    QCOMPARE(editor.marginWidthN(1), 0);
    QCOMPARE(editor.markerSymbolDefined(0),
             static_cast<sptr_t>(Scintilla::MarkerSymbol::Empty));
    QVERIFY(editor.goToNextBookmark());
    QCOMPARE(editor.currentOneBasedLine(), 2);
    editor.setLineNumbersVisible(true);
    QVERIFY(editor.marginWidthN(1) > 0);
    QCOMPARE(editor.markerSymbolDefined(0),
             static_cast<sptr_t>(Scintilla::MarkerSymbol::Bookmark));
    QVERIFY(editor.hasBookmarkAtLine(2));
}

void EditorWidgetTest::contextMenuUsesSharedAppearanceAndActions()
{
    vinson::EditorWidget editor;
    editor.resize(480, 240);
    editor.show();
    editor.setTextUtf8("menu selection");
    bool inspected = false;
    QTimer::singleShot(0, &editor, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        // Close the nested exec loop even when a following assertion fails.
        menu->close();
        QCOMPARE(menu->objectName(), QStringLiteral("editorContextMenu"));
        QVERIFY(menu->findChild<QObject*>(QStringLiteral("nativeMenuAppearance")));
        QVERIFY(!menu->graphicsEffect());
        QVERIFY(!menu->testAttribute(Qt::WA_TranslucentBackground));
        const auto actions = menu->actions();
        QCOMPARE(actions.size(), 10);
        QVERIFY(!actions.at(3)->isEnabled()); // Cut without a selection.
        QVERIFY(!actions.at(4)->isEnabled()); // Copy without a selection.
        actions.at(7)->trigger(); // Select All still reaches Scintilla.
        QCOMPARE(editor.selectionEnd() - editor.selectionStart(), sptr_t(14));
        inspected = true;
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(25, 25),
                            editor.viewport()->mapToGlobal(QPoint(25, 25)));
    QApplication::sendEvent(editor.viewport(), &event);
    QVERIFY(inspected);
}

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

void EditorWidgetTest::marksRecoveredTextAsModifiedWithoutChangingIt()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8("recovered text");
    QVERIFY(!editor.modify());

    editor.markRecovered();

    QCOMPARE(editor.textUtf8(), QByteArray("recovered text"));
    QVERIFY(editor.modify());
}

void EditorWidgetTest::adjustsLineNumberMarginForLineCountAndFont()
{
    vinson::EditorWidget editor;
    editor.setTextUtf8(QByteArray(998, '\n'));
    QCOMPARE(editor.editorLineCount(), 999);
    const sptr_t threeDigitWidth = editor.marginWidthN(0);

    editor.appendTextUtf8("\n");
    QCOMPARE(editor.editorLineCount(), 1000);
    QVERIFY(editor.marginWidthN(0) > threeDigitWidth);
    editor.undo();
    QCOMPARE(editor.marginWidthN(0), threeDigitWidth);

    editor.setLineNumbersVisible(false);
    editor.appendTextUtf8(QByteArray(9001, '\n'));
    QCOMPARE(editor.editorLineCount(), 10000);
    QCOMPARE(editor.marginWidthN(0), 0);
    editor.setLineNumbersVisible(true);
    const sptr_t fiveDigitWidth = editor.marginWidthN(0);
    QVERIFY(fiveDigitWidth > threeDigitWidth);

    QFont font = editor.editorFont();
    font.setPointSizeF(font.pointSizeF() * 2);
    editor.setEditorFont(font);
    QVERIFY(editor.marginWidthN(0) > fiveDigitWidth);
    const sptr_t enlargedWidth = editor.marginWidthN(0);
    QVERIFY(editor.resetDocument());
    QVERIFY(editor.marginWidthN(0) < enlargedWidth);
    QVERIFY(editor.marginWidthN(0) > threeDigitWidth);
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

void EditorWidgetTest::defersHiddenEditHistoryRefresh()
{
    vinson::EditorWidget editor;
    vinson::EditHistoryWidget history(&editor);
    history.show();
    auto* list = history.findChild<QListWidget*>();
    auto* restoreButton = history.findChild<QPushButton*>();
    QVERIFY(list != nullptr);
    QVERIFY(restoreButton != nullptr);
    QCOMPARE(list->count(), 1);
    QSignalSpy resetSpy(list->model(), &QAbstractItemModel::modelReset);

    // Hiding before the debounce timeout must cancel the pending rebuild.
    editor.appendTextUtf8("first");
    history.hide();
    editor.markSaved();
    editor.appendTextUtf8(" second");
    QTest::qWait(150);
    QCOMPARE(resetSpy.count(), 0);

    history.show();
    QVERIFY(list->count() > 1);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(),
             editor.currentEditHistoryPosition());
    QVERIFY(!restoreButton->isEnabled());
    QCOMPARE(resetSpy.count(), 1);
    QTest::qWait(150);
    QCOMPARE(resetSpy.count(), 1);

    editor.undo();
    QTRY_COMPARE(list->currentItem()->data(Qt::UserRole).toInt(),
                 editor.currentEditHistoryPosition());
    editor.redo();
    QTRY_COMPARE(list->currentItem()->data(Qt::UserRole).toInt(),
                 editor.currentEditHistoryPosition());

    // Restoring immediately consumes the scheduled refresh as well.
    list->setCurrentRow(0);
    QVERIFY(restoreButton->isEnabled());
    resetSpy.clear();
    restoreButton->click();
    QCOMPARE(editor.textUtf8(), QByteArray());
    QCOMPARE(resetSpy.count(), 1);
    QTest::qWait(150);
    QCOMPARE(resetSpy.count(), 1);
}

void EditorWidgetTest::refreshesEditHistoryAfterDocumentSwitch()
{
    vinson::EditorWidget editor;
    vinson::EditHistoryWidget history(&editor);
    editor.appendTextUtf8("first document");
    history.show();
    auto* list = history.findChild<QListWidget*>();
    QVERIFY(list != nullptr);
    QVERIFY(list->count() > 1);

    const sptr_t firstDocument = editor.retainCurrentDocument();
    QVERIFY(editor.resetDocument());
    history.refresh();
    QCOMPARE(list->count(), 1);

    history.hide();
    QSignalSpy resetSpy(list->model(), &QAbstractItemModel::modelReset);
    editor.activateTabDocument(firstDocument, vinson::LargeFileMode::Normal);
    editor.releaseTabDocument(firstDocument);
    history.refresh();
    QCOMPARE(resetSpy.count(), 0);
    history.show();
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(list->count(), 2);
    QVERIFY(list->currentItem()->text().contains(
        QStringLiteral("first document")));
}

QTEST_MAIN(EditorWidgetTest)
#include "EditorWidgetTest.moc"
