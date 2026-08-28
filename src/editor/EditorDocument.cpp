#include "editor/EditorDocument.h"

#include <QFileInfo>

namespace vinson {

void EditorDocument::reset()
{
    path_.clear();
    encoding_ = TextEncoding::Utf8;
    lineEnding_ = LineEnding::None;
    fileSize_ = 0;
    modified_ = false;
}

void EditorDocument::adoptLoadedFile(const FileLoadInfo& info)
{
    path_ = info.path;
    encoding_ = info.encoding;
    lineEnding_ = info.lineEnding;
    fileSize_ = info.fileSize;
    modified_ = false;
}

void EditorDocument::adoptSavedFile(const FileSaveResult& result)
{
    path_ = result.path;
    encoding_ = result.encoding;
    fileSize_ = result.fileSize;
    modified_ = false;
}

const QString& EditorDocument::path() const noexcept
{
    return path_;
}

QString EditorDocument::displayName() const
{
    return path_.isEmpty() ? QStringLiteral("Untitled")
                           : QFileInfo(path_).fileName();
}

bool EditorDocument::isUntitled() const noexcept
{
    return path_.isEmpty();
}

bool EditorDocument::isModified() const noexcept
{
    return modified_;
}

TextEncoding EditorDocument::encoding() const noexcept
{
    return encoding_;
}

LineEnding EditorDocument::lineEnding() const noexcept
{
    return lineEnding_;
}

qint64 EditorDocument::fileSize() const noexcept
{
    return fileSize_;
}

void EditorDocument::setModified(bool modified) noexcept
{
    modified_ = modified;
}

} // namespace vinson
