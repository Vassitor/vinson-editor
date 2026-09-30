#include "window/MainWindow.h"

#include "editor/EditorWidget.h"
#include "file/FileManager.h"
#include "file/FileTypes.h"
#include "search/SearchController.h"
#include "settings/SettingsManager.h"
#include "settings/ThemeManager.h"
#include "ui/FindReplaceWidget.h"
#include "ui/EditHistoryWidget.h"
#include "ui/MenuAppearance.h"
#include "ui/SettingsDialog.h"
#include "window/WindowController.h"
#include "window/NativeTitleBar.h"
#include "window/NativeWindowAppearance.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QApplication>
#include <QCursor>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDir>
#include <QDebug>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOption>
#include <QStyleFactory>
#include <QToolButton>
#include <QUuid>
#include <QWheelEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QScreen>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QSizePolicy>

#include <algorithm>
#include <limits>
#include <utility>

namespace vinson {
namespace {

class DocumentTabStyle final : public QProxyStyle
{
public:
    DocumentTabStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}

    int pixelMetric(PixelMetric metric, const QStyleOption* option,
                    const QWidget* widget) const override
    {
        if (metric == PM_TabCloseIndicatorWidth || metric == PM_TabCloseIndicatorHeight)
            return 20;
        if (metric == PM_TabBarTabOverlap)
            return 0;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
                           const QSize& contents, const QWidget* widget) const override
    {
        if (type == CT_TabBarTab)
            return QSize(std::clamp(contents.width() + 32, 154, 234),
                         std::max(36, contents.height() + 8));
        return QProxyStyle::sizeFromContents(type, option, contents, widget);
    }

    QRect subElementRect(SubElement element, const QStyleOption* option,
                         const QWidget* widget) const override
    {
        const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (tab && (element == SE_TabBarTabText || element == SE_TabBarTabRightButton
                    || element == SE_TabBarTabLeftButton)) {
            const QRect body = tab->rect.adjusted(12, 4, -14, 0);
            QRect result = body;
            if (element == SE_TabBarTabText) {
                result.adjust(tab->leftButtonSize.isEmpty() ? 0 : tab->leftButtonSize.width() + 8,
                              0, tab->rightButtonSize.isEmpty() ? 0 : -tab->rightButtonSize.width() - 8, 0);
            } else {
                const QSize size = element == SE_TabBarTabLeftButton
                    ? tab->leftButtonSize : tab->rightButtonSize;
                result = QRect(QPoint(element == SE_TabBarTabLeftButton ? body.left()
                                        : body.right() - size.width() + 1,
                                      body.center().y() - size.height() / 2), size);
            }
            return visualRect(tab->direction, tab->rect, result);
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }

    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget) const override
    {
        const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (element != CE_TabBarTab || !tab || !widget) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }
        // Use the supplied rect for both shape and label, including Qt's drag
        // offsets and the moving-tab pixmap. Do not re-query tabRect(tabIndex).
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF body = QRectF(tab->rect).adjusted(0, 4, -2, 0);
        QPainterPath shape;
        shape.moveTo(body.bottomLeft());
        shape.lineTo(body.left(), body.top() + 10);
        shape.quadTo(body.topLeft(), QPointF(body.left() + 10, body.top()));
        shape.lineTo(body.right() - 10, body.top());
        shape.quadTo(body.topRight(), QPointF(body.right(), body.top() + 10));
        shape.lineTo(body.bottomRight());
        shape.closeSubpath();
        const bool selected = tab->state & State_Selected;
        const bool hovered = tab->state & State_MouseOver;
        const QColor background = selected ? tab->palette.color(QPalette::Window)
            : widget->property(hovered ? "tabHover" : "tabInactive").value<QColor>();
        painter->fillPath(shape, background);
        const QColor text = widget->property("tabForeground").value<QColor>();
        const auto* bar = qobject_cast<const QTabBar*>(widget);
        // Paint each shared edge once. Both edges of a highlighted tab disappear.
        if (bar && !selected && !hovered && !widget->property("tabDragging").toBool()
            && tab->tabIndex >= 0 && tab->tabIndex + 1 < bar->count()
            && tab->tabIndex + 1 != bar->currentIndex()
            && tab->tabIndex + 1 != widget->property("hoveredTab").toInt()) {
            QColor separator = text;
            separator.setAlpha(55);
            painter->setPen(QPen(separator, 1));
            const qreal x = tab->direction == Qt::RightToLeft
                ? tab->rect.left() + 0.5 : tab->rect.right() - 0.5;
            painter->drawLine(QPointF(x, body.center().y() - 5),
                              QPointF(x, body.center().y() + 5));
        }
        painter->setPen(text);
        const QRect textRect = subElementRect(SE_TabBarTabText, tab, widget);
        const QFontMetrics metrics(widget->font());
        const int textWidth = metrics.horizontalAdvance(tab->text);
        if (textWidth <= textRect.width()) {
            painter->drawText(textRect,
                              Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                              tab->text);
        } else if (!textRect.isEmpty()) {
            // Keep the file name itself intact and fade its clipped right edge
            // instead of substituting an ellipsis. Drawing the glyphs with an
            // alpha gradient preserves a translucent title-bar background.
            constexpr int preferredFadeWidth = 18;
            const int fadeWidth = std::min(preferredFadeWidth, textRect.width());
            const qreal baseline = textRect.center().y()
                + (metrics.ascent() - metrics.descent()) / 2.0;
            QPainterPath textPath;
            textPath.addText(textRect.left(), baseline, widget->font(), tab->text);
            QColor transparentText = text;
            transparentText.setAlpha(0);
            QLinearGradient fade(textRect.left(), 0, textRect.right() + 1, 0);
            const qreal fadeStart = 1.0
                - static_cast<qreal>(fadeWidth) / textRect.width();
            fade.setColorAt(0, text);
            fade.setColorAt(std::max(0.0, fadeStart), text);
            fade.setColorAt(1, transparentText);
            painter->setClipRect(textRect);
            painter->fillPath(textPath, fade);
        }
        painter->restore();
    }

    int styleHint(StyleHint hint, const QStyleOption* option,
                  const QWidget* widget, QStyleHintReturn* data) const override
    {
        if (hint == SH_TabBar_Alignment) {
            return Qt::AlignLeft;
        }
        return QProxyStyle::styleHint(hint, option, widget, data);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget) const override
    {
        if (element == PE_FrameFocusRect)
            return; // The selected tab already identifies the active document.
        if (element == PE_PanelButtonTool) {
            const QWidget* owner = widget;
            while (owner && !qobject_cast<const QTabBar*>(owner))
                owner = owner->parentWidget();
            painter->fillRect(option->rect, owner ? owner->property("tabInactive").value<QColor>()
                                                : option->palette.color(QPalette::Window));
            if (option->state & (State_MouseOver | State_Sunken)) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                QColor hover = option->palette.color(QPalette::WindowText);
                hover.setAlpha(28);
                painter->setPen(Qt::NoPen);
                painter->setBrush(hover);
                painter->drawRoundedRect(option->rect.adjusted(1, 1, -1, -1), 6, 6);
                painter->restore();
            }
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

class DocumentTabCloseButton final : public QAbstractButton
{
public:
    explicit DocumentTabCloseButton(QWidget* parent) : QAbstractButton(parent)
    {
        setFixedSize(20, 20);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setAccessibleName(QTabBar::tr("Close Tab"));
    }

protected:
    bool hitButton(const QPoint& point) const override
    {
        return property("showTabClose").toBool() && rect().contains(point);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor text = parentWidget()->property("tabForeground").value<QColor>();
        const QPointF center = QRectF(rect()).center();
        if (!property("showTabClose").toBool()) {
            if (property("documentModified").toBool()) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(text);
                painter.drawEllipse(center, 3, 3);
            }
            return;
        }
        if (underMouse() || isDown()) {
            QColor background = text;
            background.setAlpha(isDown() ? 48 : 28);
            painter.setPen(Qt::NoPen);
            painter.setBrush(background);
            painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
        }
        painter.setPen(QPen(text, 1.5));
        painter.drawLine(center + QPointF(-3, -3), center + QPointF(3, 3));
        painter.drawLine(center + QPointF(-3, 3), center + QPointF(3, -3));
    }
};

class DocumentTabBar final : public QTabBar
{
public:
    explicit DocumentTabBar(QWidget* parent) : QTabBar(parent)
    {
        auto* closeStyle = new DocumentTabStyle;
        closeStyle->setParent(this);
        setStyle(closeStyle);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover);
        setProperty("hoveredTab", -1);
        connect(this, &QTabBar::currentChanged, this, [this] { updateIndicators(); });
        connect(this, &QTabBar::tabMoved, this, [this] { updateIndicators(); });
    }

    QSize sizeHint() const override
    {
        QSize size = QTabBar::sizeHint();
        if (minimumWidth() == maximumWidth()) {
            size.setWidth(minimumWidth());
        }
        return size;
    }

    int contentWidth() const { return QTabBar::sizeHint().width(); }

    void setDocumentModified(int index, bool modified)
    {
        if (auto* button = tabButton(index, QTabBar::RightSide))
            button->setProperty("documentModified", modified);
        updateIndicators();
    }

protected:
    QSize minimumTabSizeHint(int index) const override
    {
        QSize size = QTabBar::minimumTabSizeHint(index);
        const QFontMetrics metrics(font());
        const QString text = tabText(index);
        int characterLength = 1;
        if (text.size() > 1 && text.front().isHighSurrogate()
            && text.at(1).isLowSurrogate()) {
            characterLength = 2;
        }
        const int characterWidth = text.isEmpty()
            ? metrics.averageCharWidth()
            : metrics.horizontalAdvance(text.left(characterLength));
        const QWidget* closeButton = tabButton(index, QTabBar::RightSide);
        const int closeWidth = closeButton
            ? std::max(closeButton->minimumWidth(), closeButton->sizeHint().width()) : 0;
        // Match DocumentTabStyle's 12/14 px body insets and the 8 px gap
        // before the close button. This is the final compression limit: one
        // title character remains alongside the full close control.
        size.setWidth(12 + std::max(1, characterWidth)
                      + (closeButton ? 8 + closeWidth : 0) + 14);
        return size;
    }

    void tabInserted(int index) override
    {
        QTabBar::tabInserted(index);
        auto* button = new DocumentTabCloseButton(this);
        setTabButton(index, QTabBar::RightSide, button);
        button->installEventFilter(this);
        connect(button, &QAbstractButton::clicked, this, [this, button] {
            for (int i = 0; i < count(); ++i) {
                if (tabButton(i, QTabBar::RightSide) == button) {
                    emit tabCloseRequested(i);
                    break;
                }
            }
        });
        updateIndicators();
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::HoverMove || event->type() == QEvent::HoverEnter)
            setHoveredTab(tabAt(static_cast<QHoverEvent*>(event)->position().toPoint()));
        else if (event->type() == QEvent::HoverLeave)
            setHoveredTab(-1);
        return QTabBar::event(event);
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            for (int i = 0; i < count(); ++i)
                if (tabButton(i, QTabBar::RightSide) == object)
                    setHoveredTab(i);
        } else if (event->type() == QEvent::Leave) {
            setHoveredTab(tabAt(mapFromGlobal(QCursor::pos())));
        }
        return QTabBar::eventFilter(object, event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
            pressPosition_ = event->position().toPoint();
        QTabBar::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event->buttons().testFlag(Qt::LeftButton)
            && (event->position().toPoint() - pressPosition_).manhattanLength()
                >= QApplication::startDragDistance()) {
            setProperty("tabDragging", true);
            updateIndicators();
            update();
        }
        QTabBar::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        QTabBar::mouseReleaseEvent(event);
        setProperty("tabDragging", false);
        setHoveredTab(tabAt(event->position().toPoint()));
        updateIndicators();
        update();
    }

    void tabLayoutChange() override
    {
        QTabBar::tabLayoutChange();
        const int desiredWidth = contentWidth();
        if (desiredWidth != lastContentWidth_) {
            lastContentWidth_ = desiredWidth;
            if (parentWidget()) {
                QCoreApplication::postEvent(parentWidget(), new QEvent(QEvent::LayoutRequest));
            }
        }
    }

    void wheelEvent(QWheelEvent* event) override
    {
        // Use the native scroll controls so scrolling never selects a document.
        const QPoint delta = event->angleDelta().isNull()
            ? event->pixelDelta() : event->angleDelta();
        const int movement = delta.x() != 0 ? delta.x() : delta.y();
        if (movement != 0) {
            const auto direction = movement < 0 ? Qt::RightArrow : Qt::LeftArrow;
            for (auto* button : findChildren<QToolButton*>()) {
                if (button->arrowType() == direction
                    && button->isVisible() && button->isEnabled()) {
                    button->click();
                    break;
                }
            }
        }
        event->accept();
    }

