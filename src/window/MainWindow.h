#pragma once

#include "editor/EditorDocument.h"
#include "file/FileChangeMonitor.h"
#include "largefile/LargeFilePolicy.h"
#include "session/DocumentHistory.h"
#include "session/RecoveryManager.h"
#include "settings/Appearance.h"

#include <QMainWindow>
#include <QDateTime>
#include <QByteArray>
#include <QKeySequence>
#include <QMap>
#include <QStringList>

#include <functional>
#include <optional>
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
class QTimer;

namespace vinson {

class EditorWidget;
class EditHistoryWidget;
class FileManager;
class FindReplaceWidget;
class SearchController;
class SettingsManager;
class ThemeManager;
class WindowController;
class NativeTitleBar;
class PluginManager;
struct PluginCommand;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr,
                        bool restorePersistentState = true,
                        const QString& pluginDirectory = {});
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
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    struct DiskStamp {
        qint64 size = 0;
        QDateTime modifiedAt;
        QByteArray contentHash;

        bool operator==(const DiskStamp&) const = default;
    };

    struct TabState {
        LineEnding insertionLineEnding = LineEnding::None;
        qintptr documentHandle = 0;
        EditorDocument document;
        LargeFileMode largeFileMode = LargeFileMode::Normal;
        qint64 caret = 0;
        qint64 anchor = 0;
        qint64 firstVisibleLine = 0;
        qint64 horizontalOffset = 0;
        QString recoveryId;
        bool recoveryLimitNotified = false;
        std::optional<FileChangeMonitor::Change> externalChange;
        std::optional<DiskStamp> diskStamp;
    };

    void createMenus();
    void rebuildPluginMenu();
    void refreshPluginShortcuts();
    void runPluginCommand(const QString& pluginId, const PluginCommand& command);
    void connectFileManager();
    void connectFileChangeMonitor();
    void connectSearch();
    void initializeCrashRecovery();
    void restoreRecoveryEntries(const QVector<RecoveryEntry>& entries);
    void scheduleRecoverySnapshot();
    void writeCurrentRecoverySnapshot();
    void removeRecoverySnapshot(const TabState& tab);
    void showFindReplace(bool replaceMode);
    void showGoToLine();
    void goToBookmark(bool forward);
    void newDocument();
    int addBlankTab(bool activate = true);
    void switchToTab(int index, bool force = false);
    void switchRelativeTab(int delta);
    void synchronizeTabOrder();
    void applyTabBarAppearance(const Appearance& appearance);
    void showTabContextMenu(const QPoint& position);
    void requestCloseTabs(std::vector<qintptr> documents, qintptr returnTo);
    void rebuildAppearancePresetActions();
    void applyAppearancePreset(int index);
    void cycleAppearancePreset(int direction);
    void applyShortcuts(const QMap<QString, QKeySequence>& shortcuts);
    [[nodiscard]] QMap<QString, QKeySequence> shortcuts() const;
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
    void handleExternalFileChange(const QString& path,
                                  FileChangeMonitor::Change change);
    void processCurrentExternalFileChange();
    bool startExternalReload(const QString& path);
    void resumeSuspendedFileWatch();
    void reopenClosedTab();
    void addRecentFile(const QString& path);
    void removeRecentFile(const QString& path);
    void rebuildRecentFilesMenu();
    bool saveDocument();
    bool saveDocumentAs();
    bool startSave(const QString& path);
    [[nodiscard]] static std::optional<DiskStamp> diskStampForPath(
        const QString& path);
    void reloadDocument();
    void requestAfterUnsavedCheck(std::function<void()> action);
    void updateWindowTitle();
    void updateDocumentStatus();
    void updateBusyUi();
    [[nodiscard]] bool isDocumentBusy() const;
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
    FileChangeMonitor* fileChangeMonitor_ = nullptr;
    RecoveryManager* recoveryManager_ = nullptr;
    QTimer* recoveryTimer_ = nullptr;
    SearchController* searchController_ = nullptr;
    FindReplaceWidget* findReplaceWidget_ = nullptr;
    ThemeManager* themeManager_ = nullptr;
    SettingsManager* settingsManager_ = nullptr;
    PluginManager* pluginManager_ = nullptr;
    QMenu* pluginsMenu_ = nullptr;
    QVector<QAction*> pluginActions_;
    qintptr pluginDocumentHandle_ = 0;
    qint64 pluginSelectionStart_ = 0;
    qint64 pluginSelectionEnd_ = 0;
    quint64 documentRevision_ = 0;
    quint64 pluginDocumentRevision_ = 0;
    WindowController* windowController_ = nullptr;
    NativeTitleBar* nativeTitleBar_ = nullptr;
    QLabel* cursorPositionLabel_ = nullptr;
    QLabel* documentInfoLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* cancelOperationButton_ = nullptr;
    QAction* newAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* reopenClosedTabAction_ = nullptr;
    QMenu* recentFilesMenu_ = nullptr;
    QMenu* appearancePresetsMenu_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* saveAsAction_ = nullptr;
    QAction* reloadAction_ = nullptr;
    QVector<QAction*> encodingActions_;
    QVector<QAction*> lineEndingActions_;
    QAction* findAction_ = nullptr;
    QAction* replaceAction_ = nullptr;
    QAction* findNextAction_ = nullptr;
    QAction* findPreviousAction_ = nullptr;
    QAction* goToLineAction_ = nullptr;
    QAction* toggleBookmarkAction_ = nullptr;
    QVector<QAction*> bookmarkActions_;
    QAction* wrapAction_ = nullptr;
    QAction* lineNumberAction_ = nullptr;
    QAction* editHistoryAction_ = nullptr;
    QAction* framelessAction_ = nullptr;
    QAction* minimalModeAction_ = nullptr;
    QAction* exitMinimalModeAction_ = nullptr;
    QAction* increaseBackgroundAlphaAction_ = nullptr;
    QAction* decreaseBackgroundAlphaAction_ = nullptr;
    QAction* previousAppearancePresetAction_ = nullptr;
    QAction* nextAppearancePresetAction_ = nullptr;
    QVector<QAction*> appearancePresetActions_;
    QVector<AppearancePreset> appearancePresets_;
    std::vector<TabState> tabs_;
    int currentTabIndex_ = -1;
    int loadingTabIndex_ = -1;
    int pendingNewTabs_ = 0;
    QStringList pendingOpenPaths_;
    QStringList closingSessionPaths_;
    QString suspendedFileWatchPath_;
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
    bool handlingExternalFileChange_ = false;
    bool crashRecoveryChecked_ = false;
    bool persistentStateEnabled_ = true;
    bool restoreTabsOnStartup_ = true;
    qint64 currentLine_ = 1;
    QString lastDirectory_;
    DocumentHistory documentHistory_;
    QKeySequence bossKey_;
    QKeySequence focusShortcut_;
};

} // namespace vinson
