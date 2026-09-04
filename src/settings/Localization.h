#pragma once

#include <QLocale>

class QCoreApplication;
class QTranslator;

namespace vinson {

[[nodiscard]] bool installApplicationTranslation(
    QCoreApplication& application, QTranslator& applicationTranslator,
    QTranslator& qtTranslator,
    const QLocale& locale);

} // namespace vinson