private:
    void setHoveredTab(int index)
    {
        if (property("hoveredTab").toInt() == index)
            return;
        setProperty("hoveredTab", index);
        updateIndicators();
        update();
    }

    void updateIndicators()
    {
        for (int i = 0; i < count(); ++i) {
            if (auto* button = tabButton(i, QTabBar::RightSide)) {
                const bool modified = button->property("documentModified").toBool();
                button->setProperty("showTabClose", property("hoveredTab").toInt() == i
                    || (i == currentIndex() && (!modified || property("tabDragging").toBool())));
                button->setAccessibleDescription(modified ? MainWindow::tr("Unsaved changes") : QString());
                button->update();
            }
        }
    }

    QPoint pressPosition_;
    int lastContentWidth_ = -1;
};

class DocumentMenuBar final : public QMenuBar
{
public:
    using QMenuBar::QMenuBar;

protected:
    bool event(QEvent* event) override
    {
        const bool result = QMenuBar::event(event);
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::LayoutRequest:
        case QEvent::ActionChanged:
        case QEvent::FontChange:
        case QEvent::StyleChange:
            // Qt can recreate its internal overflow menu after construction.
            for (auto* menu : findChildren<QMenu*>()) {
                applyMenuAppearance(menu);
            }
            if (auto* tabs = static_cast<DocumentTabBar*>(cornerWidget(Qt::TopRightCorner))) {
                int menuEnd = 0;
                for (auto* action : actions()) {
                    if (action->isVisible()) {
                        menuEnd = std::max(menuEnd, actionGeometry(action).right() + 1);
                    }
                }
                const int margin = style()->pixelMetric(QStyle::PM_MenuBarHMargin,
                                                        nullptr, this);
                const int panel = style()->pixelMetric(QStyle::PM_MenuBarPanelWidth,
                                                       nullptr, this);
                // Reserve the native overflow button before giving tabs space
                // from the menu. QMenuBar keeps overflowed menus in its popup.
                const int extension = style()->pixelMetric(QStyle::PM_ToolBarExtensionExtent,
                                                           nullptr, this);
                const int maximum = std::max(0, width() - 2 * (margin + panel)
                                                - extension - 4);
                const int remaining = std::max(0, width() - menuEnd - margin - panel - 4);
                // Tabs scroll within the remaining space instead of forcing
                // the main menus into QMenuBar's overflow popup.
                const int available = std::min(maximum, remaining);
                if (tabs->width() != available) {
                    tabs->setFixedWidth(available);
                    setCornerWidget(tabs, Qt::TopRightCorner);
                }
                tabs->move(width() - margin - panel - available, tabs->y());
            }
            break;
        default:
            break;
        }
        return result;
    }
};

} // namespace

MainWindow::MainWindow(QWidget* parent, bool restorePersistentState)
    : QMainWindow(parent)
    , editor_(new EditorWidget(this))
    , tabBar_(new DocumentTabBar(this))
    , fileManager_(new FileManager(this))
    , fileChangeMonitor_(new FileChangeMonitor(this))
    , searchController_(new SearchController(editor_, this))
    , findReplaceWidget_(new FindReplaceWidget(this))
    , cursorPositionLabel_(new QLabel(QStringLiteral("Ln 1, Col 1"), this))
    , documentInfoLabel_(new QLabel(this))
    , progressBar_(new QProgressBar(this))
    , cancelOperationButton_(new QPushButton(tr("Cancel"), this))
    , persistentStateEnabled_(restorePersistentState)
{
    // Transparency capability has to exist before the native top-level window
    // is created. Later phases can change only the painted background alpha.
    setObjectName(QStringLiteral("vinsonMainWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    setMenuBar(new DocumentMenuBar(this));
    menuBar()->setContextMenuPolicy(Qt::PreventContextMenu);
    themeManager_ = new ThemeManager(editor_, this, this);
    settingsManager_ = new SettingsManager(this);
    recoveryManager_ = new RecoveryManager(
        QDir(QFileInfo(settingsManager_->fileName()).absolutePath())
            .filePath(QStringLiteral("recovery")),
        this);
    recoveryManager_->setObjectName(QStringLiteral("recoveryManager"));
    setAcceptDrops(true);
    auto* centralWidget = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    tabBar_->setObjectName(QStringLiteral("documentTabBar"));
    tabBar_->setDocumentMode(true);
    tabBar_->setDrawBase(false);
    tabBar_->setExpanding(false);
    tabBar_->setTabsClosable(true);
    tabBar_->setMovable(true);
    // DocumentTabStyle clips long names with a right-edge alpha fade.
    tabBar_->setElideMode(Qt::ElideNone);
    tabBar_->setUsesScrollButtons(true);
    tabBar_->setMinimumWidth(0);
    tabBar_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tabBar_->setProperty("browserStyle", true);
    tabBar_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar_, &QWidget::customContextMenuRequested,
            this, &MainWindow::showTabContextMenu);
    centralLayout->addWidget(findReplaceWidget_);
    centralLayout->addWidget(editor_, 1);
    setCentralWidget(centralWidget);

    TabState initialTab;
    initialTab.documentHandle = editor_->retainCurrentDocument();
    initialTab.recoveryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tabs_.push_back(std::move(initialTab));
    currentTabIndex_ = 0;
    const int initialTabIndex = tabBar_->addTab(tr("Untitled"));
    tabBar_->setTabData(initialTabIndex,
                        QVariant::fromValue(tabs_.front().documentHandle));
    tabBar_->setCurrentIndex(0);
    connect(tabBar_, &QTabBar::currentChanged,
            this, [this](int index) { switchToTab(index); });
    connect(tabBar_, &QTabBar::tabCloseRequested,
            this, &MainWindow::requestCloseTab);
    connect(tabBar_, &QTabBar::tabMoved, this,
            [this](int, int) {
                synchronizeTabOrder();
                savePersistentSettings();
            });
    editHistoryWidget_ = new EditHistoryWidget(editor_, this);
    editHistoryDock_ = new QDockWidget(tr("Edit History"), this);
    editHistoryDock_->setObjectName(QStringLiteral("editHistoryDock"));
    editHistoryDock_->setAllowedAreas(Qt::LeftDockWidgetArea
                                      | Qt::RightDockWidgetArea);
    editHistoryDock_->setWidget(editHistoryWidget_);
    editHistoryDock_->setMinimumWidth(240);
    addDockWidget(Qt::RightDockWidgetArea, editHistoryDock_);
    editHistoryDock_->hide();
    findReplaceWidget_->applyAppearance(themeManager_->appearance(), false);
    editHistoryWidget_->applyAppearance(themeManager_->appearance());
    applyEditHistoryDockAppearance(editHistoryDock_,
                                   themeManager_->appearance(), false);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            editHistoryWidget_, &EditHistoryWidget::applyAppearance);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            this, [this](const Appearance& appearance) {
                applyEditHistoryDockAppearance(
                    editHistoryDock_, appearance,
                    editHistoryDock_->isFloating());
            });
    connect(editHistoryDock_, &QDockWidget::topLevelChanged,
            this, [this](bool floating) {
                applyEditHistoryDockAppearance(
                    editHistoryDock_, themeManager_->appearance(), floating);
                editor_->refreshScrollBarLayout();
                QTimer::singleShot(0, editor_,
                                   &EditorWidget::refreshScrollBarLayout);
            });
    connect(editHistoryDock_, &QDockWidget::visibilityChanged,
            this, [this](bool) {
                editor_->refreshScrollBarLayout();
                QTimer::singleShot(0, editor_,
                                   &EditorWidget::refreshScrollBarLayout);
            });
    windowController_ = new WindowController(this, this);
    windowController_->configureMinimalMode(editor_, findReplaceWidget_);
    connect(themeManager_, &ThemeManager::appearanceChanged,
            this, [this](const Appearance& appearance) {
                findReplaceWidget_->applyAppearance(
                    appearance, windowController_->isFrameless());
            });
    connect(themeManager_, &ThemeManager::appearanceChanged,
            windowController_, &WindowController::refreshMinimalMinimumSize);
    connect(windowController_, &WindowController::framelessChanged,
            themeManager_, &ThemeManager::setFramelessMode);
    connect(windowController_, &WindowController::framelessChanged,
            this, [this](bool frameless) {
                findReplaceWidget_->applyAppearance(
                    themeManager_->appearance(), frameless);
                updateTabBarVisibility();
            });
    createMenus();
    menuBar()->setCornerWidget(tabBar_, Qt::TopRightCorner);
    applyTabBarAppearance(themeManager_->appearance());
    connect(themeManager_, &ThemeManager::appearanceChanged,
            this, &MainWindow::applyTabBarAppearance);
    nativeTitleBar_ = new NativeTitleBar(this, tabBar_);
    connect(windowController_, &WindowController::framelessChanged,
            this, [this] { nativeTitleBar_->refresh(); });
    connectFileManager();
    connectFileChangeMonitor();
    connectSearch();
    qApp->installEventFilter(this);
    recoveryTimer_ = new QTimer(this);
    recoveryTimer_->setObjectName(QStringLiteral("recoveryTimer"));
    recoveryTimer_->setSingleShot(true);
    recoveryTimer_->setInterval(2500);
    connect(recoveryTimer_, &QTimer::timeout,
            this, &MainWindow::writeCurrentRecoverySnapshot);

    progressBar_->setTextVisible(false);
    progressBar_->setMaximumWidth(180);
    progressBar_->hide();
    cancelOperationButton_->hide();
    statusBar()->addPermanentWidget(progressBar_);
    statusBar()->addPermanentWidget(cancelOperationButton_);
    statusBar()->addPermanentWidget(documentInfoLabel_);
    statusBar()->addPermanentWidget(cursorPositionLabel_);
    statusBar()->showMessage(tr("Ready"));
    resize(900, 600);
    if (persistentStateEnabled_) {
        restorePersistentSettings();
    } else {
        crashRecoveryChecked_ = true;
    }
    updateWindowTitle();
    updateDocumentStatus();

    connect(editor_, &EditorWidget::cursorPositionChanged, this,
            [this](qsizetype line, qsizetype column) {
                currentLine_ = line;
                cursorPositionLabel_->setText(
                    tr("Ln %1, Col %2").arg(line).arg(column));
            });
    connect(editor_, &EditorWidget::documentModified, this,
            [this](bool modified) {
                if (!fileManager_->isBusy() && !switchingTabs_) {
                    currentTab().document.setModified(modified);
                    updateWindowTitle();
                    if (currentTab().document.isModified()) {
                        scheduleRecoverySnapshot();
                    } else {
                        removeRecoverySnapshot(currentTab());
                    }
                }
            });
    connect(editor_, &EditorWidget::editHistoryChanged, this, [this] {
        if (!switchingTabs_ && currentTab().document.isModified()) {
            scheduleRecoverySnapshot();
        }
    });
    connect(editor_, &EditorWidget::lineEndingChanged, this, [this](LineEnding ending) {
        if (!fileManager_->isBusy() && !switchingTabs_) {
            currentTab().document.setLineEnding(ending);
            updateDocumentStatus();
        }
    });
    connect(cancelOperationButton_, &QPushButton::clicked,
            this, [this] {
                if (searchController_->isSearching()) {
                    searchController_->cancelSearch();
                } else {
                    fileManager_->cancelCurrentOperation();
                }
            });
    connect(editor_, &ScintillaEditBase::uriDropped, this,
            [this](const QString& uri) {
                const QUrl url(uri);
                if (url.isLocalFile()) {
                    requestOpenFile(url.toLocalFile());
                }
            });
    connect(editor_, &EditorWidget::findRequested,
            this, [this] { showFindReplace(false); });
    connect(editor_, &EditorWidget::fontSizeAdjustmentRequested,
            this, &MainWindow::adjustFontSize);

    if (persistentStateEnabled_) {
        QTimer::singleShot(0, this, &MainWindow::initializeCrashRecovery);
    }
}

