#pragma once

class QColor;
class QWidget;

namespace vinson {

QColor titleBarBackground(const QColor& editorBackground);
void applyNativeTitleBarColors(QWidget* window, const QColor& background,
                               const QColor& text);
[[nodiscard]] bool setNativeBackgroundAlphaEnabled(QWidget* window,
                                                    bool enabled);
void setNativeTaskbarVisible(QWidget* window, bool visible);

} // namespace vinson
