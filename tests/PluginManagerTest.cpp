#include "plugins/PluginManager.h"
#include "editor/EditorWidget.h"
#include "ui/PluginDialog.h"
#include "ui/PluginOptionsDialog.h"
#include "window/MainWindow.h"
#include "settings/Localization.h"

#include <QAction>
#include <QDir>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QTranslator>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QtTest>

using namespace vinson;

namespace {
QJsonObject package(const QString& script = QStringLiteral("return context.text.toUpperCase();"))
{
    return {{"apiVersion", 1}, {"id", "test.tools"}, {"name", "Test Tools"},
        {"version", "1.0"}, {"description", "Test package"},
        {"commands", QJsonArray{QJsonObject{{"id", "uppercase"}, {"title", "Uppercase"},
            {"input", "selection"}, {"output", "replaceSelection"}, {"script", script}}}}};
}

QString writePackage(const QString& root, const QJsonObject& object)
{
    const auto path = QDir(root).filePath(QStringLiteral("source.vinson-plugin"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write(QJsonDocument(object).toJson());
    return path;
}

QJsonObject configurablePackage()
{
    auto object = package("return context.settings.prefix + context.text.repeat(context.parameters.count);");
    object["settings"] = QJsonArray{
        QJsonObject{{"id", "enabled"}, {"label", "Enabled"}, {"type", "boolean"}, {"default", true}},
        QJsonObject{{"id", "width"}, {"label", "Width"}, {"type", "integer"}, {"default", 2}, {"minimum", 1}, {"maximum", 8}},
        QJsonObject{{"id", "prefix"}, {"label", "Prefix"}, {"type", "string"}, {"default", ">"}, {"maxLength", 4}},
        QJsonObject{{"id", "order"}, {"label", "Order"}, {"type", "choice"}, {"default", "asc"}, {"choices", QJsonArray{"asc", "desc"}}}};
    auto command = object["commands"].toArray().at(0).toObject();
    command["shortcut"] = "Ctrl+Alt+Shift+U";
    command["parameters"] = QJsonArray{QJsonObject{{"id", "count"}, {"label", "Count"},
        {"type", "integer"}, {"default", 2}, {"minimum", 1}, {"maximum", 4}}};
    object["commands"] = QJsonArray{command};
    return object;
}
}

class PluginManagerTest final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void installsPersistsDisablesAndUninstalls();
    void rejectsInvalidPackages_data();
    void rejectsInvalidPackages();
    void reportsAndRemovesBrokenPackages();
    void executesIsolatedUnicodeCommand();
    void runtimeFailure_data();
    void runtimeFailure();
    void cancelsAndTimesOutWithoutApplyingResults();
    void destructorInterruptsRunningScript();
    void appliesSelectionAndDocumentAsSingleUndo();
    void insertsAtCaretAndPreservesLineEnding();
    void discardsResultAfterDocumentChanges();
    void enforcesHostInputLimit();
    void exampleCanBeInstalledFromManagerDialog();
    void persistsTypedSettingsAndValidatesParameters();
    void validatesSettingsAndShortcutConflicts();
    void preservesCorruptPreferenceFile();
    void updatesPackageWithoutLosingPreferences();
    void editsPluginConfigurationThroughDialog();
    void supportsLineAndSelectionOrDocumentInputs_data();
    void supportsLineAndSelectionOrDocumentInputs();
    void promptsForParametersAndCancelsCleanly();
    void runsCommandShortcutAndRejectsEditorConflict();
    void copiesResultsAndCreatesUnsavedTabs();
    void extendedExamplesExecute();
    void scansSecurityPatterns_data();
    void scansSecurityPatterns();
    void requiresConsentForExactPackage();
    void confirmsRiskyImportsAndUpdates();
    void previewsBundledDeveloperGuides();
    void boundsSecurityFindingsAndPreservesLineLocations();
private:
    QTemporaryDir settings_;
};

void PluginManagerTest::initTestCase()
{
    QVERIFY(settings_.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("VinsonPluginTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PluginTests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
}

void PluginManagerTest::installsPersistsDisablesAndUninstalls()
{
    QTemporaryDir temporary;
    const auto path = writePackage(temporary.path(), package());
    const auto directory = temporary.filePath("installed");
    QString error;
    PluginManager manager(directory);
    QVERIFY2(manager.install(path, &error), qPrintable(error));
    QCOMPARE(manager.plugins().size(), 1);
    QVERIFY(manager.plugins().front().enabled);
    QVERIFY(!manager.install(path, &error));
    QVERIFY(error.contains("already installed"));
    PluginManager restarted(directory);
    QVERIFY(restarted.plugins().front().enabled);
    QVERIFY(manager.setEnabled("test.tools", false, &error));
    restarted.reload();
    QVERIFY(!restarted.plugins().front().enabled);
    QVERIFY(!manager.execute("test.tools", "uppercase", {}, &error));
    QVERIFY(manager.setEnabled("test.tools", true, &error));
    QVERIFY(manager.uninstall("test.tools", &error));
    QCOMPARE(manager.plugins().size(), 0);
    QVERIFY(QFileInfo::exists(path)); // The user's source package is preserved.
    restarted.reload();
    QVERIFY(restarted.plugins().isEmpty());
}

void PluginManagerTest::rejectsInvalidPackages_data()
{
    QTest::addColumn<QJsonObject>("object");
    auto invalid = package();
    invalid["id"] = "../escape";
    QTest::newRow("path traversal") << invalid;
    invalid = package(); invalid["apiVersion"] = 2;
    QTest::newRow("unsupported API") << invalid;
    invalid = package(); invalid["apiVersion"] = 1.5;
    QTest::newRow("fractional API") << invalid;
    invalid = package(); invalid["commands"] = QJsonArray{};
    QTest::newRow("empty commands") << invalid;
    invalid = package("return {;");
    QTest::newRow("syntax error") << invalid;
    invalid = package("}); while (true) {} ; (function(context) {");
    QTest::newRow("cannot escape function body during import") << invalid;
    invalid = package();
    auto commands = invalid["commands"].toArray(); commands.append(commands.at(0));
    invalid["commands"] = commands;
    QTest::newRow("duplicate command") << invalid;
    invalid = package(); auto command = invalid["commands"].toArray().at(0).toObject();
    command["output"] = "shell"; invalid["commands"] = QJsonArray{command};
    QTest::newRow("unsupported output") << invalid;
    invalid = configurablePackage();
    auto fields = invalid["settings"].toArray(); auto field = fields.at(1).toObject();
    field["default"] = 1.5; fields[1] = field; invalid["settings"] = fields;
    QTest::newRow("fractional integer default") << invalid;
    field["default"] = 20; fields[1] = field; invalid["settings"] = fields;
    QTest::newRow("out-of-range default") << invalid;
    invalid = configurablePackage(); command = invalid["commands"].toArray().at(0).toObject();
    command["shortcut"] = "Esc"; invalid["commands"] = QJsonArray{command};
    QTest::newRow("escape shortcut") << invalid;
    command["shortcut"] = "Ctrl+K, Ctrl+C"; invalid["commands"] = QJsonArray{command};
    QTest::newRow("multi-stroke shortcut") << invalid;
    invalid = package(); command = invalid["commands"].toArray().at(0).toObject();
    command["input"] = "none"; command["output"] = "replaceInput";
    invalid["commands"] = QJsonArray{command};
    QTest::newRow("replaceInput needs input") << invalid;
}

void PluginManagerTest::rejectsInvalidPackages()
{
    QFETCH(QJsonObject, object);
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(!manager.install(writePackage(temporary.path(), object), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(manager.plugins().isEmpty());
    QVERIFY(!QFileInfo::exists(temporary.filePath("escape.vinson-plugin")));
}

void PluginManagerTest::reportsAndRemovesBrokenPackages()
{
    QTemporaryDir temporary;
    const auto path = writePackage(temporary.path(), package("return {;"));
    PluginManager manager(temporary.path());
    QCOMPARE(manager.plugins().size(), 1);
    QVERIFY(!manager.plugins().front().error.isEmpty());
    QVERIFY(!manager.plugins().front().enabled);
    QString error;
    QVERIFY(!manager.uninstall(temporary.filePath("outside.vinson-plugin"), &error));
    QVERIFY(manager.uninstall(path, &error));
    QVERIFY(!QFileInfo::exists(path));
}

void PluginManagerTest::executesIsolatedUnicodeCommand()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), package(
        "return context.text.toUpperCase() + ':' + typeof Qt + ':' + typeof fetch;")), &error));
    QSignalSpy finished(&manager, &PluginManager::commandFinished);
    QSignalSpy running(&manager, &PluginManager::runningChanged);
    QVERIFY(manager.execute("test.tools", "uppercase", {{"text", QString::fromUtf8("héllo 世界")}}, &error));
    QVERIFY(manager.isRunning());
    QVERIFY(!manager.setEnabled("test.tools", false, &error));
    QVERIFY(!manager.execute("test.tools", "uppercase", {}, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.front().at(0).toString(), "replaceSelection");
    QCOMPARE(finished.front().at(1).toString(), QString::fromUtf8("HÉLLO 世界:undefined:undefined"));
    QVERIFY(finished.front().at(2).toString().isEmpty());
    QCOMPARE(running.count(), 2);
    QVERIFY(!manager.isRunning());
}

void PluginManagerTest::runtimeFailure_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QString>("errorPart");
    QTest::newRow("runtime exception") << "throw new Error('test failure');" << "test failure";
    QTest::newRow("non-string result") << "return 42;" << "return a string";
    QTest::newRow("output length") << "return 'a'.repeat(8 * 1024 * 1024 + 1);" << "8 MiB";
}