MainWindow::~MainWindow()
{
    recoveryTimer_->stop();
    for (const TabState& tab : tabs_) {
        removeRecoverySnapshot(tab);
    }
    recoveryManager_->flush();
    for (const TabState& tab : tabs_) {
        editor_->releaseTabDocument(
            static_cast<sptr_t>(tab.documentHandle));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    searchController_->cancelSearch();
    if (closeToTrayEnabled_) {
        savePersistentSettings();
        hide();
        event->ignore();
        return;
    }
    if (closeAfterSave_ && !isDocumentBusy()) {
        savePersistentSettings();
        event->accept();
        return;
    }
    if (isDocumentBusy()) {
        statusBar()->showMessage(tr("Cancel or wait for the current file operation."),
                                 4000);
        event->ignore();
        return;
    }
    event->ignore();
    if (!closeAllTabsInProgress_) {
        beginCloseAllTabs(false);
    }
}

void MainWindow::setCloseToTrayEnabled(bool enabled) noexcept
{
    closeToTrayEnabled_ = enabled;
}

const QKeySequence& MainWindow::bossKey() const noexcept
{
    return bossKey_;
}

const QKeySequence& MainWindow::focusShortcut() const noexcept
{
    return focusShortcut_;
}

void MainWindow::focusEditor()
{
    editor_->QWidget::setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::requestApplicationQuit()
{
    searchController_->cancelSearch();
    if (isDocumentBusy()) {
        show();
        raise();
        activateWindow();
        statusBar()->showMessage(
            tr("Cancel or wait for the current file operation."), 4000);
        return;
    }

    beginCloseAllTabs(true);
}

void MainWindow::handleBossKeyRegistrationFailure(
    const QKeySequence& activeShortcut, const QString& message)
{
    bossKey_ = activeShortcut;
    savePersistentSettings();
    statusBar()->showMessage(message, 6000);
}

void MainWindow::handleFocusShortcutRegistrationFailure(
    const QKeySequence& activeShortcut, const QString& message)
{
    focusShortcut_ = activeShortcut;
    savePersistentSettings();
    statusBar()->showMessage(message, 6000);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!isDocumentBusy() && event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        if (urls.size() == 1 && urls.first().isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.size() != 1 || !urls.first().isLocalFile()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    requestOpenFile(urls.first().toLocalFile());
}

void MainWindow::createMenus()
{
    int shortcutOrder = 0;
    auto makeShortcutCustomizable = [this, &shortcutOrder](
                                        QAction* action, const char* id) {
        action->setProperty("shortcutId", QString::fromLatin1(id));
        action->setProperty("shortcutOrder", shortcutOrder++);
        action->setProperty("defaultShortcut", action->shortcut());
        action->setShortcutContext(Qt::WindowShortcut);
        addAction(action);
    };
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    newAction_ = fileMenu->addAction(tr("&New"));
    newAction_->setObjectName(QStringLiteral("newDocumentAction"));
    newAction_->setShortcut(QKeySequence::New);
    makeShortcutCustomizable(newAction_, "new");
    connect(newAction_, &QAction::triggered, this, &MainWindow::newDocument);

    openAction_ = fileMenu->addAction(tr("&Open…"));
    openAction_->setShortcut(QKeySequence::Open);
    makeShortcutCustomizable(openAction_, "open");
    connect(openAction_, &QAction::triggered, this, &MainWindow::chooseAndOpenFile);

    recentFilesMenu_ = fileMenu->addMenu(tr("Open &Recent"));
    recentFilesMenu_->setObjectName(QStringLiteral("recentFilesMenu"));
    rebuildRecentFilesMenu();

    reopenClosedTabAction_ = fileMenu->addAction(tr("Reopen Closed Tab"));
    reopenClosedTabAction_->setObjectName(
        QStringLiteral("reopenClosedTabAction"));
    makeShortcutCustomizable(reopenClosedTabAction_, "reopenClosedTab");
    reopenClosedTabAction_->setEnabled(false);
    connect(reopenClosedTabAction_, &QAction::triggered,
            this, &MainWindow::reopenClosedTab);

    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setShortcut(QKeySequence::Save);
    makeShortcutCustomizable(saveAction_, "save");
    connect(saveAction_, &QAction::triggered, this, [this] { saveDocument(); });

    saveAsAction_ = fileMenu->addAction(tr("Save &As…"));
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    makeShortcutCustomizable(saveAsAction_, "saveAs");
    connect(saveAsAction_, &QAction::triggered,
            this, [this] { saveDocumentAs(); });

    reloadAction_ = fileMenu->addAction(tr("&Reload"));
    reloadAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    makeShortcutCustomizable(reloadAction_, "reload");
    connect(reloadAction_, &QAction::triggered, this, &MainWindow::reloadDocument);
    auto* encodingMenu = fileMenu->addMenu(tr("Save &Encoding"));
    auto* encodingGroup = new QActionGroup(this);
    for (TextEncoding encoding : {TextEncoding::Utf8, TextEncoding::Utf8Bom,
                                 TextEncoding::Utf16Le, TextEncoding::Utf16Be}) {
        auto* action = encodingMenu->addAction(encodingName(encoding));
        action->setObjectName(QStringLiteral("saveEncoding%1").arg(static_cast<int>(encoding)));
        action->setCheckable(true);
        action->setData(static_cast<int>(encoding));
        encodingGroup->addAction(action);
        encodingActions_.append(action);
        connect(action, &QAction::triggered, this, [this, encoding] {
            if (isDocumentBusy()) return;
            currentTab().document.setEncoding(encoding);
            updateWindowTitle();
            updateDocumentStatus();
            if (currentTab().document.isModified()) scheduleRecoverySnapshot();
            else removeRecoverySnapshot(currentTab());
        });
    }
    auto* lineEndingMenu = fileMenu->addMenu(tr("Convert &Line Endings"));
    for (LineEnding ending : {LineEnding::Lf, LineEnding::CrLf, LineEnding::Cr}) {
        auto* action = lineEndingMenu->addAction(lineEndingName(ending));
        action->setObjectName(QStringLiteral("convertLineEnding%1").arg(static_cast<int>(ending)));
        lineEndingActions_.append(action);
        connect(action, &QAction::triggered, this, [this, ending] {
            if (isDocumentBusy()) return;
            currentTab().insertionLineEnding = ending;
            editor_->convertLineEndings(ending);
            currentTab().document.setLineEnding(editor_->detectedLineEnding());
            updateWindowTitle();
            updateDocumentStatus();
        });
    }

    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction(tr("E&xit"));
    quitAction->setShortcut(QKeySequence::Quit);
    makeShortcutCustomizable(quitAction, "quit");
    connect(quitAction, &QAction::triggered,
            this, &MainWindow::requestApplicationQuit);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto addEditorAction = [this, editMenu, &makeShortcutCustomizable](
                               const QString& text,
                               const QKeySequence& shortcut,
                               const char* id, auto slot) {
        auto* action = editMenu->addAction(text);
        action->setShortcut(shortcut);
        makeShortcutCustomizable(action, id);
        connect(action, &QAction::triggered, editor_, slot);
    };
    addEditorAction(tr("&Undo"), QKeySequence::Undo, "undo", &ScintillaEdit::undo);
    addEditorAction(tr("&Redo"), QKeySequence::Redo, "redo", &ScintillaEdit::redo);
    editMenu->addSeparator();
    addEditorAction(tr("Cu&t"), QKeySequence::Cut, "cut", &ScintillaEdit::cut);
    addEditorAction(tr("&Copy"), QKeySequence::Copy, "copy", &ScintillaEdit::copy);
    addEditorAction(tr("&Paste"), QKeySequence::Paste, "paste", &ScintillaEdit::paste);
    editMenu->addSeparator();
    addEditorAction(tr("Select &All"), QKeySequence::SelectAll, "selectAll",
                    &ScintillaEdit::selectAll);

    auto* searchMenu = menuBar()->addMenu(tr("&Search"));
    findAction_ = searchMenu->addAction(tr("&Find…"));
    findAction_->setShortcut(QKeySequence::Find);
    makeShortcutCustomizable(findAction_, "find");
    connect(findAction_, &QAction::triggered,
            this, [this] { showFindReplace(false); });

    replaceAction_ = searchMenu->addAction(tr("&Replace…"));
    replaceAction_->setShortcut(QKeySequence::Replace);
    makeShortcutCustomizable(replaceAction_, "replace");
    connect(replaceAction_, &QAction::triggered,
            this, [this] { showFindReplace(true); });

    searchMenu->addSeparator();
    findNextAction_ = searchMenu->addAction(tr("Find &Next"));
    findNextAction_->setShortcut(QKeySequence::FindNext);
    makeShortcutCustomizable(findNextAction_, "findNext");
    connect(findNextAction_, &QAction::triggered,
            searchController_, &SearchController::findNext);

    findPreviousAction_ = searchMenu->addAction(tr("Find &Previous"));
    findPreviousAction_->setShortcut(QKeySequence::FindPrevious);
    makeShortcutCustomizable(findPreviousAction_, "findPrevious");
    connect(findPreviousAction_, &QAction::triggered,
            searchController_, &SearchController::findPrevious);

    goToLineAction_ = searchMenu->addAction(tr("&Go To Line…"));
    goToLineAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    makeShortcutCustomizable(goToLineAction_, "goToLine");
    connect(goToLineAction_, &QAction::triggered,
            this, &MainWindow::showGoToLine);

    searchMenu->addSeparator();
    auto* bookmarksMenu = searchMenu->addMenu(tr("&Bookmarks"));
    const auto addBookmarkAction = [this, bookmarksMenu, &makeShortcutCustomizable](
        const QString& text, const QKeySequence& shortcut, const char* id) {
        auto* action = bookmarksMenu->addAction(text);
        action->setObjectName(QString::fromLatin1(id) + QStringLiteral("Action"));
        action->setShortcut(shortcut);
        makeShortcutCustomizable(action, id);
        bookmarkActions_.append(action);
        return action;
    };
    toggleBookmarkAction_ = addBookmarkAction(tr("Toggle &Bookmark"),
        QKeySequence(Qt::CTRL | Qt::Key_F2), "toggleBookmark");
    toggleBookmarkAction_->setCheckable(true);
    connect(toggleBookmarkAction_, &QAction::triggered, this, [this] {
        if (!isDocumentBusy()) {
            editor_->toggleBookmarkAtLine(editor_->currentOneBasedLine());
        }
    });
    auto* nextBookmark = addBookmarkAction(tr("&Next Bookmark"),
        QKeySequence(Qt::Key_F2), "nextBookmark");
    connect(nextBookmark, &QAction::triggered,
            this, [this] { goToBookmark(true); });
    auto* previousBookmark = addBookmarkAction(tr("&Previous Bookmark"),
        QKeySequence(Qt::SHIFT | Qt::Key_F2), "previousBookmark");
    connect(previousBookmark, &QAction::triggered,
            this, [this] { goToBookmark(false); });
    bookmarksMenu->addSeparator();
    auto* clearBookmarks = addBookmarkAction(tr("&Clear All Bookmarks"),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F2), "clearBookmarks");
    connect(clearBookmarks, &QAction::triggered, this, [this] {
        if (!isDocumentBusy()) {
            editor_->clearAllBookmarks();
            statusBar()->showMessage(tr("Bookmarks cleared."), 3000);
        }
    });
    connect(editor_, &EditorWidget::bookmarksChanged, this, [this] {
        toggleBookmarkAction_->setChecked(
            editor_->hasBookmarkAtLine(editor_->currentOneBasedLine()));
    });
    connect(editor_, &EditorWidget::cursorPositionChanged, this,
            [this](qsizetype line, qsizetype) {
                toggleBookmarkAction_->setChecked(editor_->hasBookmarkAtLine(line));
            });
    connect(editor_, &EditorWidget::bookmarkToggled, this,
            [this](qint64 line, bool enabled) {
                statusBar()->showMessage((enabled
                    ? tr("Bookmark added at line %1.")
                    : tr("Bookmark removed from line %1.")).arg(line), 3000);
            });

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    wrapAction_ = viewMenu->addAction(tr("Word &Wrap"));
    wrapAction_->setObjectName(QStringLiteral("wordWrapAction"));
    wrapAction_->setCheckable(true);
    makeShortcutCustomizable(wrapAction_, "wordWrap");
    connect(wrapAction_, &QAction::toggled, this, [this](bool enabled) {
        preferredWordWrap_ = enabled;
        editor_->setWordWrapEnabled(enabled);
        if (enabled && LargeFilePolicy::usesLargeDocument(
                           currentTab().largeFileMode)) {
            statusBar()->showMessage(
                tr("Word wrap may be slow in %1.")
                    .arg(LargeFilePolicy::displayName(
                        currentTab().largeFileMode)),
                5000);
        }
        savePersistentSettings();
    });

    lineNumberAction_ = viewMenu->addAction(tr("Line &Numbers"));
    lineNumberAction_->setObjectName(QStringLiteral("lineNumbersAction"));
    lineNumberAction_->setCheckable(true);
    lineNumberAction_->setChecked(true);
    makeShortcutCustomizable(lineNumberAction_, "lineNumbers");
    connect(lineNumberAction_, &QAction::toggled, this, [this](bool visible) {
        editor_->setLineNumbersVisible(visible);
        savePersistentSettings();
    });

    editHistoryAction_ = viewMenu->addAction(tr("Edit &History"));
    editHistoryAction_->setObjectName(QStringLiteral("editHistoryAction"));
    editHistoryAction_->setCheckable(true);
    editHistoryAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    makeShortcutCustomizable(editHistoryAction_, "editHistory");
    connect(editHistoryAction_, &QAction::toggled,
            editHistoryDock_, &QDockWidget::setVisible);
    connect(editHistoryDock_, &QDockWidget::visibilityChanged,
            editHistoryAction_, &QAction::setChecked);

    viewMenu->addSeparator();
    auto* alwaysOnTopAction = viewMenu->addAction(tr("Always on &Top"));
    alwaysOnTopAction->setObjectName(QStringLiteral("alwaysOnTopAction"));
    alwaysOnTopAction->setCheckable(true);
    alwaysOnTopAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    makeShortcutCustomizable(alwaysOnTopAction, "alwaysOnTop");
    connect(alwaysOnTopAction, &QAction::toggled,
            windowController_, &WindowController::setAlwaysOnTop);
    connect(windowController_, &WindowController::alwaysOnTopChanged,
            alwaysOnTopAction, &QAction::setChecked);
    connect(windowController_, &WindowController::alwaysOnTopChanged,
            this, [this] { savePersistentSettings(); });

    framelessAction_ = viewMenu->addAction(tr("&Frameless Mode"));
    framelessAction_->setObjectName(QStringLiteral("framelessAction"));
    framelessAction_->setCheckable(true);
    framelessAction_->setShortcut(QKeySequence(Qt::Key_F11));
    makeShortcutCustomizable(framelessAction_, "frameless");
    // Keep the shortcut registered on the top-level window after frameless
    // mode hides the menu bar that also owns this action.
    connect(framelessAction_, &QAction::toggled,
            windowController_, &WindowController::setFrameless);
    connect(windowController_, &WindowController::framelessChanged,
            framelessAction_, &QAction::setChecked);
    connect(windowController_, &WindowController::framelessChanged,
            this, [this] {
                if (!windowController_->isMinimalMode()) {
                    savePersistentSettings();
                }
            });

    minimalModeAction_ = viewMenu->addAction(tr("&Minimal Mode"));
    minimalModeAction_->setObjectName(QStringLiteral("minimalModeAction"));
    minimalModeAction_->setCheckable(true);
    minimalModeAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    makeShortcutCustomizable(minimalModeAction_, "minimalMode");
    connect(minimalModeAction_, &QAction::toggled,
            windowController_, &WindowController::setMinimalMode);
    connect(windowController_, &WindowController::minimalModeChanged,
            minimalModeAction_, &QAction::setChecked);

    exitMinimalModeAction_ = new QAction(tr("Exit Minimal Mode"), this);
    exitMinimalModeAction_->setObjectName(
        QStringLiteral("exitMinimalModeAction"));
    exitMinimalModeAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    exitMinimalModeAction_->setEnabled(false);
    makeShortcutCustomizable(exitMinimalModeAction_, "exitMinimalMode");
    connect(exitMinimalModeAction_, &QAction::triggered,
            this, [this] { windowController_->setMinimalMode(false); });

    connect(windowController_, &WindowController::minimalModeChanged,
            this, [this](bool enabled) {
                framelessAction_->setEnabled(!enabled);
                editHistoryAction_->setEnabled(!enabled);
                exitMinimalModeAction_->setEnabled(enabled);
                if (enabled) {
                    editHistoryDock_->hide();
                }
                if (!enabled) {
                    editor_->QWidget::setFocus();
                }
                updateTabBarVisibility();
            });

    auto* previousTabAction = new QAction(tr("Previous Tab"), this);
    previousTabAction->setObjectName(QStringLiteral("previousTabAction"));
    previousTabAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
    makeShortcutCustomizable(previousTabAction, "previousTab");
    previousTabAction->setShortcutContext(Qt::WindowShortcut);
    connect(previousTabAction, &QAction::triggered,
            this, [this] { switchRelativeTab(-1); });

    auto* nextTabAction = new QAction(tr("Next Tab"), this);
    nextTabAction->setObjectName(QStringLiteral("nextTabAction"));
    nextTabAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));
    makeShortcutCustomizable(nextTabAction, "nextTab");
    nextTabAction->setShortcutContext(Qt::WindowShortcut);
    connect(nextTabAction, &QAction::triggered,
            this, [this] { switchRelativeTab(1); });

    auto* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    appearancePresetsMenu_ = settingsMenu->addMenu(tr("Custom &Styles"));
    appearancePresetsMenu_->setObjectName(QStringLiteral("appearancePresetsMenu"));
    previousAppearancePresetAction_ = appearancePresetsMenu_->addAction(
        tr("Previous Style"));
    previousAppearancePresetAction_->setObjectName(
        QStringLiteral("previousAppearancePresetAction"));
    previousAppearancePresetAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_PageUp));
    makeShortcutCustomizable(previousAppearancePresetAction_, "previousStyle");
    connect(previousAppearancePresetAction_, &QAction::triggered,
            this, [this] { cycleAppearancePreset(-1); });
    nextAppearancePresetAction_ = appearancePresetsMenu_->addAction(
        tr("Next Style"));
    nextAppearancePresetAction_->setObjectName(
        QStringLiteral("nextAppearancePresetAction"));
    nextAppearancePresetAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_PageDown));
    makeShortcutCustomizable(nextAppearancePresetAction_, "nextStyle");
    connect(nextAppearancePresetAction_, &QAction::triggered,
            this, [this] { cycleAppearancePreset(1); });
    appearancePresetsMenu_->addSeparator();

    auto* appearanceAction = settingsMenu->addAction(
        tr("&Appearance and Shortcuts…"));
    appearanceAction->setObjectName(
        QStringLiteral("appearanceAndShortcutsAction"));
    makeShortcutCustomizable(appearanceAction, "settings");
    connect(appearanceAction, &QAction::triggered,
            this, &MainWindow::showSettings);

    settingsMenu->addSeparator();
    increaseBackgroundAlphaAction_ =
        settingsMenu->addAction(tr("Increase Background Opacity"));
    increaseBackgroundAlphaAction_->setObjectName(
        QStringLiteral("increaseBackgroundAlphaAction"));
    increaseBackgroundAlphaAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_Up));
    makeShortcutCustomizable(increaseBackgroundAlphaAction_, "increaseBackgroundOpacity");
    connect(increaseBackgroundAlphaAction_, &QAction::triggered,
            this, [this] { adjustBackgroundAlpha(5); });

    decreaseBackgroundAlphaAction_ =
        settingsMenu->addAction(tr("Decrease Background Opacity"));
    decreaseBackgroundAlphaAction_->setObjectName(
        QStringLiteral("decreaseBackgroundAlphaAction"));
    decreaseBackgroundAlphaAction_->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_Down));
    makeShortcutCustomizable(decreaseBackgroundAlphaAction_, "decreaseBackgroundOpacity");
    connect(decreaseBackgroundAlphaAction_, &QAction::triggered,
            this, [this] { adjustBackgroundAlpha(-5); });

    // Include submenus and QMenuBar's overflow popup as well as the five
    // top-level menus, sharing one appearance with the tray menu.
    menuBar()->ensurePolished();
    for (QMenu* menu : menuBar()->findChildren<QMenu*>()) {
        applyMenuAppearance(menu);
    }
}

