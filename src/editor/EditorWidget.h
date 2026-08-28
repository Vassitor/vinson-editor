#pragma once

#include <ScintillaEdit.h>

#include <QByteArray>
#include <QByteArrayView>
#include <QColor>
#include <QFont>

#include "file/FileTypes.h"
#include "largefile/LargeFilePolicy.h"
#include "search/SearchTypes.h"

class QContextMenuEvent;

namespace vinson {

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
    void markSaved();
    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] LargeFileMode largeFileMode() const noexcept;
    [[nodiscard]] int documentOptionFlags() const;

    void setEditorFont(const QFont& font);
    void setTextColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setCursorColor(const QColor& color);
    void setSelectionTextColor(const QColor& color);
    [[nodiscard]] const QFont& editorFont() const noexcept;
    [[nodiscard]] const QColor& textColor() const noexcept;
    [[nodiscard]] const QColor& backgroundColor() const noexcept;
    [[nodiscard]] const QColor& cursorColor() const noexcept;
    [[nodiscard]] const QColor& selectionTextColor() const noexcept;
    void setWordWrapEnabled(bool enabled);
    [[nodiscard]] bool isWordWrapEnabled() const;
    void setLineNumbersVisible(bool visible);
    [[nodiscard]] bool areLineNumbersVisible() const;

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

    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void cursorPositionChanged(qsizetype line, qsizetype column);
    void documentModified(bool modified);
    void findRequested();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    static sptr_t scintillaColor(const QColor& color);
    static sptr_t scintillaRgbaStyleColor(const QColor& color);
    [[nodiscard]] bool replaceDocument(LargeFileMode mode,
                                       qint64 initialBytes = 0);
    void refreshLineNumberMargin();
    void emitCursorPosition();

    bool lineNumbersVisible_ = true;
    QFont editorFont_;
    QColor textColor_;
    QColor backgroundColor_;
    QColor cursorColor_;
    QColor selectionTextColor_;
    LargeFileMode largeFileMode_ = LargeFileMode::Normal;
};

} // namespace vinson
