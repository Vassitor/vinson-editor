#include "ui/PluginOptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vinson {
namespace {
QScrollArea* scrollPage(QWidget* contents, QWidget* parent)
{
    auto* area = new QScrollArea(parent);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(contents);
    return area;
}
}

PluginFieldForm::PluginFieldForm(const QVector<PluginField>& fields, const QVariantMap& values,
                                 QWidget* parent)
    : QWidget(parent), fields_(fields)
{
    auto* layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    for (const auto& field : fields_) {
        QWidget* control;
        if (field.type == QLatin1String("boolean")) control = new QCheckBox(this);
        else if (field.type == QLatin1String("integer")) {
            auto* spin = new QSpinBox(this);
            spin->setRange(field.minimum, field.maximum);
            control = spin;
        } else if (field.type == QLatin1String("choice")) {
            auto* combo = new QComboBox(this);
            combo->addItems(field.choices);
            control = combo;
        } else {
            auto* edit = new QLineEdit(this);
            edit->setMaxLength(field.maximumLength);
            control = edit;
        }
        control->setObjectName(QStringLiteral("pluginField.%1").arg(field.id));
        controls_.insert(field.id, control);
        auto* label = new QLabel(field.label + QStringLiteral(":"), this);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        layout->addRow(label, control);
    }
    setValues(values);
}

QVariantMap PluginFieldForm::values() const
{
    QVariantMap result;
    for (const auto& field : fields_) {
        auto* control = controls_.value(field.id);
        if (auto* check = qobject_cast<QCheckBox*>(control)) result.insert(field.id, check->isChecked());
        else if (auto* spin = qobject_cast<QSpinBox*>(control)) result.insert(field.id, spin->value());
        else if (auto* combo = qobject_cast<QComboBox*>(control)) result.insert(field.id, combo->currentText());
        else result.insert(field.id, qobject_cast<QLineEdit*>(control)->text());
    }
    return result;
}

void PluginFieldForm::setValues(const QVariantMap& values)
{
    for (const auto& field : fields_) {
        const auto value = values.value(field.id, field.defaultValue);
        auto* control = controls_.value(field.id);
        if (auto* check = qobject_cast<QCheckBox*>(control)) check->setChecked(value.toBool());
        else if (auto* spin = qobject_cast<QSpinBox*>(control)) spin->setValue(value.toInt());
        else if (auto* combo = qobject_cast<QComboBox*>(control)) combo->setCurrentText(value.toString());
        else qobject_cast<QLineEdit*>(control)->setText(value.toString());
    }
}

PluginParametersDialog::PluginParametersDialog(const PluginCommand& command, QWidget* parent)
    : QDialog(parent), form_(new PluginFieldForm(command.parameters,
        PluginManager::defaults(command.parameters), this))
{
    setObjectName(QStringLiteral("pluginParametersDialog"));
    setWindowTitle(command.title);
    resize(460, 320);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Set the options for this run."), this));
    layout->addWidget(scrollPage(form_, this), 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QVariantMap PluginParametersDialog::values() const { return form_->values(); }

PluginConfigurationDialog::PluginConfigurationDialog(PluginManager* manager, const EditorPlugin& plugin,
                                                       QWidget* parent)
    : QDialog(parent), manager_(manager), plugin_(plugin),
      form_(new PluginFieldForm(plugin.settings, plugin.values, this))
{
    setObjectName(QStringLiteral("pluginConfigurationDialog"));
    setWindowTitle(tr("Plugin Settings: %1").arg(plugin.name));
    resize(600, 450);
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    auto* settingsPage = new QWidget(tabs);
    auto* settingsLayout = new QVBoxLayout(settingsPage);
    if (plugin.settings.isEmpty()) settingsLayout->addWidget(new QLabel(tr("This plugin has no settings."), settingsPage));
    settingsLayout->addWidget(form_);
    settingsLayout->addStretch();
    tabs->addTab(scrollPage(settingsPage, tabs), tr("Settings"));
    auto* shortcutsPage = new QWidget(tabs);
    auto* shortcutsForm = new QFormLayout(shortcutsPage);
    shortcutsForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto* explanation = new QLabel(tr("Use one key combination per command. Clear it to disable the shortcut. "
        "Shortcuts must not conflict with editor commands, styles or other enabled plugins."), shortcutsPage);
    explanation->setWordWrap(true);
    shortcutsForm->addRow(explanation);
    for (const auto& command : plugin.commands) {
        auto* edit = new QKeySequenceEdit(command.shortcut, shortcutsPage);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        edit->setObjectName(QStringLiteral("pluginShortcut.%1").arg(command.id));
        shortcuts_.insert(command.id, edit);
        shortcutsForm->addRow(command.title, edit);
    }
    tabs->addTab(scrollPage(shortcutsPage, tabs), tr("Command Shortcuts"));
    layout->addWidget(tabs, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
        | QDialogButtonBox::RestoreDefaults, this);
    layout->addWidget(buttons);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        form_->setValues(PluginManager::defaults(plugin_.settings));
        for (const auto& command : plugin_.commands) shortcuts_.value(command.id)->setKeySequence(command.defaultShortcut);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QMap<QString, QKeySequence> bindings;
        for (auto it = shortcuts_.cbegin(); it != shortcuts_.cend(); ++it)
            bindings.insert(it.key(), it.value()->keySequence());
        QString error;
        if (!manager_->configure(plugin_.id, form_->values(), bindings, &error)) {
            QMessageBox::warning(this, tr("Plugin Settings"), error);
            return;
        }
        accept();
    });
}
} // namespace vinson
