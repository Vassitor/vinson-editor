#pragma once

#include <QApplication>
#include <QTranslator>

namespace vinson {

class Application final : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);

    int run();

private:
    QTranslator translator_;
};

} // namespace vinson
