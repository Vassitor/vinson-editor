#include "ui/MenuAppearance.h"

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QMenu>

namespace vinson {

void applySoftMenuShadow(QMenu* menu)
{
    if (menu == nullptr) {
        return;
    }

    // Replace the platform's hard-edged popup shadow with a continuous blur
    // whose alpha gradually falls off around the menu.
    menu->setWindowFlag(Qt::NoDropShadowWindowHint, true);
    menu->setAttribute(Qt::WA_TranslucentBackground);

    auto* shadow = new QGraphicsDropShadowEffect(menu);
    shadow->setObjectName(QStringLiteral("softMenuShadow"));
    shadow->setBlurRadius(24.0);
    shadow->setOffset(0.0, 5.0);
    shadow->setColor(QColor(0, 0, 0, 115));
    menu->setGraphicsEffect(shadow);
}

} // namespace vinson