void MainWindow::connectSearch()
{
    connect(findReplaceWidget_, &FindReplaceWidget::searchTextChanged,
            searchController_, &SearchController::setSearchText);
    connect(findReplaceWidget_, &FindReplaceWidget::replacementTextChanged,
            searchController_, &SearchController::setReplacementText);
    connect(findReplaceWidget_, &FindReplaceWidget::optionsChanged,
            searchController_, &SearchController::setOptions);
    connect(findReplaceWidget_, &FindReplaceWidget::findNextRequested,
            searchController_, &SearchController::findNext);
    connect(findReplaceWidget_, &FindReplaceWidget::findPreviousRequested,
            searchController_, &SearchController::findPrevious);
    connect(findReplaceWidget_, &FindReplaceWidget::replaceRequested,
            searchController_, &SearchController::replaceCurrent);
    connect(findReplaceWidget_, &FindReplaceWidget::replaceAllRequested,
            this, [this] {
                if (LargeFilePolicy::requiresReplaceAllConfirmation(
                        currentTab().largeFileMode)
                    && QMessageBox::warning(
                           this, tr("Replace All in a very large file"),
                           tr("Replace All may take a long time and create a "
                              "large undo record. Continue?"),
                           QMessageBox::Yes | QMessageBox::No,
                           QMessageBox::No) != QMessageBox::Yes) {
                    return;
                }
                searchController_->replaceAll();
            });
    connect(findReplaceWidget_, &FindReplaceWidget::closeRequested,
            this, [this] {
                searchController_->cancelSearch();
                findReplaceWidget_->hide();
                editor_->QWidget::setFocus();
            });
    connect(findReplaceWidget_, &FindReplaceWidget::cancelSearchRequested,
            searchController_, &SearchController::cancelSearch);
    connect(searchController_, &SearchController::searchingChanged,
            this, [this](bool searching) {
                findReplaceWidget_->setSearching(searching);
                updateBusyUi();
                if (!searching) {
                    QTimer::singleShot(0, this, [this] {
                        processPendingOpenFiles();
                        processCurrentExternalFileChange();
                    });
                }
            });
    connect(searchController_, &SearchController::progressChanged,
            this, [this](int percent) { progressBar_->setValue(percent); });
    connect(searchController_, &SearchController::resultChanged,
            this, [this](SearchResult result, const QString& message) {
                findReplaceWidget_->setResult(result, message);
                statusBar()->showMessage(
                    message, result == SearchResult::Searching ? 0 : 3000);
            });
}

void MainWindow::showFindReplace(bool replaceMode)
{
    // Reject large selections before copying them out of Scintilla. The find
    // field accepts at most 256 UTF-16 code units (at most 1024 UTF-8 bytes).
    QString initialText;
    const qint64 selectedBytes = editor_->selectionEndPosition()
        - editor_->selectionStartPosition();
    if (selectedBytes <= 1024) {
        initialText = QString::fromUtf8(editor_->selectedTextUtf8());
    }
    if (initialText.contains(QLatin1Char('\n'))
        || initialText.contains(QLatin1Char('\r'))
        || initialText.size() > 256) {
        initialText.clear();
    }
    findReplaceWidget_->open(replaceMode, initialText);
}

void MainWindow::showGoToLine()
{
    const int maximumLine = static_cast<int>(std::min<qint64>(
        editor_->editorLineCount(), std::numeric_limits<int>::max()));
    bool accepted = false;
    const int line = QInputDialog::getInt(
        this, tr("Go To Line"), tr("Line number:"),
        static_cast<int>(std::min<qint64>(currentLine_, maximumLine)),
        1, std::max(1, maximumLine), 1, &accepted);
    if (accepted) {
        searchController_->goToLine(line);
    }
}

void MainWindow::goToBookmark(bool forward)
{
    if (isDocumentBusy()) {
        return;
    }
    const bool moved = forward ? editor_->goToNextBookmark()
                               : editor_->goToPreviousBookmark();
    statusBar()->showMessage(moved
        ? tr("Moved to bookmark at line %1.").arg(editor_->currentOneBasedLine())
        : tr("No bookmarks in this document."), 3000);
}

void MainWindow::showSettings()
{
    const Appearance original = themeManager_->appearance();
    SettingsDialog dialog(original, bossKey_, focusShortcut_,
                          restoreTabsOnStartup_, this);
    QVector<QAction*> shortcutActions;
    for (QAction* action : findChildren<QAction*>()) {
        if (action->property("shortcutId").isValid()) {
            shortcutActions.append(action);
        }
    }
    std::sort(shortcutActions.begin(), shortcutActions.end(),
              [](const QAction* first, const QAction* second) {
                  return first->property("shortcutOrder").toInt()
                      < second->property("shortcutOrder").toInt();
              });
    QVector<ShortcutBinding> bindings;
    bindings.reserve(shortcutActions.size());
    for (const QAction* action : std::as_const(shortcutActions)) {
        QString name = action->text();
        name.remove(QLatin1Char('&'));
        name.remove(QChar(0x2026));
        bindings.append({action->property("shortcutId").toString(),
                         name.trimmed(), action->shortcut(),
                         action->property("defaultShortcut").value<QKeySequence>()});
    }
    dialog.setShortcutBindings(bindings);
    dialog.setAppearancePresets(appearancePresets_);
    connect(&dialog, &SettingsDialog::previewChanged,
            themeManager_, &ThemeManager::applyAppearance);
    if (dialog.exec() != QDialog::Accepted) {
        themeManager_->applyAppearance(original);
    } else {
        const bool bossKeyWasChanged = bossKey_ != dialog.bossKey();
        const bool focusShortcutWasChanged =
            focusShortcut_ != dialog.focusShortcut();
        if (bossKeyWasChanged && focusShortcutWasChanged) {
            // Release both registrations first so users can swap the two
            // shortcuts without either old registration blocking the other.
            emit bossKeyChanged(QKeySequence());
            emit focusShortcutChanged(QKeySequence());
        }
        if (bossKeyWasChanged) {
            bossKey_ = dialog.bossKey();
            emit bossKeyChanged(bossKey_);
        }
        if (focusShortcutWasChanged) {
            focusShortcut_ = dialog.focusShortcut();
            emit focusShortcutChanged(focusShortcut_);
        }
        restoreTabsOnStartup_ = dialog.restoreTabsOnStartup();
        appearancePresets_ = dialog.appearancePresets();
        rebuildAppearancePresetActions();
        QMap<QString, QKeySequence> updatedShortcuts;
        for (const ShortcutBinding& binding : dialog.shortcutBindings()) {
            updatedShortcuts.insert(binding.id, binding.shortcut);
        }
        applyShortcuts(updatedShortcuts);
        savePersistentSettings();
    }
}

void MainWindow::rebuildAppearancePresetActions()
{
    for (QAction* action : std::as_const(appearancePresetActions_)) {
        removeAction(action);
        appearancePresetsMenu_->removeAction(action);
        delete action;
    }
    appearancePresetActions_.clear();
    const bool hasPresets = !appearancePresets_.isEmpty();
    previousAppearancePresetAction_->setEnabled(hasPresets);
    nextAppearancePresetAction_->setEnabled(hasPresets);

    for (qsizetype index = 0; index < appearancePresets_.size(); ++index) {
        const AppearancePreset& preset = appearancePresets_.at(index);
        auto* action = new QAction(preset.name, this);
        action->setObjectName(
            QStringLiteral("appearancePresetAction%1").arg(index));
        action->setProperty("appearancePresetAction", true);
        action->setShortcut(preset.shortcut);
        action->setShortcutContext(Qt::WindowShortcut);
        appearancePresetsMenu_->addAction(action);
        addAction(action);
        connect(action, &QAction::triggered, this,
                [this, index] { applyAppearancePreset(static_cast<int>(index)); });
        appearancePresetActions_.append(action);
    }
}

