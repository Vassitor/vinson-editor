#include "ui/MenuAppearance.h"

#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>

#include <algorithm>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <dwmapi.h>
#endif

namespace vinson {
namespace {

bool isDark(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

// An opaque, single-window popup lets the OS composite its border and shadow.
// Fusion supplies menu layout/checks/icons without the Windows 11 Qt style's
// layered-window graphics effect. QMenu still owns all action/input behavior.
class PopupMenuStyle final : public QProxyStyle
{
public:
    PopupMenuStyle() : QProxyStyle(QStyleFactory::create("Fusion"))
    {
        setObjectName(QStringLiteral("vinsonPopupMenuStyle"));
    }

    int pixelMetric(PixelMetric metric, const QStyleOption* option,
                    const QWidget* widget) const override
    {
        switch (metric) {
        case PM_MenuPanelWidth: return 1;
        case PM_MenuHMargin:
        case PM_MenuVMargin: return 6;
        case PM_SubMenuOverlap: return 0;
        default: return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                           const QSize& size, const QWidget* widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_MenuItem) {
            const auto* item = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
            if (item && item->menuItemType == QStyleOptionMenuItem::Separator)
                return QSize(result.width(), 9);
            result.setWidth(std::max(208, result.width() + 12));
            result.setHeight(std::max(32, result.height() + 8));
        }
        return result;
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget) const override
    {
        const bool dark = isDark(option->palette);
        if (element == PE_PanelMenu) {
            // Fill the entire HWND: DWM clips its corners, so there are no
            // transparent margins to intercept clicks or misalign submenus.
            painter->fillRect(option->rect, dark ? QColor(40, 40, 40) : Qt::white);
        } else if (element == PE_FrameMenu) {
            painter->save();
            painter->setPen(dark ? QColor(70, 70, 70) : QColor(225, 225, 225));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
            painter->restore();
        } else {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
        }
    }

    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget) const override
    {
        const auto* original = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
        if (element != CE_MenuItem || !original) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }
        QStyleOptionMenuItem item(*original);
        const bool dark = isDark(item.palette);
        const QColor background = dark ? QColor(40, 40, 40) : QColor(Qt::white);
        const QColor foreground = dark ? QColor(240, 240, 240) : QColor(38, 38, 38);
        const QColor muted = dark ? QColor(130, 130, 130) : QColor(150, 150, 150);
        if (item.menuItemType == QStyleOptionMenuItem::Separator) {
            painter->save();
            painter->setPen(dark ? QColor(65, 65, 65) : QColor(232, 232, 232));
            painter->drawLine(item.rect.left() + 8, item.rect.center().y(),
                              item.rect.right() - 8, item.rect.center().y());
            painter->restore();
            return;
        }
        if (item.state.testFlag(State_Selected) && item.state.testFlag(State_Enabled)) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(dark ? QColor(62, 62, 62) : QColor(241, 241, 241));
            painter->drawRoundedRect(item.rect, 5, 5);
            painter->restore();
        }
        item.state &= ~State_Selected;
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            item.palette.setColor(group, QPalette::Window, background);
            item.palette.setColor(group, QPalette::Button, background);
            for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
                item.palette.setColor(group, role, group == QPalette::Disabled ? muted : foreground);
        }
        QProxyStyle::drawControl(element, &item, painter, widget);
    }
};

class NativeMenuAppearance final : public QObject
{
public:
    explicit NativeMenuAppearance(QMenu* menu) : QObject(menu), menu_(menu)
    {
        setObjectName(QStringLiteral("nativeMenuAppearance"));
        menu->installEventFilter(this);
        if (menu->parentWidget())
            menu->parentWidget()->installEventFilter(this);
        connect(menu, &QMenu::aboutToShow, this, [this] { syncPalette(); });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == menu_->parentWidget() && event->type() == QEvent::PaletteChange)
            syncPalette();
        if (watched == menu_) {
            switch (event->type()) {
            case QEvent::WinIdChange:
            case QEvent::Show:
            case QEvent::PaletteChange:
                applyNativeFrame();
                break;
            default:
                break;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void syncPalette()
    {
        // QMenu may retain an explicit palette from the previous platform style.
        // Follow the owning menu/bar/window, including custom editor themes.
        if (auto* parent = menu_->parentWidget())
            menu_->setPalette(parent->palette());
    }

    void applyNativeFrame()
    {
#if defined(Q_OS_WIN)
        // Do not create/reparent a native window from within Qt's show/polish.
        const WId id = menu_->internalWinId();
        if (!id)
            return;
        const HWND handle = reinterpret_cast<HWND>(id);
        constexpr DWORD cornerAttribute = 33;
        constexpr DWORD borderAttribute = 34;
        const DWORD corner = 2; // DWMWCP_ROUND; ignored on pre-Windows 11.
        const bool dark = isDark(menu_->palette());
        const COLORREF border = dark ? RGB(70, 70, 70) : RGB(225, 225, 225);
        DwmSetWindowAttribute(handle, cornerAttribute, &corner, sizeof(corner));
        DwmSetWindowAttribute(handle, borderAttribute, &border, sizeof(border));
        const DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(handle, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        const MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(handle, &margins);
#endif
    }

    QMenu* menu_;
};

} // namespace

void applyMenuAppearance(QMenu* menu)
{
    if (!menu || menu->findChild<QObject*>(QStringLiteral("nativeMenuAppearance"),
                                          Qt::FindDirectChildrenOnly))
        return;
    // Install the guard first: style/flag changes can re-enter menu-bar layout.
    new NativeMenuAppearance(menu);
    menu->setAttribute(Qt::WA_WindowPropagation);
    auto* style = new PopupMenuStyle;
    style->setParent(menu);
    menu->setStyle(style);
    menu->ensurePolished();
    if (menu->parentWidget())
        menu->setPalette(menu->parentWidget()->palette());
    menu->setGraphicsEffect(nullptr);
    menu->setAttribute(Qt::WA_TranslucentBackground, false);
    menu->setAttribute(Qt::WA_NoSystemBackground, false);
    menu->setWindowFlags((menu->windowFlags() | Qt::FramelessWindowHint)
                         & ~Qt::NoDropShadowWindowHint);
}

} // namespace vinson
