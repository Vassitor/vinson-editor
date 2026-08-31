#include "settings/SettingsManager.h"

#include "settings/ThemeManager.h"
#include "window/GlobalShortcut.h"

#include <QColor>
#include <QDir>
#include <QFontDatabase>
#include <QSettings>
#include <QVariant>

#include <cmath>

namespace vinson {
namespace {

constexpr int settingsSchemaVersion = 4;
constexpr qsizetype maximumGeometryBytes = 64 * 1024;

bool readBool(const QSettings& settings, const QString& key,
              bool defaultValue)
{
    const QVariant value = settings.value(key);
    if (!value.isValid()) {
        return defaultValue;
    }
    if (value.metaType().id() == QMetaType::Bool) {
        return value.toBool();
    }
    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1")) {
        return true;
    }
    if (text == QStringLiteral("false") || text == QStringLiteral("0")) {
        return false;
    }
    return defaultValue;
}

QColor readColor(const QSettings& settings, const QString& key,
                 const QColor& defaultValue)
{
    const QVariant value = settings.value(key);
    if (!value.isValid()) {
        return defaultValue;
    }
    const QColor color(value.toString());
    return color.isValid() ? color : defaultValue;
}

QString colorText(const QColor& color)
{
    return color.name(QColor::HexArgb);
}

} // namespace

SettingsManager::SettingsManager(QObject* parent)
    : SettingsManager(std::make_unique<QSettings>(), parent)
{
}

SettingsManager::SettingsManager(const QString& iniFilePath, QObject* parent)
    : SettingsManager(
          std::make_unique<QSettings>(iniFilePath, QSettings::IniFormat), parent)
{
}

SettingsManager::~SettingsManager() = default;

SettingsManager::SettingsManager(std::unique_ptr<QSettings> settings,
                                 QObject* parent)
    : QObject(parent)
    , settings_(std::move(settings))
{
    settings_->setAtomicSyncRequired(true);
}

ApplicationSettings SettingsManager::defaults()
{
    return {
        ThemeManager::defaultAppearance(),
        {},
        QDir::homePath(),
        true,
        true,
        false,
        false,
        GlobalShortcut::defaultShortcut(),
        GlobalShortcut::defaultFocusShortcut(),
    };
}

ApplicationSettings SettingsManager::load() const
{
    ApplicationSettings loaded = defaults();
    bool validSchemaVersion = false;
    const int storedSchemaVersion = settings_->value(
        QStringLiteral("schema/version"), 0).toInt(&validSchemaVersion);
    loaded.appearance.font.setFamily(
        settings_->value(QStringLiteral("appearance/fontFamily"),
                         loaded.appearance.font.family()).toString());
    bool validSize = false;
    const qreal pointSize = settings_->value(
        QStringLiteral("appearance/fontSize"),
        loaded.appearance.font.pointSizeF()).toDouble(&validSize);
    loaded.appearance.font.setPointSizeF(
        validSize ? pointSize : loaded.appearance.font.pointSizeF());
    loaded.appearance.textColor = readColor(
        *settings_, QStringLiteral("appearance/textColor"),
        loaded.appearance.textColor);
    loaded.appearance.backgroundColor = readColor(
        *settings_, QStringLiteral("appearance/backgroundColor"),
        loaded.appearance.backgroundColor);
    loaded.appearance.cursorColor = readColor(
        *settings_, QStringLiteral("appearance/cursorColor"),
        loaded.appearance.cursorColor);
    loaded.appearance.selectionTextColor = readColor(
        *settings_, QStringLiteral("appearance/selectionTextColor"),
        loaded.appearance.selectionTextColor);

    loaded.windowGeometry = settings_->value(
        QStringLiteral("window/geometry")).toByteArray();
    loaded.wordWrap = readBool(
        *settings_, QStringLiteral("view/wordWrap"), loaded.wordWrap);
    if (!validSchemaVersion || storedSchemaVersion < 2) {
        // Version 1 defaulted word wrapping off. Treat that legacy value as an
        // old default during upgrade so existing users receive the corrected
        // wrapped, non-horizontal-scrolling behavior as well.
        loaded.wordWrap = true;
    }
    loaded.lineNumbers = readBool(
        *settings_, QStringLiteral("view/lineNumbers"), loaded.lineNumbers);
    loaded.alwaysOnTop = readBool(
        *settings_, QStringLiteral("window/alwaysOnTop"), loaded.alwaysOnTop);
    loaded.frameless = readBool(
        *settings_, QStringLiteral("window/frameless"), loaded.frameless);
    const QVariant bossKeyValue = settings_->value(
        QStringLiteral("input/bossKey"));
    if (bossKeyValue.isValid()) {
        const QString bossKeyText = bossKeyValue.toString();
        const QKeySequence bossKey = QKeySequence::fromString(
            bossKeyText, QKeySequence::PortableText);
        if (bossKeyText.isEmpty()
            || GlobalShortcut::isSupportedShortcut(bossKey)) {
            loaded.bossKey = bossKey;
        }
    }
    const QVariant focusShortcutValue = settings_->value(
        QStringLiteral("input/focusShortcut"));
    if (focusShortcutValue.isValid()) {
        const QString focusShortcutText = focusShortcutValue.toString();
        const QKeySequence focusShortcut = QKeySequence::fromString(
            focusShortcutText, QKeySequence::PortableText);
        if (focusShortcutText.isEmpty()
            || GlobalShortcut::isSupportedShortcut(focusShortcut)) {
            loaded.focusShortcut = focusShortcut;
        }
    }
    loaded.lastDirectory = settings_->value(
        QStringLiteral("files/lastDirectory"), loaded.lastDirectory).toString();
    return normalized(std::move(loaded));
}

