#include "window/MainWindow.h"

#include "editor/EditorWidget.h"
#include "file/FileManager.h"
#include "file/FileTypes.h"
#include "search/SearchController.h"
#include "settings/SettingsManager.h"
#include "settings/ThemeManager.h"
#include "ui/FindReplaceWidget.h"
#include "ui/EditHistoryWidget.h"
#include "ui/MenuAppearance.h"
#include "ui/SettingsDialog.h"
#include "window/WindowController.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDebug>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QScreen>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QSizePolicy>

#include <algorithm>
#include <limits>
#include <utility>

namespace vinson {
namespace {

QColor blendedTabColor(const QColor& background, const QColor& foreground,
                       int foregroundPercent)
{
    const int backgroundPercent = 100 - foregroundPercent;
    return QColor(
        (background.red() * backgroundPercent
         + foreground.red() * foregroundPercent) / 100,
        (background.green() * backgroundPercent
         + foreground.green() * foregroundPercent) / 100,
        (background.blue() * backgroundPercent
         + foreground.blue() * foregroundPercent) / 100);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , editor_(new EditorWidget(this))
    , tabBar_(new QTabBar(this))
    , fileManager_(new FileManager(this))
    , searchController_(new SearchController(editor_, this))
    , findReplaceWidget_(new FindReplaceWidget(this))
    , cursorPositionLabel_(new QLabel(QStringLiteral("Ln 1, Col 1"), this))
    , documentInfoLabel_(new QLabel(this))
    , progressBar_(new QProgressBar(this))
    , cancelOperationButton_(new QPushButton(tr("Cancel"), this))
{
    // Transparency capability has to exist before the native top-level window
    // is created. Later phases can change only the painted background alpha.
    setObjectName(QStringLiteral("vinsonMainWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    themeManager_ = new ThemeManager(editor_, this, this);
    settingsManager_ = new SettingsManager(this);
    setAcceptDrops(true);
    auto* centralWidget = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    tabBar_->setObjectName(QStringLiteral("documentTabBar"));
    tabBar_->setDocumentMode(true);
    tabBar_->setDrawBase(false);
    tabBar_->setExpanding(false);
    tabBar_->setTabsClosable(true);
    tabBar_->setMovable(true);
    tabBar_->setElideMode(Qt::ElideMiddle);
    tabBar_->setUsesScrollButtons(true);
    tabBar_->setMinimumWidth(180);
    tabBar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    tabBar_->setProperty("browserStyle", true);
    centralLayout->addWidget(findReplaceWidget_);
    centralLayout->addWidget(editor_, 1);
    setCentralWidget(centralWidget);

    TabState initialTab;
    initialTab.documentHandle = editor_->retainCurrentDocument();
    tabs_.push_back(std::move(initialTab));
    currentTabIndex_ = 0;
    const int initialTabIndex = tabBar_->addTab(tr("Untitled"));
    tabBar_->setTabData(initialTabIndex,
                        QVariant::fromValue(tabs_.front().documentHandle));
    tabBar_->setCurrentIndex(0);
    connect(tabBar_, &QTabBar::currentChanged,
            this, [this](int index) { switchToTab(index); });
    connect(tabBar_, &QTabBar::tabCloseRequested,
            this, &MainWindow::requestCloseTab);
    connect(tabBar_, &QTabBar::tabMoved, this,
            [this](int, int) {
                synchronizeTabOrder();
                savePersistentSettings();
            });
    editHistoryWidget_ = new EditHistoryWidget(editor_, this);
    editHistoryDock_ = new QDockWidget(tr("Edit History"), this);
    editHistoryDock_->setObjectName(QStringLiteral("editHistoryDock"));
    editHistoryDock_->setAllowedAreas(Qt::LeftDockWidgetArea
                                      | Qt::RightDockWidgetArea);
    editHistoryDock_->setWidget(editHistoryWidget_);
    editHistoryDock_->setMinimumWidth(240);
    addDockWidget(Qt::RightDockWidgetArea, editHistoryDock_);
    editHistoryDock_->hide();
    findReplaceWidget_->applyAppearance(themeManager_->appearance(), false);
    editHistoryWidget_->applyAppearance(themeManager_->appearance());
    applyEditHistoryDockAppearance(editHistoryDock_,
                                   themeManager_->appearance(), false);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            editHistoryWidget_, &EditHistoryWidget::applyAppearance);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            this, [this](const Appearance& appearance) {
                applyEditHistoryDockAppearance(
                    editHistoryDock_, appearance,
                    editHistoryDock_->isFloating());
            });
    connect(editHistoryDock_, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                applyEditHistoryDockAppearance(
                    editHistoryDock_, themeManager_->appearance(), floating);
                editor_->refreshScrollBarLayout();
                QTimer::singleShot(0, editor_,
                                   &EditorWidget::refreshScrollBarLayout);
            });
    connect(editHistoryDock_, &QDockWidget::visibilityChanged,
            this, [this](bool) {
                editor_->refreshScrollBarLayout();
                QTimer::singleShot(0, editor_,
                                   &EditorWidget::refreshScrollBarLayout);
            });
    windowController_ = new WindowController(this, this);
    windowController_->configureMinimalMode(editor_, findReplaceWidget_);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            this, [this](const Appearance& appearance) {
                findReplaceWidget_->applyAppearance(
                    appearance, windowController_->isFrameless());
            });
    connect(themeManager_, &ThemeManager::appearanceChanged,
            windowController_, &WindowController::refreshMinimalMinimumSize);
    connect(windowController_, &WindowController::framelessChanged,
            themeManager_, &ThemeManager::setFramelessMode);
    connect(windowController_, &WindowController::framelessChanged,
            this, [this](bool frameless) {
                findReplaceWidget_->applyAppearance(
                    themeManager_->appearance(), frameless);
                updateTabBarVisibility();
            });
    createMenus();
    menuBar()->setCornerWidget(tabBar_, Qt::TopRightCorner);
    applyTabBarAppearance(themeManager_->appearance());
    connect(themeManager_, &ThemeManager::appearanceChanged,
            this, &MainWindow::applyTabBarAppearance);
    connectFileManager();
    connectSearch();

    progressBar_->setTextVisible(false);
    progressBar_->setMaximumWidth(180);
    progressBar_->hide();
    cancelOperationButton_->hide();
    statusBar()->addPermanentWidget(progressBar_);
    statusBar()->addPermanentWidget(cancelOperationButton_);
    statusBar()->addPermanentWidget(documentInfoLabel_);
    statusBar()->addPermanentWidget(cursorPositionLabel_);
    statusBar()->showMessage(tr("Ready"));
    resize(900, 600);
    restorePersistentSettings();
    updateWindowTitle();
    updateDocumentStatus();

