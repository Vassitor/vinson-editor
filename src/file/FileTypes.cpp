#include "file/FileTypes.h"

namespace vinson {

QString encodingName(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Ascii:
        return QStringLiteral("ASCII");
    case TextEncoding::Utf8:
        return QStringLiteral("UTF-8");
    case TextEncoding::Utf8Bom:
        return QStringLiteral("UTF-8 BOM");
    case TextEncoding::Utf16Le:
        return QStringLiteral("UTF-16 LE");
    case TextEncoding::Utf16Be:
        return QStringLiteral("UTF-16 BE");
    }
    return QStringLiteral("Unknown");
}

QString lineEndingName(LineEnding lineEnding)
{
    switch (lineEnding) {
    case LineEnding::None:
        return QStringLiteral("—");
    case LineEnding::Lf:
        return QStringLiteral("LF");
    case LineEnding::CrLf:
        return QStringLiteral("CRLF");
    case LineEnding::Cr:
        return QStringLiteral("CR");
    case LineEnding::Mixed:
        return QStringLiteral("Mixed EOL");
    }
    return QStringLiteral("Unknown");
}

} // namespace vinson
