#include "settings/ThemeManager.h"

#include "editor/EditorWidget.h"

#include <QFontDatabase>
#include <QPalette>
#include <QWidget>

#include <algorithm>

namespace vinson {

ThemeManager::ThemeManager(EditorWidget* editor, QWidget* window, QObject* parent)
    : QObject(parent)
    , editor_(editor)
    , window_(window)
{
    Q_ASSERT(editor_ != nullptr);
    Q_ASSERT(window_ != nullptr);
    qRegisterMetaType<Appearance>();
    applyAppearance(defaultAppearance());
}

Appearance ThemeManager::defaultAppearance()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSizeF(12.0);
    return {
        font,
        QColor(32, 33, 36),
        QColor(250, 250, 250, 255),
        QColor(32, 33, 36),
        QColor(255, 255, 255),
    };
}

const Appearance& ThemeManager::appearance() const noexcept
{
    return appearance_;
}

void ThemeManager::applyAppearance(const Appearance& appearance)
{
    appearance_ = normalized(appearance);
    editor_->setEditorFont(appearance_.font);
    editor_->setTextColor(appearance_.textColor);
    editor_->setBackgroundColor(appearance_.backgroundColor);
    editor_->setCursorColor(appearance_.cursorColor);
    editor_->setSelectionTextColor(appearance_.selectionTextColor);

    QPalette palette = window_->palette();
    palette.setColor(QPalette::Window, appearance_.backgroundColor);
    window_->setPalette(palette);
    window_->setAutoFillBackground(true);
    window_->update();
    emit appearanceChanged(appearance_);
}

Appearance ThemeManager::normalized(Appearance appearance)
{
    if (appearance.font.family().isEmpty()) {
        appearance.font = defaultAppearance().font;
    }
    const qreal pointSize = appearance.font.pointSizeF();
    appearance.font.setPointSizeF(std::clamp(pointSize, 6.0, 72.0));

    const Appearance defaults = defaultAppearance();
    if (!appearance.textColor.isValid()) {
        appearance.textColor = defaults.textColor;
    }
    if (!appearance.backgroundColor.isValid()) {
        appearance.backgroundColor = defaults.backgroundColor;
    }
    if (!appearance.cursorColor.isValid()) {
        appearance.cursorColor = defaults.cursorColor;
    }
    if (!appearance.selectionTextColor.isValid()) {
        appearance.selectionTextColor = defaults.selectionTextColor;
    }
    appearance.textColor.setAlpha(255);
    appearance.cursorColor.setAlpha(255);
    appearance.selectionTextColor.setAlpha(255);
    return appearance;
}

} // namespace vinson