bool SettingsManager::save(const ApplicationSettings& settings)
{
    const ApplicationSettings safe = normalized(settings);
    settings_->setValue(QStringLiteral("schema/version"), settingsSchemaVersion);
    settings_->setValue(QStringLiteral("appearance/fontFamily"),
                        safe.appearance.font.family());
    settings_->setValue(QStringLiteral("appearance/fontSize"),
                        safe.appearance.font.pointSizeF());
    settings_->setValue(QStringLiteral("appearance/textColor"),
                        colorText(safe.appearance.textColor));
    settings_->setValue(QStringLiteral("appearance/backgroundColor"),
                        colorText(safe.appearance.backgroundColor));
    settings_->setValue(QStringLiteral("appearance/cursorColor"),
                        colorText(safe.appearance.cursorColor));
    settings_->setValue(QStringLiteral("appearance/selectionTextColor"),
                        colorText(safe.appearance.selectionTextColor));
    settings_->setValue(QStringLiteral("window/geometry"), safe.windowGeometry);
    settings_->setValue(QStringLiteral("window/alwaysOnTop"), safe.alwaysOnTop);
    settings_->setValue(QStringLiteral("window/frameless"), safe.frameless);
    settings_->setValue(
        QStringLiteral("input/bossKey"),
        safe.bossKey.toString(QKeySequence::PortableText));
    settings_->setValue(
        QStringLiteral("input/focusShortcut"),
        safe.focusShortcut.toString(QKeySequence::PortableText));
    settings_->setValue(QStringLiteral("view/wordWrap"), safe.wordWrap);
    settings_->setValue(QStringLiteral("view/lineNumbers"), safe.lineNumbers);
    settings_->setValue(QStringLiteral("files/lastDirectory"),
                        safe.lastDirectory);
    settings_->sync();
    return settings_->status() == QSettings::NoError;
}

QString SettingsManager::fileName() const
{
    return settings_->fileName();
}

ApplicationSettings SettingsManager::normalized(ApplicationSettings settings)
{
    const ApplicationSettings fallback = defaults();
    if (settings.appearance.font.family().isEmpty()
        || !QFontDatabase::hasFamily(settings.appearance.font.family())) {
        settings.appearance.font.setFamily(fallback.appearance.font.family());
    }
    const qreal pointSize = settings.appearance.font.pointSizeF();
    if (!std::isfinite(pointSize) || pointSize < 6.0 || pointSize > 72.0) {
        settings.appearance.font.setPointSizeF(
            fallback.appearance.font.pointSizeF());
    }

    auto validOr = [](QColor color, const QColor& defaultColor) {
        return color.isValid() ? color : defaultColor;
    };
    settings.appearance.textColor = validOr(
        settings.appearance.textColor, fallback.appearance.textColor);
    settings.appearance.backgroundColor = validOr(
        settings.appearance.backgroundColor, fallback.appearance.backgroundColor);
    settings.appearance.cursorColor = validOr(
        settings.appearance.cursorColor, fallback.appearance.cursorColor);
    settings.appearance.selectionTextColor = validOr(
        settings.appearance.selectionTextColor,
        fallback.appearance.selectionTextColor);
    settings.appearance.cursorColor.setAlpha(255);
    settings.appearance.selectionTextColor.setAlpha(255);

    if (settings.windowGeometry.size() > maximumGeometryBytes) {
        settings.windowGeometry.clear();
    }
    if (!QDir(settings.lastDirectory).exists()) {
        settings.lastDirectory = fallback.lastDirectory;
    }
    if (!GlobalShortcut::isSupportedShortcut(settings.bossKey)) {
        settings.bossKey = fallback.bossKey;
    }
    if (!GlobalShortcut::isSupportedShortcut(settings.focusShortcut)
        || (!settings.focusShortcut.isEmpty()
            && settings.focusShortcut == settings.bossKey)) {
        settings.focusShortcut = fallback.focusShortcut;
    }
    return settings;
}

} // namespace vinson
