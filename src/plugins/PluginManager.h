#pragma once

#include <QObject>
#include "plugins/PluginSafety.h"
#include <QByteArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <memory>
#include <optional>

class QThread;
class QTimer;

namespace vinson {

struct PluginField {
    QString id;
    QString label;
    QString type;
    QVariant defaultValue;
    QStringList choices;
    int minimum = -1000000;
    int maximum = 1000000;
    int maximumLength = 4096;
};

struct PluginCommand {
    QString id;
    QString title;
    QString input;
    QString output;
    QString script;
    QKeySequence defaultShortcut;
    QKeySequence shortcut;
    QVector<PluginField> parameters;
};

struct EditorPlugin {
    QString id;
    QString name;
    QString version;
    QString description;
    QString filePath;
    QString error;
    QByteArray packageHash;
    QVector<PluginCommand> commands;
    QVector<PluginRisk> risks;
    bool enabled = false;
    QVector<PluginField> settings;
    QVariantMap values;
};

class PluginManager final : public QObject
{
    Q_OBJECT
public:
    static constexpr qint64 maximumTextBytes = 8 * 1024 * 1024;
    explicit PluginManager(const QString& directory, QObject* parent = nullptr);
    ~PluginManager() override;
    [[nodiscard]] QString directory() const;
    [[nodiscard]] const QVector<EditorPlugin>& plugins() const;
    [[nodiscard]] bool isRunning() const;
    void reload();
    [[nodiscard]] static EditorPlugin inspect(const QString& path);
    [[nodiscard]] bool install(const QString& path, QString* error, bool replaceExisting = false,
                               const QByteArray& reviewedPackageHash = {});
    [[nodiscard]] bool setEnabled(const QString& id, bool enabled, QString* error);
    [[nodiscard]] bool uninstall(const QString& id, QString* error);
    [[nodiscard]] bool configure(const QString& id, const QVariantMap& values,
                                 const QMap<QString, QKeySequence>& shortcuts,
                                 QString* error);
    void setReservedShortcuts(const QList<QKeySequence>& shortcuts);
    [[nodiscard]] static QVariantMap defaults(const QVector<PluginField>& fields);
    [[nodiscard]] static bool validateValues(const QVector<PluginField>& fields,
                                           const QVariantMap& values, QString* error);
    [[nodiscard]] static bool shortcutsConflict(const QKeySequence& first,
                                               const QKeySequence& second);
    [[nodiscard]] bool execute(const QString& pluginId, const QString& commandId,
                               const QVariantMap& context, QString* error,
                               int timeoutMs = 3000);
    void cancel();
signals:
    void pluginsChanged();
    void runningChanged(bool running);
    void commandFinished(const QString& output, const QString& text,
                         const QString& error);
private:
    struct Execution;
    [[nodiscard]] static EditorPlugin readPlugin(const QString& path);
    [[nodiscard]] bool saveEnabled(const QStringList& ids, QString* error);
    [[nodiscard]] QStringList enabledIds() const;
    [[nodiscard]] std::optional<QJsonObject> readPreferences(QString* error) const;
    [[nodiscard]] bool savePreferences(const QJsonObject& preferences, QString* error);
    QList<QKeySequence> reservedShortcuts_;
    QString directory_;
    QVector<EditorPlugin> plugins_;
    QThread* worker_ = nullptr;
    QTimer* timeout_ = nullptr;
    std::shared_ptr<Execution> execution_;
};

} // namespace vinson
