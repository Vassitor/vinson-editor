#pragma once

#include <QLocale>

class QCoreApplication;
class QTranslator;

namespace vinson {

[[nodiscard]] bool installApplicationTranslation(
    QCoreApplication& application, QTranslator& translator,
    const QLocale& locale);

} // namespace vinson
