#include "ui/EditHistoryWidget.h"

#include "editor/EditorWidget.h"
#include "settings/Appearance.h"

#include <QDockWidget>
#include <QLabel>
#include <QLocale>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QPalette>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace vinson {
namespace {

QColor opaqueColor(QColor color)
{
    color.setAlpha(255);
    return color;
}

QColor blendedColor(const QColor& background, const QColor& foreground,
                    int foregroundPercent)
{
    const int backgroundPercent = 100 - foregroundPercent;
    QColor color(
        (background.red() * backgroundPercent
         + foreground.red() * foregroundPercent) / 100,
        (background.green() * backgroundPercent
         + foreground.green() * foregroundPercent) / 100,
        (background.blue() * backgroundPercent
         + foreground.blue() * foregroundPercent) / 100);
    color.setAlpha(255);
    return color;
}

QString colorName(const QColor& color)
{
    return color.name(color.alpha() == 255 ? QColor::HexRgb
                                           : QColor::HexArgb);
}

QString entryPrefix(EditHistoryKind kind)
{
    switch (kind) {
    case EditHistoryKind::Insert:
        return QStringLiteral("+");
    case EditHistoryKind::Delete:
        return QString(QChar(0x2212));
    case EditHistoryKind::Replace:
        return QString(QChar(0x2194));
    case EditHistoryKind::Other:
        return QString(QChar(0x2022));
    }
    return {};
}

QString entryText(const EditHistoryEntry& entry)
{
    QString operation;
    switch (entry.kind) {
    case EditHistoryKind::Insert:
        operation = EditHistoryWidget::tr("Inserted %1 bytes").arg(
            QLocale().toString(entry.insertedBytes));
        break;
    case EditHistoryKind::Delete:
        operation = EditHistoryWidget::tr("Deleted %1 bytes").arg(
            QLocale().toString(entry.deletedBytes));
        break;
    case EditHistoryKind::Replace:
        operation = EditHistoryWidget::tr("Replaced text");
        break;
    case EditHistoryKind::Other:
        operation = EditHistoryWidget::tr("Edited document");
        break;
    }

    if (!entry.preview.isEmpty()) {
        operation += QStringLiteral("  “%1”").arg(entry.preview);
    }
    return operation;
}

} // namespace

