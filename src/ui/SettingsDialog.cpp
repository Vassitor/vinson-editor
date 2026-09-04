#include "ui/SettingsDialog.h"

#include "settings/ThemeManager.h"
#include "window/GlobalShortcut.h"

#include <QApplication>
#include <QColorDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vinson {
namespace {

QColor blendedColor(const QColor& background, const QColor& foreground,
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

SettingsDialog::SettingsDialog(const Appearance& appearance,
                               const QKeySequence& bossKey,
                               const QKeySequence& focusShortcut,
                               bool restoreTabsOnStartup,
                               QWidget* parent)
    : QDialog(parent)
    , fontCombo_(new QFontComboBox(this))
    , fontSizeSpin_(new QDoubleSpinBox(this))
    , textColorButton_(new QPushButton(this))
    , backgroundColorButton_(new QPushButton(this))
    , cursorColorButton_(new QPushButton(this))
    , selectionTextColorButton_(new QPushButton(this))
    , backgroundAlphaSlider_(new QSlider(Qt::Horizontal, this))
    , backgroundAlphaSpin_(new QSpinBox(this))
    , textAlphaSlider_(new QSlider(Qt::Horizontal, this))
    , textAlphaSpin_(new QSpinBox(this))
    , bossKeyEdit_(new QKeySequenceEdit(this))
    , focusShortcutEdit_(new QKeySequenceEdit(this))
    , restoreTabsOnStartupCheck_(new QCheckBox(
          tr("Restore open tabs on startup"), this))
{
    qRegisterMetaType<Appearance>();
    setPalette(QApplication::palette());
    setAutoFillBackground(true);
    setWindowTitle(tr("Settings"));
    setModal(true);
    setMinimumSize(520, 540);

    fontCombo_->setObjectName(QStringLiteral("fontFamily"));
    fontSizeSpin_->setObjectName(QStringLiteral("fontSize"));
    fontSizeSpin_->setRange(6.0, 72.0);
    fontSizeSpin_->setDecimals(1);
    fontSizeSpin_->setSingleStep(0.5);
    fontSizeSpin_->setSuffix(tr(" pt"));
    textColorButton_->setObjectName(QStringLiteral("textColor"));
    backgroundColorButton_->setObjectName(QStringLiteral("backgroundColor"));
    cursorColorButton_->setObjectName(QStringLiteral("cursorColor"));
    selectionTextColorButton_->setObjectName(QStringLiteral("selectionTextColor"));
    backgroundAlphaSlider_->setObjectName(QStringLiteral("backgroundAlpha"));
    backgroundAlphaSlider_->setRange(0, 255);
    backgroundAlphaSpin_->setRange(0, 255);
    textAlphaSlider_->setObjectName(QStringLiteral("textAlpha"));
    textAlphaSlider_->setRange(0, 255);
    textAlphaSpin_->setRange(0, 255);
    bossKeyEdit_->setObjectName(QStringLiteral("bossKey"));
    bossKeyEdit_->setMaximumSequenceLength(1);
    focusShortcutEdit_->setObjectName(QStringLiteral("focusShortcut"));
    focusShortcutEdit_->setMaximumSequenceLength(1);
    restoreTabsOnStartupCheck_->setObjectName(
        QStringLiteral("restoreTabsOnStartup"));
    restoreTabsOnStartupCheck_->setChecked(restoreTabsOnStartup);

    auto* alphaRow = new QWidget(this);
    auto* alphaLayout = new QHBoxLayout(alphaRow);
    alphaLayout->setContentsMargins(0, 0, 0, 0);
    alphaLayout->addWidget(backgroundAlphaSlider_, 1);
    alphaLayout->addWidget(backgroundAlphaSpin_);

    auto* textAlphaRow = new QWidget(this);
    auto* textAlphaLayout = new QHBoxLayout(textAlphaRow);
    textAlphaLayout->setContentsMargins(0, 0, 0, 0);
    textAlphaLayout->addWidget(textAlphaSlider_, 1);
    textAlphaLayout->addWidget(textAlphaSpin_);

    auto* explanation = new QLabel(
        tr("Background alpha changes only the background. Font opacity changes editor text while window controls remain opaque."),
        this);
    explanation->setObjectName(QStringLiteral("transparencyExplanation"));
    explanation->setWordWrap(true);
    auto* bossKeyExplanation = new QLabel(
        tr("The boss key works system-wide. Include Ctrl, Alt, Shift, or the Windows key."),
        this);
    bossKeyExplanation->setWordWrap(true);
    auto* focusShortcutExplanation = new QLabel(
        tr("The focus shortcut shows, restores, and activates the window, then focuses the editor for immediate typing."),
        this);
    focusShortcutExplanation->setWordWrap(true);

    auto* categories = new QTabWidget(this);
    categories->setObjectName(QStringLiteral("settingsCategories"));

    auto* appearancePage = new QWidget(categories);
    appearancePage->setObjectName(QStringLiteral("appearanceCategory"));
    auto* appearanceLayout = new QVBoxLayout(appearancePage);
    appearanceLayout->setContentsMargins(12, 12, 12, 12);
    appearanceLayout->setSpacing(12);

    auto* typographyGroup = new QGroupBox(tr("Typography"), appearancePage);
    typographyGroup->setObjectName(QStringLiteral("typographyGroup"));
    auto* typographyForm = new QFormLayout(typographyGroup);
    typographyForm->addRow(tr("Font:"), fontCombo_);
    typographyForm->addRow(tr("Font size:"), fontSizeSpin_);

    auto* colorsGroup = new QGroupBox(tr("Colors"), appearancePage);
    colorsGroup->setObjectName(QStringLiteral("colorsGroup"));
    auto* colorsForm = new QFormLayout(colorsGroup);
    colorsForm->addRow(tr("Text color:"), textColorButton_);
    colorsForm->addRow(tr("Background color:"), backgroundColorButton_);
    colorsForm->addRow(tr("Cursor color:"), cursorColorButton_);
    colorsForm->addRow(tr("Selected text color:"),
                       selectionTextColorButton_);

    auto* transparencyGroup = new QGroupBox(
        tr("Transparency"), appearancePage);
    transparencyGroup->setObjectName(QStringLiteral("transparencyGroup"));
    auto* transparencyLayout = new QVBoxLayout(transparencyGroup);
    auto* transparencyForm = new QFormLayout;
    transparencyForm->addRow(tr("Font opacity:"), textAlphaRow);
    transparencyForm->addRow(tr("Background alpha:"), alphaRow);
    transparencyLayout->addLayout(transparencyForm);
    transparencyLayout->addWidget(explanation);

    appearanceLayout->addWidget(typographyGroup);
    appearanceLayout->addWidget(colorsGroup);
    appearanceLayout->addWidget(transparencyGroup);
    appearanceLayout->addStretch();

    auto* shortcutsPage = new QWidget(categories);
    shortcutsPage->setObjectName(QStringLiteral("shortcutsCategory"));
    auto* shortcutsLayout = new QVBoxLayout(shortcutsPage);
    shortcutsLayout->setContentsMargins(12, 12, 12, 12);
    shortcutsLayout->setSpacing(10);
    auto* shortcutsGroup = new QGroupBox(
        tr("Global Shortcuts"), shortcutsPage);
    shortcutsGroup->setObjectName(QStringLiteral("shortcutsGroup"));
    auto* shortcutsForm = new QFormLayout(shortcutsGroup);
    shortcutsForm->addRow(tr("Boss key:"), bossKeyEdit_);
    shortcutsForm->addRow(tr("Focus shortcut:"), focusShortcutEdit_);
    shortcutsLayout->addWidget(shortcutsGroup);
    shortcutsLayout->addWidget(bossKeyExplanation);
    shortcutsLayout->addWidget(focusShortcutExplanation);
    shortcutsLayout->addStretch();

    auto* sessionPage = new QWidget(categories);
    sessionPage->setObjectName(QStringLiteral("sessionCategory"));
    auto* sessionLayout = new QVBoxLayout(sessionPage);
    sessionLayout->setContentsMargins(12, 12, 12, 12);
    sessionLayout->setSpacing(10);
    auto* sessionGroup = new QGroupBox(tr("Session"), sessionPage);
    sessionGroup->setObjectName(QStringLiteral("sessionGroup"));
    auto* sessionGroupLayout = new QVBoxLayout(sessionGroup);
    sessionGroupLayout->addWidget(restoreTabsOnStartupCheck_);
    auto* sessionExplanation = new QLabel(
        tr("Reopen file-backed tabs from the previous session. Unsaved new tabs are not stored."),
        sessionPage);
    sessionExplanation->setWordWrap(true);
    sessionGroupLayout->addWidget(sessionExplanation);
    sessionLayout->addWidget(sessionGroup);
    sessionLayout->addStretch();

    categories->addTab(appearancePage, tr("Appearance"));
    categories->addTab(shortcutsPage, tr("Shortcuts"));
    categories->addTab(sessionPage, tr("Session"));

    const QColor dialogBackground = palette().color(QPalette::Window);
    const QColor dialogText = palette().color(QPalette::WindowText);
    const QColor categoryText = blendedColor(dialogBackground, dialogText, 58);
    const QColor categoryBorder = blendedColor(
        dialogBackground, dialogText, 18);
    setProperty("categoryTextColor", categoryText);
    setProperty("categoryBorderColor", categoryBorder);
    setStyleSheet(QStringLiteral(R"(
QGroupBox {
    border: 1px solid %1;
    border-radius: 6px;
    color: %2;
    font-weight: 400;
    margin-top: 10px;
    padding-top: 8px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
}
QTabWidget::pane {
    border: 1px solid %1;
    top: -1px;
}
QTabBar::tab {
    border: 1px solid %1;
    color: %2;
    padding: 7px 16px;
}
QTabBar::tab:selected { color: %3; }
QLabel#transparencyExplanation { color: %2; }
)")
        .arg(categoryBorder.name(QColor::HexRgb),
             categoryText.name(QColor::HexRgb),
             dialogText.name(QColor::HexRgb)));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
            | QDialogButtonBox::RestoreDefaults,
        this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(categories, 1);
    layout->addWidget(buttons);

    connect(fontCombo_, &QFontComboBox::currentFontChanged,
            this, [this](const QFont& font) {
                if (!updating_) {
                    appearance_.font.setFamily(font.family());
                    emitPreview();
                }
            });
    connect(fontSizeSpin_, &QDoubleSpinBox::valueChanged,
            this, [this](double size) {
                if (!updating_) {
                    appearance_.font.setPointSizeF(size);
                    emitPreview();
                }
            });
    connect(backgroundAlphaSlider_, &QSlider::valueChanged,
            backgroundAlphaSpin_, &QSpinBox::setValue);
    connect(backgroundAlphaSpin_, &QSpinBox::valueChanged,
            backgroundAlphaSlider_, &QSlider::setValue);
    connect(backgroundAlphaSpin_, &QSpinBox::valueChanged,
            this, [this](int alpha) {
                if (!updating_) {
                    appearance_.backgroundColor.setAlpha(alpha);
                    emitPreview();
                }
            });
    connect(textAlphaSlider_, &QSlider::valueChanged,
            textAlphaSpin_, &QSpinBox::setValue);
    connect(textAlphaSpin_, &QSpinBox::valueChanged,
            textAlphaSlider_, &QSlider::setValue);
    connect(textAlphaSpin_, &QSpinBox::valueChanged,
            this, [this](int alpha) {
                if (!updating_) {
                    appearance_.textColor.setAlpha(alpha);
                    emitPreview();
                }
            });
    connect(textColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.textColor, textColorButton_, tr("Text Color"));
    });
    connect(backgroundColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.backgroundColor, backgroundColorButton_,
                    tr("Background Color"));
    });
    connect(cursorColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.cursorColor, cursorColorButton_, tr("Cursor Color"));
    });
    connect(selectionTextColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.selectionTextColor, selectionTextColorButton_,
                    tr("Selected Text Color"));
    });
    connect(bossKeyEdit_, &QKeySequenceEdit::keySequenceChanged,
            this, [this](const QKeySequence& shortcut) {
                if (!updating_) {
                    bossKey_ = shortcut;
                }
            });
    connect(focusShortcutEdit_, &QKeySequenceEdit::keySequenceChanged,
            this, [this](const QKeySequence& shortcut) {
                if (!updating_) {
                    focusShortcut_ = shortcut;
                }
            });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!GlobalShortcut::isSupportedShortcut(bossKey_)) {
            QMessageBox::warning(
                this, tr("Boss key"),
                tr("Use one shortcut containing at least one modifier key."));
            return;
        }
        if (!GlobalShortcut::isSupportedShortcut(focusShortcut_)) {
            QMessageBox::warning(
                this, tr("Focus shortcut"),
                tr("Use one shortcut containing at least one modifier key."));
            return;
        }
        if (!bossKey_.isEmpty() && bossKey_ == focusShortcut_) {
            QMessageBox::warning(
                this, tr("Global shortcuts"),
                tr("The boss key and focus shortcut must be different."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),
            &QPushButton::clicked, this, [this] {
                setAppearance(ThemeManager::defaultAppearance());
                setBossKey(GlobalShortcut::defaultShortcut());
                setFocusShortcut(GlobalShortcut::defaultFocusShortcut());
                emitPreview();
            });

    setAppearance(appearance);
    setBossKey(bossKey);
    setFocusShortcut(focusShortcut);
}