    connect(editor_, &EditorWidget::cursorPositionChanged, this,
            [this](qsizetype line, qsizetype column) {
                currentLine_ = line;
                cursorPositionLabel_->setText(
                    tr("Ln %1, Col %2").arg(line).arg(column));
            });
    connect(editor_, &EditorWidget::documentModified, this,
            [this](bool modified) {
                if (!fileManager_->isBusy() && !switchingTabs_) {
                    currentTab().document.setModified(modified);
                    updateWindowTitle();
                }
            });
    connect(cancelOperationButton_, &QPushButton::clicked,
            fileManager_, &FileManager::cancelCurrentOperation);
    connect(editor_, &ScintillaEditBase::uriDropped, this,
            [this](const QString& uri) {
                const QUrl url(uri);
                if (url.isLocalFile()) {
                    requestOpenFile(url.toLocalFile());
                }
            });
    connect(editor_, &EditorWidget::findRequested,
            this, [this] { showFindReplace(false); });
    connect(editor_, &EditorWidget::fontSizeAdjustmentRequested,
            this, &MainWindow::adjustFontSize);

    QTimer::singleShot(0, this, &MainWindow::processPendingOpenFiles);
}

MainWindow::~MainWindow()
{
    for (const TabState& tab : tabs_) {
        editor_->releaseTabDocument(
            static_cast<sptr_t>(tab.documentHandle));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (closeToTrayEnabled_) {
        savePersistentSettings();
        hide();
        event->ignore();
        return;
    }
    if (closeAfterSave_ && !fileManager_->isBusy()) {
        savePersistentSettings();
        event->accept();
        return;
    }
    if (fileManager_->isBusy()) {
        statusBar()->showMessage(tr("Cancel or wait for the current file operation."),
                                 4000);
        event->ignore();
        return;
    }
    event->ignore();
    if (!closeAllTabsInProgress_) {
        beginCloseAllTabs(false);
    }
}

void MainWindow::setCloseToTrayEnabled(bool enabled) noexcept
{
    closeToTrayEnabled_ = enabled;
}

const QKeySequence& MainWindow::bossKey() const noexcept
{
    return bossKey_;
}

const QKeySequence& MainWindow::focusShortcut() const noexcept
{
    return focusShortcut_;
}

void MainWindow::focusEditor()
{
    editor_->QWidget::setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::requestApplicationQuit()
{
    if (fileManager_->isBusy()) {
        show();
        raise();
        activateWindow();
        statusBar()->showMessage(
            tr("Cancel or wait for the current file operation."), 4000);
        return;
    }

    beginCloseAllTabs(true);
}

void MainWindow::handleBossKeyRegistrationFailure(
    const QKeySequence& activeShortcut, const QString& message)
{
    bossKey_ = activeShortcut;
    savePersistentSettings();
    statusBar()->showMessage(message, 6000);
}

void MainWindow::handleFocusShortcutRegistrationFailure(
    const QKeySequence& activeShortcut, const QString& message)
{
    focusShortcut_ = activeShortcut;
    savePersistentSettings();
    statusBar()->showMessage(message, 6000);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!fileManager_->isBusy() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        if (urls.size() == 1 && urls.first().isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.size() != 1 || !urls.first().isLocalFile()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    requestOpenFile(urls.first().toLocalFile());
}

void MainWindow::createMenus()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    newAction_ = fileMenu->addAction(tr("&New"));
    newAction_->setObjectName(QStringLiteral("newDocumentAction"));
    newAction_->setShortcut(QKeySequence::New);
    connect(newAction_, &QAction::triggered, this, &MainWindow::newDocument);

    openAction_ = fileMenu->addAction(tr("&Open…"));
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::chooseAndOpenFile);

    recentFilesMenu_ = fileMenu->addMenu(tr("Open &Recent"));
    recentFilesMenu_->setObjectName(QStringLiteral("recentFilesMenu"));
    rebuildRecentFilesMenu();

    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveDocument(); });

