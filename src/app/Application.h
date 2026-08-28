#pragma once

#include <QApplication>

namespace vinson {

class Application final : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);

    int run();
};

} // namespace vinson
