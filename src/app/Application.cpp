#include "app/Application.h"

#include "app/Version.h"
#include "settings/Localization.h"
#include "window/MainWindow.h"
#include "window/NativeWindowAppearance.h"
#include "window/TrayController.h"

#include <QCoreApplication>
#include <QIcon>
#include <QLocale>
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
    (void)installApplicationTranslation(*this, translator_, interfaceLocale);
}

int Application::run()
{
    const bool smokeTest = arguments().contains(QStringLiteral("--smoke-test"));
    MainWindow mainWindow;
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

    // The smoke mode exercises native widget creation in CI without leaving
    // an interactive application running indefinitely.
    if (smokeTest) {
        QTimer::singleShot(100, this, &QCoreApplication::quit);
    }

    return exec();
}

} // namespace vinson