    saveAsAction_ = fileMenu->addAction(tr("Save &As…"));
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered,
            this, [this] { saveDocumentAs(); });

    reloadAction_ = fileMenu->addAction(tr("&Reload"));
    reloadAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    connect(reloadAction_, &QAction::triggered, this, &MainWindow::reloadDocument);

    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction(tr("E&xit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered,
            this, &MainWindow::requestApplicationQuit);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto addEditorAction = [this, editMenu](const QString& text,
                                            const QKeySequence& shortcut,
                                            auto slot) {
        auto* action = editMenu->addAction(text);
        action->setShortcut(shortcut);
        connect(action, &QAction::triggered, editor_, slot);
    };
    addEditorAction(tr("&Undo"), QKeySequence::Undo, &ScintillaEdit::undo);
    addEditorAction(tr("&Redo"), QKeySequence::Redo, &ScintillaEdit::redo);
    editMenu->addSeparator();
    addEditorAction(tr("Cu&t"), QKeySequence::Cut, &ScintillaEdit::cut);
    addEditorAction(tr("&Copy"), QKeySequence::Copy, &ScintillaEdit::copy);
    addEditorAction(tr("&Paste"), QKeySequence::Paste, &ScintillaEdit::paste);
    editMenu->addSeparator();
    addEditorAction(tr("Select &All"), QKeySequence::SelectAll,
                    &ScintillaEdit::selectAll);

    auto* searchMenu = menuBar()->addMenu(tr("&Search"));
    findAction_ = searchMenu->addAction(tr("&Find…"));
    findAction_->setShortcut(QKeySequence::Find);
    connect(findAction_, &QAction::triggered,
            this, [this] { showFindReplace(false); });

    replaceAction_ = searchMenu->addAction(tr("&Replace…"));
    replaceAction_->setShortcut(QKeySequence::Replace);
    connect(replaceAction_, &QAction::triggered,
            this, [this] { showFindReplace(true); });

    searchMenu->addSeparator();
    findNextAction_ = searchMenu->addAction(tr("Find &Next"));
    findNextAction_->setShortcut(QKeySequence::FindNext);
    connect(findNextAction_, &QAction::triggered,
            searchController_, &SearchController::findNext);

    findPreviousAction_ = searchMenu->addAction(tr("Find &Previous"));
    findPreviousAction_->setShortcut(QKeySequence::FindPrevious);
    connect(findPreviousAction_, &QAction::triggered,
            searchController_, &SearchController::findPrevious);

    goToLineAction_ = searchMenu->addAction(tr("&Go To Line…"));
    goToLineAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(goToLineAction_, &QAction::triggered,
            this, &MainWindow::showGoToLine);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    wrapAction_ = viewMenu->addAction(tr("Word &Wrap"));
    wrapAction_->setObjectName(QStringLiteral("wordWrapAction"));
    wrapAction_->setCheckable(true);
    connect(wrapAction_, &QAction::toggled, this, [this](bool enabled) {
        preferredWordWrap_ = enabled;
        editor_->setWordWrapEnabled(enabled);
        if (enabled && LargeFilePolicy::usesLargeDocument(
                           currentTab().largeFileMode)) {
            statusBar()->showMessage(
                tr("Word wrap may be slow in %1.")
                    .arg(LargeFilePolicy::displayName(
                        currentTab().largeFileMode)),
                5000);
        }
        savePersistentSettings();
    });

    lineNumberAction_ = viewMenu->addAction(tr("Line &Numbers"));
    lineNumberAction_->setObjectName(QStringLiteral("lineNumbersAction"));
    lineNumberAction_->setCheckable(true);
    lineNumberAction_->setChecked(true);
    connect(lineNumberAction_, &QAction::toggled, this, [this](bool visible) {
        editor_->setLineNumbersVisible(visible);
        savePersistentSettings();
    });

    editHistoryAction_ = viewMenu->addAction(tr("Edit &History"));
    editHistoryAction_->setObjectName(QStringLiteral("editHistoryAction"));
    editHistoryAction_->setCheckable(true);
    editHistoryAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    connect(editHistoryAction_, &QAction::toggled,
            editHistoryDock_, &QDockWidget::setVisible);
    connect(editHistoryDock_, &QDockWidget::visibilityChanged,
            editHistoryAction_, &QAction::setChecked);

    viewMenu->addSeparator();
    auto* alwaysOnTopAction = viewMenu->addAction(tr("Always on &Top"));
    alwaysOnTopAction->setObjectName(QStringLiteral("alwaysOnTopAction"));
    alwaysOnTopAction->setCheckable(true);
    alwaysOnTopAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(alwaysOnTopAction, &QAction::toggled,
            windowController_, &WindowController::setAlwaysOnTop);
    connect(windowController_, &WindowController::alwaysOnTopChanged,
            alwaysOnTopAction, &QAction::setChecked);
    connect(windowController_, &WindowController::alwaysOnTopChanged,
            this, [this] { savePersistentSettings(); });

    framelessAction_ = viewMenu->addAction(tr("&Frameless Mode"));
    framelessAction_->setObjectName(QStringLiteral("framelessAction"));
    framelessAction_->setCheckable(true);
    framelessAction_->setShortcut(QKeySequence(Qt::Key_F11));
    // Keep the shortcut registered on the top-level window after frameless
    // mode hides the menu bar that also owns this action.
    addAction(framelessAction_);
    connect(framelessAction_, &QAction::toggled,
            windowController_, &WindowController::setFrameless);
    connect(windowController_, &WindowController::framelessChanged,
            framelessAction_, &QAction::setChecked);
    connect(windowController_, &WindowController::framelessChanged,
            this, [this] {
                if (!windowController_->isMinimalMode()) {
                    savePersistentSettings();
                }
            });

    minimalModeAction_ = viewMenu->addAction(tr("&Minimal Mode"));
    minimalModeAction_->setObjectName(QStringLiteral("minimalModeAction"));
    minimalModeAction_->setCheckable(true);
    minimalModeAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(minimalModeAction_, &QAction::toggled,
            windowController_, &WindowController::setMinimalMode);
    connect(windowController_, &WindowController::minimalModeChanged,
            minimalModeAction_, &QAction::setChecked);

    connect(windowController_, &WindowController::minimalModeChanged,
            this, [this](bool enabled) {
                framelessAction_->setEnabled(!enabled);
                editHistoryAction_->setEnabled(!enabled);
                if (enabled) {
                    editHistoryDock_->hide();
                }
                if (!enabled) {
                    editor_->QWidget::setFocus();
                }
                updateTabBarVisibility();
            });

    auto* previousTabAction = new QAction(this);
    previousTabAction->setObjectName(QStringLiteral("previousTabAction"));
    previousTabAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
    previousTabAction->setShortcutContext(Qt::WindowShortcut);
    addAction(previousTabAction);
    connect(previousTabAction, &QAction::triggered,
            this, [this] { switchRelativeTab(-1); });

    auto* nextTabAction = new QAction(this);
    nextTabAction->setObjectName(QStringLiteral("nextTabAction"));
    nextTabAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));
    nextTabAction->setShortcutContext(Qt::WindowShortcut);
    addAction(nextTabAction);
    connect(nextTabAction, &QAction::triggered,
            this, [this] { switchRelativeTab(1); });

    auto* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    auto* appearanceAction = settingsMenu->addAction(
        tr("&Appearance and Shortcuts…"));
    appearanceAction->setObjectName(
        QStringLiteral("appearanceAndShortcutsAction"));
    connect(appearanceAction, &QAction::triggered,
            this, &MainWindow::showSettings);

    settingsMenu->addSeparator();
    increaseBackgroundAlphaAction_ =
        settingsMenu->addAction(tr("Increase Background Opacity"));
    increaseBackgroundAlphaAction_->setObjectName(
        QStringLiteral("increaseBackgroundAlphaAction"));
    increaseBackgroundAlphaAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_Up));
    addAction(increaseBackgroundAlphaAction_);
    connect(increaseBackgroundAlphaAction_, &QAction::triggered,
            this, [this] { adjustBackgroundAlpha(5); });

    decreaseBackgroundAlphaAction_ =
        settingsMenu->addAction(tr("Decrease Background Opacity"));
    decreaseBackgroundAlphaAction_->setObjectName(
        QStringLiteral("decreaseBackgroundAlphaAction"));
    decreaseBackgroundAlphaAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_Down));
    addAction(decreaseBackgroundAlphaAction_);
    connect(decreaseBackgroundAlphaAction_, &QAction::triggered,
            this, [this] { adjustBackgroundAlpha(-5); });

    for (QMenu* menu : {fileMenu, editMenu, searchMenu, viewMenu,
                        settingsMenu}) {
        applyNativeMenuShadow(menu);
    }
}

