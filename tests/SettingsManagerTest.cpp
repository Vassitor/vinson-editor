#include "settings/SettingsManager.h"
#include "settings/ThemeManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class SettingsManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsDefaultsForMissingFile();
    void roundTripsValidatedSettings();
    void rejectsInvalidPersistedValues();
};

void SettingsManagerTest::returnsDefaultsForMissingFile()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("missing.ini"));
    vinson::SettingsManager manager(path);

    QVERIFY(manager.load() == vinson::SettingsManager::defaults());
    QCOMPARE(manager.fileName(), path);
    QVERIFY(!QFileInfo::exists(path));
}

void SettingsManagerTest::roundTripsValidatedSettings()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("settings.ini"));
    vinson::ApplicationSettings expected = vinson::SettingsManager::defaults();
    expected.appearance.font.setPointSizeF(18.5);
    expected.appearance.textColor = QColor(1, 2, 3, 255);
    expected.appearance.backgroundColor = QColor(4, 5, 6, 17);
    expected.appearance.cursorColor = QColor(7, 8, 9, 255);
    expected.appearance.selectionTextColor = QColor(10, 11, 12, 255);
    expected.windowGeometry = QByteArray("geometry-state");
    expected.lastDirectory = directory.path();
    expected.wordWrap = true;
    expected.lineNumbers = false;
    expected.alwaysOnTop = true;
    expected.frameless = true;

    {
        vinson::SettingsManager manager(path);
        QVERIFY(manager.save(expected));
    }

    vinson::SettingsManager restoredManager(path);
    QVERIFY(restoredManager.load() == expected);
    QVERIFY(QFileInfo::exists(path));
}

void SettingsManagerTest::rejectsInvalidPersistedValues()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("invalid.ini"));
    {
        QSettings raw(path, QSettings::IniFormat);
        raw.setValue(QStringLiteral("appearance/fontFamily"),
                     QStringLiteral("__missing_font_family__"));
        raw.setValue(QStringLiteral("appearance/fontSize"),
                     QStringLiteral("nan"));
        raw.setValue(QStringLiteral("appearance/textColor"),
                     QStringLiteral("not-a-color"));
        raw.setValue(QStringLiteral("appearance/backgroundColor"),
                     QStringLiteral("#xyz"));
        raw.setValue(QStringLiteral("appearance/cursorColor"),
                     QStringLiteral("invalid"));
        raw.setValue(QStringLiteral("appearance/selectionTextColor"),
                     QStringLiteral("invalid"));
        raw.setValue(QStringLiteral("window/geometry"),
                     QByteArray(64 * 1024 + 1, 'x'));
        raw.setValue(QStringLiteral("view/wordWrap"),
                     QStringLiteral("sometimes"));
        raw.setValue(QStringLiteral("view/lineNumbers"),
                     QStringLiteral("2"));
        raw.setValue(QStringLiteral("window/alwaysOnTop"),
                     QStringLiteral("invalid"));
        raw.setValue(QStringLiteral("window/frameless"),
                     QStringLiteral("invalid"));
        raw.setValue(QStringLiteral("files/lastDirectory"),
                     directory.filePath(QStringLiteral("missing")));
        raw.sync();
    }

    vinson::SettingsManager manager(path);
    QVERIFY(manager.load() == vinson::SettingsManager::defaults());
}

QTEST_MAIN(SettingsManagerTest)
#include "SettingsManagerTest.moc"
