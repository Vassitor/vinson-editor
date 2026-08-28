#include "largefile/LargeFilePolicy.h"

#include <QCoreApplication>
#include <QString>

namespace vinson {

QString LargeFilePolicy::displayName(LargeFileMode mode)
{
    switch (mode) {
    case LargeFileMode::Normal:
        return {};
    case LargeFileMode::Large:
        return QCoreApplication::translate("LargeFilePolicy",
                                           "Large File Mode");
    case LargeFileMode::VeryLarge:
        return QCoreApplication::translate("LargeFilePolicy",
                                           "Very Large File Mode");
    }
    return {};
}

} // namespace vinson
