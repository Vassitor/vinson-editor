#pragma once

#include "plugins/PluginManager.h"
#include <QDialog>
#include <QMap>

class QKeySequenceEdit;

namespace vinson {

class PluginFieldForm final : public QWidget
{
public:
    PluginFieldForm(const QVector<PluginField>& fields, const QVariantMap& values,
                    QWidget* parent = nullptr);
    [[nodiscard]] QVariantMap values() const;
    void setValues(const QVariantMap& values);
private:
    QVector<PluginField> fields_;
    QMap<QString, QWidget*> controls_;
};

class PluginParametersDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit PluginParametersDialog(const PluginCommand& command, QWidget* parent = nullptr);
    [[nodiscard]] QVariantMap values() const;
private:
    PluginFieldForm* form_;
};

class PluginConfigurationDialog final : public QDialog
{
    Q_OBJECT
public:
    PluginConfigurationDialog(PluginManager* manager, const EditorPlugin& plugin,
                              QWidget* parent = nullptr);
private:
    PluginManager* manager_;
    EditorPlugin plugin_;
    PluginFieldForm* form_;
    QMap<QString, QKeySequenceEdit*> shortcuts_;
};
} // namespace vinson
