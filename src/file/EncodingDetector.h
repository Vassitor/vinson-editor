#pragma once

#include "file/FileTypes.h"

#include <QByteArrayView>

namespace vinson {

struct EncodingDetection {
    TextEncoding encoding = TextEncoding::Utf8;
    qsizetype bomLength = 0;
};

class LineEndingDetector final
{
public:
    void consume(QByteArrayView data) noexcept;
    [[nodiscard]] LineEnding result() const noexcept;

private:
    qint64 lfCount_ = 0;
    qint64 crlfCount_ = 0;
    qint64 crCount_ = 0;
    bool pendingCarriageReturn_ = false;
};

class EncodingDetector final
{
public:
    [[nodiscard]] static EncodingDetection detect(QByteArrayView prefix);
    [[nodiscard]] static bool isAscii(QByteArrayView data) noexcept;
    [[nodiscard]] static LineEnding detectLineEnding(QByteArrayView utf8Text) noexcept;
};

} // namespace vinson
