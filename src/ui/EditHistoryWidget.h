#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QPaintEvent;
class QPushButton;
class QTimer;
class QDockWidget;

namespace vinson {

struct Appearance;
class EditorWidget;

class EditHistoryWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit EditHistoryWidget(EditorWidget* editor,
                               QWidget* parent = nullptr);
    void applyAppearance(const Appearance& appearance);

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void scheduleRefresh();
    void restoreSelectedEntry();

    EditorWidget* editor_ = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QPushButton* restoreButton_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
};

} // namespace vinson

void applyEditHistoryDockAppearance(QDockWidget* dock,
                                    const vinson::Appearance& appearance,
                                    bool floating);
