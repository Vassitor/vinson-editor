#include "window/TrayController.h"

#include "ui/MenuAppearance.h"
#include "window/GlobalShortcut.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>

namespace vinson {

TrayController::TrayController(QMainWindow* window, QObject* parent)
    : QObject(parent)
    , window_(window)
    , trayIcon_(new QSystemTrayIcon(this))
    , trayMenu_(new QMenu(window))
    , bossKeyShortcut_(new GlobalShortcut(this))
{
    Q_ASSERT(window_ != nullptr);
    available_ = QSystemTrayIcon::isSystemTrayAvailable();
    trayMenu_->setObjectName(QStringLiteral("trayMenu"));
    applyNativeMenuShadow(trayMenu_);

    trayIcon_->setObjectName(QStringLiteral("systemTrayIcon"));
    trayIcon_->setIcon(QApplication::windowIcon());
    trayIcon_->setToolTip(QCoreApplication::applicationName());

    toggleAction_ = trayMenu_->addAction(QString());
    toggleAction_->setObjectName(QStringLiteral("trayToggleWindowAction"));
    connect(toggleAction_, &QAction::triggered,
            this, &TrayController::toggleWindowVisibility);
    settingsAction_ = trayMenu_->addAction(tr("&Settings…"));
    settingsAction_->setObjectName(QStringLiteral("traySettingsAction"));
    connect(settingsAction_, &QAction::triggered, this, [this] {
        showWindow();
        emit settingsRequested();
    });
    bossKeyAction_ = trayMenu_->addAction(QString());
    bossKeyAction_->setObjectName(QStringLiteral("trayBossKeyAction"));
    bossKeyAction_->setEnabled(false);
    trayMenu_->addSeparator();
    quitAction_ = trayMenu_->addAction(tr("E&xit"));
    quitAction_->setObjectName(QStringLiteral("trayQuitAction"));
    connect(quitAction_, &QAction::triggered,
            this, &TrayController::quitRequested);
    connect(trayMenu_, &QMenu::aboutToShow,
            this, &TrayController::updateToggleAction);
    trayIcon_->setContextMenu(trayMenu_);

    connect(trayIcon_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    toggleWindowVisibility();
                }
            });
    connect(bossKeyShortcut_, &GlobalShortcut::activated,
            this, &TrayController::toggleWindowVisibility);
    updateToggleAction();
}

bool TrayController::isAvailable() const noexcept
{
    return available_;
}

QKeySequence TrayController::bossKey() const
{
    return bossKeyShortcut_->shortcut();
}

void TrayController::show()
{
    if (available_) {
        trayIcon_->show();
    }
}

void TrayController::toggleWindowVisibility()
{
    if (window_->isVisible() && !window_->isMinimized()) {
        hideWindow();
    } else {
        showWindow();
    }
}

void TrayController::showWindow()
{
    if (window_->isMinimized()) {
        window_->showNormal();
    } else {
        window_->show();
    }
    window_->raise();
    window_->activateWindow();
    updateToggleAction();
}

void TrayController::hideWindow()
{
    window_->hide();
    updateToggleAction();
}

bool TrayController::setBossKey(const QKeySequence& shortcut)
{
    const QKeySequence previous = bossKeyShortcut_->shortcut();
    QString error;
    if (bossKeyShortcut_->setShortcut(shortcut, &error)) {
        updateToggleAction();
        return true;
    }

    QString restoreError;
    (void)bossKeyShortcut_->setShortcut(previous, &restoreError);
    updateToggleAction();
    emit bossKeyRegistrationFailed(bossKeyShortcut_->shortcut(), error);
    return false;
}

void TrayController::updateToggleAction()
{
    const bool shown = window_->isVisible() && !window_->isMinimized();
    toggleAction_->setText(shown ? tr("&Hide Window") : tr("&Show Window"));
    const QKeySequence shortcut = bossKeyShortcut_->shortcut();
    bossKeyAction_->setText(shortcut.isEmpty()
        ? tr("Boss key: Disabled")
        : tr("Boss key: %1").arg(shortcut.toString(QKeySequence::NativeText)));
}

} // namespace vinson
