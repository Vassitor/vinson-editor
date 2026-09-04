#pragma once

#include "editor/EditorDocument.h"
#include "largefile/LargeFilePolicy.h"

#include <QMainWindow>
#include <QKeySequence>
#include <QStringList>

#include <functional>
#include <vector>

class QAction;
class QCloseEvent;
class QDockWidget;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QMenu;
class QProgressBar;
class QPushButton;
class QTabBar;

namespace vinson {

struct Appearance;
class EditorWidget;
class EditHistoryWidget;
class FileManager;
class FindReplaceWidget;
class SearchController;
class SettingsManager;
class ThemeManager;
class WindowController;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void setCloseToTrayEnabled(bool enabled) noexcept;
    [[nodiscard]] const QKeySequence& bossKey() const noexcept;
    [[nodiscard]] const QKeySequence& focusShortcut() const noexcept;
    void openFiles(const QStringList& paths);
    void handleExternalOpenRequest(const QStringList& paths);

public slots:
    void requestApplicationQuit();
    void showSettings();
    void focusEditor();
    void handleBossKeyRegistrationFailure(
        const QKeySequence& activeShortcut, const QString& message);
    void handleFocusShortcutRegistrationFailure(
        const QKeySequence& activeShortcut, const QString& message);

signals:
    void applicationQuitAccepted();
    void bossKeyChanged(const QKeySequence& shortcut);
    void focusShortcutChanged(const QKeySequence& shortcut);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    struct TabState {
        qintptr documentHandle = 0;
        EditorDocument document;
        LargeFileMode largeFileMode = LargeFileMode::Normal;
        qint64 caret = 0;
        qint64 anchor = 0;
        qint64 firstVisibleLine = 0;
        qint64 horizontalOffset = 0;
    };

    void createMenus();
    void connectFileManager();
    void connectSearch();
    void showFindReplace(bool replaceMode);
    void showGoToLine();
    void newDocument();
    int addBlankTab(bool activate = true);
    void switchToTab(int index, bool force = false);
    void switchRelativeTab(int delta);
    void synchronizeTabOrder();
    void applyTabBarAppearance(const Appearance& appearance);
    void requestCloseTab(int index);
    void closeTab(int index);
    void beginCloseAllTabs(bool quitApplication);
    void continueCloseAllTabs();
    void updateTabBarVisibility();
    void updateTabTitle(int index);
    void snapshotCurrentTabView();
    void replaceCurrentTabDocumentHandle();
    [[nodiscard]] TabState& currentTab();
    [[nodiscard]] const TabState& currentTab() const;
    [[nodiscard]] bool currentTabIsPristineUntitled() const;
    [[nodiscard]] int tabIndexForPath(const QString& path) const;
    [[nodiscard]] QStringList sessionTabPaths() const;
    void chooseAndOpenFile();
    void requestOpenFile(const QString& path);
    void processPendingOpenFiles();
    void addRecentFile(const QString& path);
    void removeRecentFile(const QString& path);
    void rebuildRecentFilesMenu();
    bool saveDocument();
    bool saveDocumentAs();
    bool startSave(const QString& path);
    void reloadDocument();
    void requestAfterUnsavedCheck(std::function<void()> action);
    void updateWindowTitle();
    void updateDocumentStatus();
    void updateBusyUi();
    void applyLargeFileMode(LargeFileMode mode);
    void restorePersistentSettings();
    void savePersistentSettings();
    void adjustBackgroundAlpha(int delta);
    void adjustFontSize(int steps);
    void ensureWindowOnScreen();
    void handleLoadFailureState();
    [[nodiscard]] QString chooseSavePath();

    EditorWidget* editor_ = nullptr;
    QTabBar* tabBar_ = nullptr;
    EditHistoryWidget* editHistoryWidget_ = nullptr;
    QDockWidget* editHistoryDock_ = nullptr;
    FileManager* fileManager_ = nullptr;
    SearchController* searchController_ = nullptr;
    FindReplaceWidget* findReplaceWidget_ = nullptr;
    ThemeManager* themeManager_ = nullptr;
    SettingsManager* settingsManager_ = nullptr;
    WindowController* windowController_ = nullptr;
    QLabel* cursorPositionLabel_ = nullptr;
    QLabel* documentInfoLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* cancelOperationButton_ = nullptr;
    QAction* newAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QMenu* recentFilesMenu_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* saveAsAction_ = nullptr;
    QAction* reloadAction_ = nullptr;
    QAction* findAction_ = nullptr;
    QAction* replaceAction_ = nullptr;
    QAction* findNextAction_ = nullptr;
    QAction* findPreviousAction_ = nullptr;
    QAction* goToLineAction_ = nullptr;
    QAction* wrapAction_ = nullptr;
    QAction* lineNumberAction_ = nullptr;
    QAction* editHistoryAction_ = nullptr;
    QAction* framelessAction_ = nullptr;
    QAction* minimalModeAction_ = nullptr;
    QAction* increaseBackgroundAlphaAction_ = nullptr;
    QAction* decreaseBackgroundAlphaAction_ = nullptr;
    std::vector<TabState> tabs_;
    int currentTabIndex_ = -1;
    int loadingTabIndex_ = -1;
    int pendingNewTabs_ = 0;
    QStringList pendingOpenPaths_;
    QStringList closingSessionPaths_;
    std::function<void()> pendingAfterSave_;
    bool loadReplacedDocument_ = false;
    bool closeAfterSave_ = false;
    bool closeAllTabsInProgress_ = false;
    bool quitAfterClosingTabs_ = false;
    bool closeToTrayEnabled_ = false;
    bool preferredWordWrap_ = false;
    bool restoringSettings_ = false;
    bool switchingTabs_ = false;
    bool synchronizingTabOrder_ = false;
    bool restoreTabsOnStartup_ = true;
    qint64 currentLine_ = 1;
    QString lastDirectory_;
    QStringList recentFiles_;
    QKeySequence bossKey_;
    QKeySequence focusShortcut_;
};

} // namespace vinson
