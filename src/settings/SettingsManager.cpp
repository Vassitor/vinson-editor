#include "settings/SettingsManager.h"

#include "settings/ThemeManager.h"
#include "window/GlobalShortcut.h"

#include <QColor>
#include <QDir>
#include <QFontDatabase>
#include <QFileInfo>
#include <QSettings>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <utility>

namespace vinson {
namespace {

constexpr int settingsSchemaVersion = 9;
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

bool isValidLocalShortcut(const QKeySequence& shortcut) noexcept
{
    if (shortcut.isEmpty()) {
        return true;
    }
    if (shortcut.count() != 1) {
        return false;
    }
    const Qt::Key key = shortcut[0].key();
    return key != Qt::Key_unknown && key != Qt::Key_Control
        && key != Qt::Key_Shift && key != Qt::Key_Alt
        && key != Qt::Key_Meta;
}

Appearance readAppearance(const QSettings& settings, const QString& prefix,
                          Appearance appearance)
{
    appearance.font.setFamily(
        settings.value(prefix + QStringLiteral("fontFamily"),
                         appearance.font.family()).toString());
    bool validSize = false;
    const qreal pointSize = settings.value(
        prefix + QStringLiteral("fontSize"),
        appearance.font.pointSizeF()).toDouble(&validSize);
    appearance.font.setPointSizeF(
        validSize ? pointSize : appearance.font.pointSizeF());
    appearance.font.setBold(readBool(
        settings, prefix + QStringLiteral("fontBold"), appearance.font.bold()));
    appearance.font.setItalic(readBool(
        settings, prefix + QStringLiteral("fontItalic"), appearance.font.italic()));
    appearance.textColor = readColor(
        settings, prefix + QStringLiteral("textColor"),
        appearance.textColor);
    appearance.backgroundColor = readColor(
        settings, prefix + QStringLiteral("backgroundColor"),
        appearance.backgroundColor);
    appearance.cursorColor = readColor(
        settings, prefix + QStringLiteral("cursorColor"),
        appearance.cursorColor);
    appearance.selectionTextColor = readColor(
        settings, prefix + QStringLiteral("selectionTextColor"),
        appearance.selectionTextColor);
    appearance.selectionBackgroundColor = readColor(
        settings, prefix + QStringLiteral("selectionBackgroundColor"),
        appearance.selectionBackgroundColor);
    appearance.lineNumberColor = readColor(
        settings, prefix + QStringLiteral("lineNumberColor"),
        appearance.lineNumberColor);
    appearance.currentLineColor = readColor(
        settings, prefix + QStringLiteral("currentLineColor"),
        appearance.currentLineColor);
    bool validCursorWidth = false;
    const int cursorWidth = settings.value(
        prefix + QStringLiteral("cursorWidth"), appearance.cursorWidth)
                                .toInt(&validCursorWidth);
    appearance.cursorWidth = validCursorWidth ? cursorWidth : appearance.cursorWidth;
    bool validLineSpacing = false;
    const int lineSpacing = settings.value(
        prefix + QStringLiteral("lineSpacing"), appearance.lineSpacing)
                               .toInt(&validLineSpacing);
    appearance.lineSpacing = validLineSpacing ? lineSpacing : appearance.lineSpacing;

    return appearance;
}

void writeAppearance(QSettings& settings, const QString& prefix,
                     const Appearance& appearance)
{
    settings.setValue(prefix + QStringLiteral("fontFamily"),
                        appearance.font.family());
    settings.setValue(prefix + QStringLiteral("fontSize"),
                        appearance.font.pointSizeF());
    settings.setValue(prefix + QStringLiteral("fontBold"),
                      appearance.font.bold());
    settings.setValue(prefix + QStringLiteral("fontItalic"),
                      appearance.font.italic());
    settings.setValue(prefix + QStringLiteral("textColor"),
                        colorText(appearance.textColor));
    settings.setValue(prefix + QStringLiteral("backgroundColor"),
                        colorText(appearance.backgroundColor));
    settings.setValue(prefix + QStringLiteral("cursorColor"),
                        colorText(appearance.cursorColor));
    settings.setValue(prefix + QStringLiteral("selectionTextColor"),
                        colorText(appearance.selectionTextColor));
    settings.setValue(prefix + QStringLiteral("selectionBackgroundColor"),
                      colorText(appearance.selectionBackgroundColor));
    settings.setValue(prefix + QStringLiteral("lineNumberColor"),
                      colorText(appearance.lineNumberColor));
    settings.setValue(prefix + QStringLiteral("currentLineColor"),
                      colorText(appearance.currentLineColor));
    settings.setValue(prefix + QStringLiteral("cursorWidth"),
                      appearance.cursorWidth);
    settings.setValue(prefix + QStringLiteral("lineSpacing"),
                      appearance.lineSpacing);
}

Appearance normalizedAppearance(Appearance appearance)
{
    const Appearance fallback = ThemeManager::defaultAppearance();
    if (appearance.font.family().isEmpty()
        || !QFontDatabase::hasFamily(appearance.font.family())) {
        appearance.font.setFamily(fallback.font.family());
    }
    const qreal pointSize = appearance.font.pointSizeF();
    if (!std::isfinite(pointSize) || pointSize < 6.0 || pointSize > 72.0) {
        appearance.font.setPointSizeF(
            fallback.font.pointSizeF());
    }

    auto validOr = [](QColor color, const QColor& defaultColor) {
        return color.isValid() ? color : defaultColor;
    };
    appearance.textColor = validOr(
        appearance.textColor, fallback.textColor);
    appearance.backgroundColor = validOr(
        appearance.backgroundColor, fallback.backgroundColor);
    appearance.cursorColor = validOr(
        appearance.cursorColor, fallback.cursorColor);
    appearance.selectionTextColor = validOr(
        appearance.selectionTextColor,
        fallback.selectionTextColor);
    appearance.selectionBackgroundColor = validOr(
        appearance.selectionBackgroundColor,
        fallback.selectionBackgroundColor);
    appearance.lineNumberColor = validOr(
        appearance.lineNumberColor, fallback.lineNumberColor);
    appearance.currentLineColor = validOr(
        appearance.currentLineColor, fallback.currentLineColor);
    appearance.cursorColor.setAlpha(255);
    appearance.selectionTextColor.setAlpha(255);
    appearance.lineNumberColor.setAlpha(255);
    appearance.cursorWidth = std::clamp(appearance.cursorWidth, 1, 5);
    appearance.lineSpacing = std::clamp(appearance.lineSpacing, 0, 20);

    return appearance;
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
    ApplicationSettings settings;
    settings.appearance = ThemeManager::defaultAppearance();
    settings.lastDirectory = QDir::homePath();
    settings.wordWrap = true;
    settings.lineNumbers = true;
    settings.restoreTabsOnStartup = true;
    settings.bossKey = GlobalShortcut::defaultShortcut();
    settings.focusShortcut = GlobalShortcut::defaultFocusShortcut();
    return settings;
}

ApplicationSettings SettingsManager::load() const
{
    ApplicationSettings loaded = defaults();
    bool validSchemaVersion = false;
    const int storedSchemaVersion = settings_->value(
        QStringLiteral("schema/version"), 0).toInt(&validSchemaVersion);
    loaded.appearance = readAppearance(
        *settings_, QStringLiteral("appearance/"), loaded.appearance);
    const int presetCount = std::min(
        settings_->beginReadArray(QStringLiteral("appearancePresets")),
        static_cast<int>(maximumAppearancePresets));
    for (int index = 0; index < presetCount; ++index) {
        settings_->setArrayIndex(index);
        AppearancePreset preset;
        preset.name = settings_->value(QStringLiteral("name")).toString();
        preset.appearance = readAppearance(
            *settings_, QStringLiteral("appearance/"), defaults().appearance);
        preset.shortcut = QKeySequence::fromString(
            settings_->value(QStringLiteral("shortcut")).toString(),
            QKeySequence::PortableText);
        loaded.appearancePresets.append(std::move(preset));
    }
    settings_->endArray();
    settings_->beginGroup(QStringLiteral("shortcuts"));
    for (const QString& id : settings_->childKeys()) {
        loaded.shortcuts.insert(
            id, QKeySequence::fromString(settings_->value(id).toString(),
                                         QKeySequence::PortableText));
    }
    settings_->endGroup();

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
    loaded.recentFiles = settings_->value(
        QStringLiteral("files/recentFiles")).toStringList();
    loaded.openTabs = settings_->value(
        QStringLiteral("session/openTabs")).toStringList();
    loaded.restoreTabsOnStartup = readBool(
        *settings_, QStringLiteral("session/restoreTabsOnStartup"),
        loaded.restoreTabsOnStartup);
    return normalized(std::move(loaded));
}

bool SettingsManager::save(const ApplicationSettings& settings)
{
    const ApplicationSettings safe = normalized(settings);
    settings_->setValue(QStringLiteral("schema/version"), settingsSchemaVersion);
    writeAppearance(*settings_, QStringLiteral("appearance/"), safe.appearance);
    settings_->remove(QStringLiteral("appearancePresets"));
    settings_->beginWriteArray(QStringLiteral("appearancePresets"),
                               static_cast<int>(safe.appearancePresets.size()));
    for (qsizetype index = 0; index < safe.appearancePresets.size(); ++index) {
        settings_->setArrayIndex(static_cast<int>(index));
        const AppearancePreset& preset = safe.appearancePresets.at(index);
        settings_->setValue(QStringLiteral("name"), preset.name);
        settings_->setValue(QStringLiteral("shortcut"),
                            preset.shortcut.toString(QKeySequence::PortableText));
        writeAppearance(*settings_, QStringLiteral("appearance/"), preset.appearance);
    }
    settings_->endArray();
    settings_->remove(QStringLiteral("shortcuts"));
    settings_->beginGroup(QStringLiteral("shortcuts"));
    for (auto iterator = safe.shortcuts.cbegin();
         iterator != safe.shortcuts.cend(); ++iterator) {
        settings_->setValue(
            iterator.key(),
            iterator.value().toString(QKeySequence::PortableText));
    }
    settings_->endGroup();
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
    settings_->setValue(QStringLiteral("files/recentFiles"), safe.recentFiles);
    settings_->setValue(QStringLiteral("session/openTabs"), safe.openTabs);
    settings_->setValue(QStringLiteral("session/restoreTabsOnStartup"),
                        safe.restoreTabsOnStartup);
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
    settings.appearance = normalizedAppearance(std::move(settings.appearance));
    QVector<AppearancePreset> presets;
    QStringList names;
    QList<QKeySequence> shortcuts;
    for (AppearancePreset preset : std::as_const(settings.appearancePresets)) {
        preset.name = preset.name.trimmed().left(maximumAppearancePresetNameLength);
        if (preset.name.isEmpty() || names.contains(preset.name, Qt::CaseInsensitive)) {
            continue;
        }
        names.append(preset.name);
        preset.appearance = normalizedAppearance(std::move(preset.appearance));
        if (!isValidLocalShortcut(preset.shortcut)
            || shortcuts.contains(preset.shortcut)) {
            preset.shortcut = {};
        }
        if (!preset.shortcut.isEmpty()) {
            shortcuts.append(preset.shortcut);
        }
        presets.append(std::move(preset));
        if (presets.size() == maximumAppearancePresets) {
            break;
        }
    }
    settings.appearancePresets = std::move(presets);
    QMap<QString, QKeySequence> shortcutsById;
    for (auto iterator = settings.shortcuts.cbegin();
         iterator != settings.shortcuts.cend(); ++iterator) {
        const QString id = iterator.key().trimmed().left(64);
        if (!id.isEmpty() && !id.contains(QLatin1Char('/'))
            && isValidLocalShortcut(iterator.value())) {
            shortcutsById.insert(id, iterator.value());
        }
    }
    settings.shortcuts = std::move(shortcutsById);

    if (settings.windowGeometry.size() > maximumGeometryBytes) {
        settings.windowGeometry.clear();
    }
    if (!QDir(settings.lastDirectory).exists()) {
        settings.lastDirectory = fallback.lastDirectory;
    }
    QStringList recentFiles;
    for (const QString& path : std::as_const(settings.recentFiles)) {
        if (path.trimmed().isEmpty()) {
            continue;
        }
        const QString absolutePath = QDir::cleanPath(
            QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
        constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
        constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
        if (!recentFiles.contains(absolutePath, pathCaseSensitivity)) {
            recentFiles.append(absolutePath);
        }
        if (recentFiles.size() == maximumRecentFiles) {
            break;
        }
    }
    settings.recentFiles = std::move(recentFiles);
    QStringList openTabs;
    for (const QString& path : std::as_const(settings.openTabs)) {
        if (path.trimmed().isEmpty()) {
            continue;
        }
        const QString absolutePath = QDir::cleanPath(
            QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
        constexpr Qt::CaseSensitivity tabPathCaseSensitivity =
            Qt::CaseInsensitive;
#else
        constexpr Qt::CaseSensitivity tabPathCaseSensitivity =
            Qt::CaseSensitive;
#endif
        if (!openTabs.contains(absolutePath, tabPathCaseSensitivity)) {
            openTabs.append(absolutePath);
        }
        if (openTabs.size() == maximumRestoredTabs) {
            break;
        }
    }
    settings.openTabs = std::move(openTabs);
    if (!GlobalShortcut::isSupportedShortcut(settings.bossKey)) {
        settings.bossKey = fallback.bossKey;
    }
    if (!GlobalShortcut::isSupportedShortcut(settings.focusShortcut)
        || (!settings.focusShortcut.isEmpty()
            && settings.focusShortcut == settings.bossKey)) {
        settings.focusShortcut = fallback.focusShortcut;
    }
    // A customized boss key may itself use the default focus shortcut.
    // Falling back must not register the same system-wide key twice.
    if (!settings.focusShortcut.isEmpty()
        && settings.focusShortcut == settings.bossKey) {
        settings.focusShortcut = {};
    }
    return settings;
}

} // namespace vinson
