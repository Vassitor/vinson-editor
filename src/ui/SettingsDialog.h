#pragma once

#include "settings/Appearance.h"

#include <QDialog>
#include <QKeySequence>

class QDoubleSpinBox;
class QCheckBox;
class QFontComboBox;
class QKeySequenceEdit;
class QPushButton;
class QSlider;
class QSpinBox;

namespace vinson {

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

signals:
    void previewChanged(const vinson::Appearance& appearance);

private:
    void chooseColor(QColor& color, QPushButton* button, const QString& title);
    void emitPreview();
    void refreshColorButtons();
    static void styleColorButton(QPushButton* button, const QColor& color);

    Appearance appearance_;
    QKeySequence bossKey_;
    QKeySequence focusShortcut_;
    QFontComboBox* fontCombo_ = nullptr;
    QDoubleSpinBox* fontSizeSpin_ = nullptr;
    QPushButton* textColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QPushButton* cursorColorButton_ = nullptr;
    QPushButton* selectionTextColorButton_ = nullptr;
    QSlider* backgroundAlphaSlider_ = nullptr;
    QSpinBox* backgroundAlphaSpin_ = nullptr;
    QSlider* textAlphaSlider_ = nullptr;
    QSpinBox* textAlphaSpin_ = nullptr;
    QKeySequenceEdit* bossKeyEdit_ = nullptr;
    QKeySequenceEdit* focusShortcutEdit_ = nullptr;
    QCheckBox* restoreTabsOnStartupCheck_ = nullptr;
    bool updating_ = false;
};

} // namespace vinson
