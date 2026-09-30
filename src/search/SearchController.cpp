#include "search/SearchController.h"

#include "editor/EditorWidget.h"

#include <QTimer>

#include <algorithm>
#include <utility>

namespace vinson {

SearchController::SearchController(EditorWidget* editor, QObject* parent)
    : QObject(parent)
    , editor_(editor)
    , searchTimer_(new QTimer(this))
{
    Q_ASSERT(editor_ != nullptr);
    qRegisterMetaType<SearchResult>();
    searchTimer_->setSingleShot(true);
    searchTimer_->setInterval(1);
    connect(searchTimer_, &QTimer::timeout,
            this, &SearchController::searchNextSlice);
    // Programmatic edits must also invalidate positions captured by a search.
    connect(editor_, &EditorWidget::editHistoryChanged,
            this, &SearchController::cancelSearch);
}

void SearchController::setSearchText(const QString& text)
{
    if (searchText_ != text) {
        cancelSearch();
    }
    searchText_ = text;
}

void SearchController::setReplacementText(const QString& text)
{
    replacementText_ = text;
}

void SearchController::setOptions(const SearchOptions& options)
{
    if (options_.matchCase != options.matchCase
        || options_.wholeWord != options.wholeWord
        || options_.wrapAround != options.wrapAround) {
        cancelSearch();
    }
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

bool SearchController::isSearching() const noexcept
{
    return pendingSearch_.has_value();
}

void SearchController::cancelSearch()
{
    if (isSearching()) {
        finishSearch(SearchResult::Cancelled);
    }
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
    if (isSearching()) {
        return false;
    }
    if (searchText_.isEmpty()) {
        publish(SearchResult::EmptyQuery, tr("Enter text to find."));
        return false;
    }
    if (!selectionMatchesQuery()) {
        const SearchResult result = findNext();
        return result == SearchResult::Found || result == SearchResult::Wrapped
            || result == SearchResult::Searching;
    }

    const QByteArray replacement = replacementText_.toUtf8();
    editor_->replaceSelectionUtf8(replacement);
    findNext();
    return true;
}

qsizetype SearchController::replaceAll()
{
    if (isSearching()) {
        return 0;
    }
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
    if (isSearching()) {
        return false;
    }
    const bool moved = editor_->goToOneBasedLine(oneBasedLine);
    publish(moved ? SearchResult::Found : SearchResult::NotFound,
            moved ? tr("Moved to line %1.").arg(oneBasedLine)
                  : tr("Line %1 is outside the document.").arg(oneBasedLine));
    return moved;
}

SearchResult SearchController::find(SearchDirection direction)
{
    if (isSearching()) {
        return SearchResult::Searching;
    }
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

    if (LargeFilePolicy::usesLargeDocument(editor_->largeFileMode())) {
        PendingSearch search;
        search.query = query;
        search.options = options_;
        search.direction = direction;
        search.document = editor_->docPointer();
        search.documentEnd = documentEnd;
        search.origin = origin;
        search.position = origin;
        search.rangeStart = origin;
        search.rangeEnd = direction == SearchDirection::Forward ? documentEnd : 0;
        search.totalBytes = options_.wrapAround ? documentEnd
            : (direction == SearchDirection::Forward ? documentEnd - origin : origin);
        pendingSearch_ = std::move(search);
        publish(SearchResult::Searching, tr("Searching… Press Esc to cancel."));
        emit searchingChanged(true);
        emit progressChanged(0);
        if (isSearching()) {
            searchTimer_->start();
        }
        return SearchResult::Searching;
    }

    SearchRange match = direction == SearchDirection::Forward
        ? editor_->findTextUtf8(query, origin, documentEnd, options_)
        : editor_->findTextUtf8(query, origin, 0, options_);
    SearchResult result = SearchResult::Found;
    if (!match.isValid() && options_.wrapAround) {
        match = direction == SearchDirection::Forward
            ? editor_->findTextUtf8(query, 0, origin, options_)
            : editor_->findTextUtf8(query, documentEnd, origin,
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

void SearchController::searchNextSlice()
{
    if (!isSearching()) {
        return;
    }
    PendingSearch& search = *pendingSearch_;
    if (editor_->docPointer() != search.document
        || editor_->documentLength() != search.documentEnd) {
        cancelSearch();
        return;
    }

    const bool forward = search.direction == SearchDirection::Forward;
    const qint64 overlap = std::max<qint64>(0, search.query.size() - 1);
    const qint64 sliceSize = std::max(
        LargeFilePolicy::responsiveSearchSlice, overlap + 1);
    const qint64 sliceEnd = forward
        ? std::min(search.rangeEnd, search.position + sliceSize)
        : std::max(search.rangeEnd, search.position - sliceSize);
    const SearchRange match = editor_->findTextUtf8(
        search.query, search.position, sliceEnd, search.options);
    if (match.isValid()) {
        finishSearch(search.wrapped ? SearchResult::Wrapped : SearchResult::Found,
                     match);
        return;
    }

    const qint64 scannedBytes = search.completedBytes
        + (forward ? sliceEnd - search.rangeStart : search.rangeStart - sliceEnd);
    const int percent = search.totalBytes > 0
        ? std::clamp(static_cast<int>(100.0 * static_cast<double>(scannedBytes)
                                     / static_cast<double>(search.totalBytes)), 0, 100)
        : 100;
    if (sliceEnd == search.rangeEnd) {
        if (!search.options.wrapAround || search.wrapped) {
            finishSearch(SearchResult::NotFound);
            return;
        }
        search.wrapped = true;
        search.completedBytes = scannedBytes;
        search.position = forward ? 0 : search.documentEnd;
        search.rangeStart = search.position;
        search.rangeEnd = search.origin;
    } else {
        search.position = forward
            ? std::max(search.position + 1, sliceEnd - overlap)
            : std::min(search.position - 1, sliceEnd + overlap);
    }

    // No nested event loop: queued user input runs between bounded scans.
    emit progressChanged(percent);
    if (isSearching()) {
        searchTimer_->start();
    }
}

void SearchController::finishSearch(SearchResult result, const SearchRange& match)
{
    const QString query = QString::fromUtf8(pendingSearch_->query);
    searchTimer_->stop();
    pendingSearch_.reset();
    emit searchingChanged(false);
    if (match.isValid()) {
        editor_->selectSearchRange(match);
    }
    switch (result) {
    case SearchResult::Cancelled:
        publish(result, tr("Search cancelled."));
        break;
    case SearchResult::NotFound:
        publish(result, tr("No matches for “%1”.").arg(query));
        break;
    case SearchResult::Wrapped:
        publish(result, tr("Search wrapped at the document boundary."));
        break;
    default:
        publish(result, tr("Match found."));
        break;
    }
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
