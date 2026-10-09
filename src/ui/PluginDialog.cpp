#include "ui/PluginDialog.h"
#include "plugins/PluginManager.h"
#include "ui/PluginOptionsDialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace vinson {
PluginDialog::PluginDialog(PluginManager* manager, QWidget* parent)
    : QDialog(parent), manager_(manager), table_(new QTableWidget(this)),
      details_(new QLabel(this))
{
    setObjectName(QStringLiteral("pluginDialog"));
    setWindowTitle(tr("Manage Plugins"));
    resize(820, 540);
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("pluginTabs"));
    layout->addWidget(tabs, 1);
    auto* manage = new QWidget(tabs);
    auto* manageLayout = new QVBoxLayout(manage);
    tabs->addTab(manage, tr("Installed Plugins"));
    auto* description = new QLabel(tr("Import a .vinson-plugin package to add commands to the Plugins menu. "
        "Only install plugins from authors you trust. Commands can read and change the current document."), this);
    description->setWordWrap(true);
    manageLayout->addWidget(description);
    table_->setObjectName(QStringLiteral("pluginTable"));
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({tr("Plugin"), tr("Version"), tr("Status")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    manageLayout->addWidget(table_, 1);
    details_->setTextFormat(Qt::PlainText);
    details_->setWordWrap(true);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    manageLayout->addWidget(details_);
    auto* buttons = new QHBoxLayout;
    import_ = new QPushButton(tr("Import Plugin…"), this);
    import_->setObjectName(QStringLiteral("importPluginButton"));
    example_ = new QPushButton(tr("Install Example"), this);
    toggle_ = new QPushButton(tr("Enable"), this);
    remove_ = new QPushButton(tr("Uninstall"), this);
    reload_ = new QPushButton(tr("Reload"), this);
    configure_ = new QPushButton(tr("Plugin Settings…"), this);
    configure_->setObjectName(QStringLiteral("configurePluginButton"));
    for (auto* button : {import_, example_, toggle_, configure_, remove_, reload_}) buttons->addWidget(button);
    manageLayout->addLayout(buttons);
    auto* guide = new QWidget(tabs);
    auto* guideLayout = new QVBoxLayout(guide);
    auto* languages = new QComboBox(guide);
    languages->setObjectName(QStringLiteral("pluginGuideLanguage"));
    languages->addItem(QStringLiteral("简体中文"), QStringLiteral("PLUGINS.zh-CN.md"));
    languages->addItem(QStringLiteral("English"), QStringLiteral("PLUGINS.md"));
    guideLayout->addWidget(languages);
    auto* preview = new QTextBrowser(guide);
    preview->setObjectName(QStringLiteral("pluginGuidePreview"));
    preview->setOpenLinks(false);
    preview->setOpenExternalLinks(false);
    guideLayout->addWidget(preview, 1);
    auto showGuide = [languages, preview] {
        QFile file(QStringLiteral(":/plugin-docs/") + languages->currentData().toString());
        if (file.open(QIODevice::ReadOnly)) preview->setMarkdown(QString::fromUtf8(file.readAll()));
        else preview->setPlainText(tr("Could not load the bundled developer guide."));
        preview->moveCursor(QTextCursor::Start);
    };
    connect(languages, &QComboBox::currentIndexChanged, this, showGuide);
    connect(preview, &QTextBrowser::anchorClicked, this, [languages, preview](const QUrl& url) {
        const QString path = url.path();
        if (path == QLatin1String("PLUGINS.md")) languages->setCurrentIndex(1);
        else if (path == QLatin1String("PLUGINS.zh-CN.md")) languages->setCurrentIndex(0);
        else if (path.isEmpty() && url.hasFragment()) preview->scrollToAnchor(url.fragment());
        else if (path.endsWith(QLatin1String("text-tools.vinson-plugin"))) {
            QFile file(QStringLiteral(":/plugins/text-tools.vinson-plugin"));
            if (file.open(QIODevice::ReadOnly)) {
                auto* source = new QDialog(preview);
                source->setAttribute(Qt::WA_DeleteOnClose);
                source->setWindowTitle(tr("Example Plugin Source"));
                source->resize(720, 520);
                auto* sourceLayout = new QVBoxLayout(source);
                auto* text = new QTextBrowser(source);
                text->setObjectName(QStringLiteral("pluginExampleSource"));
                text->setPlainText(QString::fromUtf8(file.readAll()));
                sourceLayout->addWidget(text);
                auto* close = new QDialogButtonBox(QDialogButtonBox::Close, source);
                connect(close, &QDialogButtonBox::rejected, source, &QDialog::reject);
                sourceLayout->addWidget(close);
                source->show();
            }
        }
    });
    // Use the translated UI language initially; both guides remain available offline.
    languages->setCurrentIndex(tr("Developer Guide") == QStringLiteral("开发文档") ? 0 : 1);
    showGuide();
    tabs->addTab(guide, tr("Developer Guide"));
    auto* footer = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* folder = footer->addButton(tr("Open Plugin Folder"), QDialogButtonBox::ActionRole);
    layout->addWidget(footer);
    connect(footer, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(folder, &QPushButton::clicked, this, [this] {
        if (!QDir().mkpath(manager_->directory())
            || !QDesktopServices::openUrl(QUrl::fromLocalFile(manager_->directory())))
            QMessageBox::warning(this, tr("Plugins"), tr("Could not open the plugin folder."));
    });
    connect(import_, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, tr("Import Plugin"), {},
            tr("Vinson plugins (*.vinson-plugin)"));
        if (!path.isEmpty()) importPlugin(path);
    });
    connect(example_, &QPushButton::clicked, this, [this] {
        importPlugin(QStringLiteral(":/plugins/text-tools.vinson-plugin"));
    });
    connect(reload_, &QPushButton::clicked, manager_, &PluginManager::reload);
    connect(configure_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0 || row >= manager_->plugins().size()) return;
        PluginConfigurationDialog dialog(manager_, manager_->plugins().at(row), this);
        dialog.exec();
    });
    connect(toggle_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0 || row >= manager_->plugins().size()) return;
        const auto plugin = manager_->plugins().at(row);
        QString error;
        if (!manager_->setEnabled(plugin.id, !plugin.enabled, &error))
            QMessageBox::warning(this, tr("Plugins"), error);
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0 || row >= manager_->plugins().size()) return;
        const auto plugin = manager_->plugins().at(row);
        if (QMessageBox::question(this, tr("Uninstall Plugin"),
                tr("Uninstall %1?").arg(plugin.name)) != QMessageBox::Yes) return;
        QString error;
        if (!manager_->uninstall(plugin.filePath, &error))
            QMessageBox::warning(this, tr("Plugins"), error);
    });
    connect(table_, &QTableWidget::itemSelectionChanged, this, &PluginDialog::updateSelection);
    connect(manager_, &PluginManager::pluginsChanged, this, &PluginDialog::refresh);
    connect(manager_, &PluginManager::runningChanged, this, [this] { updateSelection(); });
    refresh();
}

