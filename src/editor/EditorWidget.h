#pragma once

#include <ScintillaEdit.h>

#include <QByteArray>
#include <QByteArrayView>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QString>
#include <QVector>

#include "file/FileTypes.h"
#include "largefile/LargeFilePolicy.h"
#include "search/SearchTypes.h"

class QContextMenuEvent;
class QPaintEvent;
class QWheelEvent;

namespace vinson {

enum class EditHistoryKind {
    Insert,
    Delete,
    Replace,
    Other,
};

struct EditHistoryEntry {
    int undoPosition = 0;
    EditHistoryKind kind = EditHistoryKind::Other;
    qint64 position = 0;
    qint64 insertedBytes = 0;
    qint64 deletedBytes = 0;
    QString preview;
};

class EditorWidget final : public ScintillaEdit
{
    Q_OBJECT

public:
    explicit EditorWidget(QWidget* parent = nullptr);

    void setTextUtf8(QByteArrayView text);
    [[nodiscard]] QByteArray textUtf8() const;
    [[nodiscard]] QByteArray textRangeUtf8(qint64 start, qint64 length) const;
    void appendTextUtf8(QByteArrayView text);
    [[nodiscard]] bool beginFileLoad(LargeFileMode mode,
                                     qint64 expectedUtf8Bytes = 0);
    [[nodiscard]] bool resetDocument();
    void completeFileLoad(LineEnding lineEnding);
    void setInsertionLineEnding(LineEnding lineEnding);
    void convertLineEndings(LineEnding lineEnding);
    [[nodiscard]] LineEnding detectedLineEnding() const;
    void markSaved();
    void markRecovered();
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] LargeFileMode largeFileMode() const noexcept;
    [[nodiscard]] int documentOptionFlags() const;
    [[nodiscard]] sptr_t retainCurrentDocument();
    [[nodiscard]] sptr_t createTabDocument();
    void activateTabDocument(sptr_t document, LargeFileMode mode);
    void releaseTabDocument(sptr_t document);

    void setEditorFont(const QFont& font);
    void setTextColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setCursorColor(const QColor& color);
    void setSelectionTextColor(const QColor& color);
    void setSelectionBackgroundColor(const QColor& color);
    void setLineNumberColor(const QColor& color);
    void setCurrentLineColor(const QColor& color);
    void setCursorWidth(int width);
    void setLineSpacing(int spacing);
    [[nodiscard]] const QFont& editorFont() const noexcept;
    [[nodiscard]] const QColor& textColor() const noexcept;
    [[nodiscard]] const QColor& backgroundColor() const noexcept;
    [[nodiscard]] const QColor& cursorColor() const noexcept;
    [[nodiscard]] const QColor& selectionTextColor() const noexcept;
    [[nodiscard]] const QColor& selectionBackgroundColor() const noexcept;
    [[nodiscard]] const QColor& lineNumberColor() const noexcept;
    [[nodiscard]] const QColor& currentLineColor() const noexcept;
    [[nodiscard]] int cursorWidth() const noexcept;
    [[nodiscard]] int lineSpacing() const noexcept;
    void setWordWrapEnabled(bool enabled);
    [[nodiscard]] bool isWordWrapEnabled() const;
    void setLineNumbersVisible(bool visible);
    [[nodiscard]] bool areLineNumbersVisible() const;
    void refreshScrollBarLayout();

    [[nodiscard]] qint64 documentLength() const;
    [[nodiscard]] qint64 selectionStartPosition() const;
    [[nodiscard]] qint64 selectionEndPosition() const;
    [[nodiscard]] qint64 currentPosition() const;
    [[nodiscard]] QByteArray selectedTextUtf8() const;
    [[nodiscard]] SearchRange findTextUtf8(QByteArrayView text,
                                           qint64 rangeStart,
                                           qint64 rangeEnd,
                                           const SearchOptions& options);
    [[nodiscard]] SearchRange findTextUtf8Responsive(
        QByteArrayView text, qint64 rangeStart, qint64 rangeEnd,
        const SearchOptions& options);
    void selectSearchRange(const SearchRange& range);
    void replaceSelectionUtf8(QByteArrayView replacement);
    [[nodiscard]] qsizetype replaceAllUtf8(QByteArrayView text,
                                           QByteArrayView replacement,
                                           const SearchOptions& options);
    [[nodiscard]] qint64 editorLineCount() const;
    [[nodiscard]] qint64 currentOneBasedLine() const;
    bool goToOneBasedLine(qint64 line);

    [[nodiscard]] bool hasBookmarkAtLine(qint64 oneBasedLine) const;
    bool toggleBookmarkAtLine(qint64 oneBasedLine);
    void clearAllBookmarks();
    bool goToNextBookmark();
    bool goToPreviousBookmark();

    [[nodiscard]] QVector<EditHistoryEntry> editHistory() const;
    [[nodiscard]] int currentEditHistoryPosition() const;
    [[nodiscard]] int savedEditHistoryPosition() const;
    bool restoreEditHistoryPosition(int undoPosition);

    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void cursorPositionChanged(qsizetype line, qsizetype column);
    void documentModified(bool modified);
    void editHistoryChanged();
    void findRequested();
    void fontSizeAdjustmentRequested(int steps);
    void bookmarksChanged();
    void bookmarkToggled(qint64 oneBasedLine, bool enabled);
    void lineEndingChanged(vinson::LineEnding lineEnding);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    static sptr_t scintillaColor(const QColor& color);
    static sptr_t scintillaRgbaStyleColor(const QColor& color);
    void restoreLineNumberStyle();
    [[nodiscard]] bool replaceDocument(LargeFileMode mode,
                                       qint64 initialBytes = 0);
    void refreshLineNumberMargin();
    void emitCursorPosition();
    bool goToBookmark(bool forward);
    void trackLineEndingEdit(qint64 position, const QByteArray& text, bool inserted);
    struct LineEndingCounts {
        qint64 cr = 0;
        qint64 lf = 0;
        qint64 pairs = 0;
    };
    QHash<sptr_t, LineEndingCounts> lineEndingCounts_;

    bool lineNumbersVisible_ = true;
    QFont editorFont_;
    QColor textColor_;
    QColor backgroundColor_;
    QColor cursorColor_;
    QColor selectionTextColor_;
    QColor selectionBackgroundColor_;
    QColor lineNumberColor_;
    QColor currentLineColor_;
    int cursorWidth_ = 2;
    int lineSpacing_ = 0;
    LargeFileMode largeFileMode_ = LargeFileMode::Normal;
    int controlWheelDelta_ = 0;
};

} // namespace vinson
