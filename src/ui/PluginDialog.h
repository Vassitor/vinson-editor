#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;

namespace vinson {
class PluginManager;

class PluginDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit PluginDialog(PluginManager* manager, QWidget* parent = nullptr);
    void importPlugin(const QString& path);
private:
    void refresh();
    void updateSelection();
    PluginManager* manager_;
    QTableWidget* table_;
    QLabel* details_;
    QPushButton* import_;
    QPushButton* example_;
    QPushButton* toggle_;
    QPushButton* remove_;
    QPushButton* reload_;
    QPushButton* configure_;
};
} // namespace vinson