const Appearance& SettingsDialog::appearance() const noexcept
{
    return appearance_;
}

const QKeySequence& SettingsDialog::bossKey() const noexcept
{
    return bossKey_;
}

const QKeySequence& SettingsDialog::focusShortcut() const noexcept
{
    return focusShortcut_;
}

bool SettingsDialog::restoreTabsOnStartup() const noexcept
{
    return restoreTabsOnStartupCheck_->isChecked();
}

void SettingsDialog::setAppearance(const Appearance& appearance)
{
    updating_ = true;
    appearance_ = appearance;
    fontCombo_->setCurrentFont(appearance_.font);
    fontSizeSpin_->setValue(appearance_.font.pointSizeF());
    backgroundAlphaSlider_->setValue(appearance_.backgroundColor.alpha());
    backgroundAlphaSpin_->setValue(appearance_.backgroundColor.alpha());
    textAlphaSlider_->setValue(appearance_.textColor.alpha());
    textAlphaSpin_->setValue(appearance_.textColor.alpha());
    refreshColorButtons();
    updating_ = false;
}

void SettingsDialog::setBossKey(const QKeySequence& bossKey)
{
    updating_ = true;
    bossKey_ = bossKey;
    bossKeyEdit_->setKeySequence(bossKey);
    updating_ = false;
}

