#pragma once

#include "settings/Appearance.h"

#include <QObject>

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

signals:
    void appearanceChanged(const vinson::Appearance& appearance);

private:
    [[nodiscard]] static Appearance normalized(Appearance appearance);

    EditorWidget* editor_ = nullptr;
    QWidget* window_ = nullptr;
    Appearance appearance_;
};

} // namespace vinson
