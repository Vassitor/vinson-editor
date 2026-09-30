#pragma once

class QMenu;

namespace vinson {

// Share popup styling and let the window system own the shadow and corners.
void applyMenuAppearance(QMenu* menu);

} // namespace vinson
