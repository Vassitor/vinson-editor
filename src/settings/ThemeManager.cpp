#include "settings/ThemeManager.h"

#include "editor/EditorWidget.h"
#include "window/NativeWindowAppearance.h"

#include <QEvent>
#include <QFontDatabase>
#include <QMainWindow>
#include <QMenuBar>
#include <QPalette>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

#include <algorithm>

namespace vinson {
namespace {

QColor blendedColor(const QColor& background, const QColor& foreground,
                    int foregroundPercent)
{
    const int backgroundPercent = 100 - foregroundPercent;
    return QColor(
        (background.red() * backgroundPercent
         + foreground.red() * foregroundPercent) / 100,
        (background.green() * backgroundPercent
         + foreground.green() * foregroundPercent) / 100,
        (background.blue() * backgroundPercent
         + foreground.blue() * foregroundPercent) / 100);
}

QColor opaqueColor(QColor color)
{
    color.setAlpha(255);
    return color;
}

} // namespace

ThemeManager::ThemeManager(EditorWidget* editor, QWidget* window, QObject* parent)
    : QObject(parent)
    , editor_(editor)
    , window_(window)
{
    Q_ASSERT(editor_ != nullptr);
    Q_ASSERT(window_ != nullptr);
    qRegisterMetaType<Appearance>();
    framelessMode_ = window_->windowFlags().testFlag(Qt::FramelessWindowHint);
    window_->installEventFilter(this);
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
    editor_->setBackgroundColor(paintedBackgroundColor());
    editor_->setCursorColor(appearance_.cursorColor);
    editor_->setSelectionTextColor(appearance_.selectionTextColor);

    applyScrollBarAppearance();
    applyWindowAppearance();
    applyNativeWindowAppearance();
    emit appearanceChanged(appearance_);
}

void ThemeManager::setFramelessMode(bool frameless)
{
    if (framelessMode_ == frameless) {
        return;
    }
    framelessMode_ = frameless;

    // Both native-frame and custom-frame windows paint background alpha per
    // pixel. DWM redirection-bitmap alpha keeps foreground glyphs opaque in a
    // framed window instead of applying opacity to the complete window.
    editor_->setBackgroundColor(paintedBackgroundColor());
    applyScrollBarAppearance();
    applyWindowAppearance();
    applyNativeWindowAppearance();
}

bool ThemeManager::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == window_ && event->type() == QEvent::Show) {
        // DWM attribute changes inside QWidget's synchronous Show delivery can
        // re-enter Qt's native-window transition. Apply them on the next event
        // loop turn, after the HWND is fully registered.
        QTimer::singleShot(0, this,
                           &ThemeManager::applyNativeWindowAppearance);
    }
    return QObject::eventFilter(watched, event);
}

void ThemeManager::applyScrollBarAppearance()
{
    const QColor trackColor = paintedBackgroundColor();
    const QColor handleColor = blendedColor(
        trackColor, appearance_.textColor, 24);
    const QColor hoverColor = blendedColor(
        trackColor, appearance_.textColor, 38);
    const QColor pressedColor = blendedColor(
        trackColor, appearance_.textColor, 52);

    editor_->setStyleSheet(QStringLiteral(R"(
QScrollBar:vertical {
    background: %1;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %2;
    min-height: 28px;
    border: 2px solid %1;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover { background: %3; }
QScrollBar::handle:vertical:pressed { background: %4; }
QScrollBar:horizontal {
    background: %1;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: %2;
    min-width: 28px;
    border: 2px solid %1;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover { background: %3; }
QScrollBar::handle:horizontal:pressed { background: %4; }
QScrollBar::add-line,
QScrollBar::sub-line {
    width: 0;
    height: 0;
    border: none;
    background: none;
}
QScrollBar::add-page,
QScrollBar::sub-page { background: transparent; }
)")
        .arg(trackColor.name(trackColor.alpha() == 255
                                 ? QColor::HexRgb : QColor::HexArgb),
             handleColor.name(QColor::HexRgb),
             hoverColor.name(QColor::HexRgb),
             pressedColor.name(QColor::HexRgb)));
}

void ThemeManager::applyWindowAppearance()
{
    const QColor background = paintedBackgroundColor();
    const QColor chromeText = opaqueColor(appearance_.textColor);
    QPalette palette = window_->palette();
    for (const QPalette::ColorGroup group : {
             QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        palette.setColor(group, QPalette::Window,
                         background);
        palette.setColor(group, QPalette::Base,
                         background);
        palette.setColor(group, QPalette::Button,
                         background);
        palette.setColor(group, QPalette::WindowText,
                         chromeText);
        palette.setColor(group, QPalette::Text,
                         chromeText);
        palette.setColor(group, QPalette::ButtonText,
                         chromeText);
    }
    window_->setPalette(palette);
    window_->setAutoFillBackground(!framelessMode_);
    // Whole-window opacity also fades text, line numbers, menus, and the
    // caret. Transparency is represented only by painted background pixels.
    window_->setWindowOpacity(1.0);

    if (auto* mainWindow = qobject_cast<QMainWindow*>(window_)) {
        mainWindow->menuBar()->setPalette(palette);
        mainWindow->menuBar()->setAutoFillBackground(true);
        mainWindow->statusBar()->setPalette(palette);
        mainWindow->statusBar()->setAutoFillBackground(true);
    }
    window_->update();
}

QColor ThemeManager::paintedBackgroundColor() const
{
    return appearance_.backgroundColor;
}

void ThemeManager::applyNativeWindowAppearance()
{
    if (!window_->isVisible()) {
        return;
    }
    (void)setNativeBackgroundAlphaEnabled(window_, true);
    applyNativeTitleBarColors(window_, appearance_.backgroundColor,
                              opaqueColor(appearance_.textColor));
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
    appearance.cursorColor.setAlpha(255);
    appearance.selectionTextColor.setAlpha(255);
    return appearance;
}

} // namespace vinson
