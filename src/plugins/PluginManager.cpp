#include "plugins/PluginManager.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <mutex>
#include <cmath>
#include <limits>

namespace vinson {
namespace {
constexpr qint64 maximumPackageBytes = 1024 * 1024;

bool validId(const QString& id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9.-]{0,79}$"));
    return pattern.match(id).hasMatch();
}

QString scriptError(const QJSValue& value)
{
    return QStringLiteral("%1 (line %2)").arg(value.toString().left(1000))
        .arg(value.property(QStringLiteral("lineNumber")).toInt());
}

QJSValue compileCommand(QJSEngine& engine, const QString& script)
{
    // Pass the body as a value to Function, never interpolate untrusted code
    // into a wrapper that could be closed to execute code during installation.
    return engine.globalObject().property(QStringLiteral("Function"))
        .callAsConstructor({QJSValue(QStringLiteral("context")),
            QJSValue(QStringLiteral("\"use strict\";\n") + script)});
}

bool validShortcut(const QKeySequence& shortcut)
{
    if (shortcut.isEmpty()) return true;
    if (shortcut.count() != 1) return false;
    const auto key = shortcut[0].key();
    return key != Qt::Key_unknown && key != Qt::Key_Control && key != Qt::Key_Shift
        && key != Qt::Key_Alt && key != Qt::Key_Meta && key != Qt::Key_Escape;
}

bool validFieldValue(const PluginField& field, const QVariant& value)
{
    const auto json = QJsonValue::fromVariant(value);
    if (field.type == QLatin1String("boolean")) return json.isBool();
    if (field.type == QLatin1String("integer")) {
        const double number = json.toDouble(std::numeric_limits<double>::quiet_NaN());
        return json.isDouble() && std::isfinite(number) && std::floor(number) == number
            && number >= field.minimum && number <= field.maximum;
    }
    if (!json.isString() || json.toString().size() > field.maximumLength) return false;
    return field.type != QLatin1String("choice") || field.choices.contains(json.toString());
}

bool readFields(const QJsonValue& value, QVector<PluginField>& fields)
{
    if (value.isUndefined()) return true;
    if (!value.isArray() || value.toArray().size() > 32) return false;
    QSet<QString> ids;
    for (const auto& item : value.toArray()) {
        const auto object = item.toObject();
        PluginField field;
        field.id = object.value(QStringLiteral("id")).toString();
        field.label = object.value(QStringLiteral("label")).toString().trimmed();
        field.type = object.value(QStringLiteral("type")).toString();
        field.defaultValue = object.value(QStringLiteral("default")).toVariant();
        if (!validId(field.id) || ids.contains(field.id) || field.label.isEmpty()
            || field.label.size() > 120 || !object.contains(QStringLiteral("default"))
            || !QStringList{"boolean", "integer", "string", "choice"}.contains(field.type)) return false;
        auto readInteger = [&object](const char* name, int& target, int minimum, int maximum) {
            const auto value = object.value(QString::fromLatin1(name));
            if (value.isUndefined()) return true;
            const auto number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
            if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number
                || number < minimum || number > maximum) return false;
            target = static_cast<int>(number);
            return true;
        };
        if (!readInteger("minimum", field.minimum, -1000000, 1000000)
            || !readInteger("maximum", field.maximum, -1000000, 1000000)
            || !readInteger("maxLength", field.maximumLength, 1, 16384)
            || field.minimum > field.maximum) return false;
        if (field.type == QLatin1String("choice")) {
            const auto choices = object.value(QStringLiteral("choices"));
            if (!choices.isArray() || choices.toArray().isEmpty() || choices.toArray().size() > 64) return false;
            for (const auto& choice : choices.toArray()) {
                if (!choice.isString() || choice.toString().isEmpty() || choice.toString().size() > 120
                    || choice.toString().size() > field.maximumLength
                    || field.choices.contains(choice.toString())) return false;
                field.choices.append(choice.toString());
            }
        }
        if (!validFieldValue(field, field.defaultValue)) return false;
        ids.insert(field.id);
        fields.append(field);
    }
    return true;
}
}

struct PluginManager::Execution {
    std::mutex mutex;
    QJSEngine* engine = nullptr;
    bool cancelled = false;
    QString text;
    QString error;
};

PluginManager::PluginManager(const QString& directory, QObject* parent)
    : QObject(parent), directory_(QDir(directory).absolutePath()), timeout_(new QTimer(this))
{
    setObjectName(QStringLiteral("pluginManager"));
    timeout_->setSingleShot(true);
    connect(timeout_, &QTimer::timeout, this, &PluginManager::cancel);
    reload();
}

