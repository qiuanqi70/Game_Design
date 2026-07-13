#pragma once

#include "../common/game_state.h"
#include "../common/notification.h"

#include <QMainWindow>

#include <cstdint>
#include <functional>

namespace alleyfist {

class GameWidget;

/// @brief 游戏主窗口，承载 GameWidget 作为中心控件。
///
/// MainWindow 是 View 层对外的入口之一。
/// App 层创建 MainWindow 后调用 show() 即可显示游戏窗口。
/// App 层负责把 ViewModel 暴露的属性、命令和通知绑定到这里。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // properties
    void set_game_state(const viewmodel::GameViewState* state) noexcept;

    // commands
    void set_tick_command(std::function<void(float, std::uint64_t)> command);
    void set_move_left_command(std::function<void(bool)> command);
    void set_move_right_command(std::function<void(bool)> command);
    void set_move_up_command(std::function<void(bool)> command);
    void set_move_down_command(std::function<void(bool)> command);
    void set_primary_action_command(std::function<void()> command);
    void set_secondary_action_command(std::function<void()> command);
    void set_state_toggle_command(std::function<void()> command);
    void set_reset_command(std::function<void()> command);
    void set_confirm_command(std::function<void()> command);
    void set_pause_command(std::function<void()> command);

    // notification
    EventNotification get_notification();

    // methods
    GameWidget& get_game_widget() noexcept;

private:
    GameWidget* m_gameWidget = nullptr;
};

} // namespace alleyfist
