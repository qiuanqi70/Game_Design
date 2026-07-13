#pragma once

#include "../common/game_state.h"

#include "InputDefs.h"

#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <utility>

namespace alleyfist {

/// @brief 游戏渲染画布，负责键盘输入、定时器驱动和所有画面绘制。
///
/// GameWidget 只保存 ViewModel 暴露出来的只读显示属性指针，
/// 不直接操作游戏数据。
/// 它把 Qt 键盘事件翻译成已绑定命令，把只读属性翻译成像素绘制。
///
/// 键盘映射和 MovementIntent 聚合属于 View 层内部实现，
/// 换输入设备（手柄 / 触屏）只需改本文件的按键处理。
class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    /// 外部（App 层）调用，传入 ViewModel 持有的只读显示属性。
    void set_game_state(const viewmodel::GameViewState* state) noexcept;

    /// 设置游戏循环是否运行。
    void setRunning(bool running);

    /// Tick 回调：定时器通过此回调直接发给绑定器，不经过 Qt signal。
    void set_tick_command(std::function<void(float, std::uint64_t)> command)
    {
        m_tickCommand = std::move(command);
    }

    void set_move_left_command(std::function<void(bool)> command) { m_moveLeftCommand = std::move(command); }
    void set_move_right_command(std::function<void(bool)> command) { m_moveRightCommand = std::move(command); }
    void set_move_up_command(std::function<void(bool)> command) { m_moveUpCommand = std::move(command); }
    void set_move_down_command(std::function<void(bool)> command) { m_moveDownCommand = std::move(command); }
    void set_primary_action_command(std::function<void()> command) { m_primaryActionCommand = std::move(command); }
    void set_secondary_action_command(std::function<void()> command) { m_secondaryActionCommand = std::move(command); }
    void set_state_toggle_command(std::function<void()> command) { m_stateToggleCommand = std::move(command); }
    void set_reset_command(std::function<void()> command) { m_resetCommand = std::move(command); }
    void set_confirm_command(std::function<void()> command) { m_confirmCommand = std::move(command); }
    void set_pause_command(std::function<void()> command) { m_pauseCommand = std::move(command); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // ---- 输入处理 ----

    /// 根据当前按住的移动键触发移动命令。
    void emitMovement();

    /// 判断某个 Qt 按键是否被 View 处理（而非交给基类）。
    static bool isHandledKey(int qtKey);

    // ---- 绘制子过程 ----
    void drawBackground(QPainter& p);
    void drawStreet(QPainter& p);
    void drawBuildings(QPainter& p);
    void drawActor(QPainter& p, const viewmodel::ActorViewState& actor);
    void drawCharacterBody(QPainter& p, const viewmodel::ActorViewState& actor,
                           QColor bodyColor);
    void drawHealthBar(QPainter& p, const viewmodel::ActorViewState& actor);
    void drawPlayerStatus(QPainter& p);
    void drawHUD(QPainter& p);
    void drawGOIndicator(QPainter& p);
    void drawOverlay(QPainter& p);

    // ---- 坐标转换 ----
    float worldToScreenX(float worldX) const;
    float worldToScreenY(float laneY, float z) const;
    const viewmodel::GameViewState& game_state() const noexcept;

    // ---- 辅助绘制 ----
    static QColor actorBodyColor(viewmodel::Team team, viewmodel::ActorKind kind);
    static QColor healthBarColor(float ratio);
    static QColor energyBarColor(float ratio);
    void drawBar(QPainter& p, float x, float y, float w, float h,
                 float ratio, QColor fillColor, const QString& label);

    // ---- 命令（由 App 经 MainWindow 注入） ----
    std::function<void(float, std::uint64_t)> m_tickCommand;
    std::function<void(bool)> m_moveLeftCommand;
    std::function<void(bool)> m_moveRightCommand;
    std::function<void(bool)> m_moveUpCommand;
    std::function<void(bool)> m_moveDownCommand;
    std::function<void()> m_primaryActionCommand;
    std::function<void()> m_secondaryActionCommand;
    std::function<void()> m_stateToggleCommand;
    std::function<void()> m_resetCommand;
    std::function<void()> m_confirmCommand;
    std::function<void()> m_pauseCommand;

    // ---- 状态 ----
    const viewmodel::GameViewState* m_gameState = nullptr;
    viewmodel::GameViewState m_emptyState;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    std::uint64_t m_frameIndex = 0;

    /// 当前按住的移动键状态，用于聚合 Direction。
    view::MovementIntent m_movement;

    /// 跟踪本轮按下已经触发过的单次动作，防止重复 Triggered。
    QSet<int> m_triggeredThisPress;

    // GO 闪烁
    mutable float m_goBlinkTimer = 0.0f;

    // 视口缩放
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
};

} // namespace alleyfist