void PluginManagerTest::runtimeFailure()
{
    QFETCH(QString, script);
    QFETCH(QString, errorPart);
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), package(script)), &error));
    QSignalSpy finished(&manager, &PluginManager::commandFinished);
    QVERIFY(manager.execute("test.tools", "uppercase", {}, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QVERIFY(finished.front().at(1).toString().isEmpty());
    QVERIFY2(finished.front().at(2).toString().contains(errorPart), qPrintable(finished.front().at(2).toString()));
}

void PluginManagerTest::cancelsAndTimesOutWithoutApplyingResults()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    // Importing a plugin compiles the function without running this infinite loop.
    const auto path = writePackage(temporary.path(), package("while (true) {} return 'lost';"));
    QVERIFY(manager.install(path, &error, false, PluginManager::inspect(path).packageHash));
    QSignalSpy finished(&manager, &PluginManager::commandFinished);
    QVERIFY(manager.execute("test.tools", "uppercase", {}, &error, 30));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QVERIFY(finished.front().at(1).toString().isEmpty());
    QVERIFY(finished.front().at(2).toString().contains("timed out"));
    finished.clear();
    QVERIFY(manager.execute("test.tools", "uppercase", {}, &error));
    manager.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QVERIFY(finished.front().at(2).toString().contains("cancelled"));
    QVERIFY(!manager.isRunning());
}