void MainWindow::connectSearch()
{
    connect(findReplaceWidget_, &FindReplaceWidget::searchTextChanged,
            searchController_, &SearchController::setSearchText);
    connect(findReplaceWidget_, &FindReplaceWidget::replacementTextChanged,
            searchController_, &SearchController::setReplacementText);
    connect(findReplaceWidget_, &FindReplaceWidget::optionsChanged,
            searchController_, &SearchController::setOptions);
    connect(findReplaceWidget_, &FindReplaceWidget::findNextRequested,
            searchController_, &SearchController::findNext);
    connect(findReplaceWidget_, &FindReplaceWidget::findPreviousRequested,
            searchController_, &SearchController::findPrevious);
    connect(findReplaceWidget_, &FindReplaceWidget::replaceRequested,
            searchController_, &SearchController::replaceCurrent);
    connect(findReplaceWidget_, &FindReplaceWidget::replaceAllRequested,
            this, [this] {
                if (LargeFilePolicy::requiresReplaceAllConfirmation(
                        currentTab().largeFileMode)
                    && QMessageBox::warning(
                           this, tr("Replace All in a very large file"),
                           tr("Replace All may take a long time and create a "
                              "large undo record. Continue?"),
                           QMessageBox::Yes | QMessageBox::No,
                           QMessageBox::No) != QMessageBox::Yes) {
                    return;
                }
                searchController_->replaceAll();
            });
    connect(findReplaceWidget_, &FindReplaceWidget::closeRequested,
            this, [this] {
                findReplaceWidget_->hide();
                editor_->QWidget::setFocus();
            });
    connect(searchController_, &SearchController::resultChanged,
            this, [this](SearchResult result, const QString& message) {
                findReplaceWidget_->setResult(result, message);
                statusBar()->showMessage(message, 3000);
            });
}

void MainWindow::showFindReplace(bool replaceMode)
{
    QString initialText = QString::fromUtf8(editor_->selectedTextUtf8());
    if (initialText.contains(QLatin1Char('\n'))
        || initialText.contains(QLatin1Char('\r'))
        || initialText.size() > 256) {
        initialText.clear();
    }
    findReplaceWidget_->open(replaceMode, initialText);
}

void MainWindow::showGoToLine()
{
    const int maximumLine = static_cast<int>(std::min<qint64>(
        editor_->editorLineCount(), std::numeric_limits<int>::max()));
    bool accepted = false;
    const int line = QInputDialog::getInt(
        this, tr("Go To Line"), tr("Line number:"),
        static_cast<int>(std::min<qint64>(currentLine_, maximumLine)),
        1, std::max(1, maximumLine), 1, &accepted);
    if (accepted) {
        searchController_->goToLine(line);
    }
}

void MainWindow::showSettings()
{
    const Appearance original = themeManager_->appearance();
    SettingsDialog dialog(original, bossKey_, focusShortcut_,
                          restoreTabsOnStartup_, this);
    connect(&dialog, &SettingsDialog::previewChanged,
            themeManager_, &ThemeManager::applyAppearance);
    if (dialog.exec() != QDialog::Accepted) {
        themeManager_->applyAppearance(original);
    } else {
        const bool bossKeyWasChanged = bossKey_ != dialog.bossKey();
        const bool focusShortcutWasChanged =
            focusShortcut_ != dialog.focusShortcut();
        if (bossKeyWasChanged && focusShortcutWasChanged) {
            // Release both registrations first so users can swap the two
            // shortcuts without either old registration blocking the other.
            emit bossKeyChanged(QKeySequence());
            emit focusShortcutChanged(QKeySequence());
        }
        if (bossKeyWasChanged) {
            bossKey_ = dialog.bossKey();
            emit bossKeyChanged(bossKey_);
        }
        if (focusShortcutWasChanged) {
            focusShortcut_ = dialog.focusShortcut();
            emit focusShortcutChanged(focusShortcut_);
        }
        restoreTabsOnStartup_ = dialog.restoreTabsOnStartup();
        savePersistentSettings();
    }
}

void MainWindow::connectFileManager()
{
    connect(fileManager_, &FileManager::operationChanged,
            this, [this](FileManager::Operation) { updateBusyUi(); });
    connect(fileManager_, &FileManager::loadPrepared, this,
            [this](const FileLoadInfo& info) {
                const LargeFileMode mode =
                    LargeFilePolicy::modeForSize(info.fileSize);
                if (!editor_->beginFileLoad(mode, info.fileSize)) {
                    fileManager_->cancelCurrentOperation();
                    QMessageBox::critical(
                        this, tr("Open file"),
                        tr("Scintilla could not create a document for this file."));
                    return;
                }
                replaceCurrentTabDocumentHandle();
                loadReplacedDocument_ = true;
                applyLargeFileMode(mode);
                progressBar_->setRange(0, info.fileSize > 0 ? 1000 : 0);
                progressBar_->setValue(0);
                statusBar()->showMessage(tr("Loading %1").arg(info.path));
            });
    connect(fileManager_, &FileManager::saveChunkRequested, this,
            [this](qint64 offset, qint64 maximumBytes) {
                const QByteArray chunk =
                    editor_->textRangeUtf8(offset, maximumBytes);
                fileManager_->provideSaveChunk(
                    chunk, offset + chunk.size() >= editor_->documentLength());
            });
    connect(fileManager_, &FileManager::loadChunk, this,
            [this](const QByteArray& chunkData, qint64 bytesRead,
                   qint64 totalBytes) {
                if (!loadReplacedDocument_) {
                    return;
                }
                editor_->appendTextUtf8(chunkData);
                if (totalBytes > 0) {
                    progressBar_->setValue(static_cast<int>(
                        std::min<qint64>(1000, bytesRead * 1000 / totalBytes)));
                }
                statusBar()->showMessage(
                    tr("Loading %1 / %2")
                        .arg(QLocale().formattedDataSize(bytesRead),
                             QLocale().formattedDataSize(totalBytes)));
            });
    connect(fileManager_, &FileManager::loadCompleted, this,
            [this](const FileLoadInfo& info) {
                editor_->completeFileLoad(info.lineEnding);
                currentTab().document.adoptLoadedFile(info);
                lastDirectory_ = QFileInfo(info.path).absolutePath();
                addRecentFile(info.path);
                loadReplacedDocument_ = false;
                loadingTabIndex_ = -1;
                updateWindowTitle();
                updateDocumentStatus();
                statusBar()->showMessage(tr("Loaded %1").arg(info.path), 3000);
                savePersistentSettings();
                processPendingOpenFiles();
            });
    connect(fileManager_, &FileManager::saveProgress, this,
            [this](qint64 written, qint64 total) {
                if (total > 0) {
                    progressBar_->setRange(0, 1000);
                    progressBar_->setValue(static_cast<int>(
                        std::min<qint64>(1000, written * 1000 / total)));
                }
            });
    connect(fileManager_, &FileManager::saveCompleted, this,
            [this](const FileSaveResult& result) {
                editor_->markSaved();
                currentTab().document.adoptSavedFile(result);
                if (closeAllTabsInProgress_
                    && !closingSessionPaths_.contains(result.path)) {
                    closingSessionPaths_.append(result.path);
                }
                lastDirectory_ = QFileInfo(result.path).absolutePath();
                addRecentFile(result.path);
                updateWindowTitle();
                updateDocumentStatus();
                statusBar()->showMessage(tr("Saved %1").arg(result.path), 3000);
                savePersistentSettings();

                auto continuation = std::move(pendingAfterSave_);
                pendingAfterSave_ = {};
                if (continuation) {
                    continuation();
                }
            });
    connect(fileManager_, &FileManager::operationFailed, this,
            [this](const QString& message) {
                handleLoadFailureState();
                pendingAfterSave_ = {};
                QMessageBox::critical(this, tr("File operation failed"), message);
                processPendingOpenFiles();
            });
    connect(fileManager_, &FileManager::operationCanceled, this, [this] {
        handleLoadFailureState();
        pendingAfterSave_ = {};
        statusBar()->showMessage(tr("File operation canceled"), 3000);
        processPendingOpenFiles();
    });
}

