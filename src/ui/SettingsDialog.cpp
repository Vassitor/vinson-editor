#include "ui/SettingsDialog.h"

#include "settings/ThemeManager.h"

#include <QApplication>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vinson {

SettingsDialog::SettingsDialog(const Appearance& appearance, QWidget* parent)
    : QDialog(parent)
    , fontCombo_(new QFontComboBox(this))
    , fontSizeSpin_(new QDoubleSpinBox(this))
    , textColorButton_(new QPushButton(this))
    , backgroundColorButton_(new QPushButton(this))
    , cursorColorButton_(new QPushButton(this))
    , selectionTextColorButton_(new QPushButton(this))
    , backgroundAlphaSlider_(new QSlider(Qt::Horizontal, this))
    , backgroundAlphaSpin_(new QSpinBox(this))
{
    qRegisterMetaType<Appearance>();
    setPalette(QApplication::palette());
    setAutoFillBackground(true);
    setWindowTitle(tr("Appearance"));
    setModal(true);
    setMinimumWidth(440);

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

    auto* alphaRow = new QWidget(this);
    auto* alphaLayout = new QHBoxLayout(alphaRow);
    alphaLayout->setContentsMargins(0, 0, 0, 0);
    alphaLayout->addWidget(backgroundAlphaSlider_, 1);
    alphaLayout->addWidget(backgroundAlphaSpin_);

    auto* form = new QFormLayout;
    form->addRow(tr("Font:"), fontCombo_);
    form->addRow(tr("Font size:"), fontSizeSpin_);
    form->addRow(tr("Text color:"), textColorButton_);
    form->addRow(tr("Background color:"), backgroundColorButton_);
    form->addRow(tr("Background alpha:"), alphaRow);
    form->addRow(tr("Cursor color:"), cursorColorButton_);
    form->addRow(tr("Selected text color:"), selectionTextColorButton_);

    auto* explanation = new QLabel(
        tr("Alpha 0 makes only the background transparent; text and the cursor remain opaque."),
        this);
    explanation->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
            | QDialogButtonBox::RestoreDefaults,
        this);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(explanation);
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
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),
            &QPushButton::clicked, this, [this] {
                setAppearance(ThemeManager::defaultAppearance());
                emitPreview();
            });

    setAppearance(appearance);
}

const Appearance& SettingsDialog::appearance() const noexcept
{
    return appearance_;
}

void SettingsDialog::setAppearance(const Appearance& appearance)
{
    updating_ = true;
    appearance_ = appearance;
    fontCombo_->setCurrentFont(appearance_.font);
    fontSizeSpin_->setValue(appearance_.font.pointSizeF());
    backgroundAlphaSlider_->setValue(appearance_.backgroundColor.alpha());
    backgroundAlphaSpin_->setValue(appearance_.backgroundColor.alpha());
    refreshColorButtons();
    updating_ = false;
}

void SettingsDialog::chooseColor(QColor& color, QPushButton* button,
                                 const QString& title)
{
    QColor initial = color;
    initial.setAlpha(255);
    const QColor chosen = QColorDialog::getColor(initial, this, title);
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
    button->setText(color.name(QColor::HexRgb).toUpper());
    button->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; color: %2; }")
            .arg(color.name(QColor::HexRgb), text.name(QColor::HexRgb)));
}

} // namespace vinson
