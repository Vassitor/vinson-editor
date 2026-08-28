#include "app/Application.h"

#include "app/Version.h"
#include "window/MainWindow.h"

#include <QCoreApplication>
#include <QTimer>

namespace vinson {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("VinsonEditor"));
    QCoreApplication::setApplicationName(QStringLiteral("Vinson Editor"));
    QCoreApplication::setApplicationVersion(QStringLiteral(VINSON_APP_VERSION));
}

int Application::run()
{
    MainWindow mainWindow;
    mainWindow.show();

    // The smoke mode exercises native widget creation in CI without leaving
    // an interactive application running indefinitely.
    if (arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(100, this, &QCoreApplication::quit);
    }

    return exec();
}

} // namespace vinson
