#include "settings/Localization.h"

#include <QCoreApplication>
#include <QTranslator>

namespace vinson {

bool installApplicationTranslation(QCoreApplication& application,
                                   QTranslator& applicationTranslator,
                                   QTranslator& qtTranslator,
                                   const QLocale& locale)
{
    if (locale.language() != QLocale::Chinese) {
        return false;
    }

    if (!qtTranslator.load(QStringLiteral(
            ":/i18n/qt/qtbase_zh_CN.qm"))
        || !applicationTranslator.load(QStringLiteral(
            ":/i18n/vinson-editor_zh_CN.qm"))) {
        return false;
    }
    return application.installTranslator(&qtTranslator)
        && application.installTranslator(&applicationTranslator);
}

} // namespace vinson
