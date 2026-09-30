#pragma once

#include <QColor>
#include <QFont>
#include <QMetaType>
#include <QKeySequence>
#include <QString>
#include <QVector>

namespace vinson {

struct Appearance
{
    QFont font;
    QColor textColor;
    QColor backgroundColor;
    QColor cursorColor;
    QColor selectionTextColor;
    QColor selectionBackgroundColor;
    QColor lineNumberColor;
    QColor currentLineColor;
    int cursorWidth = 2;
    int lineSpacing = 0;

    friend bool operator==(const Appearance&, const Appearance&) = default;
};

inline constexpr qsizetype maximumAppearancePresets = 32;
inline constexpr qsizetype maximumAppearancePresetNameLength = 64;

struct AppearancePreset
{
    QString name;
    Appearance appearance;
    QKeySequence shortcut;

    friend bool operator==(const AppearancePreset&, const AppearancePreset&) = default;
};

} // namespace vinson

Q_DECLARE_METATYPE(vinson::Appearance)
