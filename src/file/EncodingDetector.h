#pragma once

#include "file/FileTypes.h"

#include <QByteArrayView>

namespace vinson {

struct EncodingDetection {
    TextEncoding encoding = TextEncoding::Utf8;
    qsizetype bomLength = 0;
};

class EncodingDetector final
{
public:
    [[nodiscard]] static EncodingDetection detect(QByteArrayView prefix);
    [[nodiscard]] static bool isAscii(QByteArrayView data) noexcept;
    [[nodiscard]] static LineEnding detectLineEnding(QByteArrayView utf8Text) noexcept;
};

} // namespace vinson
