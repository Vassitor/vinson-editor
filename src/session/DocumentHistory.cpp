#include "session/DocumentHistory.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace vinson {

DocumentHistory::DocumentHistory(qsizetype maximumRecentFiles,
                                 qsizetype maximumClosedTabs)
    : maximumRecentFiles_(std::max<qsizetype>(0, maximumRecentFiles))
    , maximumClosedTabs_(std::max<qsizetype>(0, maximumClosedTabs))
{
}

void DocumentHistory::setRecentFiles(const QStringList& paths)
{
    recentFiles_.clear();
    if (maximumRecentFiles_ == 0) {
        return;
    }
    for (const QString& path : paths) {
        const QString normalized = normalizedPath(path);
        if (normalized.isEmpty()
            || recentFiles_.contains(normalized, pathCaseSensitivity())) {
            continue;
        }
        recentFiles_.append(normalized);
        if (recentFiles_.size() == maximumRecentFiles_) {
            break;
        }
    }
}

const QStringList& DocumentHistory::recentFiles() const noexcept
{
    return recentFiles_;
}

void DocumentHistory::addRecentFile(const QString& path)
{
    addMostRecent(recentFiles_, normalizedPath(path), maximumRecentFiles_);
}

bool DocumentHistory::removeRecentFile(const QString& path)
{
    const QString normalized = normalizedPath(path);
    const qsizetype oldSize = recentFiles_.size();
    recentFiles_.removeIf([&normalized](const QString& recentPath) {
        return recentPath.compare(normalized, pathCaseSensitivity()) == 0;
    });
    return recentFiles_.size() != oldSize;
}

void DocumentHistory::clearRecentFiles() noexcept
{
    recentFiles_.clear();
}

void DocumentHistory::recordClosedTab(const QString& path)
{
    addMostRecent(closedTabs_, normalizedPath(path), maximumClosedTabs_);
}

bool DocumentHistory::hasClosedTabs() const noexcept
{
    return !closedTabs_.isEmpty();
}

QString DocumentHistory::takeLastClosedTab()
{
    return closedTabs_.isEmpty() ? QString{} : closedTabs_.takeFirst();
}

QString DocumentHistory::normalizedPath(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

Qt::CaseSensitivity DocumentHistory::pathCaseSensitivity() noexcept
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

void DocumentHistory::addMostRecent(QStringList& paths, const QString& path,
                                    qsizetype maximumSize)
{
    if (path.isEmpty() || maximumSize == 0) {
        return;
    }
    paths.removeIf([&path](const QString& existingPath) {
        return existingPath.compare(path, pathCaseSensitivity()) == 0;
    });
    paths.prepend(path);
    if (paths.size() > maximumSize) {
        paths.resize(maximumSize);
    }
}

} // namespace vinson
