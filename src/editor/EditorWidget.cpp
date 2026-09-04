#include "editor/EditorWidget.h"

#include <ScintillaTypes.h>

#include <QFontDatabase>
#include <QAction>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMenu>
#include <QFontMetricsF>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace vinson {
namespace {

constexpr auto styleIndex(Scintilla::StylesCommon style) noexcept
{
    return static_cast<sptr_t>(style);
}

constexpr int undoActionKindMask = 0x0f;
constexpr int undoActionMayCoalesce = 0x100;
constexpr int undoActionInsert = 0;
constexpr int undoActionDelete = 1;

QString historyPreview(const QByteArray& text)
{
    QString preview = QString::fromUtf8(text);
    preview.replace(QLatin1Char('\r'), QChar(0x21b5));
    preview.replace(QLatin1Char('\n'), QChar(0x21b5));
    preview.replace(QLatin1Char('\t'), QChar(0x21e5));
    constexpr qsizetype maximumCharacters = 48;
    if (preview.size() > maximumCharacters) {
        preview = preview.left(maximumCharacters - 1) + QChar(0x2026);
    }
    return preview;
}

} // namespace

EditorWidget::EditorWidget(QWidget* parent)
    : ScintillaEdit(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAttribute(Qt::WA_TranslucentBackground);
    // Render Scintilla's intermediate surfaces at the current device-pixel
    // ratio. Its DPI-change handler invalidates these surfaces when the window
    // moves between screens, so text and line numbers are rasterized again at
    // the destination monitor's native resolution instead of being scaled
    // from the previous monitor's buffer.
    setScaleTechnique(
        static_cast<sptr_t>(Scintilla::ScaleTechnique::PixelAligned));
    setCodePage(Scintilla::CpUtf8);
    setMarginTypeN(0, static_cast<sptr_t>(Scintilla::MarginType::Number));
    setScrollWidthTracking(true);
    setWordWrapEnabled(true);

    QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editorFont.setPointSize(12);
    setEditorFont(editorFont);
    setTextColor(QColor(32, 33, 36));
    setBackgroundColor(QColor(250, 250, 250));
    setCursorColor(QColor(32, 33, 36));
    setSelectionTextColor(QColor(255, 255, 255));
    setLineNumbersVisible(true);

    connect(this, &ScintillaEditBase::savePointChanged,
            this, &EditorWidget::documentModified);
    connect(this, &ScintillaEditBase::updateUi,
            this, [this](Scintilla::Update) { emitCursorPosition(); });
    connect(this, &ScintillaEditBase::linesAdded,
            this, [this](Scintilla::Position) { refreshLineNumberMargin(); });
    connect(this, &ScintillaEditBase::modified, this,
            [this](Scintilla::ModificationFlags type, Scintilla::Position,
                   Scintilla::Position, Scintilla::Position,
                   const QByteArray&, Scintilla::Position,
                   Scintilla::FoldLevel, Scintilla::FoldLevel) {
                const int flags = static_cast<int>(type);
                const int textChanges =
                    static_cast<int>(Scintilla::ModificationFlags::InsertText)
                    | static_cast<int>(Scintilla::ModificationFlags::DeleteText);
                if ((flags & textChanges) != 0) {
                    emit editHistoryChanged();
                }
            });
}

void EditorWidget::setTextUtf8(QByteArrayView text)
{
    const QByteArray terminatedText(text.data(), text.size());
    setUndoCollection(false);
    setText(terminatedText.constData());
    emptyUndoBuffer();
    setUndoCollection(true);
    setSavePoint();
    refreshLineNumberMargin();
}

