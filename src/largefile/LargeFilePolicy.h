#pragma once

#include <QtGlobal>

class QString;

namespace vinson {

enum class LargeFileMode {
    Normal,
    Large,
    VeryLarge,
};

class LargeFilePolicy final
{
public:
    static constexpr qint64 mebibyte = 1024 * 1024;
    static constexpr qint64 largeFileThreshold = 64 * mebibyte;
    static constexpr qint64 veryLargeFileThreshold = 512 * mebibyte;
    static constexpr qint64 responsiveSearchSlice = 8 * mebibyte;
    static constexpr qint64 initialEditReserve = mebibyte;

    [[nodiscard]] static constexpr LargeFileMode modeForSize(
        qint64 fileSize) noexcept
    {
        if (fileSize >= veryLargeFileThreshold) {
            return LargeFileMode::VeryLarge;
        }
        if (fileSize >= largeFileThreshold) {
            return LargeFileMode::Large;
        }
        return LargeFileMode::Normal;
    }

    [[nodiscard]] static constexpr bool usesLargeDocument(
        LargeFileMode mode) noexcept
    {
        return mode != LargeFileMode::Normal;
    }

    [[nodiscard]] static constexpr bool defaultsWordWrapOff(
        LargeFileMode mode) noexcept
    {
        return usesLargeDocument(mode);
    }

    [[nodiscard]] static constexpr bool requiresReplaceAllConfirmation(
        LargeFileMode mode) noexcept
    {
        return mode == LargeFileMode::VeryLarge;
    }

    [[nodiscard]] static QString displayName(LargeFileMode mode);
};

} // namespace vinson