void MainWindow::applyAppearancePreset(int index)
{
    if (index < 0 || index >= appearancePresets_.size()) {
        return;
    }
    const AppearancePreset& preset = appearancePresets_.at(index);
    themeManager_->applyAppearance(preset.appearance);
    savePersistentSettings();
    statusBar()->showMessage(tr("Style: %1").arg(preset.name), 1500);
}

void MainWindow::cycleAppearancePreset(int direction)
{
    if (appearancePresets_.isEmpty() || direction == 0) {
        return;
    }
    int current = -1;
    for (qsizetype index = 0; index < appearancePresets_.size(); ++index) {
        if (appearancePresets_.at(index).appearance == themeManager_->appearance()) {
            current = static_cast<int>(index);
            break;
        }
    }
    const int count = static_cast<int>(appearancePresets_.size());
    const int next = current < 0
        ? (direction > 0 ? 0 : count - 1)
        : (current + (direction > 0 ? 1 : -1) + count) % count;
    applyAppearancePreset(next);
}

void MainWindow::applyShortcuts(
    const QMap<QString, QKeySequence>& shortcutValues)
{
    for (QAction* action : findChildren<QAction*>()) {
        const QString id = action->property("shortcutId").toString();
        const auto iterator = shortcutValues.constFind(id);
        if (!id.isEmpty() && iterator != shortcutValues.cend()) {
            action->setShortcut(iterator.value());
        }
    }
}

QMap<QString, QKeySequence> MainWindow::shortcuts() const
{
    QMap<QString, QKeySequence> result;
    for (const QAction* action : findChildren<QAction*>()) {
        const QString id = action->property("shortcutId").toString();
        if (!id.isEmpty()) {
            result.insert(id, action->shortcut());
        }
    }
    return result;
}

void MainWindow::connectFileManager()
{
    connect(fileManager_, &FileManager::operationChanged,
            this, [this](FileManager::Operation operation) {
                if (operation != FileManager::Operation::None) {
                    searchController_->cancelSearch();
                }
                updateBusyUi();
            });
    connect(fileManager_, &FileManager::loadPrepared, this,
            [this](const FileLoadInfo& info) {
                const LargeFileMode mode =
                    LargeFilePolicy::modeForSize(info.fileSize);
                if (!editor_->beginFileLoad(mode, info.fileSize)) {
                    fileManager_->cancelCurrentOperation();
                    QMessageBox::critical(
                        this, tr("Open file"),
                        tr("Scintilla could not create a document for this file."));
                    return;
                }
                replaceCurrentTabDocumentHandle();
                loadReplacedDocument_ = true;
                applyLargeFileMode(mode);
                progressBar_->setRange(0, info.fileSize > 0 ? 1000 : 0);
                progressBar_->setValue(0);
                statusBar()->showMessage(tr("Loading %1").arg(info.path));
            });
    connect(fileManager_, &FileManager::saveChunkRequested, this,
            [this](qint64 offset, qint64 maximumBytes) {
                const QByteArray chunk =
                    editor_->textRangeUtf8(offset, maximumBytes);
                fileManager_->provideSaveChunk(
                    chunk, offset + chunk.size() >= editor_->documentLength());
            });
    connect(fileManager_, &FileManager::loadChunk, this,
            [this](const QByteArray& chunkData, qint64 bytesRead,
                   qint64 totalBytes) {
                if (!loadReplacedDocument_) {
                    return;
                }
                editor_->appendTextUtf8(chunkData);
                if (totalBytes > 0) {
                    progressBar_->setValue(static_cast<int>(
                        std::min<qint64>(1000, bytesRead * 1000 / totalBytes)));
                }
                statusBar()->showMessage(
                    tr("Loading %1 / %2")
                        .arg(QLocale().formattedDataSize(bytesRead),
                             QLocale().formattedDataSize(totalBytes)));
            });
    connect(fileManager_, &FileManager::loadCompleted, this,
            [this](const FileLoadInfo& info) {
                const QString previousPath = currentTab().document.path();
                editor_->completeFileLoad(info.lineEnding);
                currentTab().document.adoptLoadedFile(info);
                currentTab().insertionLineEnding = info.lineEnding;
                currentTab().diskStamp = info.modifiedAt.isValid()
                    ? std::make_optional(DiskStamp{
                          info.fileSize, info.modifiedAt, info.contentHash})
                    : std::nullopt;
                removeRecoverySnapshot(currentTab());
                currentTab().externalChange.reset();
                if (!previousPath.isEmpty() && previousPath != info.path) {
                    fileChangeMonitor_->unwatchFile(previousPath);
                }
                fileChangeMonitor_->watchFile(info.path);
                suspendedFileWatchPath_.clear();
                lastDirectory_ = QFileInfo(info.path).absolutePath();
                addRecentFile(info.path);
                loadReplacedDocument_ = false;
                loadingTabIndex_ = -1;
                updateWindowTitle();
                updateDocumentStatus();
                statusBar()->showMessage(tr("Loaded %1").arg(info.path), 3000);
                savePersistentSettings();
                processPendingOpenFiles();
            });
    connect(fileManager_, &FileManager::saveProgress, this,
            [this](qint64 written, qint64 total) {
                if (total > 0) {
                    progressBar_->setRange(0, 1000);
                    progressBar_->setValue(static_cast<int>(
                        std::min<qint64>(1000, written * 1000 / total)));
                }
            });
    connect(fileManager_, &FileManager::saveCompleted, this,
            [this](const FileSaveResult& result) {
                const QString previousPath = currentTab().document.path();
                editor_->markSaved();
                currentTab().document.adoptSavedFile(result);
                currentTab().diskStamp = diskStampForPath(result.path);
                removeRecoverySnapshot(currentTab());
                currentTab().externalChange.reset();
                if (!previousPath.isEmpty() && previousPath != result.path) {
                    fileChangeMonitor_->unwatchFile(previousPath);
                }
                fileChangeMonitor_->watchFile(result.path);
                suspendedFileWatchPath_.clear();
                if (closeAllTabsInProgress_
                    && !closingSessionPaths_.contains(result.path)) {
                    closingSessionPaths_.append(result.path);
                }
                lastDirectory_ = QFileInfo(result.path).absolutePath();
                addRecentFile(result.path);
                updateWindowTitle();
                updateDocumentStatus();
                statusBar()->showMessage(tr("Saved %1").arg(result.path), 3000);
                savePersistentSettings();

                auto continuation = std::move(pendingAfterSave_);
                pendingAfterSave_ = {};
                if (continuation) {
                    continuation();
                }
            });
    connect(fileManager_, &FileManager::operationFailed, this,
            [this](const QString& message) {
                resumeSuspendedFileWatch();
                handleLoadFailureState();
                pendingAfterSave_ = {};
                QMessageBox::critical(this, tr("File operation failed"), message);
                processPendingOpenFiles();
            });
    connect(fileManager_, &FileManager::operationCanceled, this, [this] {
        resumeSuspendedFileWatch();
        handleLoadFailureState();
        pendingAfterSave_ = {};
        statusBar()->showMessage(tr("File operation canceled"), 3000);
        processPendingOpenFiles();
    });
}

void MainWindow::connectFileChangeMonitor()
{
    connect(fileChangeMonitor_, &FileChangeMonitor::fileChangedExternally,
            this, &MainWindow::handleExternalFileChange);
}

void MainWindow::initializeCrashRecovery()
{
    if (crashRecoveryChecked_) {
        processPendingOpenFiles();
        return;
    }

    const QVector<RecoveryEntry> recoverable = recoveryManager_->entries();
    if (!recoverable.isEmpty() && !property("skipCrashRecovery").toBool()) {
        QMessageBox message(
            QMessageBox::Question, tr("Recover unsaved documents"),
            tr("Vinson Editor found %1 document(s) with unsaved changes from "
               "a previous session.").arg(recoverable.size()),
            QMessageBox::NoButton, this);
        auto* restoreButton = message.addButton(
            tr("Restore Documents"), QMessageBox::AcceptRole);
        auto* discardButton = message.addButton(
            tr("Discard Recovery Data"), QMessageBox::DestructiveRole);
        message.exec();
        if (message.clickedButton() == restoreButton) {
            restoreRecoveryEntries(recoverable);
        } else if (message.clickedButton() == discardButton) {
            recoveryManager_->clearAll();
        }
    }

    crashRecoveryChecked_ = true;
    processPendingOpenFiles();
}

void MainWindow::restoreRecoveryEntries(
    const QVector<RecoveryEntry>& entries)
{
    bool usedInitialTab = false;
    int restored = 0;
    for (const RecoveryEntry& entry : entries) {
        const QByteArray content = recoveryManager_->loadContent(entry.id);
        if (entry.contentSize > 0 && content.isEmpty()) {
            continue;
        }
        if (!usedInitialTab && currentTabIsPristineUntitled()) {
            usedInitialTab = true;
        } else if (addBlankTab() < 0) {
            break;
        }

        editor_->setTextUtf8(content);
        editor_->completeFileLoad(entry.lineEnding);
        editor_->markRecovered();
        TabState& tab = currentTab();
        tab.recoveryId = entry.id;
        tab.insertionLineEnding = entry.lineEnding;
        tab.largeFileMode = LargeFileMode::Normal;
        tab.externalChange.reset();
        // A recovered draft may predate changes to its on-disk file.
        tab.diskStamp.reset();
        if (entry.originalPath.isEmpty()) {
            tab.document.reset();
            tab.document.setEncoding(entry.encoding);
            tab.document.setLineEnding(entry.lineEnding);
            tab.document.setModified(true);
        } else {
            tab.document.adoptRecoveredFile(
                entry.originalPath, entry.encoding, entry.lineEnding,
                entry.originalFileSize);
            fileChangeMonitor_->watchFile(entry.originalPath);
        }
        applyLargeFileMode(LargeFileMode::Normal);
        updateWindowTitle();
        updateDocumentStatus();
        ++restored;
    }
    if (restored > 0) {
        statusBar()->showMessage(
            tr("Restored %1 unsaved document(s)").arg(restored), 5000);
    }
}

void MainWindow::scheduleRecoverySnapshot()
{
    if (!crashRecoveryChecked_ || switchingTabs_
        || isDocumentBusy()) {
        return;
    }
    recoveryTimer_->start();
}