void PluginManagerTest::destructorInterruptsRunningScript()
{
    QTemporaryDir temporary;
    auto manager = std::make_unique<PluginManager>(temporary.filePath("installed"));
    QString error;
    const auto path = writePackage(temporary.path(), package("while (true) {}"));
    QVERIFY(manager->install(path, &error, false, PluginManager::inspect(path).packageHash));
    QVERIFY(manager->execute("test.tools", "uppercase", {}, &error));
    manager.reset();
}

void PluginManagerTest::appliesSelectionAndDocumentAsSingleUndo()
{
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>();
    auto* editor = window.findChild<EditorWidget*>();
    QString error;
    QVERIFY(manager->install(writePackage(temporary.path(), package()), &error));
    editor->setTextUtf8("hello world");
    editor->markSaved();
    editor->setSel(0, 5);
    auto* action = window.findChild<QAction*>("plugin.test.tools.uppercase");
    QVERIFY(action);
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    action->trigger();
    QVERIFY(!editor->isEnabled());
    QVERIFY(!window.findChild<QTabBar*>("documentTabBar")->isEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), QByteArray("HELLO world"));
    QVERIFY(editor->isEnabled());
    editor->undo();
    QCOMPARE(editor->textUtf8(), QByteArray("hello world"));
    QVERIFY(!editor->modify());
    QVERIFY(manager->setEnabled("test.tools", false, &error));
    QVERIFY(!window.findChild<QAction*>("plugin.test.tools.uppercase"));
    QVERIFY(manager->uninstall("test.tools", &error));
    auto object = package("return context.text + '\\nadded';");
    auto command = object["commands"].toArray().at(0).toObject();
    command["input"] = "document"; command["output"] = "replaceDocument";
    object["commands"] = QJsonArray{command};
    QVERIFY(manager->install(writePackage(temporary.path(), object), &error));
    finished.clear();
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), QByteArray("hello world\nadded"));
    editor->undo();
    QCOMPARE(editor->textUtf8(), QByteArray("hello world"));
}

void PluginManagerTest::discardsResultAfterDocumentChanges()
{
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>();
    auto* editor = window.findChild<EditorWidget*>();
    QString error;
    QVERIFY(manager->install(writePackage(temporary.path(), package()), &error));
    editor->setTextUtf8("hello world"); editor->setSel(0, 5);
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    editor->appendTextUtf8(" changed"); // Simulate a programmatic edit while the UI is locked.
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), QByteArray("hello changed world"));
}

void PluginManagerTest::insertsAtCaretAndPreservesLineEnding()
{
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>();
    auto* editor = window.findChild<EditorWidget*>();
    QString error;
    auto object = package("return context.lineEnding + '生成';");
    auto command = object["commands"].toArray().at(0).toObject();
    command["input"] = "none"; command["output"] = "insert";
    object["commands"] = QJsonArray{command};
    QVERIFY(manager->install(writePackage(temporary.path(), object), &error));
    editor->setTextUtf8("abcd");
    editor->markSaved();
    editor->setInsertionLineEnding(LineEnding::CrLf);
    editor->setSel(1, 3);
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), QString::fromUtf8("abc\r\n生成d").toUtf8());
    editor->undo();
    QCOMPARE(editor->textUtf8(), QByteArray("abcd"));
}

void PluginManagerTest::enforcesHostInputLimit()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), package()), &error));
    QVERIFY(!manager.execute("test.tools", "uppercase",
        {{"text", QString(PluginManager::maximumTextBytes + 1, 'a')}}, &error));
    QVERIFY(error.contains("8 MiB"));
    QVERIFY(!manager.isRunning());
}