QByteArray EditorWidget::textUtf8() const
{
    const sptr_t documentLength = textLength();
    if (documentLength < 0 ||
        documentLength >= static_cast<sptr_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    QByteArray result(static_cast<int>(documentLength) + 1, '\0');
    send(SCI_GETTEXT, static_cast<uptr_t>(result.size()),
         reinterpret_cast<sptr_t>(result.data()));
    result.chop(1);
    return result;
}

QByteArray EditorWidget::textRangeUtf8(qint64 start, qint64 length) const
{
    const qint64 available = documentLength();
    if (start < 0 || length <= 0 || start >= available) {
        return {};
    }

    const qint64 boundedLength = std::min(length, available - start);
    if (boundedLength > std::numeric_limits<qsizetype>::max()) {
        return {};
    }
    const sptr_t pointer = rangePointer(static_cast<sptr_t>(start),
                                        static_cast<sptr_t>(boundedLength));
    if (pointer == 0) {
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(pointer),
                      static_cast<qsizetype>(boundedLength));
}

void EditorWidget::appendTextUtf8(QByteArrayView text)
{
    addText(static_cast<sptr_t>(text.size()), text.data());
}

bool EditorWidget::beginFileLoad(LargeFileMode mode, qint64 expectedUtf8Bytes)
{
    if (!replaceDocument(mode, expectedUtf8Bytes)) {
        return false;
    }
    setEnabled(false);
    setReadOnly(false);
    setUndoCollection(false);
    clearAll();
    return true;
}

bool EditorWidget::resetDocument()
{
    if (!replaceDocument(LargeFileMode::Normal)) {
        return false;
    }
    setReadOnly(false);
    setUndoCollection(false);
    clearAll();
    emptyUndoBuffer();
    setUndoCollection(true);
    setSavePoint();
    setEnabled(true);
    refreshLineNumberMargin();
    return true;
}

void EditorWidget::completeFileLoad(LineEnding lineEnding)
{
    const Scintilla::EndOfLine scintillaEol = [lineEnding] {
        switch (lineEnding) {
        case LineEnding::CrLf:
            return Scintilla::EndOfLine::CrLf;
        case LineEnding::Cr:
            return Scintilla::EndOfLine::Cr;
        case LineEnding::Lf:
        case LineEnding::Mixed:
            return Scintilla::EndOfLine::Lf;
        case LineEnding::None:
#if defined(Q_OS_WIN)
            return Scintilla::EndOfLine::CrLf;
#else
            return Scintilla::EndOfLine::Lf;
#endif
        }
        return Scintilla::EndOfLine::Lf;
    }();

    setEOLMode(static_cast<sptr_t>(scintillaEol));
    emptyUndoBuffer();
    setUndoCollection(true);
    setSavePoint();
    setEnabled(true);
    refreshLineNumberMargin();
}

void EditorWidget::markSaved()
{
    setSavePoint();
}

bool EditorWidget::isEmpty() const
{
    return textLength() == 0;
}

LargeFileMode EditorWidget::largeFileMode() const noexcept
{
    return largeFileMode_;
}

int EditorWidget::documentOptionFlags() const
{
    return static_cast<int>(documentOptions());
}

sptr_t EditorWidget::retainCurrentDocument()
{
    const sptr_t document = docPointer();
    addRefDocument(document);
    return document;
}

sptr_t EditorWidget::createTabDocument()
{
    return createDocument(
        0, static_cast<sptr_t>(Scintilla::DocumentOption::Default));
}

void EditorWidget::activateTabDocument(sptr_t document, LargeFileMode mode)
{
    if (document == 0) {
        return;
    }
    setDocPointer(document);
    setCodePage(Scintilla::CpUtf8);
    setILexer(0);
    largeFileMode_ = mode;
    refreshLineNumberMargin();
    emitCursorPosition();
    viewport()->update();
}

void EditorWidget::releaseTabDocument(sptr_t document)
{
    if (document != 0) {
        releaseDocument(document);
    }
}

void EditorWidget::setEditorFont(const QFont& font)
{
    editorFont_ = font;
    const QByteArray family = editorFont_.family().toUtf8();
    const auto defaultStyle = styleIndex(Scintilla::StylesCommon::Default);
    styleSetFont(defaultStyle, family.constData());
    styleSetSizeFractional(defaultStyle,
                           static_cast<sptr_t>(editorFont_.pointSizeF() * 100.0));
    styleSetWeight(defaultStyle, editorFont_.bold()
        ? static_cast<sptr_t>(Scintilla::FontWeight::Bold)
        : static_cast<sptr_t>(Scintilla::FontWeight::Normal));
    styleSetItalic(defaultStyle, editorFont_.italic());
    styleClearAll();
    restoreLineNumberStyle();
    refreshLineNumberMargin();
    updateGeometry();
}

void EditorWidget::setTextColor(const QColor& color)
{
    textColor_ = color;
    styleSetFore(styleIndex(Scintilla::StylesCommon::Default),
                 scintillaRgbaStyleColor(textColor_));
    styleClearAll();
    restoreLineNumberStyle();
}

void EditorWidget::setBackgroundColor(const QColor& color)
{
    backgroundColor_ = color;
    styleSetBack(styleIndex(Scintilla::StylesCommon::Default),
                 scintillaRgbaStyleColor(backgroundColor_));
    styleClearAll();
    restoreLineNumberStyle();
    setBufferedDraw(backgroundColor_.alpha() == 255);
    viewport()->update();
}

void EditorWidget::paintEvent(QPaintEvent* event)
{
    if (backgroundColor_.alpha() < 255) {
        // Scintilla paints directly onto the viewport. With a translucent
        // background its normal SourceOver fill cannot erase glyphs already
        // present in Qt's backing store, which makes old text survive the
        // scroll blit and overlap the newly painted lines. Clear only the
        // damaged area first so the following Scintilla paint starts from a
        // transparent surface without giving up its efficient scroll path.
        QPainter clearPainter(viewport());
        clearPainter.setCompositionMode(QPainter::CompositionMode_Source);
        clearPainter.fillRect(event->rect(), Qt::transparent);
    }

    ScintillaEdit::paintEvent(event);

    // Scintilla can leave the final logical pixel beside a DPI-scaled scroll
    // bar untouched after a resize. Paint that unused edge last so it has the
    // configured background (including alpha) instead of exposing the black
    // native translucent surface on Windows.
    const int rightEdge = viewport()->width() - 1;
    if (rightEdge >= 0 && event->rect().right() >= rightEdge) {
        QPainter seamPainter(viewport());
        seamPainter.setCompositionMode(QPainter::CompositionMode_Source);
        seamPainter.fillRect(rightEdge, event->rect().top(), 1,
                             event->rect().height(), backgroundColor_);
    }
}

void EditorWidget::setCursorColor(const QColor& color)
{
    cursorColor_ = color;
    cursorColor_.setAlpha(255);
    setCaretFore(scintillaColor(cursorColor_));
}

void EditorWidget::setSelectionTextColor(const QColor& color)
{
    selectionTextColor_ = color;
    selectionTextColor_.setAlpha(255);
    setSelFore(true, scintillaColor(selectionTextColor_));
}

const QFont& EditorWidget::editorFont() const noexcept
{
    return editorFont_;
}

const QColor& EditorWidget::textColor() const noexcept
{
    return textColor_;
}

const QColor& EditorWidget::backgroundColor() const noexcept
{
    return backgroundColor_;
}

const QColor& EditorWidget::cursorColor() const noexcept
{
    return cursorColor_;
}

const QColor& EditorWidget::selectionTextColor() const noexcept
{
    return selectionTextColor_;
}

void EditorWidget::setWordWrapEnabled(bool enabled)
{
    setWrapMode(static_cast<sptr_t>(enabled ? Scintilla::Wrap::Word
                                            : Scintilla::Wrap::None));
    setHScrollBar(!enabled);
}

bool EditorWidget::isWordWrapEnabled() const
{
    return wrapMode() != static_cast<sptr_t>(Scintilla::Wrap::None);
}

void EditorWidget::setLineNumbersVisible(bool visible)
{
    lineNumbersVisible_ = visible;
    refreshLineNumberMargin();
}

bool EditorWidget::areLineNumbersVisible() const
{
    return lineNumbersVisible_;
}

void EditorWidget::refreshScrollBarLayout()
{
    const bool horizontalVisible = hScrollBar();
    const bool verticalVisible = vScrollBar();
    if (horizontalVisible) {
        setHScrollBar(false);
        setHScrollBar(true);
    }
    if (verticalVisible) {
        setVScrollBar(false);
        setVScrollBar(true);
    }
    updateGeometry();
    verticalScrollBar()->updateGeometry();
    horizontalScrollBar()->updateGeometry();
    viewport()->update();
    update();
}

qint64 EditorWidget::documentLength() const
{
    return static_cast<qint64>(textLength());
}

qint64 EditorWidget::selectionStartPosition() const
{
    return static_cast<qint64>(selectionStart());
}

qint64 EditorWidget::selectionEndPosition() const
{
    return static_cast<qint64>(selectionEnd());
}

qint64 EditorWidget::currentPosition() const
{
    return static_cast<qint64>(currentPos());
}

QByteArray EditorWidget::selectedTextUtf8() const
{
    return const_cast<EditorWidget*>(this)->getSelText();
}

SearchRange EditorWidget::findTextUtf8(QByteArrayView text, qint64 rangeStart,
                                       qint64 rangeEnd,
                                       const SearchOptions& options)
{
    if (text.isEmpty()) {
        return {};
    }

    Scintilla::FindOption flags = Scintilla::FindOption::None;
    if (options.matchCase) {
        flags |= Scintilla::FindOption::MatchCase;
    }
    if (options.wholeWord) {
        flags |= Scintilla::FindOption::WholeWord;
    }
    setSearchFlags(static_cast<sptr_t>(flags));
    setTargetRange(static_cast<sptr_t>(rangeStart),
                   static_cast<sptr_t>(rangeEnd));
    if (searchInTarget(static_cast<sptr_t>(text.size()), text.data()) < 0) {
        return {};
    }
    return {static_cast<qint64>(targetStart()),
            static_cast<qint64>(targetEnd())};
}

SearchRange EditorWidget::findTextUtf8Responsive(
    QByteArrayView text, qint64 rangeStart, qint64 rangeEnd,
    const SearchOptions& options)
{
    if (!LargeFilePolicy::usesLargeDocument(largeFileMode_)
        || text.isEmpty()) {
        return findTextUtf8(text, rangeStart, rangeEnd, options);
    }

    const qint64 overlap = std::max<qint64>(0, text.size() - 1);
    const qint64 sliceSize = std::max(
        LargeFilePolicy::responsiveSearchSlice, overlap + 1);
    const auto keepUiResponsive = [] {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };

    if (rangeStart <= rangeEnd) {
        qint64 sliceStart = rangeStart;
        while (sliceStart < rangeEnd) {
            const qint64 sliceEnd = std::min(rangeEnd, sliceStart + sliceSize);
            const SearchRange match = findTextUtf8(
                text, sliceStart, sliceEnd, options);
            if (match.isValid() || sliceEnd == rangeEnd) {
                return match;
            }
            sliceStart = std::max(sliceStart + 1, sliceEnd - overlap);
            keepUiResponsive();
        }
    } else {
        qint64 sliceStart = rangeStart;
        while (sliceStart > rangeEnd) {
            const qint64 sliceEnd = std::max(rangeEnd, sliceStart - sliceSize);
            const SearchRange match = findTextUtf8(
                text, sliceStart, sliceEnd, options);
            if (match.isValid() || sliceEnd == rangeEnd) {
                return match;
            }
            sliceStart = std::min(sliceStart - 1, sliceEnd + overlap);
            keepUiResponsive();
        }
    }
    return {};
}

void EditorWidget::selectSearchRange(const SearchRange& range)
{
    if (!range.isValid()) {
        return;
    }
    setSel(static_cast<sptr_t>(range.start), static_cast<sptr_t>(range.end));
    scrollCaret();
    QWidget::setFocus();
}

void EditorWidget::replaceSelectionUtf8(QByteArrayView replacement)
{
    setTargetRange(selectionStart(), selectionEnd());
    replaceTarget(static_cast<sptr_t>(replacement.size()), replacement.data());
    setSel(targetEnd(), targetEnd());
}

qsizetype EditorWidget::replaceAllUtf8(QByteArrayView text,
                                       QByteArrayView replacement,
                                       const SearchOptions& options)
{
    if (text.isEmpty()) {
        return 0;
    }

    qsizetype replacements = 0;
    qint64 position = 0;
    beginUndoAction();
    while (position <= documentLength()) {
        const SearchRange match = findTextUtf8(
            text, position, documentLength(), options);
        if (!match.isValid()) {
            break;
        }
        setTargetRange(static_cast<sptr_t>(match.start),
                       static_cast<sptr_t>(match.end));
        const sptr_t insertedLength = replaceTarget(
            static_cast<sptr_t>(replacement.size()), replacement.data());
        position = match.start + static_cast<qint64>(insertedLength);
        ++replacements;
    }
    endUndoAction();
    return replacements;
}

qint64 EditorWidget::editorLineCount() const
{
    return static_cast<qint64>(lineCount());
}

qint64 EditorWidget::currentOneBasedLine() const
{
    auto* self = const_cast<EditorWidget*>(this);
    return static_cast<qint64>(self->lineFromPosition(self->currentPos()) + 1);
}

bool EditorWidget::goToOneBasedLine(qint64 line)
{
    if (line < 1 || line > editorLineCount()) {
        return false;
    }
    gotoLine(static_cast<sptr_t>(line - 1));
    scrollCaret();
    QWidget::setFocus();
    return true;
}

QVector<EditHistoryEntry> EditorWidget::editHistory() const
{
    QVector<EditHistoryEntry> entries;
    const int actionCount = static_cast<int>(undoActions());
    if (actionCount <= 0) {
        return entries;
    }

    EditHistoryEntry entry;
    entry.position = std::numeric_limits<qint64>::max();
    QByteArray previewText;
    for (int action = 0; action < actionCount; ++action) {
        const int type = static_cast<int>(undoActionType(action));
        const int kind = type & undoActionKindMask;
        const qint64 textLength = static_cast<qint64>(
            send(SCI_GETUNDOACTIONTEXT, static_cast<uptr_t>(action), 0));
        entry.position = std::min(
            entry.position,
            static_cast<qint64>(undoActionPosition(action)));
        if (kind == undoActionInsert) {
            entry.insertedBytes += textLength;
        } else if (kind == undoActionDelete) {
            entry.deletedBytes += textLength;
        }
        constexpr qsizetype maximumPreviewBytes = 192;
        const qsizetype available = maximumPreviewBytes - previewText.size();
        if (textLength > 0 && textLength <= available) {
            previewText.append(undoActionText(action));
        }

        const bool completesEntry =
            (type & undoActionMayCoalesce) == 0 || action + 1 == actionCount;
        if (!completesEntry) {
            continue;
        }

        entry.undoPosition = action + 1;
        entry.preview = historyPreview(previewText);
        if (entry.insertedBytes > 0 && entry.deletedBytes > 0) {
            entry.kind = EditHistoryKind::Replace;
        } else if (entry.insertedBytes > 0) {
            entry.kind = EditHistoryKind::Insert;
        } else if (entry.deletedBytes > 0) {
            entry.kind = EditHistoryKind::Delete;
        }
        if (entry.position == std::numeric_limits<qint64>::max()) {
            entry.position = 0;
        }
        entries.append(entry);
        entry = {};
        entry.position = std::numeric_limits<qint64>::max();
        previewText.clear();
    }
    return entries;
}

int EditorWidget::currentEditHistoryPosition() const
{
    return static_cast<int>(undoCurrent());
}

int EditorWidget::savedEditHistoryPosition() const
{
    return static_cast<int>(undoSavePoint());
}

bool EditorWidget::restoreEditHistoryPosition(int undoPosition)
{
    if (undoPosition < 0
        || static_cast<sptr_t>(undoPosition) > undoActions()) {
        return false;
    }

    while (undoCurrent() > undoPosition && canUndo()) {
        undo();
    }
    while (undoCurrent() < undoPosition && canRedo()) {
        redo();
    }
    const bool restored = undoCurrent() == undoPosition;
    if (restored) {
        scrollCaret();
        QWidget::setFocus();
        emit editHistoryChanged();
    }
    return restored;
}

QSize EditorWidget::minimumSizeHint() const
{
    const QFontMetricsF metrics(editorFont_);
    return QSize(40, static_cast<int>(std::ceil(metrics.height())) + 4);
}

void EditorWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    auto* undoAction = menu.addAction(tr("Undo"), this, &ScintillaEdit::undo);
    auto* redoAction = menu.addAction(tr("Redo"), this, &ScintillaEdit::redo);
    undoAction->setEnabled(canUndo());
    redoAction->setEnabled(canRedo());
    menu.addSeparator();

    const bool hasSelection = selectionStart() != selectionEnd();
    auto* cutAction = menu.addAction(tr("Cut"), this, &ScintillaEdit::cut);
    auto* copyAction = menu.addAction(tr("Copy"), this, &ScintillaEdit::copy);
    auto* pasteAction = menu.addAction(tr("Paste"), this, &ScintillaEdit::paste);
    cutAction->setEnabled(hasSelection);
    copyAction->setEnabled(hasSelection);
    pasteAction->setEnabled(canPaste());
    menu.addSeparator();
    menu.addAction(tr("Select All"), this, &ScintillaEdit::selectAll);
    menu.addSeparator();
    menu.addAction(tr("Find…"), this, &EditorWidget::findRequested);
    menu.exec(event->globalPos());
}

