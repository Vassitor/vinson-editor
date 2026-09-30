#pragma once

#include "settings/Appearance.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace vinson {

struct AppearanceBundle
{
    Appearance appearance;
    QVector<AppearancePreset> presets;

    friend bool operator==(const AppearanceBundle&, const AppearanceBundle&) = default;
};

[[nodiscard]] QByteArray exportAppearanceBundle(const AppearanceBundle& bundle);
[[nodiscard]] std::optional<AppearanceBundle> importAppearanceBundle(
    const QByteArray& json, QString* errorMessage = nullptr);

} // namespace vinson