void MainWindow::newDocument()
{
    if (fileManager_->isBusy()) {
        return;
    }
    if (addBlankTab() >= 0) {
        statusBar()->showMessage(tr("New document"), 2000);
    }
}

int MainWindow::addBlankTab(bool activate)
{
    const sptr_t document = editor_->createTabDocument();
    if (document == 0) {
        QMessageBox::critical(
            this, tr("New document"),
            tr("Scintilla could not create a new document."));
        return -1;
    }

    TabState tab;
    tab.documentHandle = document;
    tabs_.push_back(std::move(tab));
    const int index = static_cast<int>(tabs_.size()) - 1;
    const int tabBarIndex = tabBar_->addTab(tr("Untitled"));
    tabBar_->setTabData(tabBarIndex,
                        QVariant::fromValue(tabs_.back().documentHandle));
    if (activate) {
        switchToTab(index);
    }
    savePersistentSettings();
    return index;
}

void MainWindow::snapshotCurrentTabView()
{
    if (currentTabIndex_ < 0
        || currentTabIndex_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    TabState& tab = tabs_.at(currentTabIndex_);
    tab.caret = editor_->currentPos();
    tab.anchor = editor_->anchor();
    tab.firstVisibleLine = editor_->firstVisibleLine();
    tab.horizontalOffset = editor_->xOffset();
}

void MainWindow::switchToTab(int index, bool force)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())
        || fileManager_->isBusy()) {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->setCurrentIndex(currentTabIndex_);
        return;
    }
    synchronizeTabOrder();
    if (!force && index == currentTabIndex_) {
        return;
    }

    snapshotCurrentTabView();
    switchingTabs_ = true;
    currentTabIndex_ = index;
    TabState& tab = currentTab();
    editor_->activateTabDocument(
        static_cast<sptr_t>(tab.documentHandle), tab.largeFileMode);
    editor_->setSel(static_cast<sptr_t>(tab.anchor),
                    static_cast<sptr_t>(tab.caret));
    editor_->setFirstVisibleLine(
        static_cast<sptr_t>(tab.firstVisibleLine));
    editor_->setXOffset(static_cast<sptr_t>(tab.horizontalOffset));
    applyLargeFileMode(tab.largeFileMode);
    switchingTabs_ = false;

    {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->setCurrentIndex(index);
    }
    editHistoryWidget_->refresh();
    currentLine_ = editor_->currentOneBasedLine();
    cursorPositionLabel_->setText(
        tr("Ln %1, Col %2")
            .arg(currentLine_)
            .arg(editor_->column(editor_->currentPos()) + 1));
    updateWindowTitle();
    updateDocumentStatus();
    editor_->QWidget::setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::synchronizeTabOrder()
{
    if (synchronizingTabOrder_
        || tabBar_->count() != static_cast<int>(tabs_.size())) {
        return;
    }

    bool orderChanged = false;
    for (int index = 0; index < tabBar_->count(); ++index) {
        if (tabBar_->tabData(index).value<qintptr>()
            != tabs_.at(index).documentHandle) {
            orderChanged = true;
            break;
        }
    }
    if (!orderChanged) {
        return;
    }

    const qintptr activeDocument = currentTabIndex_ >= 0
        && currentTabIndex_ < static_cast<int>(tabs_.size())
        ? tabs_.at(currentTabIndex_).documentHandle : 0;
    synchronizingTabOrder_ = true;
    std::vector<TabState> reorderedTabs;
    reorderedTabs.reserve(tabs_.size());
    for (int index = 0; index < tabBar_->count(); ++index) {
        const qintptr documentHandle =
            tabBar_->tabData(index).value<qintptr>();
        const auto found = std::find_if(
            tabs_.begin(), tabs_.end(),
            [documentHandle](const TabState& tab) {
                return tab.documentHandle == documentHandle;
            });
        if (found == tabs_.end()) {
            synchronizingTabOrder_ = false;
            return;
        }
        reorderedTabs.push_back(std::move(*found));
    }
    tabs_ = std::move(reorderedTabs);
    const auto active = std::find_if(
        tabs_.cbegin(), tabs_.cend(),
        [activeDocument](const TabState& tab) {
            return tab.documentHandle == activeDocument;
        });
    currentTabIndex_ = active == tabs_.cend()
        ? tabBar_->currentIndex()
        : static_cast<int>(active - tabs_.cbegin());
    synchronizingTabOrder_ = false;
}

void MainWindow::applyTabBarAppearance(const Appearance& appearance)
{
    QColor background = appearance.backgroundColor;
    QColor text = appearance.textColor;
    background.setAlpha(255);
    text.setAlpha(255);
    const QColor inactive = blendedTabColor(background, text, 7);
    const QColor hover = blendedTabColor(background, text, 13);
    const QColor border = blendedTabColor(background, text, 22);
    tabBar_->setStyleSheet(QStringLiteral(R"(
QTabBar#documentTabBar {
    background: transparent;
    border: none;
}
QTabBar#documentTabBar::tab {
    min-width: 104px;
    max-width: 220px;
    min-height: 25px;
    margin: 3px 1px 0 0;
    padding: 2px 9px;
    background: %1;
    color: %2;
    border: 1px solid %4;
    border-bottom: none;
    border-top-left-radius: 7px;
    border-top-right-radius: 7px;
}
QTabBar#documentTabBar::tab:hover:!selected { background: %3; }
QTabBar#documentTabBar::tab:selected {
    background: %5;
    border-color: %4;
}
QTabBar#documentTabBar::close-button {
    width: 16px;
    height: 16px;
    margin-left: 5px;
}
)")
        .arg(inactive.name(QColor::HexRgb), text.name(QColor::HexRgb),
             hover.name(QColor::HexRgb), border.name(QColor::HexRgb),
             background.name(QColor::HexRgb)));
}

