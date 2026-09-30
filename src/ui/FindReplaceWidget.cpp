#include "ui/FindReplaceWidget.h"

#include "settings/Appearance.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPalette>
#include <QShortcut>
#include <QStyle>
#include <QVBoxLayout>

namespace vinson {

FindReplaceWidget::FindReplaceWidget(QWidget* parent)
    : QFrame(parent)
    , findEdit_(new QLineEdit(this))
    , replacementEdit_(new QLineEdit(this))
    , matchCaseCheck_(new QCheckBox(tr("Match case"), this))
    , wholeWordCheck_(new QCheckBox(tr("Whole word"), this))
    , wrapAroundCheck_(new QCheckBox(tr("Wrap around"), this))
    , resultLabel_(new QLabel(this))
    , replacementRow_(new QWidget(this))
{
    setObjectName(QStringLiteral("findReplaceWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::StyledPanel);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    findEdit_->setObjectName(QStringLiteral("findText"));
    replacementEdit_->setObjectName(QStringLiteral("replacementText"));
    matchCaseCheck_->setObjectName(QStringLiteral("matchCase"));
    wholeWordCheck_->setObjectName(QStringLiteral("wholeWord"));
    wrapAroundCheck_->setObjectName(QStringLiteral("wrapAround"));
    wrapAroundCheck_->setChecked(true);
    findEdit_->setClearButtonEnabled(true);
    replacementEdit_->setClearButtonEnabled(true);

    nextButton_ = new QPushButton(tr("Next"), this);
    previousButton_ = new QPushButton(tr("Previous"), this);
    cancelSearchButton_ = new QPushButton(tr("Cancel Search"), this);
    cancelSearchButton_->setObjectName(QStringLiteral("cancelSearchButton"));
    cancelSearchButton_->setToolTip(QStringLiteral("Esc"));
    cancelSearchButton_->hide();
    resultLabel_->setObjectName(QStringLiteral("searchResultLabel"));
    auto* closeButton = new QPushButton(tr("Close"), this);
    auto* replaceButton = new QPushButton(tr("Replace"), replacementRow_);
    auto* replaceAllButton = new QPushButton(tr("Replace All"), replacementRow_);

    auto* findRow = new QHBoxLayout;
    findRow->setContentsMargins(0, 0, 0, 0);
    findRow->addWidget(new QLabel(tr("Find:"), this));
    findRow->addWidget(findEdit_, 1);
    findRow->addWidget(previousButton_);
    findRow->addWidget(nextButton_);
    findRow->addWidget(cancelSearchButton_);
    findRow->addWidget(closeButton);

    auto* replaceRowLayout = new QHBoxLayout(replacementRow_);
    replaceRowLayout->setContentsMargins(0, 0, 0, 0);
    replaceRowLayout->addWidget(new QLabel(tr("Replace:"), replacementRow_));
    replaceRowLayout->addWidget(replacementEdit_, 1);
    replaceRowLayout->addWidget(replaceButton);
    replaceRowLayout->addWidget(replaceAllButton);

    auto* optionsRow = new QHBoxLayout;
    optionsRow->setContentsMargins(0, 0, 0, 0);
    optionsRow->addWidget(matchCaseCheck_);
    optionsRow->addWidget(wholeWordCheck_);
    optionsRow->addWidget(wrapAroundCheck_);
    optionsRow->addWidget(resultLabel_, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addLayout(findRow);
    layout->addWidget(replacementRow_);
    layout->addLayout(optionsRow);

    findEdit_->installEventFilter(this);
    replacementEdit_->installEventFilter(this);

    connect(findEdit_, &QLineEdit::textChanged,
            this, &FindReplaceWidget::searchTextChanged);
    connect(replacementEdit_, &QLineEdit::textChanged,
            this, &FindReplaceWidget::replacementTextChanged);
    connect(matchCaseCheck_, &QCheckBox::toggled,
            this, [this] { publishOptions(); });
    connect(wholeWordCheck_, &QCheckBox::toggled,
            this, [this] { publishOptions(); });
    connect(wrapAroundCheck_, &QCheckBox::toggled,
            this, [this] { publishOptions(); });
    connect(nextButton_, &QPushButton::clicked,
            this, &FindReplaceWidget::findNextRequested);
    connect(previousButton_, &QPushButton::clicked,
            this, &FindReplaceWidget::findPreviousRequested);
    connect(replaceButton, &QPushButton::clicked,
            this, &FindReplaceWidget::replaceRequested);
    connect(replaceAllButton, &QPushButton::clicked,
            this, &FindReplaceWidget::replaceAllRequested);
    connect(closeButton, &QPushButton::clicked,
            this, &FindReplaceWidget::closeRequested);
    connect(cancelSearchButton_, &QPushButton::clicked,
            this, &FindReplaceWidget::cancelSearchRequested);

    auto* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escapeShortcut, &QShortcut::activated,
            this, [this] {
                if (searching_) {
                    emit cancelSearchRequested();
                } else {
                    emit closeRequested();
                }
            });
    hide();
}

void FindReplaceWidget::open(bool replaceMode, const QString& initialText)
{
    replaceMode_ = replaceMode;
    replacementRow_->setVisible(replaceMode_);
    if (!initialText.isEmpty()) {
        findEdit_->setText(initialText);
    }
    resultLabel_->clear();
    show();
    findEdit_->setFocus();
    findEdit_->selectAll();
    emit searchTextChanged(findEdit_->text());
    emit replacementTextChanged(replacementEdit_->text());
    publishOptions();
}

bool FindReplaceWidget::isReplaceMode() const noexcept
{
    return replaceMode_;
}

QString FindReplaceWidget::findText() const
{
    return findEdit_->text();
}

QString FindReplaceWidget::replacementText() const
{
    return replacementEdit_->text();
}

SearchOptions FindReplaceWidget::options() const
{
    return {matchCaseCheck_->isChecked(), wholeWordCheck_->isChecked(),
            wrapAroundCheck_->isChecked()};
}

void FindReplaceWidget::applyAppearance(const Appearance& appearance,
                                        bool frameless)
{
    QColor background = appearance.backgroundColor;
    if (!frameless) {
        background.setAlpha(255);
    }
    setProperty("findPanelBackgroundColor", background);
    setStyleSheet(QStringLiteral(R"(
QFrame#findReplaceWidget {
    background: %1;
}
)").arg(background.name(background.alpha() == 255
                            ? QColor::HexRgb : QColor::HexArgb)));

