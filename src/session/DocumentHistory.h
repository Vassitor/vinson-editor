#pragma once

#include <QString>
#include <QStringList>

namespace vinson {

class DocumentHistory final
{
public:
    explicit DocumentHistory(qsizetype maximumRecentFiles = 10,
                             qsizetype maximumClosedTabs = 10);

    void setRecentFiles(const QStringList& paths);
    [[nodiscard]] const QStringList& recentFiles() const noexcept;
    void addRecentFile(const QString& path);
    bool removeRecentFile(const QString& path);
    void clearRecentFiles() noexcept;

    void recordClosedTab(const QString& path);
    [[nodiscard]] bool hasClosedTabs() const noexcept;
    [[nodiscard]] QString takeLastClosedTab();

private:
    [[nodiscard]] static QString normalizedPath(const QString& path);
    [[nodiscard]] static Qt::CaseSensitivity pathCaseSensitivity() noexcept;
    static void addMostRecent(QStringList& paths, const QString& path,
                              qsizetype maximumSize);

    qsizetype maximumRecentFiles_ = 10;
    qsizetype maximumClosedTabs_ = 10;
    QStringList recentFiles_;
    QStringList closedTabs_;
};

} // namespace vinson
