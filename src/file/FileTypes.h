#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QDateTime>
#include <QString>

namespace vinson {

inline constexpr qint64 maximumContentFingerprintBytes = 8 * 1024 * 1024;

enum class TextEncoding {
    Ascii,
    Utf8,
    Utf8Bom,
    Utf16Le,
    Utf16Be,
};

enum class LineEnding {
    None,
    Lf,
    CrLf,
    Cr,
    Mixed,
};

struct FileLoadInfo {
    QString path;
    TextEncoding encoding = TextEncoding::Utf8;
    LineEnding lineEnding = LineEnding::None;
    qint64 fileSize = 0;
    QDateTime modifiedAt;
    QByteArray contentHash;
};

struct FileSaveResult {
    QString path;
    TextEncoding encoding = TextEncoding::Utf8;
    qint64 fileSize = 0;
};

[[nodiscard]] QString encodingName(TextEncoding encoding);
[[nodiscard]] QString lineEndingName(LineEnding lineEnding);

} // namespace vinson

Q_DECLARE_METATYPE(vinson::FileLoadInfo)
Q_DECLARE_METATYPE(vinson::FileSaveResult)