void EditorWidget::wheelEvent(QWheelEvent* event)
{
    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        controlWheelDelta_ = 0;
        ScintillaEdit::wheelEvent(event);
        return;
    }

    const int verticalDelta = event->angleDelta().y();
    if (verticalDelta != 0) {
        controlWheelDelta_ += verticalDelta;
        const int steps = controlWheelDelta_ / 120;
        controlWheelDelta_ %= 120;
        if (steps != 0) {
            emit fontSizeAdjustmentRequested(steps);
        }
    }
    event->accept();
}

sptr_t EditorWidget::scintillaColor(const QColor& color)
{
    return static_cast<sptr_t>(color.red() | (color.green() << 8)
                               | (color.blue() << 16));
}

sptr_t EditorWidget::scintillaRgbaStyleColor(const QColor& color)
{
    constexpr quint64 rgbaMarker = quint64{1} << 32;
    const quint64 rgba = static_cast<quint64>(color.red())
        | (static_cast<quint64>(color.green()) << 8)
        | (static_cast<quint64>(color.blue()) << 16)
        | (static_cast<quint64>(color.alpha()) << 24);
    return static_cast<sptr_t>(rgbaMarker | rgba);
}

void EditorWidget::restoreLineNumberStyle()
{
    const auto lineNumberStyle =
        styleIndex(Scintilla::StylesCommon::LineNumber);
    if (textColor_.isValid()) {
        styleSetFore(lineNumberStyle,
                     scintillaRgbaStyleColor(textColor_));
    }
    if (backgroundColor_.isValid()) {
        styleSetBack(lineNumberStyle,
                     scintillaRgbaStyleColor(backgroundColor_));
    }
}

