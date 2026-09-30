#pragma once

#include <QtGlobal>

namespace vinson {

enum class SearchDirection {
    Forward,
    Backward,
};

struct SearchOptions {
    bool matchCase = false;
    bool wholeWord = false;
    bool wrapAround = true;
};

struct SearchRange {
    qint64 start = -1;
    qint64 end = -1;

    [[nodiscard]] bool isValid() const noexcept
    {
        return start >= 0 && end >= start;
    }
};

enum class SearchResult {
    Found,
    Wrapped,
    NotFound,
    EmptyQuery,
    Searching,
    Cancelled,
};

} // namespace vinson
