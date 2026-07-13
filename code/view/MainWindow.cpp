#include "MainWindow.h"
#include "GameWidget.h"

#include <QApplication>
#include <QScreen>

#include <utility>

namespace alleyfist {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Alley Fist");
    resize(960, 540);

    if (auto* screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move((avail.width() - width()) / 2, (avail.height() - height()) / 2);
    }

    setMinimumSize(480, 270);

    auto* widget = new GameWidget(this);
    m_gameWidget = widget;
    setCentralWidget(widget);
}

void MainWindow::set_game_state(const viewmodel::GameViewState* state) noexcept
{
    m_gameWidget->set_game_state(state);
}

void MainWindow::set_tick_command(std::function<void(float, std::uint64_t)> command)
{
    m_gameWidget->set_tick_command(std::move(command));
}

void MainWindow::set_move_left_command(std::function<void(bool)> command)
{
    m_gameWidget->set_move_left_command(std::move(command));
}

void MainWindow::set_move_right_command(std::function<void(bool)> command)
{
    m_gameWidget->set_move_right_command(std::move(command));
}

void MainWindow::set_move_up_command(std::function<void(bool)> command)
{
    m_gameWidget->set_move_up_command(std::move(command));
}

void MainWindow::set_move_down_command(std::function<void(bool)> command)
{
    m_gameWidget->set_move_down_command(std::move(command));
}

void MainWindow::set_primary_action_command(std::function<void()> command)
{
    m_gameWidget->set_primary_action_command(std::move(command));
}

void MainWindow::set_secondary_action_command(std::function<void()> command)
{
    m_gameWidget->set_secondary_action_command(std::move(command));
}

void MainWindow::set_state_toggle_command(std::function<void()> command)
{
    m_gameWidget->set_state_toggle_command(std::move(command));
}

void MainWindow::set_reset_command(std::function<void()> command)
{
    m_gameWidget->set_reset_command(std::move(command));
}

void MainWindow::set_confirm_command(std::function<void()> command)
{
    m_gameWidget->set_confirm_command(std::move(command));
}

void MainWindow::set_pause_command(std::function<void()> command)
{
    m_gameWidget->set_pause_command(std::move(command));
}

EventNotification MainWindow::get_notification()
{
    return [this](std::uint32_t) {
        if (m_gameWidget != nullptr) {
            m_gameWidget->update();
        }
    };
}

GameWidget& MainWindow::get_game_widget() noexcept
{
    return *m_gameWidget;
}

} // namespace alleyfist