void PluginDialog::importPlugin(const QString& path)
{
    QString error;
    const auto incoming = PluginManager::inspect(path);
    if (!incoming.error.isEmpty()) {
        QMessageBox::warning(this, tr("Import Plugin"), incoming.error);
        return;
    }
    bool replace = false;
    for (const auto& installed : manager_->plugins()) {
        if (installed.id != incoming.id) continue;
        if (QMessageBox::question(this, tr("Update Plugin"),
                tr("Replace %1 (%2) with version %3? Settings and enabled state will be kept.")
                    .arg(installed.name, installed.version, incoming.version)) != QMessageBox::Yes) return;
        replace = true;
        break;
    }
    if (!incoming.risks.isEmpty()) {
        QDialog review(this);
        review.setObjectName(QStringLiteral("pluginSecurityReview"));
        review.setWindowTitle(tr("Plugin Security Review"));
        review.resize(760, 560);
        auto* layout = new QVBoxLayout(&review);
        auto* heading = new QLabel(tr("Potential risks were found in %1 (%2). Review them before installing.")
            .arg(incoming.name, incoming.version), &review);
        heading->setTextFormat(Qt::PlainText);
        heading->setWordWrap(true);
        layout->addWidget(heading);
        auto* findings = new QTextBrowser(&review);
        findings->setObjectName(QStringLiteral("pluginSecurityFindings"));
        QStringList report;
        for (const auto& risk : incoming.risks) {
            report << tr("%1 — %2 (%3), script line %4\n%5\nCode: %6")
                .arg(risk.category, risk.commandTitle, risk.commandId).arg(risk.line)
                .arg(risk.explanation, risk.evidence);
        }
        findings->setPlainText(report.join(QStringLiteral("\n\n")));
        layout->addWidget(findings, 1);
        auto* countLimit = new QLabel(tr("Up to 64 findings per command are shown."), &review);
        countLimit->setWordWrap(true);
        layout->addWidget(countLimit);
        auto* limits = new QLabel(tr("This static check does not execute commands and cannot detect every risk. "
            "Plugins can read or change the supplied text. The current runtime exposes no file, network or system APIs, "
            "but it runs inside the editor process and cannot guarantee protection from memory exhaustion. "
            "Only continue if you trust the author and accept these risks."), &review);
        limits->setWordWrap(true);
        layout->addWidget(limits);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &review);
        auto* proceed = buttons->addButton(tr("Install Anyway"), QDialogButtonBox::AcceptRole);
        proceed->setObjectName(QStringLiteral("confirmRiskyPluginButton"));
        proceed->setAutoDefault(false);
        proceed->setDefault(false);
        buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
        connect(buttons, &QDialogButtonBox::accepted, &review, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &review, &QDialog::reject);
        layout->addWidget(buttons);
        if (review.exec() != QDialog::Accepted) return;
    }
    if (!manager_->install(path, &error, replace, incoming.packageHash))
        QMessageBox::warning(this, tr("Import Plugin"), error);
}

