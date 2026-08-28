#include "search/SearchController.h"

#include "editor/EditorWidget.h"

namespace vinson {

SearchController::SearchController(EditorWidget* editor, QObject* parent)
    : QObject(parent)
    , editor_(editor)
{
    Q_ASSERT(editor_ != nullptr);
    qRegisterMetaType<SearchResult>();
}

void SearchController::setSearchText(const QString& text)
{
    searchText_ = text;
}

void SearchController::setReplacementText(const QString& text)
{
    replacementText_ = text;
}

void SearchController::setOptions(const SearchOptions& options)
{
    options_ = options;
}

const QString& SearchController::searchText() const noexcept
{
    return searchText_;
}

const QString& SearchController::replacementText() const noexcept
{
    return replacementText_;
}

const SearchOptions& SearchController::options() const noexcept
{
    return options_;
}

SearchResult SearchController::findNext()
{
    return find(SearchDirection::Forward);
}

SearchResult SearchController::findPrevious()
{
    return find(SearchDirection::Backward);
}

bool SearchController::replaceCurrent()
{
    if (searchText_.isEmpty()) {
        publish(SearchResult::EmptyQuery, tr("Enter text to find."));
        return false;
    }
    if (!selectionMatchesQuery()) {
        const SearchResult result = findNext();
        return result == SearchResult::Found || result == SearchResult::Wrapped;
    }

    const QByteArray replacement = replacementText_.toUtf8();
    editor_->replaceSelectionUtf8(replacement);
    findNext();
    return true;
}

qsizetype SearchController::replaceAll()
{
    if (searchText_.isEmpty()) {
        publish(SearchResult::EmptyQuery, tr("Enter text to find."));
        return 0;
    }

    const QByteArray query = searchText_.toUtf8();
    const QByteArray replacement = replacementText_.toUtf8();
    const qsizetype count = editor_->replaceAllUtf8(query, replacement, options_);
    publish(count > 0 ? SearchResult::Found : SearchResult::NotFound,
            tr("Replaced %n occurrence(s).", nullptr, count));
    return count;
}

bool SearchController::goToLine(qint64 oneBasedLine)
{
    const bool moved = editor_->goToOneBasedLine(oneBasedLine);
    publish(moved ? SearchResult::Found : SearchResult::NotFound,
            moved ? tr("Moved to line %1.").arg(oneBasedLine)
                  : tr("Line %1 is outside the document.").arg(oneBasedLine));
    return moved;
}

SearchResult SearchController::find(SearchDirection direction)
{
    const QByteArray query = searchText_.toUtf8();
    if (query.isEmpty()) {
        publish(SearchResult::EmptyQuery, tr("Enter text to find."));
        return SearchResult::EmptyQuery;
    }

    const qint64 selectionStart = editor_->selectionStartPosition();
    const qint64 selectionEnd = editor_->selectionEndPosition();
    const bool hasSelection = selectionStart != selectionEnd;
    const qint64 documentEnd = editor_->documentLength();
    const qint64 origin = direction == SearchDirection::Forward
        ? (hasSelection ? selectionEnd : editor_->currentPosition())
        : (hasSelection ? selectionStart : editor_->currentPosition());

    SearchRange match = direction == SearchDirection::Forward
        ? editor_->findTextUtf8Responsive(query, origin, documentEnd, options_)
        : editor_->findTextUtf8Responsive(query, origin, 0, options_);
    SearchResult result = SearchResult::Found;
    if (!match.isValid() && options_.wrapAround) {
        match = direction == SearchDirection::Forward
            ? editor_->findTextUtf8Responsive(query, 0, origin, options_)
            : editor_->findTextUtf8Responsive(query, documentEnd, origin,
                                              options_);
        result = SearchResult::Wrapped;
    }

    if (!match.isValid()) {
        publish(SearchResult::NotFound,
                tr("No matches for “%1”.").arg(searchText_));
        return SearchResult::NotFound;
    }

    editor_->selectSearchRange(match);
    publish(result, result == SearchResult::Wrapped
        ? tr("Search wrapped at the document boundary.")
        : tr("Match found."));
    return result;
}

bool SearchController::selectionMatchesQuery()
{
    const qint64 start = editor_->selectionStartPosition();
    const qint64 end = editor_->selectionEndPosition();
    if (start == end) {
        return false;
    }
    const QByteArray query = searchText_.toUtf8();
    const SearchRange match = editor_->findTextUtf8(query, start, end, options_);
    return match.start == start && match.end == end;
}

void SearchController::publish(SearchResult result, const QString& message)
{
    emit resultChanged(result, message);
}

} // namespace vinson
