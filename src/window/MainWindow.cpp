#include "window/MainWindow.h"

#include "editor/EditorWidget.h"
#include "file/FileManager.h"
#include "file/FileTypes.h"
#include "search/SearchController.h"
#include "settings/SettingsManager.h"
#include "settings/ThemeManager.h"
#include "ui/FindReplaceWidget.h"
#include "ui/SettingsDialog.h"
#include "window/WindowController.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDebug>
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
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <utility>

namespace vinson {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , editor_(new EditorWidget(this))
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
    setAttribute(Qt::WA_TranslucentBackground);
    themeManager_ = new ThemeManager(editor_, this, this);
    settingsManager_ = new SettingsManager(this);
    setAcceptDrops(true);
    auto* centralWidget = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(findReplaceWidget_);
    centralLayout->addWidget(editor_, 1);
    setCentralWidget(centralWidget);
    windowController_ = new WindowController(this, this);
    windowController_->configureMinimalMode(editor_, findReplaceWidget_);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            windowController_, &WindowController::refreshMinimalMinimumSize);
    createMenus();
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
                if (!fileManager_->isBusy()) {
                    document_.setModified(modified);
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
}

void MainWindow::closeEvent(QCloseEvent* event)
{
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
    if (!document_.isModified()) {
        savePersistentSettings();
        event->accept();
        return;
    }

    event->ignore();
    requestAfterUnsavedCheck([this] {
        closeAfterSave_ = true;
        close();
    });
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
    newAction_->setShortcut(QKeySequence::New);
    connect(newAction_, &QAction::triggered, this, &MainWindow::newDocument);

    openAction_ = fileMenu->addAction(tr("&Open…"));
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::chooseAndOpenFile);

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
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

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
        if (enabled && LargeFilePolicy::usesLargeDocument(largeFileMode_)) {
            statusBar()->showMessage(
                tr("Word wrap may be slow in %1.")
                    .arg(LargeFilePolicy::displayName(largeFileMode_)),
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
                if (!enabled) {
                    editor_->QWidget::setFocus();
                }
            });

    auto* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    auto* appearanceAction = settingsMenu->addAction(tr("&Appearance…"));
    connect(appearanceAction, &QAction::triggered,
            this, &MainWindow::showAppearanceSettings);
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
                        largeFileMode_)
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

void MainWindow::showAppearanceSettings()
{
    const Appearance original = themeManager_->appearance();
    SettingsDialog dialog(original, this);
    connect(&dialog, &SettingsDialog::previewChanged,
            themeManager_, &ThemeManager::applyAppearance);
    if (dialog.exec() != QDialog::Accepted) {
        themeManager_->applyAppearance(original);
    } else {
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
            [this](const QByteArray& data, qint64 bytesRead, qint64 totalBytes) {
                if (!loadReplacedDocument_) {
                    return;
                }
                editor_->appendTextUtf8(data);
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
                document_.adoptLoadedFile(info);
                lastDirectory_ = QFileInfo(info.path).absolutePath();
                loadReplacedDocument_ = false;
                updateWindowTitle();
                updateDocumentStatus();
                statusBar()->showMessage(tr("Loaded %1").arg(info.path), 3000);
                savePersistentSettings();
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
                document_.adoptSavedFile(result);
                lastDirectory_ = QFileInfo(result.path).absolutePath();
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
            });
    connect(fileManager_, &FileManager::operationCanceled, this, [this] {
        handleLoadFailureState();
        pendingAfterSave_ = {};
        statusBar()->showMessage(tr("File operation canceled"), 3000);
    });
}

void MainWindow::newDocument()
{
    requestAfterUnsavedCheck([this] {
        if (!editor_->resetDocument()) {
            QMessageBox::critical(
                this, tr("New document"),
                tr("Scintilla could not create a new document."));
            return;
        }
        applyLargeFileMode(LargeFileMode::Normal);
        document_.reset();
        updateWindowTitle();
        updateDocumentStatus();
        statusBar()->showMessage(tr("New document"), 2000);
    });
}

void MainWindow::chooseAndOpenFile()
{
    const QString initialDirectory = document_.isUntitled()
        ? lastDirectory_ : QFileInfo(document_.path()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Text File"), initialDirectory,
        tr("Text files (*);;All files (*)"));
    if (!path.isEmpty()) {
        requestOpenFile(path);
    }
}

