#pragma once

#include <QApplication>
#include <QTranslator>

namespace vinson {

class SingleInstance;

class Application final : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);

    int run();

private:
    [[nodiscard]] QStringList startupFilePaths() const;

    QTranslator applicationTranslator_;
    QTranslator qtTranslator_;
    SingleInstance* singleInstance_ = nullptr;
};

} // namespace vinson
