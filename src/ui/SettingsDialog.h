#pragma once

#include "settings/Appearance.h"

#include <QDialog>
#include <QKeySequence>

class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QFontComboBox;
class QKeySequenceEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;

namespace vinson {

struct ShortcutBinding
{
    QString id;
    QString name;
    QKeySequence shortcut;
    QKeySequence defaultShortcut;

    friend bool operator==(const ShortcutBinding&, const ShortcutBinding&) = default;
};

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const Appearance& appearance,
                            const QKeySequence& bossKey,
                            const QKeySequence& focusShortcut,
                            bool restoreTabsOnStartup,
                            QWidget* parent = nullptr);

    [[nodiscard]] const Appearance& appearance() const noexcept;
    [[nodiscard]] const QKeySequence& bossKey() const noexcept;
    [[nodiscard]] const QKeySequence& focusShortcut() const noexcept;
    [[nodiscard]] bool restoreTabsOnStartup() const noexcept;
    void setAppearance(const Appearance& appearance);
    void setBossKey(const QKeySequence& bossKey);
    void setFocusShortcut(const QKeySequence& focusShortcut);
    void setAppearancePresets(const QVector<AppearancePreset>& presets);
    [[nodiscard]] const QVector<AppearancePreset>& appearancePresets() const noexcept;
    void setShortcutBindings(const QVector<ShortcutBinding>& bindings);
    [[nodiscard]] const QVector<ShortcutBinding>& shortcutBindings() const noexcept;

signals:
    void previewChanged(const vinson::Appearance& appearance);

private:
    void chooseColor(QColor& color, QPushButton* button, const QString& title);
    void emitPreview();
    void refreshColorButtons();
    static void styleColorButton(QPushButton* button, const QColor& color);
    void refreshPresetControls(int index);
    void savePresetAs();
    void renamePreset();
    void importAppearance();
    void exportAppearance();
    [[nodiscard]] QString requestPresetName(const QString& initial, int ignoredIndex);
    [[nodiscard]] bool validateShortcuts();

    Appearance appearance_;
    QKeySequence bossKey_;
    QKeySequence focusShortcut_;
    QFontComboBox* fontCombo_ = nullptr;
    QDoubleSpinBox* fontSizeSpin_ = nullptr;
    QCheckBox* boldCheck_ = nullptr;
    QCheckBox* italicCheck_ = nullptr;
    QPushButton* textColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QPushButton* cursorColorButton_ = nullptr;
    QPushButton* selectionTextColorButton_ = nullptr;
    QPushButton* selectionBackgroundColorButton_ = nullptr;
    QSpinBox* selectionBackgroundAlphaSpin_ = nullptr;
    QPushButton* lineNumberColorButton_ = nullptr;
    QPushButton* currentLineColorButton_ = nullptr;
    QSpinBox* currentLineAlphaSpin_ = nullptr;
    QSpinBox* cursorWidthSpin_ = nullptr;
    QSpinBox* lineSpacingSpin_ = nullptr;
    QSlider* backgroundAlphaSlider_ = nullptr;
    QSpinBox* backgroundAlphaSpin_ = nullptr;
    QSlider* textAlphaSlider_ = nullptr;
    QSpinBox* textAlphaSpin_ = nullptr;
    QKeySequenceEdit* bossKeyEdit_ = nullptr;
    QKeySequenceEdit* focusShortcutEdit_ = nullptr;
    QCheckBox* restoreTabsOnStartupCheck_ = nullptr;
    bool updating_ = false;
    QVector<AppearancePreset> presets_;
    QVector<ShortcutBinding> shortcutBindings_;
    QComboBox* presetCombo_ = nullptr;
    QKeySequenceEdit* presetShortcutEdit_ = nullptr;
    QPushButton* savePresetButton_ = nullptr;
    QPushButton* updatePresetButton_ = nullptr;
    QPushButton* renamePresetButton_ = nullptr;
    QPushButton* deletePresetButton_ = nullptr;
    QPushButton* importAppearanceButton_ = nullptr;
    QPushButton* exportAppearanceButton_ = nullptr;
    QTableWidget* shortcutTable_ = nullptr;
};

} // namespace vinson