void PluginDialog::refresh()
{
    QString selected;
    if (auto* item = table_->item(table_->currentRow(), 0)) selected = item->data(Qt::UserRole).toString();
    table_->setRowCount(0);
    for (const auto& plugin : manager_->plugins()) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto* name = new QTableWidgetItem(plugin.name);
        name->setData(Qt::UserRole, plugin.filePath);
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, new QTableWidgetItem(plugin.version));
        table_->setItem(row, 2, new QTableWidgetItem(!plugin.error.isEmpty() ? tr("Error")
            : plugin.enabled ? tr("Enabled") : tr("Disabled")));
        if (plugin.filePath == selected) table_->selectRow(row);
    }
    if (table_->currentRow() < 0 && table_->rowCount() > 0) table_->selectRow(0);
    updateSelection();
}

void PluginDialog::updateSelection()
{
    const bool idle = !manager_->isRunning();
    import_->setEnabled(idle);
    example_->setEnabled(idle);
    reload_->setEnabled(idle);
    const int row = table_->currentRow();
    const bool selected = row >= 0 && row < manager_->plugins().size();
    toggle_->setEnabled(idle && selected && manager_->plugins().at(row).error.isEmpty());
    remove_->setEnabled(idle && selected);
    configure_->setEnabled(idle && selected && manager_->plugins().at(row).error.isEmpty());
    if (!selected) {
        details_->setText(tr("No plugins installed. Import a package or install the example to get started."));
        return;
    }
    const auto& plugin = manager_->plugins().at(row);
    toggle_->setText(plugin.enabled ? tr("Disable") : tr("Enable"));
    details_->setText(plugin.error.isEmpty()
        ? tr("%1\nID: %2 · %3 command(s)\n%4").arg(plugin.description, plugin.id)
            .arg(plugin.commands.size()).arg(plugin.filePath)
        : tr("%1\n%2").arg(plugin.error, plugin.filePath));
}
} // namespace vinson