void PluginManagerTest::exampleCanBeInstalledFromManagerDialog()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    PluginDialog dialog(&manager);
    auto* table = dialog.findChild<QTableWidget*>("pluginTable");
    QCOMPARE(table->rowCount(), 0);
    for (auto* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == "Install Example") { button->click(); break; }
    }
    QCOMPARE(manager.plugins().size(), 1);
    QCOMPARE(manager.plugins().front().id, "vinson.text-tools");
    QCOMPARE(manager.plugins().front().commands.size(), 11);
    QCOMPARE(table->rowCount(), 1);
}

void PluginManagerTest::persistsTypedSettingsAndValidatesParameters()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), configurablePackage()), &error));
    const QKeySequence shortcut("Ctrl+Alt+Shift+R");
    QVERIFY(manager.configure("test.tools", {{"prefix", ">>"}, {"width", 4}, {"enabled", false}, {"order", "desc"}},
        {{"uppercase", shortcut}}, &error));
    PluginManager restarted(manager.directory());
    QCOMPARE(restarted.plugins().front().values.value("prefix").toString(), ">>");
    QCOMPARE(restarted.plugins().front().values.value("width").toInt(), 4);
    QCOMPARE(restarted.plugins().front().values.value("enabled").toBool(), false);
    QCOMPARE(restarted.plugins().front().values.value("order").toString(), "desc");
    QCOMPARE(restarted.plugins().front().commands.front().shortcut, shortcut);
    QSignalSpy finished(&restarted, &PluginManager::commandFinished);
    QVERIFY(restarted.execute("test.tools", "uppercase", {{"text", "a"},
        {"settings", QVariantMap{{"prefix", "injected"}}}, {"parameters", QVariantMap{{"count", 3}}}}, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.front().at(1).toString(), ">>aaa");
    finished.clear();
    QVERIFY(!restarted.execute("test.tools", "uppercase", {{"parameters", QVariantMap{{"count", 10}}}}, &error));
    QVERIFY(error.contains("Count"));
    QVERIFY(!restarted.isRunning());
    QVERIFY(restarted.execute("test.tools", "uppercase", {{"text", "b"}}, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.front().at(1).toString(), ">>bb");
}

void PluginManagerTest::validatesSettingsAndShortcutConflicts()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), configurablePackage()), &error));
    const auto initial = manager.plugins().front().values;
    for (const auto& bad : {QVariantMap{{"width", 0}}, QVariantMap{{"enabled", "yes"}},
                           QVariantMap{{"prefix", "too long"}}, QVariantMap{{"order", "invalid"}},
                           QVariantMap{{"unknown", true}}}) {
        QVERIFY(!manager.configure("test.tools", bad, {}, &error));
        QCOMPARE(manager.plugins().front().values, initial);
    }
    manager.setReservedShortcuts({QKeySequence("Ctrl+S"), QKeySequence("Ctrl+K, Ctrl+C")});
    QVERIFY(!manager.configure("test.tools", {}, {{"uppercase", QKeySequence("Ctrl+S")}}, &error));
    QVERIFY(error.contains("already in use"));
    QVERIFY(!manager.configure("test.tools", {}, {{"uppercase", QKeySequence("Ctrl+K")}}, &error));
    QVERIFY(!manager.configure("test.tools", {}, {{"uppercase", QKeySequence("Esc")}}, &error));
    auto other = package(); other["id"] = "other.tools";
    auto command = other["commands"].toArray().at(0).toObject(); command["shortcut"] = "Ctrl+Alt+Shift+T";
    other["commands"] = QJsonArray{command};
    QVERIFY(manager.install(writePackage(temporary.path(), other), &error));
    QVERIFY(!manager.configure("test.tools", {}, {{"uppercase", QKeySequence("Ctrl+Alt+Shift+T")}}, &error));
    QVERIFY(manager.setEnabled("other.tools", false, &error));
    QVERIFY(manager.configure("test.tools", {}, {{"uppercase", QKeySequence("Ctrl+Alt+Shift+T")}}, &error));
    QVERIFY(manager.configure("test.tools", {}, {{"uppercase", QKeySequence()}}, &error));
    QVERIFY(manager.plugins().back().commands.front().shortcut.isEmpty());
}

void PluginManagerTest::preservesCorruptPreferenceFile()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), configurablePackage()), &error));
    QFile preferences(QDir(manager.directory()).filePath("preferences.json"));
    QVERIFY(preferences.open(QIODevice::WriteOnly)); preferences.write("broken-json"); preferences.close();
    manager.reload();
    QCOMPARE(manager.plugins().front().values.value("width").toInt(), 2);
    QVERIFY(!manager.configure("test.tools", {{"width", 3}}, {}, &error));
    QVERIFY(error.contains("preferences.json"));
    QVERIFY(preferences.open(QIODevice::ReadOnly));
    QCOMPARE(preferences.readAll(), QByteArray("broken-json"));
}