void MainWindow::switchRelativeTab(int delta)
{
    if (tabs_.size() < 2 || fileManager_->isBusy()) {
        return;
    }
    const int count = static_cast<int>(tabs_.size());
    switchToTab((currentTabIndex_ + delta + count) % count);
}

void MainWindow::requestCloseTab(int index)
{
    if (fileManager_->isBusy() || index < 0
        || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    switchToTab(index);
    const qintptr documentHandle = currentTab().documentHandle;
    requestAfterUnsavedCheck([this, documentHandle] {
        const auto found = std::find_if(
            tabs_.cbegin(), tabs_.cend(), [documentHandle](const TabState& tab) {
                return tab.documentHandle == documentHandle;
            });
        if (found != tabs_.cend()) {
            closeTab(static_cast<int>(found - tabs_.cbegin()));
        }
    });
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    if (tabs_.size() == 1) {
        const sptr_t oldDocument = static_cast<sptr_t>(
            tabs_.front().documentHandle);
        if (!editor_->resetDocument()) {
            return;
        }
        editor_->releaseTabDocument(oldDocument);
        tabs_.front() = TabState{};
        tabs_.front().documentHandle = editor_->retainCurrentDocument();
        applyLargeFileMode(LargeFileMode::Normal);
        updateWindowTitle();
        updateDocumentStatus();
        return;
    }

    const sptr_t removedDocument = static_cast<sptr_t>(
        tabs_.at(index).documentHandle);
    const bool removingCurrent = index == currentTabIndex_;
    tabs_.erase(tabs_.begin() + index);
    {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->removeTab(index);
    }
    if (removingCurrent) {
        currentTabIndex_ = -1;
        switchToTab(std::min(index, static_cast<int>(tabs_.size()) - 1), true);
    } else if (index < currentTabIndex_) {
        --currentTabIndex_;
        const QSignalBlocker blocker(tabBar_);
        tabBar_->setCurrentIndex(currentTabIndex_);
    }
    editor_->releaseTabDocument(removedDocument);
    savePersistentSettings();
}

void MainWindow::beginCloseAllTabs(bool quitApplication)
{
    if (fileManager_->isBusy() || closeAllTabsInProgress_) {
        return;
    }
    snapshotCurrentTabView();
    closingSessionPaths_ = sessionTabPaths();
    closeAllTabsInProgress_ = true;
    quitAfterClosingTabs_ = quitApplication;
    continueCloseAllTabs();
}

void MainWindow::continueCloseAllTabs()
{
    if (!closeAllTabsInProgress_ || fileManager_->isBusy()) {
        return;
    }
    if (tabs_.size() > 1) {
        switchToTab(0);
        requestAfterUnsavedCheck([this] {
            closeTab(0);
            QTimer::singleShot(0, this, &MainWindow::continueCloseAllTabs);
        });
        return;
    }
    if (currentTab().document.isModified()) {
        requestAfterUnsavedCheck([this] {
            closeTab(0);
            QTimer::singleShot(0, this, &MainWindow::continueCloseAllTabs);
        });
        return;
    }

    savePersistentSettings();
    closeAllTabsInProgress_ = false;
    if (quitAfterClosingTabs_) {
        quitAfterClosingTabs_ = false;
        emit applicationQuitAccepted();
    } else {
        closeAfterSave_ = true;
        close();
    }
}

void MainWindow::updateTabBarVisibility()
{
    tabBar_->setVisible(!windowController_->isFrameless()
                        && !windowController_->isMinimalMode());
}

void MainWindow::updateTabTitle(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    const TabState& tab = tabs_.at(index);
    const QString marker = tab.document.isModified()
        ? QStringLiteral("*") : QString();
    const QString displayName = tab.document.isUntitled()
        ? tr("Untitled") : tab.document.displayName();
    tabBar_->setTabText(index, marker + displayName);
    tabBar_->setTabToolTip(index, tab.document.path().isEmpty()
        ? tr("Untitled") : QDir::toNativeSeparators(tab.document.path()));
}

MainWindow::TabState& MainWindow::currentTab()
{
    return tabs_.at(currentTabIndex_);
}

const MainWindow::TabState& MainWindow::currentTab() const
{
    return tabs_.at(currentTabIndex_);
}

bool MainWindow::currentTabIsPristineUntitled() const
{
    return currentTab().document.isUntitled()
        && !currentTab().document.isModified()
        && editor_->isEmpty()
        && editor_->editHistory().isEmpty();
}

int MainWindow::tabIndexForPath(const QString& path) const
{
    const QString absolutePath = QDir::cleanPath(
        QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
    for (int index = 0; index < static_cast<int>(tabs_.size()); ++index) {
        if (tabs_.at(index).document.path().compare(
                absolutePath, pathCaseSensitivity) == 0) {
            return index;
        }
    }
    return -1;
}

QStringList MainWindow::sessionTabPaths() const
{
    if (closeAllTabsInProgress_ && !closingSessionPaths_.isEmpty()) {
        return closingSessionPaths_;
    }
    QStringList paths;
    for (const TabState& tab : tabs_) {
        if (!tab.document.isUntitled()) {
            paths.append(tab.document.path());
        }
    }
    return paths;
}

void MainWindow::replaceCurrentTabDocumentHandle()
{
    const sptr_t oldDocument = static_cast<sptr_t>(
        currentTab().documentHandle);
    currentTab().documentHandle = editor_->retainCurrentDocument();
    tabBar_->setTabData(currentTabIndex_,
                        QVariant::fromValue(currentTab().documentHandle));
    editor_->releaseTabDocument(oldDocument);
}

void MainWindow::openFiles(const QStringList& paths)
{
    for (const QString& path : paths) {
        if (!path.trimmed().isEmpty()) {
            pendingOpenPaths_.append(QDir::cleanPath(
                QFileInfo(path).absoluteFilePath()));
        }
    }
    processPendingOpenFiles();
}

void MainWindow::handleExternalOpenRequest(const QStringList& paths)
{
    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    raise();
    activateWindow();
    if (paths.isEmpty()) {
        if (fileManager_->isBusy()) {
            ++pendingNewTabs_;
        } else {
            newDocument();
        }
    } else {
        openFiles(paths);
    }
}

void MainWindow::processPendingOpenFiles()
{
    if (fileManager_->isBusy()) {
        return;
    }
    if (pendingNewTabs_ > 0) {
        --pendingNewTabs_;
        addBlankTab();
        QTimer::singleShot(0, this, &MainWindow::processPendingOpenFiles);
        return;
    }
    if (pendingOpenPaths_.isEmpty()) {
        return;
    }

    const QString path = pendingOpenPaths_.takeFirst();
    const int existingTab = tabIndexForPath(path);
    if (existingTab >= 0) {
        switchToTab(existingTab);
        QTimer::singleShot(0, this, &MainWindow::processPendingOpenFiles);
        return;
    }
    if (!currentTabIsPristineUntitled() && addBlankTab() < 0) {
        return;
    }
    loadingTabIndex_ = currentTabIndex_;
    if (!fileManager_->openFile(path)) {
        loadingTabIndex_ = -1;
        QMessageBox::warning(this, tr("Open file"),
                             tr("Another file operation is in progress."));
    }
}

void MainWindow::chooseAndOpenFile()
{
    const QString initialDirectory = currentTab().document.isUntitled()
        ? lastDirectory_
        : QFileInfo(currentTab().document.path()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Text File"), initialDirectory,
        tr("Text files (*);;All files (*)"));
    if (!path.isEmpty()) {
        requestOpenFile(path);
    }
}

void MainWindow::requestOpenFile(const QString& path)
{
    openFiles({path});
}

void MainWindow::addRecentFile(const QString& path)
{
    const QString absolutePath = QDir::cleanPath(
        QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
    recentFiles_.removeIf([&absolutePath](const QString& recentPath) {
        return recentPath.compare(absolutePath, pathCaseSensitivity) == 0;
    });
    recentFiles_.prepend(absolutePath);
    if (recentFiles_.size() > maximumRecentFiles) {
        recentFiles_.resize(maximumRecentFiles);
    }
    rebuildRecentFilesMenu();
}

void MainWindow::removeRecentFile(const QString& path)
{
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
    recentFiles_.removeIf([&path](const QString& recentPath) {
        return recentPath.compare(path, pathCaseSensitivity) == 0;
    });
    rebuildRecentFilesMenu();
}

void MainWindow::rebuildRecentFilesMenu()
{
    if (recentFilesMenu_ == nullptr) {
        return;
    }

    recentFilesMenu_->clear();
    if (recentFiles_.isEmpty()) {
        auto* emptyAction = recentFilesMenu_->addAction(tr("No Recent Files"));
        emptyAction->setEnabled(false);
        recentFilesMenu_->setEnabled(false);
        return;
    }

    recentFilesMenu_->setEnabled(!fileManager_->isBusy());
    for (qsizetype index = 0; index < recentFiles_.size(); ++index) {
        const QString& path = recentFiles_.at(index);
        QString label = QDir::toNativeSeparators(path);
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        if (index < 9) {
            label.prepend(QStringLiteral("&%1 ").arg(index + 1));
        } else {
            label.prepend(QStringLiteral("%1 ").arg(index + 1));
        }
        auto* action = recentFilesMenu_->addAction(label);
        action->setData(path);
        action->setToolTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] {
            if (!QFileInfo::exists(path)) {
                removeRecentFile(path);
                savePersistentSettings();
                QMessageBox::warning(
                    this, tr("Open recent file"),
                    tr("The file no longer exists and was removed from the "
                       "recent files list.\n%1").arg(path));
                return;
            }
            requestOpenFile(path);
        });
    }

    recentFilesMenu_->addSeparator();
    auto* clearAction = recentFilesMenu_->addAction(tr("&Clear Recent Files"));
    clearAction->setObjectName(QStringLiteral("clearRecentFilesAction"));
    connect(clearAction, &QAction::triggered, this, [this] {
        recentFiles_.clear();
        rebuildRecentFilesMenu();
        savePersistentSettings();
    });
}

bool MainWindow::saveDocument()
{
    return currentTab().document.isUntitled()
        ? saveDocumentAs() : startSave(currentTab().document.path());
}

bool MainWindow::saveDocumentAs()
{
    const QString path = chooseSavePath();
    return !path.isEmpty() && startSave(path);
}

bool MainWindow::startSave(const QString& path)
{
    const bool started = LargeFilePolicy::usesLargeDocument(
                             currentTab().largeFileMode)
        ? fileManager_->saveFileStreaming(path, currentTab().document.encoding(),
                                          editor_->documentLength())
        : fileManager_->saveFile(path, editor_->textUtf8(),
                                 currentTab().document.encoding());
    if (!started) {
        QMessageBox::warning(this, tr("Save file"),
                             tr("Another file operation is in progress."));
        return false;
    }
    statusBar()->showMessage(tr("Saving %1").arg(path));
    return true;
}

void MainWindow::reloadDocument()
{
    if (!currentTab().document.isUntitled()) {
        requestAfterUnsavedCheck(
            [this, path = currentTab().document.path()] {
            fileManager_->openFile(path);
        });
    }
}

void MainWindow::requestAfterUnsavedCheck(std::function<void()> action)
{
    if (fileManager_->isBusy()) {
        return;
    }
    if (!currentTab().document.isModified()) {
        action();
        return;
    }

    const auto choice = QMessageBox::warning(
        this, tr("Unsaved changes"),
        tr("Save changes to %1?").arg(currentTab().document.isUntitled()
            ? tr("Untitled") : currentTab().document.displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Discard) {
        action();
    } else if (choice == QMessageBox::Save) {
        pendingAfterSave_ = std::move(action);
        if (!saveDocument()) {
            pendingAfterSave_ = {};
            if (closeAllTabsInProgress_) {
                closeAllTabsInProgress_ = false;
                quitAfterClosingTabs_ = false;
                closingSessionPaths_.clear();
            }
        }
    } else if (closeAllTabsInProgress_) {
        closeAllTabsInProgress_ = false;
        quitAfterClosingTabs_ = false;
        closingSessionPaths_.clear();
    }
}

void MainWindow::updateWindowTitle()
{
    updateTabTitle(currentTabIndex_);
    const QString marker = currentTab().document.isModified()
        ? QStringLiteral("*") : QString();
    const QString displayName = currentTab().document.isUntitled()
        ? tr("Untitled") : currentTab().document.displayName();
    setWindowTitle(QStringLiteral("%1%2 — Vinson Editor")
                       .arg(marker, displayName));
}

void MainWindow::updateDocumentStatus()
{
    QStringList fields{encodingName(currentTab().document.encoding()),
                       lineEndingName(currentTab().document.lineEnding()),
                       QLocale().formattedDataSize(
                           currentTab().document.fileSize())};
    const QString modeName = LargeFilePolicy::displayName(
        currentTab().largeFileMode);
    if (!modeName.isEmpty()) {
        fields.append(modeName);
    }
    documentInfoLabel_->setText(fields.join(QStringLiteral(" | ")));
    reloadAction_->setEnabled(!currentTab().document.isUntitled()
                              && !fileManager_->isBusy());
}

void MainWindow::applyLargeFileMode(LargeFileMode mode)
{
    currentTab().largeFileMode = mode;
    const bool wrapEnabled = LargeFilePolicy::defaultsWordWrapOff(mode)
        ? false : preferredWordWrap_;
    {
        const QSignalBlocker blocker(wrapAction_);
        wrapAction_->setChecked(wrapEnabled);
    }
    editor_->setWordWrapEnabled(wrapEnabled);
    updateDocumentStatus();
}

void MainWindow::restorePersistentSettings()
{
    restoringSettings_ = true;
    const ApplicationSettings settings = settingsManager_->load();
    lastDirectory_ = settings.lastDirectory;
    recentFiles_ = settings.recentFiles;
    restoreTabsOnStartup_ = settings.restoreTabsOnStartup;
    if (restoreTabsOnStartup_) {
        for (const QString& path : settings.openTabs) {
            if (QFileInfo::exists(path)) {
                pendingOpenPaths_.append(path);
            }
        }
    }
    rebuildRecentFilesMenu();
    themeManager_->applyAppearance(settings.appearance);
    preferredWordWrap_ = settings.wordWrap;
    {
        const QSignalBlocker wrapBlocker(wrapAction_);
        const QSignalBlocker lineBlocker(lineNumberAction_);
        wrapAction_->setChecked(settings.wordWrap);
        lineNumberAction_->setChecked(settings.lineNumbers);
    }
    editor_->setWordWrapEnabled(settings.wordWrap);
    editor_->setLineNumbersVisible(settings.lineNumbers);
    windowController_->setAlwaysOnTop(settings.alwaysOnTop);
    windowController_->setFrameless(settings.frameless);
    bossKey_ = settings.bossKey;
    focusShortcut_ = settings.focusShortcut;
    if (!settings.windowGeometry.isEmpty()) {
        restoreGeometry(settings.windowGeometry);
    }
    ensureWindowOnScreen();
    restoringSettings_ = false;
    updateTabBarVisibility();
}

void MainWindow::savePersistentSettings()
{
    if (restoringSettings_ || settingsManager_ == nullptr
        || wrapAction_ == nullptr || lineNumberAction_ == nullptr) {
        return;
    }
    ApplicationSettings settings;
    settings.appearance = themeManager_->appearance();
    settings.windowGeometry = windowController_->persistableGeometry();
    settings.lastDirectory = lastDirectory_;
    settings.recentFiles = recentFiles_;
    settings.openTabs = sessionTabPaths();
    settings.restoreTabsOnStartup = restoreTabsOnStartup_;
    settings.wordWrap = preferredWordWrap_;
    settings.lineNumbers = lineNumberAction_->isChecked();
    settings.alwaysOnTop = windowController_->isAlwaysOnTop();
    settings.frameless = windowController_->persistableFrameless();
    settings.bossKey = bossKey_;
    settings.focusShortcut = focusShortcut_;
    if (!settingsManager_->save(settings)) {
        qWarning() << "Could not persist settings to"
                   << settingsManager_->fileName();
    }
}

void MainWindow::adjustBackgroundAlpha(int delta)
{
    Appearance appearance = themeManager_->appearance();
    const int alpha = std::clamp(
        appearance.backgroundColor.alpha() + delta, 0, 255);
    if (alpha == appearance.backgroundColor.alpha()) {
        return;
    }
    appearance.backgroundColor.setAlpha(alpha);
    themeManager_->applyAppearance(appearance);
    savePersistentSettings();
    statusBar()->showMessage(tr("Background alpha: %1").arg(alpha), 1500);
}

void MainWindow::adjustFontSize(int steps)
{
    if (steps == 0) {
        return;
    }

    Appearance appearance = themeManager_->appearance();
    const qreal currentSize = appearance.font.pointSizeF();
    const qreal pointSize = std::clamp(
        currentSize + static_cast<qreal>(steps), 6.0, 72.0);
    if (qFuzzyCompare(pointSize, currentSize)) {
        return;
    }
    appearance.font.setPointSizeF(pointSize);
    themeManager_->applyAppearance(appearance);
    savePersistentSettings();
    statusBar()->showMessage(
        tr("Font size: %1 pt").arg(pointSize, 0, 'f', 1), 1500);
}

void MainWindow::ensureWindowOnScreen()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    const QRect restoredFrame = frameGeometry();
    const bool intersectsScreen = std::any_of(
        screens.cbegin(), screens.cend(), [&restoredFrame](const QScreen* screen) {
            if (screen == nullptr) {
                return false;
            }
            const QRect intersection =
                screen->availableGeometry().intersected(restoredFrame);
            return intersection.width() >= 64 && intersection.height() >= 32;
        });
    if (intersectsScreen) {
        return;
    }

    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary == nullptr) {
        return;
    }
    const QRect available = primary->availableGeometry();
    const QSize safeSize = size().boundedTo(available.size());
    resize(safeSize);
    move(available.center() - QPoint(width() / 2, height() / 2));
}

