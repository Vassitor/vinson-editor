#pragma once

#include "settings/Appearance.h"

#include <QDialog>
#include <QKeySequence>

class QDoubleSpinBox;
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
                            QWidget* parent = nullptr);

    [[nodiscard]] const Appearance& appearance() const noexcept;
    [[nodiscard]] const QKeySequence& bossKey() const noexcept;
    void setAppearance(const Appearance& appearance);
    void setBossKey(const QKeySequence& bossKey);

signals:
    void previewChanged(const vinson::Appearance& appearance);

private:
    void chooseColor(QColor& color, QPushButton* button, const QString& title);
    void emitPreview();
    void refreshColorButtons();
    static void styleColorButton(QPushButton* button, const QColor& color);

    Appearance appearance_;
    QKeySequence bossKey_;
    QFontComboBox* fontCombo_ = nullptr;
    QDoubleSpinBox* fontSizeSpin_ = nullptr;
    QPushButton* textColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QPushButton* cursorColorButton_ = nullptr;
    QPushButton* selectionTextColorButton_ = nullptr;
    QSlider* backgroundAlphaSlider_ = nullptr;
    QSpinBox* backgroundAlphaSpin_ = nullptr;
    QKeySequenceEdit* bossKeyEdit_ = nullptr;
    bool updating_ = false;
};

} // namespace vinson