void PluginManagerTest::updatesPackageWithoutLosingPreferences()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    auto object = configurablePackage();
    QVERIFY(manager.install(writePackage(temporary.path(), object), &error));
    QVERIFY(manager.configure("test.tools", {{"width", 5}}, {{"uppercase", QKeySequence()}}, &error));
    QVERIFY(manager.setEnabled("test.tools", false, &error));
    object["version"] = "2.0";
    auto fields = object["settings"].toArray();
    auto width = fields.at(1).toObject(); width["maximum"] = 4; fields[1] = width; object["settings"] = fields;
    QVERIFY(manager.install(writePackage(temporary.path(), object), &error, true));
    QCOMPARE(manager.plugins().front().version, "2.0");
    QVERIFY(!manager.plugins().front().enabled);
    QCOMPARE(manager.plugins().front().values.value("width").toInt(), 2); // Old invalid value falls back to the new schema default.
    QVERIFY(manager.plugins().front().commands.front().shortcut.isEmpty());
    auto invalid = object; invalid["commands"] = QJsonArray{};
    QVERIFY(!manager.install(writePackage(temporary.path(), invalid), &error, true));
    PluginManager restarted(manager.directory());
    QCOMPARE(restarted.plugins().front().version, "2.0");
    QVERIFY(!restarted.plugins().front().enabled);
}

void PluginManagerTest::editsPluginConfigurationThroughDialog()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    QVERIFY(manager.install(writePackage(temporary.path(), configurablePackage()), &error));
    PluginConfigurationDialog dialog(&manager, manager.plugins().front());
    dialog.findChild<QSpinBox*>("pluginField.width")->setValue(7);
    dialog.findChild<QCheckBox*>("pluginField.enabled")->setChecked(false);
    dialog.findChild<QComboBox*>("pluginField.order")->setCurrentText("desc");
    dialog.findChild<QLineEdit*>("pluginField.prefix")->setText("##");
    dialog.findChild<QKeySequenceEdit*>("pluginShortcut.uppercase")->setKeySequence(QKeySequence("Ctrl+Alt+Shift+R"));
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(manager.plugins().front().values.value("width").toInt(), 7);
    QCOMPARE(manager.plugins().front().values.value("prefix").toString(), "##");
    PluginConfigurationDialog cancelled(&manager, manager.plugins().front());
    cancelled.findChild<QSpinBox*>("pluginField.width")->setValue(1);
    cancelled.reject();
    QCOMPARE(manager.plugins().front().values.value("width").toInt(), 7);
}

void PluginManagerTest::supportsLineAndSelectionOrDocumentInputs_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("selected");
    QTest::addColumn<QByteArray>("expected");
    QTest::newRow("current Unicode line") << "line" << false << QString::fromUtf8("ab\n猫B\ncd").toUtf8();
    QTest::newRow("selected range") << "selectionOrDocument" << true << QString::fromUtf8("AB\n猫b\ncd").toUtf8();
    QTest::newRow("whole document fallback") << "selectionOrDocument" << false << QString::fromUtf8("AB\n猫B\nCD").toUtf8();
}

void PluginManagerTest::supportsLineAndSelectionOrDocumentInputs()
{
    QFETCH(QString, input); QFETCH(bool, selected); QFETCH(QByteArray, expected);
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>(); auto* editor = window.findChild<EditorWidget*>();
    auto object = package(); auto command = object["commands"].toArray().at(0).toObject();
    command["input"] = input; command["output"] = "replaceInput"; object["commands"] = QJsonArray{command};
    QString error; QVERIFY(manager->install(writePackage(temporary.path(), object), &error));
    const auto original = QString::fromUtf8("ab\n猫b\ncd").toUtf8(); editor->setTextUtf8(original);
    if (selected) editor->setSel(0, 2); else editor->setSel(6, 6);
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), expected);
    editor->undo(); QCOMPARE(editor->textUtf8(), original);
}

void PluginManagerTest::promptsForParametersAndCancelsCleanly()
{
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>(); auto* editor = window.findChild<EditorWidget*>();
    QString error; QVERIFY(manager->install(writePackage(temporary.path(), configurablePackage()), &error));
    editor->setTextUtf8("a"); editor->setSel(0, 1);
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    QTimer::singleShot(0, &window, [] {
        auto* dialog = qobject_cast<PluginParametersDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChild<QSpinBox*>("pluginField.count")->setValue(3);
        dialog->accept();
    });
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), QByteArray(">aaa"));
    editor->undo(); editor->setSel(0, 1); finished.clear();
    QTimer::singleShot(0, &window, [] {
        auto* dialog = qobject_cast<PluginParametersDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog); dialog->reject();
    });
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    QVERIFY(!manager->isRunning()); QCOMPARE(finished.count(), 0); QCOMPARE(editor->textUtf8(), QByteArray("a"));
}

