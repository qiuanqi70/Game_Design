#include "GameApplication.h"

#include "../view/MainWindow.h"
#include "../viewmodel/GameViewModel.h"

#include <QApplication>
#include <QCoreApplication>

namespace alleyfist {

// App 层是组合根：它可以创建具体 View 和具体 ViewModel，
// 并像老师 Plane 示例一样集中完成 properties / commands / notification 绑定。
struct GameApplication::Impl {
    QApplication qtApp;
    viewmodel::GameViewModel viewModel;
    MainWindow mainWindow;

    Impl(int& argc, char** argv)
        : qtApp(argc, argv)
        , viewModel()
        , mainWindow()
    {
        QCoreApplication::setApplicationName("Alley Fist");
        QCoreApplication::setApplicationVersion("0.1.0");
        QCoreApplication::setOrganizationName("AlleyFist");

        // binding

        // properties
        mainWindow.set_game_state(viewModel.get_game_state());

        // commands
        mainWindow.set_tick_command(viewModel.get_tick_command());
        mainWindow.set_move_left_command(viewModel.get_move_left_command());
        mainWindow.set_move_right_command(viewModel.get_move_right_command());
        mainWindow.set_move_up_command(viewModel.get_move_up_command());
        mainWindow.set_move_down_command(viewModel.get_move_down_command());
        mainWindow.set_primary_action_command(viewModel.get_primary_action_command());
        mainWindow.set_secondary_action_command(viewModel.get_secondary_action_command());
        mainWindow.set_state_toggle_command(viewModel.get_state_toggle_command());
        mainWindow.set_reset_command(viewModel.get_reset_command());
        mainWindow.set_confirm_command(viewModel.get_confirm_command());
        mainWindow.set_pause_command(viewModel.get_pause_command());

        // notification
        viewModel.add_notification(mainWindow.get_notification());
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