    QPalette panelPalette = palette();
    panelPalette.setColor(QPalette::Window, background);
    setPalette(panelPalette);
}

void FindReplaceWidget::setResult(SearchResult result, const QString& message)
{
    resultLabel_->setText(message);
    resultLabel_->setProperty("searchResult", static_cast<int>(result));
    resultLabel_->style()->unpolish(resultLabel_);
    resultLabel_->style()->polish(resultLabel_);
}

void FindReplaceWidget::setSearching(bool searching)
{
    const bool restoreFocus = !searching && cancelSearchButton_->hasFocus();
    searching_ = searching;
    findEdit_->setEnabled(!searching);
    replacementRow_->setEnabled(!searching);
    matchCaseCheck_->setEnabled(!searching);
    wholeWordCheck_->setEnabled(!searching);
    wrapAroundCheck_->setEnabled(!searching);
    nextButton_->setEnabled(!searching);
    previousButton_->setEnabled(!searching);
    cancelSearchButton_->setVisible(searching);
    if (isVisible() && searching) {
        cancelSearchButton_->setFocus();
    } else if (restoreFocus) {
        findEdit_->setFocus();
    }
}

bool FindReplaceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == findEdit_ || watched == replacementEdit_)
        && event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (searching_) {
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers().testFlag(Qt::ShiftModifier)) {
                emit findPreviousRequested();
            } else {
                emit findNextRequested();
            }
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void FindReplaceWidget::publishOptions()
{
    emit optionsChanged(options());
}

} // namespace vinson