void PluginManagerTest::runsCommandShortcutAndRejectsEditorConflict()
{
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>(); auto* editor = window.findChild<EditorWidget*>();
    auto object = package(); auto command = object["commands"].toArray().at(0).toObject();
    command["shortcut"] = "Ctrl+S"; object["commands"] = QJsonArray{command};
    QString error; QVERIFY(manager->install(writePackage(temporary.path(), object), &error));
    auto* action = window.findChild<QAction*>("plugin.test.tools.uppercase");
    QVERIFY(action->shortcut().isEmpty()); QVERIFY(!action->toolTip().isEmpty());
    QVERIFY(!manager->configure("test.tools", {}, {{"uppercase", QKeySequence("Ctrl+S")}}, &error));
    const QKeySequence shortcut("Ctrl+Alt+Shift+U");
    QVERIFY(manager->configure("test.tools", {}, {{"uppercase", shortcut}}, &error));
    action = window.findChild<QAction*>("plugin.test.tools.uppercase"); QCOMPARE(action->shortcut(), shortcut);
    QVERIFY(window.actions().contains(action));
    window.show(); window.activateWindow(); editor->QWidget::setFocus(); QTest::qWait(30);
    editor->setTextUtf8("abc"); editor->setSel(0, 3);
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    QTest::keyClick(editor, Qt::Key_U, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(editor->textUtf8(), QByteArray("ABC"));
}

void PluginManagerTest::copiesResultsAndCreatesUnsavedTabs()
{
    QTemporaryDir temporary;
    MainWindow window(nullptr, false, temporary.filePath("installed"));
    auto* manager = window.findChild<PluginManager*>(); auto* editor = window.findChild<EditorWidget*>();
    auto object = package("return 'report';"); auto command = object["commands"].toArray().at(0).toObject();
    command["input"] = "none"; command["output"] = "newDocument";
    auto copy = command; copy["id"] = "copy"; copy["output"] = "clipboard"; copy["script"] = "return 'copied';";
    object["commands"] = QJsonArray{command, copy};
    QString error; QVERIFY(manager->install(writePackage(temporary.path(), object), &error));
    editor->setTextUtf8("original");
    QSignalSpy finished(manager, &PluginManager::commandFinished);
    window.findChild<QAction*>("plugin.test.tools.uppercase")->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    auto* tabs = window.findChild<QTabBar*>("documentTabBar");
    QCOMPARE(tabs->count(), 2); QCOMPARE(editor->textUtf8(), QByteArray("report")); QVERIFY(editor->modify());
    editor->undo(); QVERIFY(editor->textUtf8().isEmpty());
    tabs->setCurrentIndex(0); QCOMPARE(editor->textUtf8(), QByteArray("original")); QVERIFY(!editor->modify());
    const auto clipboard = qApp->clipboard()->text();
    finished.clear(); window.findChild<QAction*>("plugin.test.tools.copy")->trigger();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(qApp->clipboard()->text(), "copied"); QCOMPARE(editor->textUtf8(), QByteArray("original"));
    QVERIFY(!editor->modify()); qApp->clipboard()->setText(clipboard);
}

void PluginManagerTest::extendedExamplesExecute()
{
    QTemporaryDir temporary; PluginManager manager(temporary.filePath("installed")); QString error;
    QVERIFY2(manager.install(":/plugins/text-tools.vinson-plugin", &error), qPrintable(error));
    QVERIFY(manager.configure("vinson.text-tools", {{"indent", 4}, {"case-sensitive", false}}, {}, &error));
    QSignalSpy finished(&manager, &PluginManager::commandFinished);
    const QVariantMap context{{"text", "b\na\nb\n"}, {"lineEnding", "\n"}};
    QVERIFY(manager.execute("vinson.text-tools", "deduplicate-lines", context, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000); QCOMPARE(finished.front().at(1).toString(), "b\na\n");
    finished.clear(); QVERIFY(manager.execute("vinson.text-tools", "sort-lines", context, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000); QCOMPARE(finished.front().at(1).toString(), "a\nb\nb\n");
    finished.clear(); QVERIFY(manager.execute("vinson.text-tools", "format-json",
        {{"text", "{\"a\":1}"}, {"lineEnding", "\r\n"}}, &error));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.front().at(1).toString(), "{\r\n    \"a\": 1\r\n}");
}

void PluginManagerTest::scansSecurityPatterns_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QString>("category");
    QTest::newRow("eval") << "// ignored\n\nreturn eval(context.text);" << "Dynamic code execution";
    QTest::newRow("computed constructor") << "// ignored\n\nreturn context.text['constructor']['constructor']('return 1')();" << "Dynamic code execution";
    QTest::newRow("escaped identifier") << "// ignored\n\nreturn \\u0065val(context.text);" << "Obfuscated code";
    QTest::newRow("network") << "// ignored\n\nfetch('https://example.invalid', {body: context.text}); return '';" << "Network access";
    QTest::newRow("escaped property") << "// ignored\n\nglobalThis['f\\x65tch'](context.text); return '';" << "Network access";
    QTest::newRow("files") << "// ignored\n\nfs.unlinkSync('file'); return '';" << "File access or deletion";
    QTest::newRow("process") << "// ignored\n\nrequire('child_process').spawn('cmd'); return '';" << "System or module access";
    QTest::newRow("loop") << "// ignored\n\nfor (;;) {}" << "Potential infinite loop";
    QTest::newRow("encoding") << "// ignored\n\nreturn atob(context.text);" << "Encoded content";
    QTest::newRow("template expression") << "// ignored\n\nreturn `result: ${eval(context.text)}`;" << "Dynamic code execution";
    QTest::newRow("safe text comments regex") << "// eval(fetch())\n/* while (true) {} */\nreturn 'require eval fetch' + /eval|fetch|while/.source;" << "";
    QTest::newRow("safe template") << "// ignored\n\nreturn `eval(fetch()) ${context.text}`;" << "";
}

void PluginManagerTest::scansSecurityPatterns()
{
    QFETCH(QString, script);
    QFETCH(QString, category);
    QTemporaryDir temporary;
    const auto inspected = PluginManager::inspect(writePackage(temporary.path(), package(script)));
    QVERIFY2(inspected.error.isEmpty(), qPrintable(inspected.error));
    if (category.isEmpty()) { QVERIFY(inspected.risks.isEmpty()); return; }
    bool found = false;
    for (const auto& risk : inspected.risks) {
        QCOMPARE(risk.commandId, "uppercase");
        QCOMPARE(risk.commandTitle, "Uppercase");
        QCOMPARE(risk.line, 3);
        QVERIFY(!risk.explanation.isEmpty());
        QVERIFY(!risk.evidence.isEmpty());
        found |= risk.category == category;
    }
    QVERIFY(found);
}

void PluginManagerTest::requiresConsentForExactPackage()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    QString error;
    auto path = writePackage(temporary.path(), package("return eval(context.text);"));
    const auto reviewed = PluginManager::inspect(path);
    QVERIFY(!manager.install(path, &error));
    QVERIFY(error.contains("confirm"));
    QVERIFY(manager.plugins().isEmpty());
    QVERIFY(!QFileInfo::exists(manager.directory()));
    QVERIFY(!manager.install(path, &error, false, QByteArray("wrong hash")));
    QVERIFY(manager.install(path, &error, false, reviewed.packageHash));
    QVERIFY(manager.setEnabled("test.tools", false, &error));
    auto update = package("while (true) {} return ''; ");
    update["version"] = "2.0";
    path = writePackage(temporary.path(), update);
    QVERIFY(!manager.install(path, &error, true, reviewed.packageHash));
    QVERIFY(!manager.install(path, &error, true));
    QCOMPARE(manager.plugins().front().version, "1.0");
    QVERIFY(!manager.plugins().front().enabled);
    QVERIFY(manager.install(path, &error, true, PluginManager::inspect(path).packageHash));
    QCOMPARE(manager.plugins().front().version, "2.0");
    QVERIFY(!manager.plugins().front().enabled);
    // Consent also binds a clean replacement to the bytes the user reviewed.
    const auto oldHash = PluginManager::inspect(path).packageHash;
    path = writePackage(temporary.path(), package());
    QVERIFY(!manager.install(path, &error, true, oldHash));
    QCOMPARE(manager.plugins().front().version, "2.0");
}