bool EditorWidget::replaceDocument(LargeFileMode mode, qint64 initialBytes)
{
    Scintilla::DocumentOption options = Scintilla::DocumentOption::Default;
    if (LargeFilePolicy::usesLargeDocument(mode)) {
        options = Scintilla::DocumentOption::TextLarge
            | Scintilla::DocumentOption::StylesNone;
    }

    const qint64 maximumBytes = std::numeric_limits<sptr_t>::max();
    const qint64 editReserve = initialBytes > 0
        ? std::min(LargeFilePolicy::initialEditReserve,
                   maximumBytes - std::min(initialBytes, maximumBytes))
        : 0;
    const qint64 boundedBytes = std::clamp<qint64>(
        initialBytes, 0, maximumBytes) + editReserve;
    const sptr_t document = createDocument(static_cast<sptr_t>(boundedBytes),
                                           static_cast<sptr_t>(options));
    if (document == 0) {
        return false;
    }

    // SETDOCPOINTER takes its own reference. Release the creator's reference
    // immediately so the editor is the sole owner of this document.
    setDocPointer(document);
    releaseDocument(document);
    setCodePage(Scintilla::CpUtf8);
    setILexer(0);
    largeFileMode_ = mode;
    return true;
}

void EditorWidget::refreshLineNumberMargin()
{
    if (!lineNumbersVisible_) {
        setMarginWidthN(0, 0);
        return;
    }

    const auto lines = std::max<sptr_t>(lineCount(), 1);
    const auto digits = std::max<std::size_t>(3, std::to_string(lines).size());
    const std::string sample(digits, '9');
    const auto width = textWidth(styleIndex(Scintilla::StylesCommon::LineNumber),
                                 sample.c_str()) + 12;
    setMarginWidthN(0, width);
}

void EditorWidget::emitCursorPosition()
{
    const auto position = currentPos();
    emit cursorPositionChanged(lineFromPosition(position) + 1,
                               column(position) + 1);
}

} // namespace vinson
