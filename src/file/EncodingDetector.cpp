#include "file/EncodingDetector.h"

#include <algorithm>

namespace vinson {

void LineEndingDetector::consume(QByteArrayView data) noexcept
{
    for (const char value : data) {
        if (pendingCarriageReturn_) {
            if (value == '\n') {
                ++crlfCount_;
                pendingCarriageReturn_ = false;
                continue;
            }
            ++crCount_;
            pendingCarriageReturn_ = false;
        }

        if (value == '\r') {
            pendingCarriageReturn_ = true;
        } else if (value == '\n') {
            ++lfCount_;
        }
    }
}

LineEnding LineEndingDetector::result() const noexcept
{
    const qint64 crCount = crCount_ + (pendingCarriageReturn_ ? 1 : 0);
    const int kinds = (lfCount_ > 0 ? 1 : 0) + (crlfCount_ > 0 ? 1 : 0)
        + (crCount > 0 ? 1 : 0);
    if (kinds == 0) {
        return LineEnding::None;
    }
    if (kinds > 1) {
        return LineEnding::Mixed;
    }
    if (crlfCount_ > 0) {
        return LineEnding::CrLf;
    }
    return crCount > 0 ? LineEnding::Cr : LineEnding::Lf;
}

EncodingDetection EncodingDetector::detect(QByteArrayView prefix)
{
    if (prefix.size() >= 3
        && static_cast<unsigned char>(prefix[0]) == 0xEF
        && static_cast<unsigned char>(prefix[1]) == 0xBB
        && static_cast<unsigned char>(prefix[2]) == 0xBF) {
        return {TextEncoding::Utf8Bom, 3};
    }
    if (prefix.size() >= 2
        && static_cast<unsigned char>(prefix[0]) == 0xFF
        && static_cast<unsigned char>(prefix[1]) == 0xFE) {
        return {TextEncoding::Utf16Le, 2};
    }
    if (prefix.size() >= 2
        && static_cast<unsigned char>(prefix[0]) == 0xFE
        && static_cast<unsigned char>(prefix[1]) == 0xFF) {
        return {TextEncoding::Utf16Be, 2};
    }
    return {isAscii(prefix) ? TextEncoding::Ascii : TextEncoding::Utf8, 0};
}

bool EncodingDetector::isAscii(QByteArrayView data) noexcept
{
    return std::all_of(data.begin(), data.end(), [](char value) {
        return static_cast<unsigned char>(value) < 0x80;
    });
}

LineEnding EncodingDetector::detectLineEnding(QByteArrayView utf8Text) noexcept
{
    LineEndingDetector detector;
    detector.consume(utf8Text);
    return detector.result();
}

} // namespace vinson
