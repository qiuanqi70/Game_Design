#pragma once

#include "../common/game_state.h"

#include "InputDefs.h"

#include <QElapsedTimer>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <unordered_map>
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
    void set_game_state(const GameState* state) noexcept;

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
    void drawForeground(QPainter& p);
    void drawActor(QPainter& p, const ActorState& actor);
    bool drawActorSprite(QPainter& p, const ActorState& actor);
    void drawCharacterBody(QPainter& p, const ActorState& actor,
                           QColor bodyColor);
    void drawHealthBar(QPainter& p, const ActorState& actor);
    void drawPlayerStatus(QPainter& p);
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

    // ---- 辅助绘制 ----
    static QColor actorBodyColor(Team team, ActorKind kind);
    static QColor healthBarColor(float ratio);
    static QColor energyBarColor(float ratio);
    static std::size_t hudFrameIndex(float elapsed);
    bool hasActorArt(const ActorState& actor) const;
    float actorSpriteWidth(const ActorState& actor) const;
    void updateActorAnimation(const ActorState& actor, float dt);
    float actorAnimationElapsed(const ActorState& actor) const;
    void drawBar(QPainter& p, float x, float y, float w, float h,
                 float ratio, QColor fillColor, const QString& label);

    struct SpriteClip {
        QPixmap sheet;
        int frameCount = 0;
        float framesPerSecond = 1.0f;
        bool looping = false;
    };

    struct ActorArtSet {
        SpriteClip idle;
        SpriteClip walk;
        SpriteClip lightAttack;
        SpriteClip heavyAttack;
        SpriteClip rangedAttack;
        SpriteClip ambush;
        SpriteClip charge;
        SpriteClip jump;
        SpriteClip airAttack;
        SpriteClip hurt;
        SpriteClip dead;
        Facing sourceFacing = Facing::Right;
        float horizontalPivot = 0.5f;
    };

    const ActorArtSet* actorArtSet(const ActorState& actor) const;
    const SpriteClip* actorClip(const ActorState& actor) const;

    struct AnimationClock {
        ActorActionState state = ActorActionState::Idle;
        std::uint32_t impactRevision = 0;
        float elapsed = 0.0f;
        bool initialized = false;
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

    /// 当前按住的移动键状态，用于聚合 Direction。
    view::MovementIntent m_movement;

    /// 跟踪本轮按下已经触发过的单次动作，防止重复 Triggered。
    QSet<int> m_triggeredThisPress;

    // GO 闪烁
    mutable float m_goBlinkTimer = 0.0f;

    // 视觉特效状态（纯 View 内部，不从 ViewModel 获取）
    float m_displayHealthRatio = 1.0f;
    float m_screenShakeTimer = 0.0f;
    std::uint32_t m_lastPlayerImpactRev = 0;
    mutable std::vector<QPointF> m_stars;
    bool m_starsGenerated = false;
    GamePhase m_observedGamePhase = GamePhase::Title;
    EncounterKind m_observedEncounterKind = EncounterKind::None;
    EncounterPhase m_observedEncounterPhase = EncounterPhase::None;
    bool m_gameOverSounded = false;
    bool m_winSounded = false;
    std::uint32_t m_attackingEnemyId = 0;
    float m_attackingEnemyTimer = 0.0f;
    int m_prevPlayerHp = 100;
    size_t m_lastPickupCount = 0;
    float m_pickupFxTimer = 0.0f;

    ActorArtSet m_playerArt;
    ActorArtSet m_patrollerArt;
    ActorArtSet m_ambusherArt;
    ActorArtSet m_chargerArt;
    ActorArtSet m_rangedGunnerArt;
    ActorArtSet m_rangedRobotArt;
    ActorArtSet m_bossArt;
    QPixmap m_actorShadow;
    QPixmap m_robotProjectile;
    QPixmap m_robotProjectileAlt;
    QPixmap m_stageTileset;
    QPixmap m_stageBack;
    QPixmap m_stageFore;
    QPixmap m_stageCar;
    QPixmap m_stageBarrel;
    QPixmap m_stageHydrant;
    std::array<QPixmap, 8> m_healthBarArt;
    std::array<QPixmap, 8> m_energyBarArt;
    std::array<QPixmap, 4> m_hudScrollArt;
    std::array<QPixmap, 9> m_interfaceFrameArt;
    std::array<QPixmap, 3> m_interfaceLogoArt;
    std::array<QPixmap, 6> m_redButtonArt;
    std::array<QPixmap, 6> m_cyanButtonArt;
    std::array<QPixmap, 6> m_greenButtonArt;
    QPixmap m_startIcon;
    QPixmap m_pauseIcon;
    QPixmap m_bossIcon;
    QPixmap m_winIcon;
    QPixmap m_lossIcon;
    QPixmap m_restartIcon;
    QString m_interfaceFontFamily = "Arial";
    std::unordered_map<std::uint32_t, AnimationClock> m_animationClocks;

    // 视口缩放
    float m_scaleX = 1.0f;
    float m_scaleY = 1.0f;
};

} // namespace alleyfist
