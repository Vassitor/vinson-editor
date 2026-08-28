#pragma once

#include "search/SearchTypes.h"

#include <QObject>
#include <QString>

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

public slots:
    SearchResult findNext();
    SearchResult findPrevious();
    bool replaceCurrent();
    qsizetype replaceAll();
    bool goToLine(qint64 oneBasedLine);

signals:
    void resultChanged(vinson::SearchResult result, QString message);

private:
    SearchResult find(SearchDirection direction);
    [[nodiscard]] bool selectionMatchesQuery();
    void publish(SearchResult result, const QString& message);

    EditorWidget* editor_ = nullptr;
    QString searchText_;
    QString replacementText_;
    SearchOptions options_;
};

} // namespace vinson

Q_DECLARE_METATYPE(vinson::SearchResult)
