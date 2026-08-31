#include "ui/MenuAppearance.h"

#include <QMenu>

namespace vinson {

void applyNativeMenuShadow(QMenu* menu)
{
    if (menu == nullptr) {
        return;
    }

    // A graphics effect attached to a top-level popup is clipped to the
    // popup's own window rectangle, so the blurred falloff outside that
    // rectangle disappears. Let the window manager composite the shadow in
    // the space surrounding the popup instead.
    menu->setWindowFlag(Qt::NoDropShadowWindowHint, false);
    menu->setAttribute(Qt::WA_TranslucentBackground, false);
    menu->setGraphicsEffect(nullptr);
}

} // namespace vinson