EditHistoryWidget::EditHistoryWidget(EditorWidget* editor, QWidget* parent)
    : QWidget(parent)
    , editor_(editor)
    , list_(new QListWidget(this))
    , emptyLabel_(new QLabel(tr("No edits in this document."), this))
    , restoreButton_(new QPushButton(tr("Restore Selected"), this))
    , refreshTimer_(new QTimer(this))
{
    setObjectName(QStringLiteral("editHistoryWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    list_->setObjectName(QStringLiteral("editHistoryList"));
    list_->viewport()->setObjectName(
        QStringLiteral("editHistoryListViewport"));
    list_->viewport()->setAttribute(Qt::WA_StyledBackground, true);
    emptyLabel_->setObjectName(QStringLiteral("editHistoryEmpty"));
    restoreButton_->setObjectName(
        QStringLiteral("editHistoryRestoreButton"));
    restoreButton_->setMinimumHeight(42);
    list_->setAlternatingRowColors(false);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setSpacing(0);
    list_->setWordWrap(true);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setWordWrap(true);
    refreshTimer_->setSingleShot(true);
    refreshTimer_->setInterval(100);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addWidget(emptyLabel_, 1);
    layout->addWidget(list_, 1);
    layout->addWidget(restoreButton_);

    connect(editor_, &EditorWidget::editHistoryChanged,
            this, &EditHistoryWidget::scheduleRefresh);
    connect(editor_, &EditorWidget::documentModified,
            this, &EditHistoryWidget::scheduleRefresh);
    connect(refreshTimer_, &QTimer::timeout,
            this, &EditHistoryWidget::refresh);
    connect(list_, &QListWidget::itemSelectionChanged, this, [this] {
        restoreButton_->setEnabled(
            list_->currentItem() != nullptr
            && list_->currentItem()->data(Qt::UserRole).toInt()
                != editor_->currentEditHistoryPosition());
    });
    connect(list_, &QListWidget::itemActivated,
            this, [this](QListWidgetItem*) { restoreSelectedEntry(); });
    connect(restoreButton_, &QPushButton::clicked,
            this, &EditHistoryWidget::restoreSelectedEntry);

    applyAppearance({editor_->editorFont(), editor_->textColor(),
                     editor_->backgroundColor(), editor_->cursorColor(),
                     editor_->selectionTextColor()});
    refresh();
}

void EditHistoryWidget::applyAppearance(const Appearance& appearance)
{
    const QColor background = appearance.backgroundColor;
    const QColor opaqueBackground = opaqueColor(background);
    const QColor text = opaqueColor(appearance.textColor);
    const QColor mutedText = blendedColor(opaqueBackground, text, 62);
    const QColor border = blendedColor(opaqueBackground, text, 24);
    const QColor hover = blendedColor(opaqueBackground, text, 8);
    const QColor selected = blendedColor(
        opaqueBackground, opaqueColor(appearance.cursorColor), 16);
    const QColor accent = opaqueColor(appearance.cursorColor);

    setProperty("historyBorderColor", border);
    setStyleSheet(QStringLiteral(R"(
QWidget#editHistoryWidget {
    background: %7;
    color: %1;
}
QLabel#editHistoryEmpty {
    color: %2;
    padding: 28px 14px;
}
QListWidget#editHistoryList {
    background: %7;
    border: none;
    outline: none;
    padding: 3px 2px;
}
QWidget#editHistoryListViewport {
    background: %7;
}
QListWidget#editHistoryList::item {
    border: none;
    border-radius: 4px;
    color: %1;
    margin: 1px 2px;
    padding: 7px 10px;
}
QListWidget#editHistoryList::item:hover {
    background: %3;
}
QListWidget#editHistoryList::item:selected {
    background: %5;
    border: none;
    color: %1;
}
QPushButton#editHistoryRestoreButton {
    background: %3;
    border: 1px solid %4;
    border-radius: 7px;
    color: %1;
    font-weight: 600;
    min-height: 36px;
    padding: 4px 14px;
}
QPushButton#editHistoryRestoreButton:hover {
    background: %5;
    border-color: %6;
}
QPushButton#editHistoryRestoreButton:pressed {
    background: %6;
    color: %7;
}
QPushButton#editHistoryRestoreButton:disabled {
    background: transparent;
    border-color: %4;
    color: %2;
}
QScrollBar:vertical {
    background: %7;
    width: 9px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %4;
    border-radius: 4px;
    min-height: 28px;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0;
    border: none;
}
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical { background: %7; }
)")
        .arg(colorName(text), colorName(mutedText), colorName(hover),
             colorName(border), colorName(selected), colorName(accent),
             colorName(opaqueBackground)));

    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, opaqueBackground);
    palette.setColor(QPalette::Base, opaqueBackground);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::ButtonText, text);
    setPalette(palette);
    list_->setPalette(palette);
    list_->viewport()->setPalette(palette);
    list_->viewport()->setAutoFillBackground(true);
    setAutoFillBackground(true);
    update();
}

void EditHistoryWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
    painter.end();
    QWidget::paintEvent(event);
}

void EditHistoryWidget::scheduleRefresh()
{
    refreshTimer_->start();
}

