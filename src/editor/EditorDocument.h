#pragma once

#include "file/FileTypes.h"

#include <QString>

namespace vinson {

class EditorDocument final
{
public:
    void reset();
    void adoptLoadedFile(const FileLoadInfo& info);
    void adoptSavedFile(const FileSaveResult& result);

    [[nodiscard]] const QString& path() const noexcept;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool isUntitled() const noexcept;
    [[nodiscard]] bool isModified() const noexcept;
    [[nodiscard]] TextEncoding encoding() const noexcept;
    [[nodiscard]] LineEnding lineEnding() const noexcept;
    [[nodiscard]] qint64 fileSize() const noexcept;

    void setModified(bool modified) noexcept;

private:
    QString path_;
    TextEncoding encoding_ = TextEncoding::Utf8;
    LineEnding lineEnding_ = LineEnding::None;
    qint64 fileSize_ = 0;
    bool modified_ = false;
};

} // namespace vinson