PluginManager::~PluginManager()
{
    if (worker_) {
        cancel();
        disconnect(worker_, nullptr, this, nullptr);
        worker_->wait();
        delete worker_;
    }
}

QString PluginManager::directory() const { return directory_; }
const QVector<EditorPlugin>& PluginManager::plugins() const { return plugins_; }
bool PluginManager::isRunning() const { return worker_ != nullptr; }
EditorPlugin PluginManager::inspect(const QString& path) { return readPlugin(path); }

EditorPlugin PluginManager::readPlugin(const QString& path)
{
    EditorPlugin plugin;
    plugin.filePath = path;
    plugin.name = QFileInfo(path).fileName();
    auto fail = [&plugin](const QString& error) {
        plugin.error = error;
        plugin.commands.clear();
        return plugin;
    };
    QFile file(path);
    if (QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly))
        return fail(tr("Cannot read plugin file."));
    if (file.size() > maximumPackageBytes)
        return fail(tr("Plugin packages must be at most 1 MiB."));
    QJsonParseError parseError;
    const auto data = file.read(maximumPackageBytes + 1);
    if (data.size() > maximumPackageBytes)
        return fail(tr("Plugin packages must be at most 1 MiB."));
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(tr("Invalid plugin JSON: %1").arg(parseError.errorString()));
    const QJsonObject object = document.object();
    plugin.packageHash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    if (object.value(QStringLiteral("apiVersion")).toDouble(-1) != 1)
        return fail(tr("Unsupported plugin API version; expected 1."));
    plugin.id = object.value(QStringLiteral("id")).toString();
    plugin.name = object.value(QStringLiteral("name")).toString().trimmed();
    plugin.version = object.value(QStringLiteral("version")).toString().trimmed();
    plugin.description = object.value(QStringLiteral("description")).toString();
    if (!validId(plugin.id) || plugin.name.isEmpty() || plugin.name.size() > 120
        || plugin.version.isEmpty() || plugin.version.size() > 40
        || plugin.description.size() > 2000)
        return fail(tr("Invalid plugin ID, name, version or description."));
    if (!readFields(object.value(QStringLiteral("settings")), plugin.settings))
        return fail(tr("Invalid plugin settings schema."));
    if (!object.value(QStringLiteral("commands")).isArray())
        return fail(tr("A plugin must contain a commands array."));
    const auto commands = object.value(QStringLiteral("commands")).toArray();
    if (commands.isEmpty() || commands.size() > 32)
        return fail(tr("A plugin must contain between 1 and 32 commands."));
    QSet<QString> ids;
    QJSEngine engine;
    for (const auto& value : commands) {
        const auto commandObject = value.toObject();
        PluginCommand command;
        command.id = commandObject.value(QStringLiteral("id")).toString();
        command.title = commandObject.value(QStringLiteral("title")).toString().trimmed();
        command.input = commandObject.value(QStringLiteral("input")).toString();
        command.output = commandObject.value(QStringLiteral("output")).toString();
        command.script = commandObject.value(QStringLiteral("script")).toString();
        const auto shortcut = commandObject.value(QStringLiteral("shortcut"));
        command.defaultShortcut = QKeySequence(shortcut.toString(), QKeySequence::PortableText);
        command.shortcut = command.defaultShortcut;
        if ((!shortcut.isUndefined() && !shortcut.isString())
            || (!shortcut.toString().isEmpty() && command.defaultShortcut.isEmpty())
            || !validShortcut(command.defaultShortcut)
            || !readFields(commandObject.value(QStringLiteral("parameters")), command.parameters))
            return fail(tr("Invalid command shortcut or parameter schema."));
        if (!validId(command.id) || ids.contains(command.id) || command.title.isEmpty()
            || command.title.size() > 120 || command.script.trimmed().isEmpty()
            || !QStringList{"selection", "document", "none", "line", "selectionOrDocument"}.contains(command.input)
            || !QStringList{"replaceSelection", "replaceDocument", "replaceInput", "insert", "message", "clipboard", "newDocument"}.contains(command.output)
            || (command.output == QLatin1String("replaceInput") && command.input == QLatin1String("none")))
            return fail(tr("Invalid or duplicate plugin command."));
        // Compile the function body without executing plugin code during import/startup.
        const auto function = compileCommand(engine, command.script);
        if (function.isError() || !function.isCallable())
            return fail(tr("Invalid script in %1: %2").arg(command.title, scriptError(function)));
        ids.insert(command.id);
        plugin.commands.append(command);
        plugin.risks += scanPluginScript(command.script, command.id, command.title);
    }
    return plugin;
}

