#include "ui/SettingsDialog.h"

#include "settings/ThemeManager.h"
#include "settings/AppearanceIO.h"
#include "window/GlobalShortcut.h"

#include <QApplication>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QSaveFile>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>
#include <cmath>

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

bool isValidLocalShortcut(const QKeySequence& shortcut) noexcept
{
    if (shortcut.isEmpty()) {
        return true;
    }
    if (shortcut.count() != 1) {
        return false;
    }
    const Qt::Key key = shortcut[0].key();
    return key != Qt::Key_unknown && key != Qt::Key_Control
        && key != Qt::Key_Shift && key != Qt::Key_Alt
        && key != Qt::Key_Meta;
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
    , boldCheck_(new QCheckBox(tr("Bold"), this))
    , italicCheck_(new QCheckBox(tr("Italic"), this))
    , textColorButton_(new QPushButton(this))
    , backgroundColorButton_(new QPushButton(this))
    , cursorColorButton_(new QPushButton(this))
    , selectionTextColorButton_(new QPushButton(this))
    , selectionBackgroundColorButton_(new QPushButton(this))
    , selectionBackgroundAlphaSpin_(new QSpinBox(this))
    , lineNumberColorButton_(new QPushButton(this))
    , currentLineColorButton_(new QPushButton(this))
    , currentLineAlphaSpin_(new QSpinBox(this))
    , cursorWidthSpin_(new QSpinBox(this))
    , lineSpacingSpin_(new QSpinBox(this))
    , backgroundAlphaSlider_(new QSlider(Qt::Horizontal, this))
    , backgroundAlphaSpin_(new QSpinBox(this))
    , textAlphaSlider_(new QSlider(Qt::Horizontal, this))
    , textAlphaSpin_(new QSpinBox(this))
    , bossKeyEdit_(new QKeySequenceEdit(this))
    , focusShortcutEdit_(new QKeySequenceEdit(this))
    , restoreTabsOnStartupCheck_(new QCheckBox(
          tr("Restore open tabs on startup"), this))
    , presetCombo_(new QComboBox(this))
    , presetShortcutEdit_(new QKeySequenceEdit(this))
    , savePresetButton_(new QPushButton(tr("Save As…"), this))
    , updatePresetButton_(new QPushButton(tr("Update"), this))
    , renamePresetButton_(new QPushButton(tr("Rename…"), this))
    , deletePresetButton_(new QPushButton(tr("Delete"), this))
    , importAppearanceButton_(new QPushButton(tr("Import…"), this))
    , exportAppearanceButton_(new QPushButton(tr("Export…"), this))
    , shortcutTable_(new QTableWidget(this))
{
    qRegisterMetaType<Appearance>();
    setObjectName(QStringLiteral("settingsDialog"));
    QPalette whitePalette = QApplication::palette();
    for (const QPalette::ColorGroup group : {
             QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        whitePalette.setColor(group, QPalette::Window, Qt::white);
        whitePalette.setColor(group, QPalette::Base, Qt::white);
        whitePalette.setColor(group, QPalette::AlternateBase, QColor(248, 249, 250));
        whitePalette.setColor(group, QPalette::Button, Qt::white);
        const QColor text = group == QPalette::Disabled
            ? QColor(128, 128, 128) : QColor(32, 33, 36);
        whitePalette.setColor(group, QPalette::WindowText, text);
        whitePalette.setColor(group, QPalette::Text, text);
        whitePalette.setColor(group, QPalette::ButtonText, text);
    }
    setPalette(whitePalette);
    setAutoFillBackground(true);
    setWindowTitle(tr("Settings"));
    setModal(true);
    setMinimumSize(560, 480);
    const QSize available = screen()->availableGeometry().size();
    resize(QSize(720, 700).boundedTo(available - QSize(48, 80)));
    setFont(QApplication::font());

    fontCombo_->setObjectName(QStringLiteral("fontFamily"));
    fontCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    fontCombo_->setMinimumContentsLength(10);
    fontSizeSpin_->setObjectName(QStringLiteral("fontSize"));
    fontSizeSpin_->setRange(6.0, 72.0);
    fontSizeSpin_->setDecimals(1);
    fontSizeSpin_->setSingleStep(0.5);
    fontSizeSpin_->setSuffix(tr(" pt"));
    textColorButton_->setObjectName(QStringLiteral("textColor"));
    backgroundColorButton_->setObjectName(QStringLiteral("backgroundColor"));
    cursorColorButton_->setObjectName(QStringLiteral("cursorColor"));
    selectionTextColorButton_->setObjectName(QStringLiteral("selectionTextColor"));
    selectionBackgroundColorButton_->setObjectName(
        QStringLiteral("selectionBackgroundColor"));
    selectionBackgroundAlphaSpin_->setObjectName(
        QStringLiteral("selectionBackgroundAlpha"));
    selectionBackgroundAlphaSpin_->setRange(0, 255);
    lineNumberColorButton_->setObjectName(QStringLiteral("lineNumberColor"));
    currentLineColorButton_->setObjectName(QStringLiteral("currentLineColor"));
    currentLineAlphaSpin_->setObjectName(QStringLiteral("currentLineAlpha"));
    currentLineAlphaSpin_->setRange(0, 255);
    boldCheck_->setObjectName(QStringLiteral("fontBold"));
    italicCheck_->setObjectName(QStringLiteral("fontItalic"));
    cursorWidthSpin_->setObjectName(QStringLiteral("cursorWidth"));
    cursorWidthSpin_->setRange(1, 5);
    cursorWidthSpin_->setSuffix(tr(" px"));
    lineSpacingSpin_->setObjectName(QStringLiteral("lineSpacing"));
    lineSpacingSpin_->setRange(0, 20);
    lineSpacingSpin_->setSuffix(tr(" px"));
    backgroundAlphaSlider_->setObjectName(QStringLiteral("backgroundAlpha"));
    backgroundAlphaSlider_->setRange(0, 255);
    backgroundAlphaSpin_->setRange(0, 255);
    backgroundAlphaSpin_->setMinimumWidth(100);
    textAlphaSlider_->setObjectName(QStringLiteral("textAlpha"));
    textAlphaSlider_->setRange(0, 255);
    textAlphaSpin_->setRange(0, 255);
    textAlphaSpin_->setMinimumWidth(100);
    bossKeyEdit_->setObjectName(QStringLiteral("bossKey"));
    bossKeyEdit_->setMaximumSequenceLength(1);
    focusShortcutEdit_->setObjectName(QStringLiteral("focusShortcut"));
    focusShortcutEdit_->setMaximumSequenceLength(1);
    restoreTabsOnStartupCheck_->setObjectName(
        QStringLiteral("restoreTabsOnStartup"));
    restoreTabsOnStartupCheck_->setChecked(restoreTabsOnStartup);
    presetCombo_->setObjectName(QStringLiteral("appearancePreset"));
    presetShortcutEdit_->setObjectName(QStringLiteral("appearancePresetShortcut"));
    presetShortcutEdit_->setMaximumSequenceLength(1);
    savePresetButton_->setObjectName(QStringLiteral("saveAppearancePreset"));
    updatePresetButton_->setObjectName(QStringLiteral("updateAppearancePreset"));
    renamePresetButton_->setObjectName(QStringLiteral("renameAppearancePreset"));
    deletePresetButton_->setObjectName(QStringLiteral("deleteAppearancePreset"));
    importAppearanceButton_->setObjectName(QStringLiteral("importAppearance"));
    exportAppearanceButton_->setObjectName(QStringLiteral("exportAppearance"));
    shortcutTable_->setObjectName(QStringLiteral("applicationShortcuts"));
    shortcutTable_->setColumnCount(2);
    shortcutTable_->setHorizontalHeaderLabels({tr("Command"), tr("Shortcut")});
    shortcutTable_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    shortcutTable_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    shortcutTable_->verticalHeader()->hide();
    shortcutTable_->setAlternatingRowColors(true);
    shortcutTable_->setSelectionMode(QAbstractItemView::NoSelection);
    shortcutTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    shortcutTable_->setShowGrid(false);
    shortcutTable_->verticalHeader()->setMinimumSectionSize(40);
    shortcutTable_->horizontalHeader()->setMinimumHeight(32);
    shortcutTable_->setMinimumHeight(180);

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
        tr("0 = transparent; 255 = opaque. Background opacity applies in frameless and minimal modes. Text opacity affects only editor text."),
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
    auto* fontStyleRow = new QWidget(typographyGroup);
    auto* fontStyleLayout = new QHBoxLayout(fontStyleRow);
    fontStyleLayout->setContentsMargins(0, 0, 0, 0);
    fontStyleLayout->addWidget(boldCheck_);
    fontStyleLayout->addWidget(italicCheck_);
    fontStyleLayout->addStretch();
    typographyForm->addRow(tr("Font style:"), fontStyleRow);
    typographyForm->addRow(tr("Cursor width:"), cursorWidthSpin_);
    typographyForm->addRow(tr("Line spacing:"), lineSpacingSpin_);

    auto* colorsGroup = new QGroupBox(tr("Colors"), appearancePage);
    colorsGroup->setObjectName(QStringLiteral("colorsGroup"));
    auto* colorsForm = new QFormLayout(colorsGroup);
    colorsForm->addRow(tr("Text color:"), textColorButton_);
    colorsForm->addRow(tr("Background color:"), backgroundColorButton_);
    colorsForm->addRow(tr("Cursor color:"), cursorColorButton_);
    colorsForm->addRow(tr("Selected text color:"),
                       selectionTextColorButton_);
    colorsForm->addRow(tr("Selection color:"),
                       selectionBackgroundColorButton_);
    colorsForm->addRow(tr("Selection opacity:"),
                       selectionBackgroundAlphaSpin_);
    colorsForm->addRow(tr("Line number color:"), lineNumberColorButton_);
    colorsForm->addRow(tr("Current line color:"), currentLineColorButton_);
    colorsForm->addRow(tr("Current line opacity:"), currentLineAlphaSpin_);

    auto* transparencyGroup = new QGroupBox(
        tr("Transparency"), appearancePage);
    transparencyGroup->setObjectName(QStringLiteral("transparencyGroup"));
    auto* transparencyLayout = new QVBoxLayout(transparencyGroup);
    auto* transparencyForm = new QFormLayout;
    transparencyForm->addRow(tr("Text opacity:"), textAlphaRow);
    transparencyForm->addRow(tr("Background opacity:"), alphaRow);
    transparencyLayout->addLayout(transparencyForm);
    transparencyLayout->addWidget(explanation);

    auto* presetsGroup = new QGroupBox(tr("Custom Styles"), appearancePage);
    presetsGroup->setObjectName(QStringLiteral("appearancePresetsGroup"));
    auto* presetsLayout = new QVBoxLayout(presetsGroup);
    auto* presetsForm = new QFormLayout;
    presetsForm->addRow(tr("Style:"), presetCombo_);
    presetsForm->addRow(tr("Switch shortcut:"), presetShortcutEdit_);
    presetsLayout->addLayout(presetsForm);
    auto* presetButtons = new QHBoxLayout;
    presetButtons->addWidget(savePresetButton_);
    presetButtons->addWidget(updatePresetButton_);
    presetButtons->addWidget(renamePresetButton_);
    presetButtons->addWidget(deletePresetButton_);
    presetButtons->addStretch();
    presetButtons->addWidget(importAppearanceButton_);
    presetButtons->addWidget(exportAppearanceButton_);
    presetsLayout->addLayout(presetButtons);
    auto* presetExplanation = new QLabel(
        tr("Styles store all appearance options. Import and export use a JSON file containing the current appearance and saved styles."),
        presetsGroup);
    presetExplanation->setWordWrap(true);
    presetsLayout->addWidget(presetExplanation);

    auto* appearanceGrid = new QGridLayout;
    appearanceGrid->setSpacing(12);
    appearanceGrid->setColumnStretch(0, 1);
    appearanceGrid->setColumnStretch(1, 1);
    appearanceGrid->addWidget(typographyGroup, 0, 0);
    appearanceGrid->addWidget(transparencyGroup, 1, 0);
    appearanceGrid->addWidget(colorsGroup, 0, 1, 2, 1);
    appearanceLayout->addLayout(appearanceGrid);
    appearanceLayout->addWidget(presetsGroup);
    appearanceLayout->addStretch();

    auto* shortcutsPage = new QWidget(categories);
    shortcutsPage->setObjectName(QStringLiteral("shortcutsCategory"));
    auto* shortcutsLayout = new QVBoxLayout(shortcutsPage);
    shortcutsLayout->setContentsMargins(12, 12, 12, 12);
    shortcutsLayout->setSpacing(12);
    auto* shortcutsGroup = new QGroupBox(
        tr("Global Shortcuts"), shortcutsPage);
    shortcutsGroup->setObjectName(QStringLiteral("shortcutsGroup"));
    auto* shortcutsForm = new QFormLayout(shortcutsGroup);
    shortcutsForm->addRow(tr("Boss key:"), bossKeyEdit_);
    shortcutsForm->addRow(tr("Focus shortcut:"), focusShortcutEdit_);
    shortcutsLayout->addWidget(shortcutsGroup);
    shortcutsForm->addRow(bossKeyExplanation);
    shortcutsForm->addRow(focusShortcutExplanation);
    auto* applicationShortcutsGroup = new QGroupBox(
        tr("Application Shortcuts"), shortcutsPage);
    applicationShortcutsGroup->setObjectName(
        QStringLiteral("applicationShortcutsGroup"));
    auto* applicationShortcutsLayout = new QVBoxLayout(
        applicationShortcutsGroup);
    applicationShortcutsLayout->addWidget(shortcutTable_);
    shortcutsLayout->addWidget(applicationShortcutsGroup, 1);

    auto* sessionPage = new QWidget(categories);
    sessionPage->setObjectName(QStringLiteral("sessionCategory"));
    auto* sessionLayout = new QVBoxLayout(sessionPage);
    sessionLayout->setContentsMargins(12, 12, 12, 12);
    sessionLayout->setSpacing(12);
    auto* sessionGroup = new QGroupBox(tr("Session"), sessionPage);
    sessionGroup->setObjectName(QStringLiteral("sessionGroup"));
    auto* sessionGroupLayout = new QVBoxLayout(sessionGroup);
    sessionGroupLayout->addWidget(restoreTabsOnStartupCheck_);
    auto* sessionExplanation = new QLabel(
        tr("Reopen previously saved files. Changes that were not saved to disk cannot be restored."),
        sessionPage);
    sessionExplanation->setWordWrap(true);
    sessionGroupLayout->addWidget(sessionExplanation);
    sessionLayout->addWidget(sessionGroup);
    sessionLayout->addStretch();

    for (auto* form : {colorsForm, transparencyForm}) {
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    }
    for (auto* form : {typographyForm, colorsForm, transparencyForm,
                       presetsForm, shortcutsForm}) {
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(6);
    }
    for (auto* group : {typographyGroup, colorsGroup, transparencyGroup,
                        presetsGroup, shortcutsGroup,
                        applicationShortcutsGroup, sessionGroup}) {
        group->layout()->setContentsMargins(12, 8, 12, 12);
        group->layout()->setSpacing(8);
    }
    colorsForm->setVerticalSpacing(8);
    for (auto* label : {explanation, bossKeyExplanation,
                        focusShortcutExplanation, presetExplanation,
                        sessionExplanation}) {
        label->setProperty("settingsHint", true);
    }
    for (auto* page : {appearancePage, shortcutsPage, sessionPage}) {
        auto* scroll = new QScrollArea(categories);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(page);
        const QString title = page == appearancePage ? tr("Appearance")
            : page == shortcutsPage ? tr("Shortcuts") : tr("Session");
        categories->addTab(scroll, title);
    }

    const QColor dialogBackground = palette().color(QPalette::Window);
    const QColor dialogText = palette().color(QPalette::WindowText);
    const QColor categoryText = blendedColor(dialogBackground, dialogText, 68);
    const QColor categoryBorder = blendedColor(
        dialogBackground, dialogText, 12);
    setProperty("categoryTextColor", categoryText);
    setProperty("categoryBorderColor", categoryBorder);
    const QString settingsStyle = QStringLiteral(R"(
QDialog#settingsDialog { background: #ffffff; color: #202124; }
QWidget#appearanceCategory,
QWidget#shortcutsCategory,
QWidget#sessionCategory {
    background: #f5f6f8;
    color: #202124;
}
QGroupBox {
    background: #ffffff;
    border: 1px solid %1;
    border-radius: 10px;
    color: %3;
    font-weight: 600;
    margin-top: 0;
    padding-top: 26px;
}
QGroupBox::title {
    subcontrol-origin: padding;
    subcontrol-position: top left;
    left: 12px;
    top: 10px;
}
QTabWidget::pane {
    border: 1px solid %1;
    border-radius: 10px;
    background: #f5f6f8;
}
QTabBar::tab {
    background: transparent;
    border: none;
    border-bottom: 3px solid transparent;
    color: %2;
    padding: 9px 18px;
    margin-right: 6px;
    margin-bottom: 4px;
}
QTabBar::tab:selected {
    color: #315fc4;
    border-bottom-color: #4775d6;
    font-weight: 600;
}
QTabBar::tab:hover { background: #f0f4fc; color: #315fc4; }
QLabel { color: %3; background: transparent; }
QLabel[settingsHint="true"] { color: %2; }
QComboBox, QAbstractSpinBox, QKeySequenceEdit QLineEdit {
    background: #ffffff;
    color: %3;
    border: 1px solid #dce0e7;
    border-radius: 6px;
    min-height: 22px;
    padding: 3px 8px;
    selection-background-color: #4775d6;
    selection-color: #ffffff;
}
QComboBox:hover, QAbstractSpinBox:hover, QKeySequenceEdit QLineEdit:hover {
    border-color: #aab9d5;
}
QComboBox:focus, QAbstractSpinBox:focus, QKeySequenceEdit QLineEdit:focus { border-color: #4775d6; }
QComboBox:disabled, QAbstractSpinBox:disabled, QKeySequenceEdit QLineEdit:disabled {
    background: #f5f6f8;
    color: #9098a6;
    border-color: #e6e9ef;
}
QComboBox { padding-right: 30px; }
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    margin: 2px;
    background: #f5f6f8;
    border: 1px solid #e6e9ef;
    border-radius: 4px;
}
QComboBox::drop-down:hover { background: #f0f4fc; border-color: #aab9d5; }
QComboBox::drop-down:on { background: #e3ebfa; border-color: #aab9d5; }
QComboBox::down-arrow, QAbstractSpinBox::down-arrow {
    image: url(:/settings/chevron-down.xpm);
    width: 12px;
    height: 8px;
}
QAbstractSpinBox { padding-right: 30px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
    subcontrol-origin: border;
    width: 22px;
    height: 10px;
    background: #f5f6f8;
    border: 1px solid #e6e9ef;
    border-radius: 3px;
}
QAbstractSpinBox::up-button { subcontrol-position: top right; margin-top: 2px; margin-right: 2px; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; margin-bottom: 2px; margin-right: 2px; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover {
    background: #f0f4fc;
    border-color: #aab9d5;
}
QAbstractSpinBox::up-button:pressed, QAbstractSpinBox::down-button:pressed {
    background: #e3ebfa;
    border-color: #aab9d5;
}
QAbstractSpinBox::up-arrow {
    image: url(:/settings/chevron-up.xpm);
    width: 12px;
    height: 8px;
}
QComboBox::drop-down:disabled,
QAbstractSpinBox::up-button:disabled, QAbstractSpinBox::down-button:disabled,
QAbstractSpinBox::up-button:off, QAbstractSpinBox::down-button:off {
    background: #f5f6f8;
    border-color: #e6e9ef;
}
QComboBox::down-arrow:disabled,
QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off {
    image: url(:/settings/chevron-down-disabled.xpm);
}
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off {
    image: url(:/settings/chevron-up-disabled.xpm);
}
QComboBox QAbstractItemView {
    background: #ffffff;
    color: %3;
    border: 1px solid #dce0e7;
    selection-background-color: #eaf0fc;
    selection-color: #244fba;
    outline: none;
}
QPushButton {
    background: #ffffff;
    color: %3;
    border: 1px solid #dce0e7;
    border-radius: 6px;
    min-height: 22px;
    padding: 3px 10px;
}
QPushButton:hover { background: #f0f4fc; border-color: #aab9d5; }
QPushButton:pressed { background: #e3ebfa; }
QPushButton:focus { border-color: #4775d6; }
QPushButton:disabled { background: #f5f6f8; color: #a0a7b2; border-color: #e6e9ef; }
QPushButton#confirmSettings { background: #3867cf; color: #ffffff; border-color: #3867cf; }
QPushButton#confirmSettings:hover { background: #2f5cbe; }
QPushButton#confirmSettings:pressed { background: #264fa8; }
QPushButton#confirmSettings:focus { border: 2px solid #203f83; padding: 2px 9px; }
QPushButton#deleteAppearancePreset:enabled { color: #b74444; }
QPushButton#deleteAppearancePreset:hover:enabled { background: #fff1f1; border-color: #e5b6b6; }
QSlider:horizontal { min-height: 24px; }
QSlider::groove:horizontal { height: 4px; background: #e4e8ef; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #6288dc; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #ffffff;
    border: 2px solid #6288dc;
    width: 12px;
    margin: -5px 0;
    border-radius: 7px;
}
QSlider::handle:horizontal:hover, QSlider::handle:horizontal:focus { background: #eaf0fc; border-color: #315fc4; }
QCheckBox { color: %3; spacing: 8px; padding: 2px 0; }
QTableWidget {
    background: #ffffff;
    alternate-background-color: #f8f9fc;
    color: %3;
    border: 1px solid #e6e9ef;
    border-radius: 6px;
}
QTableWidget::item { padding: 0 10px; }
QHeaderView::section {
    background: #f5f6f8;
    color: %2;
    border: none;
    border-bottom: 1px solid #e6e9ef;
    padding: 5px 10px;
}
QScrollBar:vertical { background: #f5f6f8; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #ccd2dc; min-height: 32px; border-radius: 3px; }
QScrollBar::handle:vertical:hover { background: #a9b4c6; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
)")
        .arg(categoryBorder.name(QColor::HexRgb),
             categoryText.name(QColor::HexRgb),
             dialogText.name(QColor::HexRgb));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
            | QDialogButtonBox::RestoreDefaults,
        this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(
        QStringLiteral("confirmSettings"));
    buttons->button(QDialogButtonBox::RestoreDefaults)->setToolTip(
        tr("Reset appearance, application and global shortcuts, and the startup option. Saved styles are kept."));
    for (auto* button : buttons->buttons()) {
        button->setMinimumWidth(76);
    }
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 10, 16, 12);
    layout->setSpacing(12);
    layout->addWidget(categories, 1);
    layout->addWidget(buttons);
    setStyleSheet(settingsStyle);

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
    connect(boldCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!updating_) {
            appearance_.font.setBold(enabled);
            emitPreview();
        }
    });
    connect(italicCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!updating_) {
            appearance_.font.setItalic(enabled);
            emitPreview();
        }
    });
    connect(cursorWidthSpin_, &QSpinBox::valueChanged, this, [this](int width) {
        if (!updating_) {
            appearance_.cursorWidth = width;
            emitPreview();
        }
    });
    connect(lineSpacingSpin_, &QSpinBox::valueChanged, this, [this](int spacing) {
        if (!updating_) {
            appearance_.lineSpacing = spacing;
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
    connect(selectionBackgroundColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.selectionBackgroundColor,
                    selectionBackgroundColorButton_, tr("Selection Color"));
    });
    connect(selectionBackgroundAlphaSpin_, &QSpinBox::valueChanged,
            this, [this](int alpha) {
                if (!updating_) {
                    appearance_.selectionBackgroundColor.setAlpha(alpha);
                    emitPreview();
                }
            });
    connect(lineNumberColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.lineNumberColor, lineNumberColorButton_,
                    tr("Line Number Color"));
    });
    connect(currentLineColorButton_, &QPushButton::clicked, this, [this] {
        chooseColor(appearance_.currentLineColor, currentLineColorButton_,
                    tr("Current Line Color"));
    });
    connect(currentLineAlphaSpin_, &QSpinBox::valueChanged,
            this, [this](int alpha) {
                if (!updating_) {
                    appearance_.currentLineColor.setAlpha(alpha);
                    emitPreview();
                }
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
    connect(presetCombo_, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                refreshPresetControls(index);
                if (index >= 0 && index < presets_.size() && !updating_) {
                    setAppearance(presets_.at(index).appearance);
                    emitPreview();
                }
            });
    connect(presetCombo_, &QComboBox::activated, this, [this](int index) {
        // Choosing the already-selected style should discard appearance edits
        // just as choosing a different style does.
        if (index >= 0 && index < presets_.size()
            && presets_.at(index).appearance != appearance_) {
            setAppearance(presets_.at(index).appearance);
            emitPreview();
        }
    });
    connect(presetShortcutEdit_, &QKeySequenceEdit::keySequenceChanged,
            this, [this](const QKeySequence& shortcut) {
                const int index = presetCombo_->currentIndex();
                if (!updating_ && index >= 0 && index < presets_.size()) {
                    presets_[index].shortcut = shortcut;
                }
            });
    connect(savePresetButton_, &QPushButton::clicked,
            this, &SettingsDialog::savePresetAs);
    connect(updatePresetButton_, &QPushButton::clicked, this, [this] {
        const int index = presetCombo_->currentIndex();
        if (index >= 0 && index < presets_.size()) {
            presets_[index].appearance = appearance_;
            refreshPresetControls(index);
        }
    });
    connect(renamePresetButton_, &QPushButton::clicked,
            this, &SettingsDialog::renamePreset);
    connect(importAppearanceButton_, &QPushButton::clicked,
            this, &SettingsDialog::importAppearance);
    connect(exportAppearanceButton_, &QPushButton::clicked,
            this, &SettingsDialog::exportAppearance);
    connect(deletePresetButton_, &QPushButton::clicked, this, [this] {
        const int index = presetCombo_->currentIndex();
        if (index < 0 || index >= presets_.size()) {
            return;
        }
        presets_.removeAt(index);
        presetCombo_->removeItem(index);
        refreshPresetControls(presetCombo_->currentIndex());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!validateShortcuts()) {
            return;
        }
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
                restoreTabsOnStartupCheck_->setChecked(true);
                auto bindings = shortcutBindings_;
                for (auto& binding : bindings) {
                    binding.shortcut = binding.defaultShortcut;
                }
                setShortcutBindings(bindings);
                setAppearancePresets(presets_);
                emitPreview();
            });

    setAppearance(appearance);
    setBossKey(bossKey);
    setFocusShortcut(focusShortcut);
    setAppearancePresets({});
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
    boldCheck_->setChecked(appearance_.font.bold());
    italicCheck_->setChecked(appearance_.font.italic());
    cursorWidthSpin_->setValue(appearance_.cursorWidth);
    lineSpacingSpin_->setValue(appearance_.lineSpacing);
    currentLineAlphaSpin_->setValue(appearance_.currentLineColor.alpha());
    selectionBackgroundAlphaSpin_->setValue(
        appearance_.selectionBackgroundColor.alpha());
    backgroundAlphaSlider_->setValue(appearance_.backgroundColor.alpha());
    backgroundAlphaSpin_->setValue(appearance_.backgroundColor.alpha());
    textAlphaSlider_->setValue(appearance_.textColor.alpha());
    textAlphaSpin_->setValue(appearance_.textColor.alpha());
    refreshColorButtons();
    updating_ = false;
    refreshPresetControls(presetCombo_->currentIndex());
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

void SettingsDialog::setAppearancePresets(
    const QVector<AppearancePreset>& presets)
{
    presets_ = presets;
    const QSignalBlocker blocker(presetCombo_);
    updating_ = true;
    presetCombo_->clear();
    for (const AppearancePreset& preset : presets_) {
        presetCombo_->addItem(preset.name);
    }
    int matchingIndex = -1;
    for (qsizetype index = 0; index < presets_.size(); ++index) {
        if (presets_.at(index).appearance == appearance_) {
            matchingIndex = static_cast<int>(index);
            break;
        }
    }
    presetCombo_->setCurrentIndex(matchingIndex);
    updating_ = false;
    refreshPresetControls(presetCombo_->currentIndex());
}

const QVector<AppearancePreset>& SettingsDialog::appearancePresets() const noexcept
{
    return presets_;
}

void SettingsDialog::setShortcutBindings(
    const QVector<ShortcutBinding>& bindings)
{
    shortcutBindings_ = bindings;
    shortcutTable_->setRowCount(static_cast<int>(bindings.size()));
    for (qsizetype index = 0; index < bindings.size(); ++index) {
        const ShortcutBinding& binding = bindings.at(index);
        auto* label = new QTableWidgetItem(binding.name);
        label->setFlags(Qt::ItemIsEnabled);
        shortcutTable_->setItem(static_cast<int>(index), 0, label);
        auto* editor = qobject_cast<QKeySequenceEdit*>(
            shortcutTable_->cellWidget(static_cast<int>(index), 1));
        if (editor == nullptr) {
            editor = new QKeySequenceEdit(shortcutTable_);
            editor->layout()->setContentsMargins(8, 4, 8, 4);
            editor->setMaximumSequenceLength(1);
            shortcutTable_->setCellWidget(static_cast<int>(index), 1, editor);
            connect(editor, &QKeySequenceEdit::keySequenceChanged,
                    this, [this, index](const QKeySequence& shortcut) {
                        if (index < shortcutBindings_.size()) {
                            shortcutBindings_[index].shortcut = shortcut;
                        }
                    });
        }
        const QSignalBlocker blocker(editor);
        editor->setKeySequence(binding.shortcut);
        editor->setAccessibleName(binding.name);
        editor->setObjectName(
            QStringLiteral("shortcut_%1").arg(binding.id));
    }
    shortcutTable_->resizeRowsToContents();
    for (int row = 0; row < shortcutTable_->rowCount(); ++row) {
        const auto* editor = shortcutTable_->cellWidget(row, 1);
        shortcutTable_->setRowHeight(row, qMax(40, editor->minimumSizeHint().height()));
    }
}

const QVector<ShortcutBinding>& SettingsDialog::shortcutBindings() const noexcept
{
    return shortcutBindings_;
}

void SettingsDialog::refreshPresetControls(int index)
{
    presetCombo_->setPlaceholderText(presets_.isEmpty()
        ? tr("No saved styles") : tr("Select a saved style"));
    const bool selected = index >= 0 && index < presets_.size();
    updatePresetButton_->setEnabled(selected
        && presets_.at(index).appearance != appearance_);
    renamePresetButton_->setEnabled(selected);
    deletePresetButton_->setEnabled(selected);
    presetShortcutEdit_->setEnabled(selected);
    const bool wasUpdating = updating_;
    updating_ = true;
    presetShortcutEdit_->setKeySequence(
        selected ? presets_.at(index).shortcut : QKeySequence());
    updating_ = wasUpdating;
}

QString SettingsDialog::requestPresetName(const QString& initial,
                                          int ignoredIndex)
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("Style Name"), tr("Name:"), QLineEdit::Normal,
        initial, &accepted).trimmed().left(maximumAppearancePresetNameLength);
    if (!accepted || name.isEmpty()) {
        return {};
    }
    for (qsizetype index = 0; index < presets_.size(); ++index) {
        if (index != ignoredIndex
            && presets_.at(index).name.compare(name, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, tr("Style Name"),
                                 tr("A style with this name already exists."));
            return {};
        }
    }
    return name;
}

void SettingsDialog::savePresetAs()
{
    if (presets_.size() >= maximumAppearancePresets) {
        QMessageBox::warning(this, tr("Custom Styles"),
                             tr("You can save up to %1 styles.")
                                 .arg(maximumAppearancePresets));
        return;
    }
    const QString name = requestPresetName({}, -1);
    if (name.isEmpty()) {
        return;
    }
    presets_.append({name, appearance_, {}});
    presetCombo_->addItem(name);
    presetCombo_->setCurrentIndex(static_cast<int>(presets_.size() - 1));
}

void SettingsDialog::renamePreset()
{
    const int index = presetCombo_->currentIndex();
    if (index < 0 || index >= presets_.size()) {
        return;
    }
    const QString name = requestPresetName(presets_.at(index).name, index);
    if (!name.isEmpty()) {
        presets_[index].name = name;
        presetCombo_->setItemText(index, name);
    }
}

void SettingsDialog::importAppearance()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Appearance"), {},
        tr("Vinson appearance files (*.json);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import Appearance"),
                             tr("Could not read the selected file: %1")
                                 .arg(file.errorString()));
        return;
    }
    QString error;
    const auto bundle = importAppearanceBundle(file.readAll(), &error);
    if (!bundle.has_value()) {
        QMessageBox::warning(this, tr("Import Appearance"),
                             tr("The appearance file is not valid: %1").arg(error));
        return;
    }
    setAppearance(bundle->appearance);
    setAppearancePresets(bundle->presets);
    emitPreview();
    QMessageBox::information(this, tr("Import Appearance"),
                             tr("Appearance and saved styles were imported."));
}

void SettingsDialog::exportAppearance()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Appearance"), QStringLiteral("vinson-appearance.json"),
        tr("Vinson appearance files (*.json);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".json");
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(exportAppearanceBundle({appearance_, presets_})) < 0
        || !file.commit()) {
        QMessageBox::warning(this, tr("Export Appearance"),
                             tr("Could not write the appearance file: %1")
                                 .arg(file.errorString()));
        return;
    }
    QMessageBox::information(this, tr("Export Appearance"),
                             tr("Appearance and saved styles were exported."));
}

bool SettingsDialog::validateShortcuts()
{
    QList<QKeySequence> shortcuts;
    for (const QKeySequence& shortcut : {bossKey_, focusShortcut_}) {
        if (!shortcut.isEmpty()) {
            shortcuts.append(shortcut);
        }
    }
    for (const ShortcutBinding& binding : std::as_const(shortcutBindings_)) {
        if (!isValidLocalShortcut(binding.shortcut)) {
            QMessageBox::warning(
                this, tr("Application Shortcut"),
                tr("The shortcut for “%1” is not valid.").arg(binding.name));
            return false;
        }
        if (!binding.shortcut.isEmpty() && shortcuts.contains(binding.shortcut)) {
            QMessageBox::warning(
                this, tr("Application Shortcut"),
                tr("The shortcut for “%1” is already in use.").arg(binding.name));
            return false;
        }
        if (!binding.shortcut.isEmpty()) {
            shortcuts.append(binding.shortcut);
        }
    }
    for (const AppearancePreset& preset : std::as_const(presets_)) {
        if (!isValidLocalShortcut(preset.shortcut)) {
            QMessageBox::warning(
                this, tr("Style Shortcut"),
                tr("The shortcut for “%1” is not valid.").arg(preset.name));
            return false;
        }
        if (!preset.shortcut.isEmpty() && shortcuts.contains(preset.shortcut)) {
            QMessageBox::warning(
                this, tr("Style Shortcut"),
                tr("The shortcut for “%1” is already in use.").arg(preset.name));
            return false;
        }
        if (!preset.shortcut.isEmpty()) {
            shortcuts.append(preset.shortcut);
        }
    }
    return true;
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
    refreshPresetControls(presetCombo_->currentIndex());
    emit previewChanged(appearance_);
}

void SettingsDialog::refreshColorButtons()
{
    styleColorButton(textColorButton_, appearance_.textColor);
    styleColorButton(backgroundColorButton_, appearance_.backgroundColor);
    styleColorButton(cursorColorButton_, appearance_.cursorColor);
    styleColorButton(selectionTextColorButton_, appearance_.selectionTextColor);
    styleColorButton(selectionBackgroundColorButton_,
                     appearance_.selectionBackgroundColor);
    styleColorButton(lineNumberColorButton_, appearance_.lineNumberColor);
    styleColorButton(currentLineColorButton_, appearance_.currentLineColor);
}

void SettingsDialog::styleColorButton(QPushButton* button, const QColor& color)
{
    const auto linear = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92
            : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const qreal luminance = 0.2126 * linear(color.redF())
        + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    const QColor text = luminance > 0.179 ? Qt::black : Qt::white;
    const QColor border = color.lightness() < 128
        ? color.lighter(150) : color.darker(135);
    button->setText(color.name(QColor::HexRgb).toUpper());
    button->setProperty("swatchColor", color.name(QColor::HexRgb));
    button->setMinimumHeight(30);
    button->setStyleSheet(
        QStringLiteral(R"(
QPushButton {
    background-color: %1;
    border: 1px solid %3;
    border-radius: 6px;
    color: %2;
    padding: 3px 10px;
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