void EditHistoryWidget::refresh()
{
    const int currentPosition = editor_->currentEditHistoryPosition();
    const int savedPosition = editor_->savedEditHistoryPosition();
    const QVector<EditHistoryEntry> history = editor_->editHistory();

    list_->clear();
    auto addState = [this, currentPosition, savedPosition](
                        const QString& text, int undoPosition) {
        QStringList suffixes;
        if (undoPosition == currentPosition) {
            suffixes.append(tr("Current"));
        }
        if (undoPosition == savedPosition) {
            suffixes.append(tr("Saved"));
        }
        const QString label = suffixes.isEmpty()
            ? text
            : tr("%1 (%2)").arg(text, suffixes.join(QStringLiteral(", ")));
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, undoPosition);
        item->setToolTip(label);
        if (undoPosition == currentPosition) {
            list_->setCurrentItem(item);
        }
    };

    addState(QStringLiteral("•  ") + tr("Initial state"), 0);
    for (const EditHistoryEntry& entry : history) {
        addState(QStringLiteral("%1  %2")
                     .arg(entryPrefix(entry.kind), entryText(entry)),
                 entry.undoPosition);
    }

    emptyLabel_->setVisible(history.isEmpty());
    list_->setVisible(!history.isEmpty());
    restoreButton_->setVisible(!history.isEmpty());
    restoreButton_->setEnabled(list_->currentItem() != nullptr
                               && list_->currentItem()->data(Qt::UserRole).toInt()
                                   != currentPosition);
    if (list_->currentItem() != nullptr) {
        list_->scrollToItem(list_->currentItem());
    }
}

void EditHistoryWidget::restoreSelectedEntry()
{
    QListWidgetItem* item = list_->currentItem();
    if (item == nullptr) {
        return;
    }
    editor_->restoreEditHistoryPosition(item->data(Qt::UserRole).toInt());
    refresh();
}

} // namespace vinson

void applyEditHistoryDockAppearance(QDockWidget* dock,
                                    const vinson::Appearance& appearance,
                                    bool floating)
{
    if (dock == nullptr) {
        return;
    }

    const QColor background = appearance.backgroundColor;
    QColor opaqueBackground = background;
    opaqueBackground.setAlpha(255);
    QColor text = appearance.textColor;
    text.setAlpha(255);
    const QColor border = vinson::blendedColor(opaqueBackground, text, 28);
    const QColor hover = vinson::blendedColor(opaqueBackground, text, 10);
    const QColor paintedBackground = opaqueBackground;
    const QString outerBorder = floating
        ? QStringLiteral("none")
        : QStringLiteral("1px solid %1").arg(vinson::colorName(border));

    dock->setTitleBarWidget(nullptr);

    dock->setProperty("historyFloating", floating);
    dock->setProperty("historyBorderColor", border);
    dock->setAttribute(Qt::WA_TranslucentBackground, false);
    dock->setAutoFillBackground(true);
    dock->setStyleSheet(QStringLiteral(R"(
QDockWidget#editHistoryDock {
    background: %1;
    border: %2;
    color: %3;
}
QDockWidget#editHistoryDock > QWidget {
    background: %1;
}
QDockWidget#editHistoryDock::title {
    background: %1;
    border-bottom: 1px solid %4;
    padding: 7px 8px;
    text-align: left;
}
QDockWidget#editHistoryDock::close-button,
QDockWidget#editHistoryDock::float-button {
    background: transparent;
    border: none;
    border-radius: 4px;
    padding: 2px;
}
QDockWidget#editHistoryDock::close-button:hover,
QDockWidget#editHistoryDock::float-button:hover {
    background: %5;
}
)")
        .arg(vinson::colorName(paintedBackground), outerBorder,
             vinson::colorName(text), vinson::colorName(border),
             vinson::colorName(hover)));

    QPalette palette = dock->palette();
    palette.setColor(QPalette::Window, opaqueBackground);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Text, text);
    dock->setPalette(palette);

    if (auto* mainWindow = qobject_cast<QMainWindow*>(dock->parentWidget())) {
        mainWindow->setProperty("historySeparatorColor", border);
        mainWindow->setStyleSheet(QStringLiteral(R"(
QMainWindow#vinsonMainWindow::separator {
    background: %1;
    width: 1px;
    height: 1px;
}
QMainWindow#vinsonMainWindow::separator:hover {
    background: %2;
}
)")
            .arg(vinson::colorName(border),
                 vinson::colorName(
                     vinson::opaqueColor(appearance.cursorColor))));
    }
    dock->style()->unpolish(dock);
    dock->style()->polish(dock);
    dock->update();
}
