#pragma once

#include "search/SearchTypes.h"

#include <QFrame>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace vinson {

class FindReplaceWidget final : public QFrame
{
    Q_OBJECT

public:
    explicit FindReplaceWidget(QWidget* parent = nullptr);

    void open(bool replaceMode, const QString& initialText = {});
    [[nodiscard]] bool isReplaceMode() const noexcept;
    [[nodiscard]] QString findText() const;
    [[nodiscard]] QString replacementText() const;
    [[nodiscard]] SearchOptions options() const;
    void setResult(SearchResult result, const QString& message);

signals:
    void searchTextChanged(QString text);
    void replacementTextChanged(QString text);
    void optionsChanged(vinson::SearchOptions options);
    void findNextRequested();
    void findPreviousRequested();
    void replaceRequested();
    void replaceAllRequested();
    void closeRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void publishOptions();

    QLineEdit* findEdit_ = nullptr;
    QLineEdit* replacementEdit_ = nullptr;
    QCheckBox* matchCaseCheck_ = nullptr;
    QCheckBox* wholeWordCheck_ = nullptr;
    QCheckBox* wrapAroundCheck_ = nullptr;
    QLabel* resultLabel_ = nullptr;
    QWidget* replacementRow_ = nullptr;
    bool replaceMode_ = false;
};

} // namespace vinson

Q_DECLARE_METATYPE(vinson::SearchOptions)
