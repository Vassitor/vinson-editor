#pragma once

#include "settings/Appearance.h"

#include <QObject>

class QEvent;
class QWidget;

namespace vinson {

class EditorWidget;

class ThemeManager final : public QObject
{
    Q_OBJECT

public:
    ThemeManager(EditorWidget* editor, QWidget* window, QObject* parent = nullptr);

    [[nodiscard]] static Appearance defaultAppearance();
    [[nodiscard]] const Appearance& appearance() const noexcept;

public slots:
    void applyAppearance(const Appearance& appearance);
    void setFramelessMode(bool frameless);

signals:
    void appearanceChanged(const vinson::Appearance& appearance);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] static Appearance normalized(Appearance appearance);
    [[nodiscard]] QColor paintedBackgroundColor() const;
    void applyScrollBarAppearance();
    void applyWindowAppearance();
    void applyNativeWindowAppearance();

    EditorWidget* editor_ = nullptr;
    QWidget* window_ = nullptr;
    Appearance appearance_;
    bool framelessMode_ = false;
};

} // namespace vinson
