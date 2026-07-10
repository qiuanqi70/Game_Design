#include "GameApplication.h"

#include "../view/MainWindow.h"
#include "../viewmodel/GameViewModel.h"

#include <QApplication>
#include <QCoreApplication>

namespace alleyfist {

// App 层是组合根：它可以创建具体 View 和具体 ViewModel，
// 但绑定细节交给 MainWindow::bind，不直接操作 GameWidget 或监听 VM 的具体信号。
struct GameApplication::Impl {
    QApplication qtApp;
    GameViewModel viewModel;
    MainWindow mainWindow;

    Impl(int& argc, char** argv)
        : qtApp(argc, argv)
        , viewModel()
        , mainWindow()
    {
        QCoreApplication::setApplicationName("Alley Fist");
        QCoreApplication::setApplicationVersion("0.1.0");
        QCoreApplication::setOrganizationName("AlleyFist");

        mainWindow.bind(viewModel, viewModel);
    }

    int run()
    {
        mainWindow.show();
        return qtApp.exec();
    }
};

GameApplication::GameApplication(int& argc, char** argv)
    : m_impl(std::make_unique<Impl>(argc, argv))
{}

GameApplication::~GameApplication() = default;

int GameApplication::run()
{
    return m_impl->run();
}

} // namespace alleyfist
