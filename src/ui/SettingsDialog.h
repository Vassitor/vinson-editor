#pragma once

#include "settings/Appearance.h"

#include <QDialog>

class QDoubleSpinBox;
class QFontComboBox;
class QPushButton;
class QSlider;
class QSpinBox;

namespace vinson {

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const Appearance& appearance,
                            QWidget* parent = nullptr);

    [[nodiscard]] const Appearance& appearance() const noexcept;
    void setAppearance(const Appearance& appearance);

signals:
    void previewChanged(const vinson::Appearance& appearance);

private:
    void chooseColor(QColor& color, QPushButton* button, const QString& title);
    void emitPreview();
    void refreshColorButtons();
    static void styleColorButton(QPushButton* button, const QColor& color);

    Appearance appearance_;
    QFontComboBox* fontCombo_ = nullptr;
    QDoubleSpinBox* fontSizeSpin_ = nullptr;
    QPushButton* textColorButton_ = nullptr;
    QPushButton* backgroundColorButton_ = nullptr;
    QPushButton* cursorColorButton_ = nullptr;
    QPushButton* selectionTextColorButton_ = nullptr;
    QSlider* backgroundAlphaSlider_ = nullptr;
    QSpinBox* backgroundAlphaSpin_ = nullptr;
    bool updating_ = false;
};

} // namespace vinson
