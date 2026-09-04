#include "app/SingleInstance.h"

#include <QCoreApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class SingleInstanceTest final : public QObject
{
    Q_OBJECT

private slots:
    void forwardsRequestsToPrimaryInstance();
};

void SingleInstanceTest::forwardsRequestsToPrimaryInstance()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QStringList paths{
        directory.filePath(QStringLiteral("one.txt")),
        directory.filePath(QStringLiteral("two.txt")),
    };
    qputenv("VINSON_SINGLE_INSTANCE_ID",
            directory.path().toUtf8());

    vinson::SingleInstance primary;
    QCOMPARE(primary.start({}),
             vinson::SingleInstance::StartResult::Primary);
    QSignalSpy requestSpy(&primary, &vinson::SingleInstance::openRequested);

    QProcess secondary;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("VINSON_SINGLE_INSTANCE_TEST_CLIENT"),
                       QStringLiteral("1"));
    secondary.setProcessEnvironment(environment);
    secondary.start(QCoreApplication::applicationFilePath(), paths);
    QVERIFY(secondary.waitForStarted(3000));
    QTRY_COMPARE_WITH_TIMEOUT(secondary.state(), QProcess::NotRunning, 5000);
    QVERIFY2(secondary.exitCode() == 0,
             secondary.readAllStandardError().constData());
    QTRY_COMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.takeFirst().at(0).toStringList(), paths);
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VinsonEditorTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SingleInstanceTest"));
    if (qEnvironmentVariableIsSet("VINSON_SINGLE_INSTANCE_TEST_CLIENT")) {
        vinson::SingleInstance secondary;
        const QStringList paths = application.arguments().mid(1);
        if (secondary.start(paths)
            == vinson::SingleInstance::StartResult::Forwarded) {
            return 0;
        }
        qWarning().noquote() << secondary.errorString();
        return 1;
    }
    SingleInstanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "SingleInstanceTest.moc"
