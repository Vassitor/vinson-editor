#pragma once

#include <QString>
#include <QVector>

namespace vinson {
struct PluginRisk {
    QString commandId;
    QString commandTitle;
    int line = 1;
    QString category;
    QString explanation;
    QString evidence;
};

// A conservative pattern scan, never an execution or a safety certification.
[[nodiscard]] QVector<PluginRisk> scanPluginScript(const QString& script,
    const QString& commandId, const QString& commandTitle);
} // namespace vinson