void MainWindow::requestOpenFile(const QString& path)
{
    requestAfterUnsavedCheck([this, path] {
        if (!fileManager_->openFile(path)) {
            QMessageBox::warning(this, tr("Open file"),
                                 tr("Another file operation is in progress."));
        }
    });
}

bool MainWindow::saveDocument()
{
    return document_.isUntitled() ? saveDocumentAs()
                                  : startSave(document_.path());
}

bool MainWindow::saveDocumentAs()
{
    const QString path = chooseSavePath();
    return !path.isEmpty() && startSave(path);
}

bool MainWindow::startSave(const QString& path)
{
    const bool started = LargeFilePolicy::usesLargeDocument(largeFileMode_)
        ? fileManager_->saveFileStreaming(path, document_.encoding(),
                                          editor_->documentLength())
        : fileManager_->saveFile(path, editor_->textUtf8(),
                                 document_.encoding());
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
    if (!document_.isUntitled()) {
        requestAfterUnsavedCheck([this, path = document_.path()] {
            fileManager_->openFile(path);
        });
    }
}

void MainWindow::requestAfterUnsavedCheck(std::function<void()> action)
{
    if (fileManager_->isBusy()) {
        return;
    }
    if (!document_.isModified()) {
        action();
        return;
    }

    const auto choice = QMessageBox::warning(
        this, tr("Unsaved changes"),
        tr("Save changes to %1?").arg(document_.displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Discard) {
        action();
    } else if (choice == QMessageBox::Save) {
        pendingAfterSave_ = std::move(action);
        if (!saveDocument()) {
            pendingAfterSave_ = {};
        }
    }
}

void MainWindow::updateWindowTitle()
{
    const QString marker = document_.isModified() ? QStringLiteral("*")
                                                   : QString();
    setWindowTitle(QStringLiteral("%1%2 — Vinson Editor")
                       .arg(marker, document_.displayName()));
}

void MainWindow::updateDocumentStatus()
{
    QStringList fields{encodingName(document_.encoding()),
                       lineEndingName(document_.lineEnding()),
                       QLocale().formattedDataSize(document_.fileSize())};
    const QString modeName = LargeFilePolicy::displayName(largeFileMode_);
    if (!modeName.isEmpty()) {
        fields.append(modeName);
    }
    documentInfoLabel_->setText(fields.join(QStringLiteral(" | ")));
    reloadAction_->setEnabled(!document_.isUntitled() && !fileManager_->isBusy());
}

void MainWindow::applyLargeFileMode(LargeFileMode mode)
{
    largeFileMode_ = mode;
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
    if (!settings.windowGeometry.isEmpty()) {
        restoreGeometry(settings.windowGeometry);
    }
    ensureWindowOnScreen();
    restoringSettings_ = false;
}

void MainWindow::savePersistentSettings()
{
    if (restoringSettings_ || settingsManager_ == nullptr
        || wrapAction_ == nullptr || lineNumberAction_ == nullptr) {
        return;
    }
    const ApplicationSettings settings{
        themeManager_->appearance(),
        windowController_->persistableGeometry(),
        lastDirectory_,
        preferredWordWrap_,
        lineNumberAction_->isChecked(),
        windowController_->isAlwaysOnTop(),
        windowController_->persistableFrameless(),
    };
    if (!settingsManager_->save(settings)) {
        qWarning() << "Could not persist settings to"
                   << settingsManager_->fileName();
    }
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
    saveAction_->setEnabled(!busy);
    saveAsAction_->setEnabled(!busy);
    reloadAction_->setEnabled(!busy && !document_.isUntitled());
    findAction_->setEnabled(!busy);
    replaceAction_->setEnabled(!busy);
    findNextAction_->setEnabled(!busy);
    findPreviousAction_->setEnabled(!busy);
    goToLineAction_->setEnabled(!busy);
    findReplaceWidget_->setEnabled(!busy);
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
    document_.reset();
    document_.setModified(!editor_->isEmpty());
    loadReplacedDocument_ = false;
    updateWindowTitle();
    updateDocumentStatus();
}

QString MainWindow::chooseSavePath()
{
    const QString suggested = document_.isUntitled()
        ? QDir(lastDirectory_).filePath(QStringLiteral("Untitled.txt"))
        : document_.path();
    return QFileDialog::getSaveFileName(
        this, tr("Save Text File"), suggested,
        tr("Text files (*.txt);;All files (*)"));
}

} // namespace vinson