QStringList PluginManager::enabledIds() const
{
    QFile file(QDir(directory_).filePath(QStringLiteral("enabled.json")));
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumPackageBytes) return {};
    QStringList ids;
    for (const auto& id : QJsonDocument::fromJson(file.readAll()).array()) {
        if (validId(id.toString())) ids.append(id.toString());
    }
    ids.removeDuplicates();
    return ids;
}

bool PluginManager::saveEnabled(const QStringList& ids, QString* error)
{
    QSaveFile file(QDir(directory_).filePath(QStringLiteral("enabled.json")));
    const QByteArray data = QJsonDocument(QJsonArray::fromStringList(ids)).toJson();
    if (!QDir().mkpath(directory_) || !file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = tr("Could not save plugin settings: %1").arg(file.errorString());
        return false;
    }
    return true;
}

void PluginManager::reload()
{
    if (isRunning()) return;
    plugins_.clear();
    const auto enabled = enabledIds();
    const auto preferences = readPreferences(nullptr).value_or(QJsonObject());
    QSet<QString> ids;
    for (const auto& file : QDir(directory_).entryInfoList({QStringLiteral("*.vinson-plugin")}, QDir::Files, QDir::Name)) {
        auto plugin = readPlugin(file.absoluteFilePath());
        if (plugin.error.isEmpty() && ids.contains(plugin.id))
            plugin.error = tr("Duplicate plugin ID.");
        if (plugin.error.isEmpty()) ids.insert(plugin.id);
        plugin.enabled = plugin.error.isEmpty() && enabled.contains(plugin.id);
        const auto preference = preferences.value(plugin.id).toObject();
        const auto values = preference.value(QStringLiteral("settings")).toObject().toVariantMap();
        plugin.values = defaults(plugin.settings);
        for (const auto& field : plugin.settings) {
            if (values.contains(field.id) && validFieldValue(field, values.value(field.id)))
                plugin.values.insert(field.id, values.value(field.id));
        }
        const auto shortcuts = preference.value(QStringLiteral("shortcuts")).toObject();
        for (auto& command : plugin.commands) {
            const auto value = shortcuts.value(command.id);
            if (!value.isString()) continue;
            const QKeySequence shortcut(value.toString(), QKeySequence::PortableText);
            if (validShortcut(shortcut) && (value.toString().isEmpty() || !shortcut.isEmpty()))
                command.shortcut = shortcut;
        }
        plugins_.append(plugin);
    }
    emit pluginsChanged();
}

QVariantMap PluginManager::defaults(const QVector<PluginField>& fields)
{
    QVariantMap result;
    for (const auto& field : fields) result.insert(field.id, field.defaultValue);
    return result;
}

bool PluginManager::validateValues(const QVector<PluginField>& fields,
                                   const QVariantMap& values, QString* error)
{
    auto remaining = values;
    for (const auto& field : fields) {
        if (values.contains(field.id) && !validFieldValue(field, values.value(field.id))) {
            if (error) *error = tr("Invalid value for %1.").arg(field.label);
            return false;
        }
        remaining.remove(field.id);
    }
    if (!remaining.isEmpty()) {
        if (error) *error = tr("Unknown plugin setting or parameter.");
        return false;
    }
    return true;
}

std::optional<QJsonObject> PluginManager::readPreferences(QString* error) const
{
    QFile file(QDir(directory_).filePath(QStringLiteral("preferences.json")));
    if (!file.exists()) return QJsonObject();
    if (file.open(QIODevice::ReadOnly) && file.size() <= maximumPackageBytes) {
        QJsonParseError parseError;
        const auto data = file.read(maximumPackageBytes + 1);
        const auto document = QJsonDocument::fromJson(data, &parseError);
        if (data.size() <= maximumPackageBytes && parseError.error == QJsonParseError::NoError && document.isObject())
            return document.object();
    }
    if (error) *error = tr("Cannot read plugin preferences. Repair preferences.json before saving settings.");
    return std::nullopt;
}

bool PluginManager::savePreferences(const QJsonObject& preferences, QString* error)
{
    const auto data = QJsonDocument(preferences).toJson();
    QSaveFile file(QDir(directory_).filePath(QStringLiteral("preferences.json")));
    if (data.size() > maximumPackageBytes || !QDir().mkpath(directory_)
        || !file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = tr("Could not save plugin preferences: %1").arg(file.errorString());
        return false;
    }
    return true;
}

void PluginManager::setReservedShortcuts(const QList<QKeySequence>& shortcuts)
{
    reservedShortcuts_ = shortcuts;
}

