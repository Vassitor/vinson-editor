#include "settings/AppearanceIO.h"

#include "settings/ThemeManager.h"

#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace vinson {
namespace {

constexpr auto formatName = "vinson-appearance";
constexpr int formatVersion = 1;

QString colorText(const QColor& color)
{
    return color.name(QColor::HexArgb);
}

QJsonObject appearanceObject(const Appearance& appearance)
{
    return {
        {QStringLiteral("fontFamily"), appearance.font.family()},
        {QStringLiteral("fontSize"), appearance.font.pointSizeF()},
        {QStringLiteral("fontBold"), appearance.font.bold()},
        {QStringLiteral("fontItalic"), appearance.font.italic()},
        {QStringLiteral("textColor"), colorText(appearance.textColor)},
        {QStringLiteral("backgroundColor"), colorText(appearance.backgroundColor)},
        {QStringLiteral("cursorColor"), colorText(appearance.cursorColor)},
        {QStringLiteral("selectionTextColor"), colorText(appearance.selectionTextColor)},
        {QStringLiteral("selectionBackgroundColor"), colorText(appearance.selectionBackgroundColor)},
        {QStringLiteral("lineNumberColor"), colorText(appearance.lineNumberColor)},
        {QStringLiteral("currentLineColor"), colorText(appearance.currentLineColor)},
        {QStringLiteral("cursorWidth"), appearance.cursorWidth},
        {QStringLiteral("lineSpacing"), appearance.lineSpacing},
    };
}

QColor readColor(const QJsonObject& object, const QString& key,
                 const QColor& fallback)
{
    const QColor color(object.value(key).toString());
    return color.isValid() ? color : fallback;
}

Appearance readAppearance(const QJsonObject& object)
{
    Appearance appearance = ThemeManager::defaultAppearance();
    const QString family = object.value(QStringLiteral("fontFamily")).toString();
    if (!family.isEmpty() && QFontDatabase::hasFamily(family)) {
        appearance.font.setFamily(family);
    }
    const double size = object.value(QStringLiteral("fontSize")).toDouble(
        appearance.font.pointSizeF());
    if (std::isfinite(size)) {
        appearance.font.setPointSizeF(std::clamp(size, 6.0, 72.0));
    }
    appearance.font.setBold(object.value(QStringLiteral("fontBold")).toBool(
        appearance.font.bold()));
    appearance.font.setItalic(object.value(QStringLiteral("fontItalic")).toBool(
        appearance.font.italic()));
    appearance.textColor = readColor(object, QStringLiteral("textColor"),
                                     appearance.textColor);
    appearance.backgroundColor = readColor(
        object, QStringLiteral("backgroundColor"), appearance.backgroundColor);
    appearance.cursorColor = readColor(object, QStringLiteral("cursorColor"),
                                       appearance.cursorColor);
    appearance.selectionTextColor = readColor(
        object, QStringLiteral("selectionTextColor"), appearance.selectionTextColor);
    appearance.selectionBackgroundColor = readColor(
        object, QStringLiteral("selectionBackgroundColor"),
        appearance.selectionBackgroundColor);
    appearance.lineNumberColor = readColor(
        object, QStringLiteral("lineNumberColor"), appearance.lineNumberColor);
    appearance.currentLineColor = readColor(
        object, QStringLiteral("currentLineColor"), appearance.currentLineColor);
    appearance.cursorColor.setAlpha(255);
    appearance.selectionTextColor.setAlpha(255);
    appearance.lineNumberColor.setAlpha(255);
    appearance.cursorWidth = std::clamp(
        object.value(QStringLiteral("cursorWidth")).toInt(appearance.cursorWidth),
        1, 5);
    appearance.lineSpacing = std::clamp(
        object.value(QStringLiteral("lineSpacing")).toInt(appearance.lineSpacing),
        0, 20);
    return appearance;
}

bool isValidShortcut(const QKeySequence& shortcut)
{
    if (shortcut.isEmpty()) {
        return true;
    }
    if (shortcut.count() != 1) {
        return false;
    }
    const Qt::Key key = shortcut[0].key();
    return key != Qt::Key_unknown && key != Qt::Key_Control
        && key != Qt::Key_Shift && key != Qt::Key_Alt && key != Qt::Key_Meta;
}

} // namespace

QByteArray exportAppearanceBundle(const AppearanceBundle& bundle)
{
    QJsonArray presets;
    for (const AppearancePreset& preset : bundle.presets) {
        presets.append(QJsonObject{
            {QStringLiteral("name"), preset.name},
            {QStringLiteral("shortcut"),
             preset.shortcut.toString(QKeySequence::PortableText)},
            {QStringLiteral("appearance"), appearanceObject(preset.appearance)},
        });
    }
    const QJsonObject root{
        {QStringLiteral("format"), QString::fromLatin1(formatName)},
        {QStringLiteral("version"), formatVersion},
        {QStringLiteral("appearance"), appearanceObject(bundle.appearance)},
        {QStringLiteral("presets"), presets},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<AppearanceBundle> importAppearanceBundle(
    const QByteArray& json, QString* errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject root = document.object();
    const bool valid = parseError.error == QJsonParseError::NoError
        && document.isObject()
        && root.value(QStringLiteral("format")).toString()
            == QString::fromLatin1(formatName)
        && root.value(QStringLiteral("version")).toInt() == formatVersion
        && root.value(QStringLiteral("appearance")).isObject();
    if (!valid) {
        if (errorMessage != nullptr) {
            *errorMessage = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Unsupported or incomplete appearance file.")
                : parseError.errorString();
        }
        return std::nullopt;
    }

    AppearanceBundle bundle;
    bundle.appearance = readAppearance(
        root.value(QStringLiteral("appearance")).toObject());
    QSet<QString> names;
    QSet<QString> shortcuts;
    for (const QJsonValue& value : root.value(QStringLiteral("presets")).toArray()) {
        if (!value.isObject() || bundle.presets.size() >= maximumAppearancePresets) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString name = object.value(QStringLiteral("name")).toString()
                                 .trimmed().left(maximumAppearancePresetNameLength);
        const QString foldedName = name.toCaseFolded();
        if (name.isEmpty() || names.contains(foldedName)
            || !object.value(QStringLiteral("appearance")).isObject()) {
            continue;
        }
        QKeySequence shortcut = QKeySequence::fromString(
            object.value(QStringLiteral("shortcut")).toString(),
            QKeySequence::PortableText);
        const QString shortcutText = shortcut.toString(QKeySequence::PortableText);
        if (!isValidShortcut(shortcut) || shortcuts.contains(shortcutText)) {
            shortcut = {};
        }
        names.insert(foldedName);
        if (!shortcut.isEmpty()) {
            shortcuts.insert(shortcutText);
        }
        bundle.presets.append({name,
            readAppearance(object.value(QStringLiteral("appearance")).toObject()),
            shortcut});
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return bundle;
}

} // namespace vinson
