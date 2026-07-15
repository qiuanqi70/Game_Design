#pragma once

#include "../common/game_state.h"

#include "AssetCatalog.h"
#include "InputDefs.h"
#include "PresentationState.h"

#include <QColor>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QHideEvent>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace alleyfist {

/// @brief 游戏渲染画布，负责键盘输入、定时器驱动和所有画面绘制。
///
/// GameWidget 只保存 ViewModel 暴露出来的只读显示属性指针，
/// 不直接操作游戏数据。
/// 它把 Qt 键盘事件翻译成已绑定命令，把只读属性翻译成像素绘制。
///
/// 键盘映射和 InputState 聚合属于 View 层内部实现，
/// 换输入设备（手柄 / 触屏）只需改本文件的按键处理。
class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);

    /// 外部（App 层）调用，传入 ViewModel 持有的只读显示属性。
    void set_game_state(const GameState* state) noexcept;

    /// Explicit lifecycle control retained for embedders and deterministic tests.
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
    void focusOutEvent(QFocusEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // ---- 输入处理 ----
    void dispatchMovement(view::MovementDirection direction, bool pressed);
    void releaseInput();
    static void playSoundCues(const std::vector<view::SoundCue>& cues);

    // ---- 绘制子过程 ----
    void drawBackground(QPainter& p);
    void drawStreet(QPainter& p);
    void drawBuildings(QPainter& p);
    void drawForeground(QPainter& p);
    void drawActor(QPainter& p, const ActorState& actor);
    bool drawActorSprite(QPainter& p, const ActorState& actor);
    void drawCharacterBody(QPainter& p, const ActorState& actor,
                           QColor bodyColor);
    void drawHealthBar(QPainter& p, const ActorState& actor);
    void drawHUD(QPainter& p);
    void drawOverlay(QPainter& p);
    void drawEncounterOverlay(QPainter& p);
    void drawInterfacePanel(QPainter& p, const QRectF& rect) const;
    void drawInterfaceButton(QPainter& p, const QRectF& rect,
                             const std::array<QPixmap, 6>& art,
                             bool bright) const;
    void drawProjectile(QPainter& p, const ProjectileState& proj);
    void drawPickup(QPainter& p, const PickupState& pickup);
    void drawParticles(QPainter& p);

    // ---- 坐标转换 ----
    float worldToScreenX(float worldX) const;
    float worldToScreenY(float laneY, float z) const;
    const GameState& game_state() const noexcept;
    void refreshViewportMetrics() noexcept;

    // ---- 辅助绘制 ----
    static QColor actorBodyColor(Team team, ActorKind kind);
    static QColor healthBarColor(float ratio);
    static QColor energyBarColor();
    static std::size_t hudFrameIndex(float elapsed);
    bool hasActorArt(const ActorState& actor) const;
    float actorSpriteWidth(const ActorState& actor) const;
    void drawBar(QPainter& p, float x, float y, float w, float h,
                 float ratio, QColor fillColor);

    struct ViewportMetrics {
        float width = 960.0f;
        float height = 540.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

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
    const GameState* m_gameState = nullptr;
    GameState m_emptyState;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    std::uint64_t m_frameIndex = 0;
    view::InputState m_inputState;
    view::PresentationState m_presentation;
    view::AssetCatalog m_assets;

    mutable std::vector<QPointF> m_stars;
    bool m_starsGenerated = false;
    ViewportMetrics m_viewport;
};

} // namespace alleyfist
