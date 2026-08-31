#pragma once

#include "editor/EditorDocument.h"
#include "largefile/LargeFilePolicy.h"

#include <QMainWindow>
#include <QKeySequence>

#include <functional>

class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QProgressBar;
class QPushButton;

namespace vinson {

class EditorWidget;
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
    void setCloseToTrayEnabled(bool enabled) noexcept;
    [[nodiscard]] const QKeySequence& bossKey() const noexcept;

public slots:
    void requestApplicationQuit();
    void showSettings();
    void handleBossKeyRegistrationFailure(
        const QKeySequence& activeShortcut, const QString& message);

signals:
    void applicationQuitAccepted();
    void bossKeyChanged(const QKeySequence& shortcut);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void createMenus();
    void connectFileManager();
    void connectSearch();
    void showFindReplace(bool replaceMode);
    void showGoToLine();
    void newDocument();
    void chooseAndOpenFile();
    void requestOpenFile(const QString& path);
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
    QAction* framelessAction_ = nullptr;
    QAction* minimalModeAction_ = nullptr;
    QAction* increaseBackgroundAlphaAction_ = nullptr;
    QAction* decreaseBackgroundAlphaAction_ = nullptr;
    EditorDocument document_;
    std::function<void()> pendingAfterSave_;
    bool loadReplacedDocument_ = false;
    bool closeAfterSave_ = false;
    bool closeToTrayEnabled_ = false;
    bool preferredWordWrap_ = false;
    bool restoringSettings_ = false;
    LargeFileMode largeFileMode_ = LargeFileMode::Normal;
    qint64 currentLine_ = 1;
    QString lastDirectory_;
    QKeySequence bossKey_;
};

} // namespace vinson