void PluginManagerTest::boundsSecurityFindingsAndPreservesLineLocations()
{
    const auto risks = scanPluginScript(QStringLiteral("eval('');\r\n\rfetch('');\u2028fs.unlink('');"), "test", "Test");
    QCOMPARE(risks.size(), 3);
    QCOMPARE(risks[0].line, 1); QCOMPARE(risks[0].evidence, "eval('');");
    QCOMPARE(risks[1].line, 3); QCOMPARE(risks[1].evidence, "fetch('');");
    QCOMPARE(risks[2].line, 4); QCOMPARE(risks[2].evidence, "fs.unlink('');");
    const auto many = scanPluginScript(QStringLiteral("eval('');\n").repeated(1000), "test", "Test");
    QCOMPARE(many.size(), 64);
    QCOMPARE(many.last().line, 64);
    const auto unicodeComment = scanPluginScript(QStringLiteral("// harmless\u2028eval('');"), "test", "Test");
    QCOMPARE(unicodeComment.size(), 1); QCOMPARE(unicodeComment.front().line, 2);
    const auto minified = scanPluginScript(QStringLiteral("void 0;").repeated(1000) + "eval(context.text);", "test", "Test");
    QCOMPARE(minified.size(), 1);
    QVERIFY(minified.front().evidence.contains("eval(context.text)"));
    QVERIFY(minified.front().evidence.size() <= 242);
}

