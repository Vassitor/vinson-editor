#include "settings/Localization.h"

#include <QCoreApplication>
#include <QTranslator>

namespace vinson {

bool installApplicationTranslation(QCoreApplication& application,
                                   QTranslator& translator,
                                   const QLocale& locale)
{
    if (locale.language() != QLocale::Chinese) {
        return false;
    }

    if (!translator.load(QStringLiteral(
            ":/i18n/vinson-editor_zh_CN.qm"))) {
        return false;
    }
    return application.installTranslator(&translator);
}

} // namespace vinson
