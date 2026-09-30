#include "settings/Localization.h"

#include <QCoreApplication>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTranslator>
#include <QtTest>

class LocalizationTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsSimplifiedChineseTranslation();
    void keepsEnglishAsFallback();
};

void LocalizationTest::loadsSimplifiedChineseTranslation()
{
    QTranslator translator;
    QTranslator qtTranslator;
    QVERIFY(vinson::installApplicationTranslation(
        *qApp, translator, qtTranslator, QLocale(QStringLiteral("zh_CN"))));

    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "&File"),
             QStringLiteral("文件(&F)"));
    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "Ready"),
             QStringLiteral("就绪"));
    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "&Bookmarks"),
             QStringLiteral("书签(&B)"));
    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "&Next Bookmark"),
             QStringLiteral("下一处书签(&N)"));
    QCOMPARE(QCoreApplication::translate("vinson::SettingsDialog", "Appearance"),
             QStringLiteral("外观"));
    QCOMPARE(QCoreApplication::translate("vinson::SettingsDialog", "Custom Styles"),
             QStringLiteral("自定义样式"));
    QCOMPARE(QCoreApplication::translate("vinson::SettingsDialog", "Background opacity:"),
             QStringLiteral("背景不透明度："));
    QCOMPARE(QCoreApplication::translate("vinson::SettingsDialog", "No saved styles"),
             QStringLiteral("暂无已保存的样式"));
    QCOMPARE(QCoreApplication::translate(
                 "vinson::SettingsDialog", "Application Shortcuts"),
             QStringLiteral("应用快捷键"));
    QCOMPARE(QCoreApplication::translate(
                 "vinson::MainWindow", "Exit Minimal Mode"),
             QStringLiteral("退出极简模式"));
    QCOMPARE(QCoreApplication::translate(
                 "vinson::MainWindow", "&Appearance and Shortcuts…"),
             QStringLiteral("外观和快捷键(&A)…"));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"),
             QStringLiteral("保存"));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"),
             QStringLiteral("丢弃"));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"),
             QStringLiteral("取消"));
    QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Restore Defaults"),
             QStringLiteral("恢复默认值"));

    QDialogButtonBox settingsButtons(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
        | QDialogButtonBox::RestoreDefaults);
    QCOMPARE(settingsButtons.button(QDialogButtonBox::Ok)->text(),
             QStringLiteral("确定"));
    QCOMPARE(settingsButtons.button(QDialogButtonBox::Cancel)->text(),
             QStringLiteral("取消"));
    QCOMPARE(settingsButtons.button(
                 QDialogButtonBox::RestoreDefaults)->text(),
             QStringLiteral("恢复默认值"));

    QMessageBox savePrompt;
    savePrompt.setStandardButtons(
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    QCOMPARE(savePrompt.button(QMessageBox::Save)->text(),
             QStringLiteral("保存"));
    QCOMPARE(savePrompt.button(QMessageBox::Discard)->text(),
             QStringLiteral("丢弃"));
    QCOMPARE(savePrompt.button(QMessageBox::Cancel)->text(),
             QStringLiteral("取消"));

    QColorDialog colorDialog;
    colorDialog.setOption(QColorDialog::DontUseNativeDialog);
    auto* colorButtons = colorDialog.findChild<QDialogButtonBox*>();
    QVERIFY(colorButtons != nullptr);
    QCOMPARE(colorButtons->button(QDialogButtonBox::Ok)->text(),
             QStringLiteral("确定"));
    QCOMPARE(colorButtons->button(QDialogButtonBox::Cancel)->text(),
             QStringLiteral("取消"));

    qApp->removeTranslator(&translator);
    qApp->removeTranslator(&qtTranslator);
}

void LocalizationTest::keepsEnglishAsFallback()
{
    QTranslator translator;
    QTranslator qtTranslator;
    QVERIFY(!vinson::installApplicationTranslation(
        *qApp, translator, qtTranslator, QLocale(QStringLiteral("en_US"))));
    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "&File"),
             QStringLiteral("&File"));
}

QTEST_MAIN(LocalizationTest)
#include "LocalizationTest.moc"