void SettingsDialog::setFocusShortcut(const QKeySequence& focusShortcut)
{
    updating_ = true;
    focusShortcut_ = focusShortcut;
    focusShortcutEdit_->setKeySequence(focusShortcut);
    updating_ = false;
}

void SettingsDialog::chooseColor(QColor& color, QPushButton* button,
                                 const QString& title)
{
    QColor initial = color;
    initial.setAlpha(255);
    const QColor chosen = QColorDialog::getColor(
        initial, this, title, QColorDialog::DontUseNativeDialog);
    if (!chosen.isValid()) {
        return;
    }
    const int alpha = color.alpha();
    color = chosen;
    color.setAlpha(alpha);
    styleColorButton(button, color);
    emitPreview();
}

void SettingsDialog::emitPreview()
{
    emit previewChanged(appearance_);
}

void SettingsDialog::refreshColorButtons()
{
    styleColorButton(textColorButton_, appearance_.textColor);
    styleColorButton(backgroundColorButton_, appearance_.backgroundColor);
    styleColorButton(cursorColorButton_, appearance_.cursorColor);
    styleColorButton(selectionTextColorButton_, appearance_.selectionTextColor);
}

void SettingsDialog::styleColorButton(QPushButton* button, const QColor& color)
{
    const QColor text = color.lightness() < 128 ? Qt::white : Qt::black;
    const QColor border = color.lightness() < 128
        ? color.lighter(150) : color.darker(135);
    button->setText(color.name(QColor::HexRgb).toUpper());
    button->setProperty("swatchColor", color.name(QColor::HexRgb));
    button->setMinimumHeight(34);
    button->setStyleSheet(
        QStringLiteral(R"(
QPushButton {
    background-color: %1;
    border: 1px solid %3;
    border-radius: 5px;
    color: %2;
    padding: 5px 12px;
}
QPushButton:hover,
QPushButton:pressed,
QPushButton:focus {
    background-color: %1;
    color: %2;
}
QPushButton:hover,
QPushButton:focus { border-color: %2; }
)")
            .arg(color.name(QColor::HexRgb), text.name(QColor::HexRgb),
                 border.name(QColor::HexRgb)));
}

} // namespace vinson
