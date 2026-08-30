#include "settings/Localization.h"

#include <QCoreApplication>
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
    QVERIFY(vinson::installApplicationTranslation(
        *qApp, translator, QLocale(QStringLiteral("zh_CN"))));

    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "&File"),
             QStringLiteral("文件(&F)"));
    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "Ready"),
             QStringLiteral("就绪"));
    QCOMPARE(QCoreApplication::translate("vinson::SettingsDialog", "Appearance"),
             QStringLiteral("外观"));

    qApp->removeTranslator(&translator);
}

void LocalizationTest::keepsEnglishAsFallback()
{
    QTranslator translator;
    QVERIFY(!vinson::installApplicationTranslation(
        *qApp, translator, QLocale(QStringLiteral("en_US"))));
    QCOMPARE(QCoreApplication::translate("vinson::MainWindow", "&File"),
             QStringLiteral("&File"));
}

QTEST_MAIN(LocalizationTest)
#include "LocalizationTest.moc"
