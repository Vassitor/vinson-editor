#pragma once

#include "settings/Appearance.h"

#include <QByteArray>
#include <QKeySequence>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QSettings;

namespace vinson {

inline constexpr qsizetype maximumRecentFiles = 10;
inline constexpr qsizetype maximumRestoredTabs = 32;

struct ApplicationSettings
{
    Appearance appearance;
    QVector<AppearancePreset> appearancePresets;
    QMap<QString, QKeySequence> shortcuts;
    QByteArray windowGeometry;
    QString lastDirectory;
    QStringList recentFiles;
    QStringList openTabs;
    bool restoreTabsOnStartup = true;
    bool wordWrap = false;
    bool lineNumbers = true;
    bool alwaysOnTop = false;
    bool frameless = false;
    QKeySequence bossKey;
    QKeySequence focusShortcut;

    friend bool operator==(const ApplicationSettings&,
                           const ApplicationSettings&) = default;
};

class SettingsManager final : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject* parent = nullptr);
    explicit SettingsManager(const QString& iniFilePath,
                             QObject* parent = nullptr);
    ~SettingsManager() override;

    [[nodiscard]] static ApplicationSettings defaults();
    [[nodiscard]] ApplicationSettings load() const;
    [[nodiscard]] bool save(const ApplicationSettings& settings);
    [[nodiscard]] QString fileName() const;

private:
    explicit SettingsManager(std::unique_ptr<QSettings> settings,
                             QObject* parent);
    [[nodiscard]] static ApplicationSettings normalized(
        ApplicationSettings settings);

    std::unique_ptr<QSettings> settings_;
};

} // namespace vinson