void MainWindow::updateBusyUi()
{
    const bool busy = fileManager_->isBusy();
    newAction_->setEnabled(!busy);
    openAction_->setEnabled(!busy);
    recentFilesMenu_->setEnabled(!busy && !recentFiles_.isEmpty());
    saveAction_->setEnabled(!busy);
    saveAsAction_->setEnabled(!busy);
    reloadAction_->setEnabled(!busy && !currentTab().document.isUntitled());
    findAction_->setEnabled(!busy);
    replaceAction_->setEnabled(!busy);
    findNextAction_->setEnabled(!busy);
    findPreviousAction_->setEnabled(!busy);
    goToLineAction_->setEnabled(!busy);
    findReplaceWidget_->setEnabled(!busy);
    tabBar_->setEnabled(!busy);
    progressBar_->setVisible(busy);
    cancelOperationButton_->setVisible(busy);
    if (fileManager_->operation() == FileManager::Operation::Saving) {
        editor_->setEnabled(false);
        progressBar_->setRange(0, 0);
    } else if (!busy && !loadReplacedDocument_) {
        editor_->setEnabled(true);
    }
}

void MainWindow::handleLoadFailureState()
{
    editor_->setEnabled(true);
    if (!loadReplacedDocument_) {
        return;
    }
    editor_->completeFileLoad(LineEnding::None);
    currentTab().document.reset();
    currentTab().document.setModified(!editor_->isEmpty());
    loadReplacedDocument_ = false;
    loadingTabIndex_ = -1;
    updateWindowTitle();
    updateDocumentStatus();
}

QString MainWindow::chooseSavePath()
{
    const QString suggested = currentTab().document.isUntitled()
        ? QDir(lastDirectory_).filePath(QStringLiteral("Untitled.txt"))
        : currentTab().document.path();
    return QFileDialog::getSaveFileName(
        this, tr("Save Text File"), suggested,
        tr("Text files (*.txt);;All files (*)"));
}

} // namespace vinson
