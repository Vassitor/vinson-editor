#pragma once

#include <QColor>
#include <QFont>
#include <QMetaType>

namespace vinson {

struct Appearance
{
    QFont font;
    QColor textColor;
    QColor backgroundColor;
    QColor cursorColor;
    QColor selectionTextColor;

    friend bool operator==(const Appearance&, const Appearance&) = default;
};

} // namespace vinson

Q_DECLARE_METATYPE(vinson::Appearance)