void MainWindow::writeCurrentRecoverySnapshot()
{
    if (!crashRecoveryChecked_ || switchingTabs_ || isDocumentBusy()
        || currentTabIndex_ < 0
        || currentTabIndex_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    TabState& tab = currentTab();
    if (!tab.document.isModified()) {
        removeRecoverySnapshot(tab);
        return;
    }
    const qint64 length = editor_->documentLength();
    if (LargeFilePolicy::usesLargeDocument(tab.largeFileMode)
        || length > RecoveryManager::maximumSnapshotBytes) {
        removeRecoverySnapshot(tab);
        if (!tab.recoveryLimitNotified) {
            tab.recoveryLimitNotified = true;
            statusBar()->showMessage(
                tr("Crash recovery is limited to documents of %1 or smaller")
                    .arg(QLocale().formattedDataSize(
                        RecoveryManager::maximumSnapshotBytes)),
                5000);
        }
        return;
    }

    RecoverySnapshot snapshot;
    snapshot.entry.id = tab.recoveryId;
    snapshot.entry.originalPath = tab.document.path();
    snapshot.entry.displayName = tab.document.isUntitled()
        ? tr("Untitled") : tab.document.displayName();
    snapshot.entry.encoding = tab.document.encoding();
    snapshot.entry.lineEnding = tab.document.lineEnding();
    snapshot.entry.originalFileSize = tab.document.fileSize();
    snapshot.content = editor_->textUtf8();
    if (snapshot.content.size() > RecoveryManager::maximumSnapshotBytes) {
        removeRecoverySnapshot(tab);
        return;
    }
    tab.recoveryLimitNotified = false;
    recoveryManager_->queueSnapshot(std::move(snapshot));
}

void MainWindow::removeRecoverySnapshot(const TabState& tab)
{
    if (!tab.recoveryId.isEmpty()) {
        recoveryManager_->removeSnapshot(tab.recoveryId);
    }
}

void MainWindow::newDocument()
{
    if (isDocumentBusy()) {
        return;
    }
    if (addBlankTab() >= 0) {
        statusBar()->showMessage(tr("New document"), 2000);
    }
}

int MainWindow::addBlankTab(bool activate)
{
    const sptr_t document = editor_->createTabDocument();
    if (document == 0) {
        QMessageBox::critical(
            this, tr("New document"),
            tr("Scintilla could not create a new document."));
        return -1;
    }

    TabState tab;
    tab.documentHandle = document;
    tab.recoveryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tabs_.push_back(std::move(tab));
    const int index = static_cast<int>(tabs_.size()) - 1;
    const int tabBarIndex = tabBar_->addTab(tr("Untitled"));
    tabBar_->setTabData(tabBarIndex,
                        QVariant::fromValue(tabs_.back().documentHandle));
    if (activate) {
        switchToTab(index);
    }
    savePersistentSettings();
    return index;
}

void MainWindow::snapshotCurrentTabView()
{
    if (currentTabIndex_ < 0
        || currentTabIndex_ >= static_cast<int>(tabs_.size())) {
        return;
    }
    TabState& tab = tabs_.at(currentTabIndex_);
    writeCurrentRecoverySnapshot();
    tab.caret = editor_->currentPos();
    tab.anchor = editor_->anchor();
    tab.firstVisibleLine = editor_->firstVisibleLine();
    tab.horizontalOffset = editor_->xOffset();
}

void MainWindow::switchToTab(int index, bool force)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())
        || isDocumentBusy()) {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->setCurrentIndex(currentTabIndex_);
        return;
    }
    synchronizeTabOrder();
    if (!force && index == currentTabIndex_) {
        return;
    }

    snapshotCurrentTabView();
    switchingTabs_ = true;
    currentTabIndex_ = index;
    TabState& tab = currentTab();
    editor_->activateTabDocument(
        static_cast<sptr_t>(tab.documentHandle), tab.largeFileMode);
    editor_->setInsertionLineEnding(tab.insertionLineEnding);
    editor_->setSel(static_cast<sptr_t>(tab.anchor),
                    static_cast<sptr_t>(tab.caret));
    editor_->setFirstVisibleLine(
        static_cast<sptr_t>(tab.firstVisibleLine));
    editor_->setXOffset(static_cast<sptr_t>(tab.horizontalOffset));
    applyLargeFileMode(tab.largeFileMode);
    switchingTabs_ = false;

    {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->setCurrentIndex(index);
    }
    editHistoryWidget_->refresh();
    currentLine_ = editor_->currentOneBasedLine();
    cursorPositionLabel_->setText(
        tr("Ln %1, Col %2")
            .arg(currentLine_)
            .arg(editor_->column(editor_->currentPos()) + 1));
    updateWindowTitle();
    updateDocumentStatus();
    editor_->QWidget::setFocus(Qt::ShortcutFocusReason);
    if (tab.externalChange.has_value()) {
        QTimer::singleShot(0, this,
                           &MainWindow::processCurrentExternalFileChange);
    }
}

void MainWindow::synchronizeTabOrder()
{
    if (synchronizingTabOrder_
        || tabBar_->count() != static_cast<int>(tabs_.size())) {
        return;
    }

    bool orderChanged = false;
    for (int index = 0; index < tabBar_->count(); ++index) {
        if (tabBar_->tabData(index).value<qintptr>()
            != tabs_.at(index).documentHandle) {
            orderChanged = true;
            break;
        }
    }
    if (!orderChanged) {
        return;
    }

    const qintptr activeDocument = currentTabIndex_ >= 0
        && currentTabIndex_ < static_cast<int>(tabs_.size())
        ? tabs_.at(currentTabIndex_).documentHandle : 0;
    synchronizingTabOrder_ = true;
    std::vector<TabState> reorderedTabs;
    reorderedTabs.reserve(tabs_.size());
    for (int index = 0; index < tabBar_->count(); ++index) {
        const qintptr documentHandle =
            tabBar_->tabData(index).value<qintptr>();
        const auto found = std::find_if(
            tabs_.begin(), tabs_.end(),
            [documentHandle](const TabState& tab) {
                return tab.documentHandle == documentHandle;
            });
        if (found == tabs_.end()) {
            synchronizingTabOrder_ = false;
            return;
        }
        reorderedTabs.push_back(std::move(*found));
    }
    tabs_ = std::move(reorderedTabs);
    const auto active = std::find_if(
        tabs_.cbegin(), tabs_.cend(),
        [activeDocument](const TabState& tab) {
            return tab.documentHandle == activeDocument;
        });
    currentTabIndex_ = active == tabs_.cend()
        ? tabBar_->currentIndex()
        : static_cast<int>(active - tabs_.cbegin());
    synchronizingTabOrder_ = false;
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (nativeTitleBar_ && nativeTitleBar_->nativeEvent(message, result))
        return true;
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::paintEvent(QPaintEvent* event)
{
    if (!nativeTitleBar_ || !nativeTitleBar_->isActive()) {
        QMainWindow::paintEvent(event);
        return;
    }
    // DWM draws the real caption buttons in this transparent part of the
    // backing surface. Painting opaque pixels there would cover the buttons.
    QPainter painter(this);
    const QRegion body = QRegion(rect()) - nativeTitleBar_->captionPaintRect();
    painter.setClipRegion(body);
    painter.fillRect(rect(), palette().color(QPalette::Window));
    painter.fillRect(QRect(0, 0, width(), contentsMargins().top()),
                     titleBarBackground(palette().color(QPalette::Window)));
}

void MainWindow::applyTabBarAppearance(const Appearance& appearance)
{
    QColor background = appearance.backgroundColor;
    QColor text = appearance.textColor;
    background.setAlpha(255);
    text.setAlpha(255);
    QPalette tabPalette = tabBar_->palette();
    tabBar_->setProperty("tabForeground", text);
    const QColor header = titleBarBackground(background);
    tabBar_->setProperty("tabInactive", header);
    tabBar_->setProperty("tabHover", header.darker(108));
    tabPalette.setColor(QPalette::WindowText, text);
    tabPalette.setColor(QPalette::ButtonText, text);
    tabPalette.setColor(QPalette::Window, background);
    tabBar_->setPalette(tabPalette);
    tabBar_->setAutoFillBackground(false);
    tabBar_->update();
}
void MainWindow::showTabContextMenu(const QPoint& position)
{
    if (isDocumentBusy() || closeAllTabsInProgress_)
        return;
    const int index = tabBar_->tabAt(position);
    const qintptr document = index >= 0 ? tabs_.at(index).documentHandle : 0;
    // Resolve identities when an action is invoked; tab positions can change.
    const auto targetIndex = [this, document] {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            if (tabs_.at(i).documentHandle == document)
                return i;
        }
        return -1;
    };
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("tabContextMenu"));
    applyMenuAppearance(menu);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    auto* newTab = menu->addAction(tr("New Tab"), this, &MainWindow::newDocument);
    newTab->setObjectName(QStringLiteral("tabNewAction"));
    if (index >= 0) {
        auto* save = menu->addAction(tr("Save Tab"), this, [this, targetIndex] {
            const int target = targetIndex();
            if (target >= 0 && !isDocumentBusy()) {
                switchToTab(target);
                saveDocument();
            }
        });
        save->setObjectName(QStringLiteral("tabSaveAction"));
        auto* copyPath = menu->addAction(tr("Copy File Path"), this, [this, targetIndex] {
            const int target = targetIndex();
            if (target >= 0 && !tabs_.at(target).document.path().isEmpty())
                QGuiApplication::clipboard()->setText(
                    QDir::toNativeSeparators(tabs_.at(target).document.path()));
        });
        copyPath->setObjectName(QStringLiteral("tabCopyPathAction"));
        copyPath->setEnabled(!tabs_.at(index).document.path().isEmpty());
        menu->addSeparator();
        auto* close = menu->addAction(tr("Close Tab"), this, [this, targetIndex] {
            requestCloseTab(targetIndex());
        });
        close->setObjectName(QStringLiteral("tabCloseAction"));
        const auto closeRange = [this, targetIndex](bool onlyRight) {
            const int target = targetIndex();
            if (target < 0 || isDocumentBusy())
                return;
            std::vector<qintptr> documents;
            for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
                if (onlyRight ? i > target : i != target)
                    documents.push_back(tabs_.at(i).documentHandle);
            }
            requestCloseTabs(std::move(documents), currentTab().documentHandle);
        };
        auto* others = menu->addAction(tr("Close Other Tabs"), this,
                                      [closeRange] { closeRange(false); });
        others->setObjectName(QStringLiteral("tabCloseOthersAction"));
        others->setEnabled(tabs_.size() > 1);
        auto* right = menu->addAction(tr("Close Tabs to the Right"), this,
                                     [closeRange] { closeRange(true); });
        right->setObjectName(QStringLiteral("tabCloseRightAction"));
        right->setEnabled(index + 1 < static_cast<int>(tabs_.size()));
        auto* all = menu->addAction(tr("Close All Tabs"), this, [this] {
            std::vector<qintptr> documents;
            for (const auto& tab : tabs_)
                documents.push_back(tab.documentHandle);
            requestCloseTabs(std::move(documents), 0);
        });
        all->setObjectName(QStringLiteral("tabCloseAllAction"));
    }
    menu->popup(tabBar_->mapToGlobal(position));
}

void MainWindow::requestCloseTabs(std::vector<qintptr> documents, qintptr returnTo)
{
    if (isDocumentBusy() || closeAllTabsInProgress_)
        return;
    // Never retain a pointer to a document this batch will release: Scintilla
    // can reuse its address for a newly opened tab.
    if (std::find(documents.begin(), documents.end(), returnTo) != documents.end())
        returnTo = 0;
    while (!documents.empty()) {
        const qintptr document = documents.back();
        documents.pop_back();
        int index = -1;
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
            if (tabs_.at(i).documentHandle == document) {
                index = i;
                break;
            }
        }
        if (index < 0)
            continue;
        switchToTab(index);
        if (!currentTab().document.isModified()) {
            closeTab(index);
            continue;
        }
        // Clean tabs finish synchronously. Only the existing unsaved-change
        // flow may suspend the batch for a prompt or an asynchronous save.
        // Cancel/save failure destroys the continuation and stops the batch.
        requestAfterUnsavedCheck(
            [this, document, documents = std::move(documents), returnTo]() mutable {
                for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
                    if (tabs_.at(i).documentHandle == document) {
                        closeTab(i);
                        break;
                    }
                }
                requestCloseTabs(std::move(documents), returnTo);
            });
        return;
    }
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        if (tabs_.at(i).documentHandle == returnTo) {
            switchToTab(i);
            break;
        }
    }
}
void MainWindow::switchRelativeTab(int delta)
{
    if (tabs_.size() < 2 || isDocumentBusy()) {
        return;
    }
    const int count = static_cast<int>(tabs_.size());
    switchToTab((currentTabIndex_ + delta + count) % count);
}

void MainWindow::requestCloseTab(int index)
{
    if (isDocumentBusy() || index < 0
        || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    switchToTab(index);
    const qintptr documentHandle = currentTab().documentHandle;
    requestAfterUnsavedCheck([this, documentHandle] {
        const auto found = std::find_if(
            tabs_.cbegin(), tabs_.cend(), [documentHandle](const TabState& tab) {
                return tab.documentHandle == documentHandle;
            });
        if (found != tabs_.cend()) {
            closeTab(static_cast<int>(found - tabs_.cbegin()));
        }
    });
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    const QString closedPath = tabs_.at(index).document.path();
    removeRecoverySnapshot(tabs_.at(index));
    if (tabs_.size() == 1) {
        const sptr_t oldDocument = static_cast<sptr_t>(
            tabs_.front().documentHandle);
        if (!editor_->resetDocument()) {
            return;
        }
        editor_->releaseTabDocument(oldDocument);
        tabs_.front() = TabState{};
        tabs_.front().documentHandle = editor_->retainCurrentDocument();
        tabs_.front().recoveryId = QUuid::createUuid().toString(
            QUuid::WithoutBraces);
        fileChangeMonitor_->unwatchFile(closedPath);
        tabBar_->setTabData(0, QVariant::fromValue(tabs_.front().documentHandle));
        applyLargeFileMode(LargeFileMode::Normal);
        updateWindowTitle();
        updateDocumentStatus();
        if (!quitAfterClosingTabs_) {
            documentHistory_.recordClosedTab(closedPath);
            reopenClosedTabAction_->setEnabled(
                documentHistory_.hasClosedTabs());
        }
        savePersistentSettings();
        return;
    }

    const sptr_t removedDocument = static_cast<sptr_t>(
        tabs_.at(index).documentHandle);
    const bool removingCurrent = index == currentTabIndex_;
    tabs_.erase(tabs_.begin() + index);
    {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->removeTab(index);
    }
    if (removingCurrent) {
        currentTabIndex_ = -1;
        switchToTab(std::min(index, static_cast<int>(tabs_.size()) - 1), true);
    } else if (index < currentTabIndex_) {
        --currentTabIndex_;
        const QSignalBlocker blocker(tabBar_);
        tabBar_->setCurrentIndex(currentTabIndex_);
    }
    editor_->releaseTabDocument(removedDocument);
    fileChangeMonitor_->unwatchFile(closedPath);
    if (!quitAfterClosingTabs_) {
        documentHistory_.recordClosedTab(closedPath);
        reopenClosedTabAction_->setEnabled(documentHistory_.hasClosedTabs());
    }
    savePersistentSettings();
}

void MainWindow::beginCloseAllTabs(bool quitApplication)
{
    if (isDocumentBusy() || closeAllTabsInProgress_) {
        return;
    }
    snapshotCurrentTabView();
    closingSessionPaths_ = sessionTabPaths();
    closeAllTabsInProgress_ = true;
    quitAfterClosingTabs_ = quitApplication;
    continueCloseAllTabs();
}

void MainWindow::continueCloseAllTabs()
{
    if (!closeAllTabsInProgress_ || isDocumentBusy()) {
        return;
    }
    if (tabs_.size() > 1) {
        switchToTab(0);
        requestAfterUnsavedCheck([this] {
            closeTab(0);
            QTimer::singleShot(0, this, &MainWindow::continueCloseAllTabs);
        });
        return;
    }
    if (currentTab().document.isModified()) {
        requestAfterUnsavedCheck([this] {
            closeTab(0);
            QTimer::singleShot(0, this, &MainWindow::continueCloseAllTabs);
        });
        return;
    }

    savePersistentSettings();
    closeAllTabsInProgress_ = false;
    if (quitAfterClosingTabs_) {
        quitAfterClosingTabs_ = false;
        emit applicationQuitAccepted();
    } else {
        closeAfterSave_ = true;
        close();
    }
}

void MainWindow::updateTabBarVisibility()
{
    tabBar_->setVisible(!windowController_->isFrameless()
                        && !windowController_->isMinimalMode());
}

void MainWindow::updateTabTitle(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }
    const TabState& tab = tabs_.at(index);
    const QString displayName = tab.document.isUntitled()
        ? tr("Untitled") : tab.document.displayName();
    tabBar_->setTabText(index, displayName);
    static_cast<DocumentTabBar*>(tabBar_)->setDocumentModified(index, tab.document.isModified());
    QString toolTip = tab.document.path().isEmpty()
        ? tr("Untitled") : QDir::toNativeSeparators(tab.document.path());
    if (tab.externalChange == FileChangeMonitor::Change::Modified) {
        toolTip.append(QStringLiteral("\n") + tr("File changed on disk"));
    } else if (tab.externalChange == FileChangeMonitor::Change::Removed) {
        toolTip.append(QStringLiteral("\n") + tr("File removed from disk"));
    }
    tabBar_->setTabToolTip(index, toolTip);
}