bool PluginManager::shortcutsConflict(const QKeySequence& first, const QKeySequence& second)
{
    return !first.isEmpty() && !second.isEmpty()
        && (first.matches(second) != QKeySequence::NoMatch || second.matches(first) != QKeySequence::NoMatch);
}

bool PluginManager::configure(const QString& id, const QVariantMap& values,
                              const QMap<QString, QKeySequence>& shortcuts, QString* error)
{
    if (!isRunning()) {
        for (const auto& plugin : plugins_) {
            if (plugin.id != id || !plugin.error.isEmpty()) continue;
            if (!validateValues(plugin.settings, values, error)) return false;
            auto occupied = reservedShortcuts_;
            for (const auto& other : plugins_) {
                if (other.id == id || !other.enabled) continue;
                for (const auto& command : other.commands) occupied.append(command.shortcut);
            }
            QJsonObject savedShortcuts;
            auto remaining = shortcuts;
            for (const auto& command : plugin.commands) {
                const auto shortcut = shortcuts.value(command.id, command.shortcut);
                if (!validShortcut(shortcut)) {
                    if (error) *error = tr("Use one key combination for %1; Escape is reserved for cancellation.").arg(command.title);
                    return false;
                }
                for (const auto& other : occupied) {
                    if (shortcutsConflict(shortcut, other)) {
                        if (error) *error = tr("Shortcut %1 for %2 is already in use.")
                            .arg(shortcut.toString(QKeySequence::NativeText), command.title);
                        return false;
                    }
                }
                occupied.append(shortcut);
                savedShortcuts.insert(command.id, shortcut.toString(QKeySequence::PortableText));
                remaining.remove(command.id);
            }
            if (!remaining.isEmpty()) {
                if (error) *error = tr("Unknown plugin command shortcut.");
                return false;
            }
            auto preferences = readPreferences(error);
            if (!preferences) return false;
            auto resolved = defaults(plugin.settings);
            for (auto it = values.cbegin(); it != values.cend(); ++it) resolved.insert(it.key(), it.value());
            preferences->insert(id, QJsonObject{
                {QStringLiteral("settings"), QJsonObject::fromVariantMap(resolved)},
                {QStringLiteral("shortcuts"), savedShortcuts}});
            if (!savePreferences(*preferences, error)) return false;
            reload();
            return true;
        }
    }
    if (error) *error = tr("Plugin is unavailable or a command is running.");
    return false;
}

bool PluginManager::install(const QString& path, QString* error, bool replaceExisting,
                            const QByteArray& reviewedPackageHash)
{
    if (isRunning()) {
        if (error) *error = tr("Wait for the running plugin command to finish.");
        return false;
    }
    const auto plugin = readPlugin(path);
    if (!plugin.error.isEmpty()) {
        if (error) *error = plugin.error;
        return false;
    }
    if ((!reviewedPackageHash.isEmpty() && reviewedPackageHash != plugin.packageHash)
        || (!plugin.risks.isEmpty() && reviewedPackageHash != plugin.packageHash)) {
        if (error) *error = tr("Review the security findings and confirm this exact package before installing. If the package changed, scan it again.");
        return false;
    }
    QString existingPath;
    for (const auto& installed : plugins_) {
        if (installed.id == plugin.id) {
            if (!replaceExisting || !existingPath.isEmpty()) {
                if (error) *error = tr("This plugin ID is already installed. Confirm replacement to update it.");
                return false;
            }
            existingPath = installed.filePath;
        }
    }
    const QString destination = existingPath.isEmpty()
        ? QDir(directory_).filePath(plugin.id + QStringLiteral(".vinson-plugin")) : existingPath;
    if ((existingPath.isEmpty() && QFileInfo::exists(destination))
        || QFileInfo(destination).isSymLink() || !QDir().mkpath(directory_)) {
        if (error) *error = tr("The plugin destination already exists or cannot be created.");
        return false;
    }
    QFile source(path);
    QSaveFile target(destination);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
        if (error) *error = tr("Could not copy the plugin package.");
        return false;
    }
    const auto data = source.read(maximumPackageBytes + 1);
    // Validate the exact bytes before an atomic replacement, preserving the old
    // package if the source changes between inspection and reading.
    if (data.size() > maximumPackageBytes
        || QCryptographicHash::hash(data, QCryptographicHash::Sha256) != plugin.packageHash) {
        if (error) *error = tr("The plugin package changed during import. Try again.");
        return false;
    }
    if (target.write(data) != data.size() || !target.commit()) {
        if (error) *error = tr("Could not copy the plugin package.");
        return false;
    }
    if (!existingPath.isEmpty()) {
        reload();
        return true;
    }
    auto ids = enabledIds();
    if (!ids.contains(plugin.id)) ids.append(plugin.id);
    if (!saveEnabled(ids, error)) {
        QFile::remove(destination);
        return false;
    }
    reload();
    return true;
}