void PluginManagerTest::confirmsRiskyImportsAndUpdates()
{
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    PluginDialog dialog(&manager);
    const auto path = writePackage(temporary.path(), package("return eval(context.text);"));
    auto review = [](bool accept) {
        auto* modal = qobject_cast<QDialog*>(qApp->activeModalWidget());
        QVERIFY(modal);
        QCOMPARE(modal->objectName(), "pluginSecurityReview");
        auto* findings = modal->findChild<QTextBrowser*>("pluginSecurityFindings");
        QVERIFY(findings);
        QVERIFY(findings->toPlainText().contains("Dynamic code execution"));
        QVERIFY(findings->toPlainText().contains("script line 1"));
        QVERIFY(findings->toPlainText().contains("eval(context.text)"));
        auto* buttons = modal->findChild<QDialogButtonBox*>();
        QVERIFY(buttons->button(QDialogButtonBox::Cancel)->isDefault());
        const auto artifacts = qEnvironmentVariable("VINSON_TEST_ARTIFACTS");
        if (!artifacts.isEmpty()) QVERIFY(modal->grab().save(QDir(artifacts).filePath("plugin-security-review.png")));
        if (accept) QTest::mouseClick(modal->findChild<QPushButton*>("confirmRiskyPluginButton"), Qt::LeftButton);
        else QTest::keyClick(modal, Qt::Key_Escape);
    };
    QTimer::singleShot(0, &dialog, [review] { review(false); });
    dialog.importPlugin(path);
    QVERIFY(manager.plugins().isEmpty());
    QTimer::singleShot(0, &dialog, [review] { review(true); });
    dialog.importPlugin(path);
    QCOMPARE(manager.plugins().size(), 1);
    auto update = package("return eval(context.text) + 'updated';");
    update["version"] = "2.0";
    writePackage(temporary.path(), update);
    auto confirmReplacement = [&dialog, review](bool accept) {
        QTimer::singleShot(0, &dialog, [&dialog, review, accept] {
            auto* question = qobject_cast<QMessageBox*>(qApp->activeModalWidget());
            QVERIFY(question);
            QTest::mouseClick(question->button(QMessageBox::Yes), Qt::LeftButton);
            QTimer::singleShot(0, &dialog, [review, accept] { review(accept); });
        });
    };
    confirmReplacement(false);
    dialog.importPlugin(path);
    QCOMPARE(manager.plugins().front().version, "1.0");
    confirmReplacement(true);
    dialog.importPlugin(path);
    QCOMPARE(manager.plugins().front().version, "2.0");
}

void PluginManagerTest::previewsBundledDeveloperGuides()
{
    QTranslator translator, qtTranslator;
    QVERIFY(installApplicationTranslation(*qApp, translator, qtTranslator, QLocale(QStringLiteral("zh_CN"))));
    QTemporaryDir temporary;
    PluginManager manager(temporary.filePath("installed"));
    PluginDialog dialog(&manager);
    auto* tabs = dialog.findChild<QTabWidget*>("pluginTabs");
    QVERIFY(tabs); QCOMPARE(tabs->count(), 2);
    auto* preview = dialog.findChild<QTextBrowser*>("pluginGuidePreview");
    auto* language = dialog.findChild<QComboBox*>("pluginGuideLanguage");
    QVERIFY(preview); QVERIFY(language); QVERIFY(preview->isReadOnly());
    QCOMPARE(language->currentIndex(), 0);
    QCOMPARE(tabs->tabText(1), QStringLiteral("开发文档"));
    language->setCurrentIndex(1);
    QVERIFY(preview->toPlainText().contains("API v1"));
    QVERIFY(preview->toPlainText().contains("Security review"));
    QVERIFY(!preview->openExternalLinks()); QVERIFY(!preview->openLinks());
    language->setCurrentIndex(0);
    QVERIFY(preview->toPlainText().contains(QStringLiteral("插件使用与开发")));
    QVERIFY(preview->toPlainText().contains(QStringLiteral("安装安全检测")));
    const auto artifacts = qEnvironmentVariable("VINSON_TEST_ARTIFACTS");
    if (!artifacts.isEmpty()) {
        tabs->setCurrentIndex(1);
        dialog.show();
        qApp->processEvents();
        QVERIFY(dialog.grab().save(QDir(artifacts).filePath("plugin-developer-guide.png")));
    }
    QVERIFY(QMetaObject::invokeMethod(preview, "anchorClicked", Qt::DirectConnection, Q_ARG(QUrl, QUrl("PLUGINS.md"))));
    QCOMPARE(language->currentIndex(), 1);
    QVERIFY(QMetaObject::invokeMethod(preview, "anchorClicked", Qt::DirectConnection,
        Q_ARG(QUrl, QUrl("../examples/plugins/text-tools.vinson-plugin"))));
    auto* source = dialog.findChild<QTextBrowser*>("pluginExampleSource");
    QVERIFY(source); QVERIFY(source->toPlainText().contains("vinson.text-tools"));
}

QTEST_MAIN(PluginManagerTest)
#include "PluginManagerTest.moc"