MainWindow::TabState& MainWindow::currentTab()
{
    return tabs_.at(currentTabIndex_);
}

const MainWindow::TabState& MainWindow::currentTab() const
{
    return tabs_.at(currentTabIndex_);
}

bool MainWindow::currentTabIsPristineUntitled() const
{
    return currentTab().document.isUntitled()
        && !currentTab().document.isModified()
        && editor_->isEmpty()
        && editor_->editHistory().isEmpty();
}

int MainWindow::tabIndexForPath(const QString& path) const
{
    const QString absolutePath = QDir::cleanPath(
        QFileInfo(path).absoluteFilePath());
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathCaseSensitivity = Qt::CaseSensitive;
#endif
    for (int index = 0; index < static_cast<int>(tabs_.size()); ++index) {
        if (tabs_.at(index).document.path().compare(
                absolutePath, pathCaseSensitivity) == 0) {
            return index;
        }
    }
    return -1;
}

QStringList MainWindow::sessionTabPaths() const
{
    if (closeAllTabsInProgress_ && !closingSessionPaths_.isEmpty()) {
        return closingSessionPaths_;
    }
    QStringList paths;
    for (const TabState& tab : tabs_) {
        if (!tab.document.isUntitled()) {
            paths.append(tab.document.path());
        }
    }
    return paths;
}

void MainWindow::replaceCurrentTabDocumentHandle()
{
    const sptr_t oldDocument = static_cast<sptr_t>(
        currentTab().documentHandle);
    currentTab().documentHandle = editor_->retainCurrentDocument();
    tabBar_->setTabData(currentTabIndex_,
                        QVariant::fromValue(currentTab().documentHandle));
    editor_->releaseTabDocument(oldDocument);
}

void MainWindow::openFiles(const QStringList& paths)
{
    for (const QString& path : paths) {
        if (!path.trimmed().isEmpty()) {
            pendingOpenPaths_.append(QDir::cleanPath(
                QFileInfo(path).absoluteFilePath()));
        }
    }
    if (crashRecoveryChecked_) {
        processPendingOpenFiles();
    }
}

void MainWindow::handleExternalOpenRequest(const QStringList& paths)
{
    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    raise();
    activateWindow();
    if (paths.isEmpty()) {
        if (isDocumentBusy()) {
            ++pendingNewTabs_;
        } else {
            newDocument();
        }
    } else {
        openFiles(paths);
    }
}

void MainWindow::processPendingOpenFiles()
{
    if (isDocumentBusy()) {
        return;
    }
    if (pendingNewTabs_ > 0) {
        --pendingNewTabs_;
        addBlankTab();
        QTimer::singleShot(0, this, &MainWindow::processPendingOpenFiles);
        return;
    }
    if (pendingOpenPaths_.isEmpty()) {
        return;
    }

    const QString path = pendingOpenPaths_.takeFirst();
    const int existingTab = tabIndexForPath(path);
    if (existingTab >= 0) {
        switchToTab(existingTab);
        QTimer::singleShot(0, this, &MainWindow::processPendingOpenFiles);
        return;
    }
    if (!currentTabIsPristineUntitled() && addBlankTab() < 0) {
        return;
    }
    loadingTabIndex_ = currentTabIndex_;
    if (!fileManager_->openFile(path)) {
        loadingTabIndex_ = -1;
        QMessageBox::warning(this, tr("Open file"),
                             tr("Another file operation is in progress."));
    }
}

void MainWindow::chooseAndOpenFile()
{
    const QString initialDirectory = currentTab().document.isUntitled()
        ? lastDirectory_
        : QFileInfo(currentTab().document.path()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Text File"), initialDirectory,
        tr("Text files (*);;All files (*)"));
    if (!path.isEmpty()) {
        requestOpenFile(path);
    }
}

void MainWindow::requestOpenFile(const QString& path)
{
    openFiles({path});
}

void MainWindow::handleExternalFileChange(
    const QString& path, FileChangeMonitor::Change change)
{
    const int index = tabIndexForPath(path);
    if (index < 0) {
        fileChangeMonitor_->unwatchFile(path);
        return;
    }
    tabs_.at(index).externalChange = change;
    updateTabTitle(index);
    if (index == currentTabIndex_ && !isDocumentBusy()
        && !handlingExternalFileChange_) {
        QTimer::singleShot(0, this,
                           &MainWindow::processCurrentExternalFileChange);
    }
}

void MainWindow::processCurrentExternalFileChange()
{
    if (handlingExternalFileChange_ || isDocumentBusy()
        || currentTabIndex_ < 0 || !currentTab().externalChange.has_value()) {
        return;
    }

    handlingExternalFileChange_ = true;
    const FileChangeMonitor::Change change = *currentTab().externalChange;
    currentTab().externalChange.reset();
    updateTabTitle(currentTabIndex_);
    const QString path = currentTab().document.path();

    if (change == FileChangeMonitor::Change::Modified
        && !currentTab().document.isModified()) {
        startExternalReload(path);
        handlingExternalFileChange_ = false;
        return;
    }

    if (change == FileChangeMonitor::Change::Modified) {
        QMessageBox message(
            QMessageBox::Warning, tr("File changed on disk"),
            tr("%1 was changed by another program. Reloading will discard "
               "your editor changes.").arg(currentTab().document.displayName()),
            QMessageBox::NoButton, this);
        auto* reloadButton = message.addButton(
            tr("Reload from Disk"), QMessageBox::AcceptRole);
        message.addButton(tr("Keep Editor Changes"), QMessageBox::RejectRole);
        message.exec();
        if (message.clickedButton() == reloadButton) {
            startExternalReload(path);
        } else {
            fileChangeMonitor_->refreshFile(path);
            statusBar()->showMessage(tr("Kept editor changes for %1")
                                         .arg(currentTab().document.displayName()),
                                     3000);
        }
    } else {
        QMessageBox message(
            QMessageBox::Warning, tr("File removed from disk"),
            tr("%1 was removed or renamed by another program.")
                .arg(currentTab().document.displayName()),
            QMessageBox::NoButton, this);
        auto* keepButton = message.addButton(
            tr("Keep Open"), QMessageBox::RejectRole);
        auto* closeButton = message.addButton(
            tr("Close Tab"), QMessageBox::DestructiveRole);
        message.exec();
        if (message.clickedButton() == closeButton) {
            requestCloseTab(currentTabIndex_);
        } else if (message.clickedButton() == keepButton) {
            fileChangeMonitor_->refreshFile(path);
        }
    }
    handlingExternalFileChange_ = false;
}

bool MainWindow::startExternalReload(const QString& path)
{
    fileChangeMonitor_->suspendFile(path);
    suspendedFileWatchPath_ = path;
    loadingTabIndex_ = currentTabIndex_;
    if (fileManager_->openFile(path)) {
        statusBar()->showMessage(tr("Reloading externally changed file %1")
                                     .arg(path));
        return true;
    }
    loadingTabIndex_ = -1;
    resumeSuspendedFileWatch();
    return false;
}

void MainWindow::resumeSuspendedFileWatch()
{
    if (suspendedFileWatchPath_.isEmpty()) {
        return;
    }
    fileChangeMonitor_->resumeFile(suspendedFileWatchPath_);
    suspendedFileWatchPath_.clear();
}

void MainWindow::reopenClosedTab()
{
    if (isDocumentBusy()) {
        return;
    }
    const QString path = documentHistory_.takeLastClosedTab();
    reopenClosedTabAction_->setEnabled(documentHistory_.hasClosedTabs());
    if (path.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(
            this, tr("Reopen closed tab"),
            tr("The file no longer exists and cannot be reopened.\n%1")
                .arg(path));
        return;
    }
    requestOpenFile(path);
}

void MainWindow::addRecentFile(const QString& path)
{
    documentHistory_.addRecentFile(path);
    rebuildRecentFilesMenu();
}

void MainWindow::removeRecentFile(const QString& path)
{
    documentHistory_.removeRecentFile(path);
    rebuildRecentFilesMenu();
}

void MainWindow::rebuildRecentFilesMenu()
{
    if (recentFilesMenu_ == nullptr) {
        return;
    }

    recentFilesMenu_->clear();
    const QStringList& recentFiles = documentHistory_.recentFiles();
    if (recentFiles.isEmpty()) {
        auto* emptyAction = recentFilesMenu_->addAction(tr("No Recent Files"));
        emptyAction->setEnabled(false);
        recentFilesMenu_->setEnabled(false);
        return;
    }

    recentFilesMenu_->setEnabled(!isDocumentBusy());
    for (qsizetype index = 0; index < recentFiles.size(); ++index) {
        const QString& path = recentFiles.at(index);
        QString label = QDir::toNativeSeparators(path);
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        if (index < 9) {
            label.prepend(QStringLiteral("&%1 ").arg(index + 1));
        } else {
            label.prepend(QStringLiteral("%1 ").arg(index + 1));
        }
        auto* action = recentFilesMenu_->addAction(label);
        action->setData(path);
        action->setToolTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] {
            if (!QFileInfo::exists(path)) {
                removeRecentFile(path);
                savePersistentSettings();
                QMessageBox::warning(
                    this, tr("Open recent file"),
                    tr("The file no longer exists and was removed from the "
                       "recent files list.\n%1").arg(path));
                return;
            }
            requestOpenFile(path);
        });
    }

    recentFilesMenu_->addSeparator();
    auto* clearAction = recentFilesMenu_->addAction(tr("&Clear Recent Files"));
    clearAction->setObjectName(QStringLiteral("clearRecentFilesAction"));
    connect(clearAction, &QAction::triggered, this, [this] {
        documentHistory_.clearRecentFiles();
        rebuildRecentFilesMenu();
        savePersistentSettings();
    });
}

bool MainWindow::saveDocument()
{
    return currentTab().document.isUntitled()
        ? saveDocumentAs() : startSave(currentTab().document.path());
}

bool MainWindow::saveDocumentAs()
{
    if (isDocumentBusy()) {
        return false;
    }
    const QString path = chooseSavePath();
    return !path.isEmpty() && startSave(path);
}

bool MainWindow::startSave(const QString& path)
{
    if (isDocumentBusy()) {
        return false;
    }
    const bool savingOverCurrentFile = !currentTab().document.isUntitled()
        && tabIndexForPath(path) == currentTabIndex_;
    if (savingOverCurrentFile
        && currentTab().diskStamp != diskStampForPath(path)) {
        QMessageBox message(
            QMessageBox::Warning, tr("File changed on disk"),
            tr("%1 may have changed on disk since it was opened. Saving now "
               "could overwrite another program's changes.")
                .arg(currentTab().document.displayName()),
            QMessageBox::NoButton, this);
        auto* overwriteButton = message.addButton(
            tr("Overwrite File"), QMessageBox::DestructiveRole);
        auto* saveAsButton = message.addButton(
            tr("Save As…"), QMessageBox::ActionRole);
        message.addButton(QMessageBox::Cancel);
        message.exec();
        if (message.clickedButton() == saveAsButton) {
            return saveDocumentAs();
        }
        if (message.clickedButton() != overwriteButton) {
            return false;
        }
    }
    if (savingOverCurrentFile) {
        fileChangeMonitor_->suspendFile(currentTab().document.path());
        suspendedFileWatchPath_ = currentTab().document.path();
    }
    const bool started = LargeFilePolicy::usesLargeDocument(
                             currentTab().largeFileMode)
        ? fileManager_->saveFileStreaming(path, currentTab().document.encoding(),
                                          editor_->documentLength())
        : fileManager_->saveFile(path, editor_->textUtf8(),
                                 currentTab().document.encoding());
    if (!started) {
        resumeSuspendedFileWatch();
        QMessageBox::warning(this, tr("Save file"),
                             tr("Another file operation is in progress."));
        return false;
    }
    statusBar()->showMessage(tr("Saving %1").arg(path));
    return true;
}