bool PluginManager::setEnabled(const QString& id, bool enabled, QString* error)
{
    if (!isRunning()) {
        for (const auto& plugin : plugins_) {
            if (plugin.id != id || !plugin.error.isEmpty()) continue;
            auto ids = enabledIds();
            ids.removeAll(id);
            if (enabled) ids.append(id);
            if (!saveEnabled(ids, error)) return false;
            reload();
            return true;
        }
    }
    if (error) *error = tr("Plugin is unavailable or a command is running.");
    return false;
}

bool PluginManager::uninstall(const QString& id, QString* error)
{
    if (!isRunning()) {
        for (const auto& plugin : plugins_) {
            if (plugin.filePath != id && (id.isEmpty() || plugin.id != id)) continue;
            // Only remove packages discovered inside our own installation directory.
            if (!QFile::remove(plugin.filePath)) {
                if (error) *error = tr("Could not remove the plugin package.");
                return false;
            }
            auto ids = enabledIds();
            ids.removeAll(plugin.id);
            const bool saved = saveEnabled(ids, error);
            reload();
            return saved;
        }
    }
    if (error) *error = tr("Plugin is unavailable or a command is running.");
    return false;
}

bool PluginManager::execute(const QString& pluginId, const QString& commandId,
                            const QVariantMap& context, QString* error, int timeoutMs)
{
    if (!isRunning()) {
        for (const auto& plugin : plugins_) {
            if (plugin.id != pluginId || !plugin.enabled) continue;
            for (const auto& command : plugin.commands) {
                if (command.id != commandId) continue;
                if (context.value(QStringLiteral("text")).toString().toUtf8().size() > maximumTextBytes) {
                    if (error) *error = tr("Plugin input exceeds the 8 MiB limit.");
                    return false;
                }
                const auto parameters = context.value(QStringLiteral("parameters")).toMap();
                if (!validateValues(command.parameters, parameters, error)) return false;
                auto executionContext = context;
                executionContext.insert(QStringLiteral("settings"), plugin.values);
                auto resolvedParameters = defaults(command.parameters);
                for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
                    resolvedParameters.insert(it.key(), it.value());
                executionContext.insert(QStringLiteral("parameters"), resolvedParameters);
                execution_ = std::make_shared<Execution>();
                const auto state = execution_;
                worker_ = QThread::create([state, command, executionContext] {
                    QJSEngine engine;
                    {
                        std::lock_guard lock(state->mutex);
                        state->engine = &engine;
                        if (state->cancelled) engine.setInterrupted(true);
                    }
                    const auto function = compileCommand(engine, command.script);
                    auto result = function;
                    if (!function.isError()) result = function.call({engine.toScriptValue(executionContext)});
                    if (result.isError()) state->error = scriptError(result);
                    else if (!result.isString()) state->error = PluginManager::tr("Plugin commands must return a string.");
                    else if (result.property(QStringLiteral("length")).toNumber() > maximumTextBytes)
                        state->error = PluginManager::tr("Plugin output exceeds the 8 MiB limit.");
                    else {
                        state->text = result.toString();
                        if (state->text.toUtf8().size() > maximumTextBytes)
                            state->error = PluginManager::tr("Plugin output exceeds the 8 MiB limit.");
                    }
                    {
                        std::lock_guard lock(state->mutex);
                        state->engine = nullptr;
                    }
                });
                connect(worker_, &QThread::finished, this, [this, state, output = command.output] {
                    timeout_->stop();
                    worker_->wait();
                    worker_->deleteLater();
                    worker_ = nullptr;
                    execution_.reset();
                    const QString failure = state->cancelled
                        ? tr("Plugin command cancelled or timed out.") : state->error;
                    // Deliver before reenabling the host's editor and processing queued opens.
                    emit commandFinished(output, failure.isEmpty() ? state->text : QString(), failure);
                    emit runningChanged(false);
                });
                emit runningChanged(true);
                timeout_->start(qBound(1, timeoutMs, 30000));
                worker_->start();
                return true;
            }
        }
    }
    if (error) *error = tr("Plugin command is unavailable or another command is running.");
    return false;
}

void PluginManager::cancel()
{
    if (!execution_) return;
    std::lock_guard lock(execution_->mutex);
    execution_->cancelled = true;
    if (execution_->engine) execution_->engine->setInterrupted(true);
}

} // namespace vinson
