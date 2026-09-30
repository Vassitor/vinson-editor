#include "editor/EditorDocument.h"

#include <QFileInfo>

#include <algorithm>

namespace vinson {

void EditorDocument::reset()
{
    path_.clear();
    encoding_ = TextEncoding::Utf8;
    savedEncoding_ = encoding_;
    lineEnding_ = LineEnding::None;
    fileSize_ = 0;
    modified_ = false;
}

void EditorDocument::adoptLoadedFile(const FileLoadInfo& info)
{
    path_ = info.path;
    encoding_ = info.encoding;
    savedEncoding_ = encoding_;
    lineEnding_ = info.lineEnding;
    fileSize_ = info.fileSize;
    modified_ = false;
}

void EditorDocument::adoptSavedFile(const FileSaveResult& result)
{
    path_ = result.path;
    encoding_ = result.encoding;
    savedEncoding_ = encoding_;
    fileSize_ = result.fileSize;
    modified_ = false;
}

void EditorDocument::adoptRecoveredFile(const QString& path,
                                        TextEncoding encoding,
                                        LineEnding lineEnding,
                                        qint64 fileSize)
{
    path_ = path;
    encoding_ = encoding;
    savedEncoding_ = encoding_;
    lineEnding_ = lineEnding;
    fileSize_ = std::max<qint64>(0, fileSize);
    modified_ = true;
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
    return modified_ || encoding_ != savedEncoding_;
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

void EditorDocument::setEncoding(TextEncoding encoding) noexcept
{
    encoding_ = encoding;
}

void EditorDocument::setLineEnding(LineEnding lineEnding) noexcept
{
    lineEnding_ = lineEnding;
}

} // namespace vinson