std::optional<MainWindow::DiskStamp> MainWindow::diskStampForPath(
    const QString& path)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    DiskStamp stamp{file.size(), file.fileTime(QFileDevice::FileModificationTime)};
    if (!stamp.modifiedAt.isValid()) {
        return std::nullopt;
    }
    if (stamp.size <= maximumContentFingerprintBytes) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 bytesRead = 0;
        while (!file.atEnd()) {
            const QByteArray chunk = file.read(256 * 1024);
            if (chunk.isEmpty()) {
                return std::nullopt;
            }
            bytesRead += chunk.size();
            if (bytesRead > maximumContentFingerprintBytes) {
                return std::nullopt;
            }
            hash.addData(chunk);
        }
        if (bytesRead != stamp.size) {
            return std::nullopt;
        }
        stamp.contentHash = hash.result();
    }
    if (file.size() != stamp.size
        || file.fileTime(QFileDevice::FileModificationTime) != stamp.modifiedAt) {
        return std::nullopt;
    }
    return stamp;
}

void MainWindow::reloadDocument()
{
    if (!currentTab().document.isUntitled()) {
        requestAfterUnsavedCheck(
            [this, path = currentTab().document.path()] {
            fileManager_->openFile(path);
        });
    }
}

void MainWindow::requestAfterUnsavedCheck(std::function<void()> action)
{
    if (isDocumentBusy()) {
        return;
    }
    if (!currentTab().document.isModified()) {
        action();
        return;
    }

    const auto choice = QMessageBox::warning(
        this, tr("Unsaved changes"),
        tr("Save changes to %1?").arg(currentTab().document.isUntitled()
            ? tr("Untitled") : currentTab().document.displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Discard) {
        action();
    } else if (choice == QMessageBox::Save) {
        pendingAfterSave_ = std::move(action);
        if (!saveDocument()) {
            pendingAfterSave_ = {};
            if (closeAllTabsInProgress_) {
                closeAllTabsInProgress_ = false;
                quitAfterClosingTabs_ = false;
                closingSessionPaths_.clear();
            }
        }
    } else if (closeAllTabsInProgress_) {
        closeAllTabsInProgress_ = false;
        quitAfterClosingTabs_ = false;
        closingSessionPaths_.clear();
    }
}

void MainWindow::updateWindowTitle()
{
    updateTabTitle(currentTabIndex_);
    const QString marker = currentTab().document.isModified()
        ? QStringLiteral("*") : QString();
    const QString displayName = currentTab().document.isUntitled()
        ? tr("Untitled") : currentTab().document.displayName();
    setWindowTitle(QStringLiteral("%1%2 — Vinson Editor")
                       .arg(marker, displayName));
}

void MainWindow::updateDocumentStatus()
{
    for (QAction* action : encodingActions_) {
        action->setChecked(action->data().toInt() == static_cast<int>(currentTab().document.encoding()));
    }
    toggleBookmarkAction_->setChecked(
        editor_->hasBookmarkAtLine(editor_->currentOneBasedLine()));
    QStringList fields{encodingName(currentTab().document.encoding()),
                       lineEndingName(currentTab().document.lineEnding()),
                       QLocale().formattedDataSize(
                           currentTab().document.fileSize())};
    const QString modeName = LargeFilePolicy::displayName(
        currentTab().largeFileMode);
    if (!modeName.isEmpty()) {
        fields.append(modeName);
    }
    documentInfoLabel_->setText(fields.join(QStringLiteral(" | ")));
    reloadAction_->setEnabled(!currentTab().document.isUntitled()
                              && !isDocumentBusy());
}

void MainWindow::applyLargeFileMode(LargeFileMode mode)
{
    currentTab().largeFileMode = mode;
    const bool wrapEnabled = LargeFilePolicy::defaultsWordWrapOff(mode)
        ? false : preferredWordWrap_;
    {
        const QSignalBlocker blocker(wrapAction_);
        wrapAction_->setChecked(wrapEnabled);
    }
    editor_->setWordWrapEnabled(wrapEnabled);
    updateDocumentStatus();
}

void MainWindow::restorePersistentSettings()
{
    restoringSettings_ = true;
    const ApplicationSettings settings = settingsManager_->load();
    lastDirectory_ = settings.lastDirectory;
    documentHistory_.setRecentFiles(settings.recentFiles);
    appearancePresets_ = settings.appearancePresets;
    restoreTabsOnStartup_ = settings.restoreTabsOnStartup;
    if (restoreTabsOnStartup_) {
        for (const QString& path : settings.openTabs) {
            if (QFileInfo::exists(path)) {
                pendingOpenPaths_.append(path);
            }
        }
    }
    rebuildRecentFilesMenu();
    rebuildAppearancePresetActions();
    applyShortcuts(settings.shortcuts);
    themeManager_->applyAppearance(settings.appearance);
    preferredWordWrap_ = settings.wordWrap;
    {
        const QSignalBlocker wrapBlocker(wrapAction_);
        const QSignalBlocker lineBlocker(lineNumberAction_);
        wrapAction_->setChecked(settings.wordWrap);
        lineNumberAction_->setChecked(settings.lineNumbers);
    }
    editor_->setWordWrapEnabled(settings.wordWrap);
    editor_->setLineNumbersVisible(settings.lineNumbers);
    windowController_->setAlwaysOnTop(settings.alwaysOnTop);
    windowController_->setFrameless(settings.frameless);
    bossKey_ = settings.bossKey;
    focusShortcut_ = settings.focusShortcut;
    if (!settings.windowGeometry.isEmpty()) {
        restoreGeometry(settings.windowGeometry);
    }
    ensureWindowOnScreen();
    restoringSettings_ = false;
    updateTabBarVisibility();
}

void MainWindow::savePersistentSettings()
{
    if (restoringSettings_ || !persistentStateEnabled_
        || settingsManager_ == nullptr
        || wrapAction_ == nullptr || lineNumberAction_ == nullptr) {
        return;
    }
    ApplicationSettings settings;
    settings.appearance = themeManager_->appearance();
    settings.appearancePresets = appearancePresets_;
    settings.shortcuts = shortcuts();
    settings.windowGeometry = windowController_->persistableGeometry();
    settings.lastDirectory = lastDirectory_;
    settings.recentFiles = documentHistory_.recentFiles();
    settings.openTabs = sessionTabPaths();
    settings.restoreTabsOnStartup = restoreTabsOnStartup_;
    settings.wordWrap = preferredWordWrap_;
    settings.lineNumbers = lineNumberAction_->isChecked();
    settings.alwaysOnTop = windowController_->isAlwaysOnTop();
    settings.frameless = windowController_->persistableFrameless();
    settings.bossKey = bossKey_;
    settings.focusShortcut = focusShortcut_;
    if (!settingsManager_->save(settings)) {
        qWarning() << "Could not persist settings to"
                   << settingsManager_->fileName();
    }
}

void MainWindow::adjustBackgroundAlpha(int delta)
{
    Appearance appearance = themeManager_->appearance();
    const int alpha = std::clamp(
        appearance.backgroundColor.alpha() + delta, 0, 255);
    if (alpha == appearance.backgroundColor.alpha()) {
        return;
    }
    appearance.backgroundColor.setAlpha(alpha);
    themeManager_->applyAppearance(appearance);
    savePersistentSettings();
    statusBar()->showMessage(tr("Background opacity: %1 / 255").arg(alpha), 1500);
}

void MainWindow::adjustFontSize(int steps)
{
    if (steps == 0) {
        return;
    }

    Appearance appearance = themeManager_->appearance();
    const qreal currentSize = appearance.font.pointSizeF();
    const qreal pointSize = std::clamp(
        currentSize + static_cast<qreal>(steps), 6.0, 72.0);
    if (qFuzzyCompare(pointSize, currentSize)) {
        return;
    }
    appearance.font.setPointSizeF(pointSize);
    themeManager_->applyAppearance(appearance);
    savePersistentSettings();
    statusBar()->showMessage(
        tr("Font size: %1 pt").arg(pointSize, 0, 'f', 1), 1500);
}

void MainWindow::ensureWindowOnScreen()
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    const QRect restoredFrame = frameGeometry();
    const bool intersectsScreen = std::any_of(
        screens.cbegin(), screens.cend(), [&restoredFrame](const QScreen* screen) {
            if (screen == nullptr) {
                return false;
            }
            const QRect intersection =
                screen->availableGeometry().intersected(restoredFrame);
            return intersection.width() >= 64 && intersection.height() >= 32;
        });
    if (intersectsScreen) {
        return;
    }

    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary == nullptr) {
        return;
    }
    const QRect available = primary->availableGeometry();
    const QSize safeSize = size().boundedTo(available.size());
    resize(safeSize);
    move(available.center() - QPoint(width() / 2, height() / 2));
}

bool MainWindow::isDocumentBusy() const
{
    return fileManager_->isBusy() || searchController_->isSearching();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (searchController_->isSearching()
        && (event->type() == QEvent::ShortcutOverride
            || event->type() == QEvent::KeyPress)) {
        const auto* widget = qobject_cast<QWidget*>(watched);
        const auto* key = static_cast<QKeyEvent*>(event);
        if (widget && widget->window() == this && key->key() == Qt::Key_Escape) {
            event->accept();
            if (event->type() == QEvent::KeyPress) {
                searchController_->cancelSearch();
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateBusyUi()
{
    const bool busy = isDocumentBusy();
    const bool searching = searchController_->isSearching();
    newAction_->setEnabled(!busy);
    openAction_->setEnabled(!busy);
    recentFilesMenu_->setEnabled(
        !busy && !documentHistory_.recentFiles().isEmpty());
    reopenClosedTabAction_->setEnabled(
        !busy && documentHistory_.hasClosedTabs());
    saveAction_->setEnabled(!busy);
    saveAsAction_->setEnabled(!busy);
    reloadAction_->setEnabled(!busy && !currentTab().document.isUntitled());
    for (QAction* action : encodingActions_) action->setEnabled(!busy);
    for (QAction* action : lineEndingActions_) action->setEnabled(!busy);
    findAction_->setEnabled(!busy);
    replaceAction_->setEnabled(!busy);
    findNextAction_->setEnabled(!busy);
    findPreviousAction_->setEnabled(!busy);
    goToLineAction_->setEnabled(!busy);
    for (QAction* action : bookmarkActions_) {
        action->setEnabled(!busy);
    }
    findReplaceWidget_->setEnabled(!fileManager_->isBusy());
    tabBar_->setEnabled(!busy);
    editHistoryWidget_->setEnabled(!busy);
    for (QAction* action : findChildren<QAction*>()) {
        const QString id = action->property("shortcutId").toString();
        if (id == QLatin1String("undo") || id == QLatin1String("redo")
            || id == QLatin1String("cut") || id == QLatin1String("copy")
            || id == QLatin1String("paste") || id == QLatin1String("selectAll")) {
            action->setEnabled(!busy);
        }
    }
    progressBar_->setVisible(busy);
    cancelOperationButton_->setVisible(busy);
    if (searching || fileManager_->operation() == FileManager::Operation::Saving) {
        editor_->setEnabled(false);
        progressBar_->setRange(0, searching ? 100 : 0);
    } else if (!busy && !loadReplacedDocument_) {
        editor_->setEnabled(true);
    }
}

void MainWindow::handleLoadFailureState()
{
    editor_->setEnabled(true);
    if (!loadReplacedDocument_) {
        return;
    }
    const QString failedPath = currentTab().document.path();
    editor_->completeFileLoad(LineEnding::None);
    currentTab().document.reset();
    currentTab().insertionLineEnding = LineEnding::None;
    currentTab().externalChange.reset();
    fileChangeMonitor_->unwatchFile(failedPath);
    currentTab().document.setModified(!editor_->isEmpty());
    loadReplacedDocument_ = false;
    loadingTabIndex_ = -1;
    updateWindowTitle();
    updateDocumentStatus();
}

QString MainWindow::chooseSavePath()
{
    const QString suggested = currentTab().document.isUntitled()
        ? QDir(lastDirectory_).filePath(QStringLiteral("Untitled.txt"))
        : currentTab().document.path();
    return QFileDialog::getSaveFileName(
        this, tr("Save Text File"), suggested,
        tr("Text files (*.txt);;All files (*)"));
}

} // namespace vinson
