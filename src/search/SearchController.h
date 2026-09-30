#pragma once

#include "search/SearchTypes.h"

#include <QObject>
#include <QString>
#include <QByteArray>

#include <optional>

class QTimer;

namespace vinson {

class EditorWidget;

class SearchController final : public QObject
{
    Q_OBJECT

public:
    explicit SearchController(EditorWidget* editor, QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setReplacementText(const QString& text);
    void setOptions(const SearchOptions& options);

    [[nodiscard]] const QString& searchText() const noexcept;
    [[nodiscard]] const QString& replacementText() const noexcept;
    [[nodiscard]] const SearchOptions& options() const noexcept;
    [[nodiscard]] bool isSearching() const noexcept;

public slots:
    SearchResult findNext();
    SearchResult findPrevious();
    bool replaceCurrent();
    qsizetype replaceAll();
    bool goToLine(qint64 oneBasedLine);
    void cancelSearch();

signals:
    void resultChanged(vinson::SearchResult result, QString message);
    void searchingChanged(bool searching);
    void progressChanged(int percent);

private:
    struct PendingSearch {
        QByteArray query;
        SearchOptions options;
        SearchDirection direction = SearchDirection::Forward;
        qintptr document = 0;
        qint64 documentEnd = 0;
        qint64 origin = 0;
        qint64 position = 0;
        qint64 rangeStart = 0;
        qint64 rangeEnd = 0;
        qint64 completedBytes = 0;
        qint64 totalBytes = 0;
        bool wrapped = false;
    };

    SearchResult find(SearchDirection direction);
    void searchNextSlice();
    void finishSearch(SearchResult result, const SearchRange& match = {});
    [[nodiscard]] bool selectionMatchesQuery();
    void publish(SearchResult result, const QString& message);

    EditorWidget* editor_ = nullptr;
    QString searchText_;
    QString replacementText_;
    SearchOptions options_;
    QTimer* searchTimer_ = nullptr;
    std::optional<PendingSearch> pendingSearch_;
};

} // namespace vinson

Q_DECLARE_METATYPE(vinson::SearchResult)
