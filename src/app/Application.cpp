#include "app/Application.h"

#include "app/Version.h"
#include "app/SingleInstance.h"
#include "settings/Localization.h"
#include "window/MainWindow.h"
#include "window/NativeWindowAppearance.h"
#include "window/TrayController.h"

#include <QCoreApplication>
#include <QIcon>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>

namespace vinson {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("VinsonEditor"));
    QCoreApplication::setApplicationName(QStringLiteral("Vinson Editor"));
    QCoreApplication::setApplicationVersion(QStringLiteral(VINSON_APP_VERSION));
    setWindowIcon(QIcon(QStringLiteral(":/icons/icon.svg")));

    QLocale interfaceLocale = QLocale::system();
    const QString languageOption = QStringLiteral("--language=");
    for (const QString& argument : arguments()) {
        if (argument.startsWith(languageOption)) {
            interfaceLocale = QLocale(argument.mid(languageOption.size()));
            break;
        }
    }
    (void)installApplicationTranslation(
        *this, applicationTranslator_, qtTranslator_, interfaceLocale);
}

int Application::run()
{
    const bool smokeTest = arguments().contains(QStringLiteral("--smoke-test"));
    const QStringList startupPaths = startupFilePaths();
    if (!smokeTest) {
        singleInstance_ = new SingleInstance(this);
        const SingleInstance::StartResult result =
            singleInstance_->start(startupPaths);
        if (result == SingleInstance::StartResult::Forwarded) {
            return 0;
        }
        if (result == SingleInstance::StartResult::Error) {
            QMessageBox::critical(
                nullptr, tr("Vinson Editor"),
                tr("Could not start the single-instance service: %1")
                    .arg(singleInstance_->errorString()));
            return 1;
        }
    }
    MainWindow mainWindow;
    if (singleInstance_ != nullptr) {
        connect(singleInstance_, &SingleInstance::openRequested,
                &mainWindow, &MainWindow::handleExternalOpenRequest);
    }
    TrayController trayController(&mainWindow, this);
    if (!smokeTest && trayController.isAvailable()) {
        setQuitOnLastWindowClosed(false);
        mainWindow.setCloseToTrayEnabled(true);
        trayController.show();
    }
    connect(&trayController, &TrayController::quitRequested,
            &mainWindow, &MainWindow::requestApplicationQuit);
    connect(&trayController, &TrayController::settingsRequested,
            &mainWindow, &MainWindow::showSettings);
    connect(&trayController, &TrayController::editorFocusRequested,
            &mainWindow, &MainWindow::focusEditor);
    connect(&mainWindow, &MainWindow::bossKeyChanged,
            &trayController, &TrayController::setBossKey);
    connect(&mainWindow, &MainWindow::focusShortcutChanged,
            &trayController, &TrayController::setFocusShortcut);
    connect(&trayController, &TrayController::bossKeyRegistrationFailed,
            &mainWindow, &MainWindow::handleBossKeyRegistrationFailure);
    connect(&trayController,
            &TrayController::focusShortcutRegistrationFailed,
            &mainWindow,
            &MainWindow::handleFocusShortcutRegistrationFailure);
    connect(&mainWindow, &MainWindow::applicationQuitAccepted,
            this, &QCoreApplication::quit);
    if (!smokeTest) {
        (void)trayController.setBossKey(mainWindow.bossKey());
        (void)trayController.setFocusShortcut(mainWindow.focusShortcut());
    }
    // The complete widget tree must exist before creating the native handle,
    // while redirection alpha must be enabled before the first framed show.
    (void)mainWindow.winId();
    (void)setNativeBackgroundAlphaEnabled(&mainWindow, true);
    mainWindow.show();
    mainWindow.openFiles(startupPaths);

    // The smoke mode exercises native widget creation in CI without leaving
    // an interactive application running indefinitely.
    if (smokeTest) {
        QTimer::singleShot(100, this, &QCoreApplication::quit);
    }

    return exec();
}

QStringList Application::startupFilePaths() const
{
    QStringList paths;
    const QStringList commandLine = arguments();
    for (qsizetype index = 1; index < commandLine.size(); ++index) {
        const QString& argument = commandLine.at(index);
        if (argument == QStringLiteral("--smoke-test")
            || argument.startsWith(QStringLiteral("--language="))
            || argument.startsWith(QLatin1Char('-'))) {
            continue;
        }
        paths.append(QFileInfo(argument).absoluteFilePath());
    }
    return paths;
}

} // namespace vinson
