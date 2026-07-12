#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

// ============================================================================
// 流程与结算
// ============================================================================

// 游戏当前处于哪个大阶段。View 根据它切换标题、游戏中、暂停、失败或胜利界面。
enum class GamePhase {
    Title,
    Playing,
    EncounterLocked,
    ClearToGo,
    Paused,
    GameOver,
    Win
};

// Game Over 的语义原因。现在只有玩家死亡，后续可以扩展为超时、掉落等。
enum class GameOverReason {
    None,
    PlayerDefeated
};

// 胜利的语义原因。现在是击败 Boss。
enum class WinReason {
    None,
    BossDefeated
};

// 胜负结算界面需要展示的数据，和实时 HUD 分开保存。
struct GameResultSnapshot {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

// ============================================================================
// 地图显示
// ============================================================================

// 地图和镜头状态：世界尺寸、视口尺寸、街道可走范围、锁屏边界和 GO 提示。
struct MapSnapshot {
    float worldWidth = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float cameraX = 0.0f;
    float streetTopY = 0.0f;
    float streetBottomY = 0.0f;
    float leftBoundaryX = 0.0f;
    float rightBoundaryX = 0.0f;
    bool showGoIndicator = false;
};

// ============================================================================
// 角色与对象
// ============================================================================

// 角色阵营，用来区分玩家、敌人和中立对象。
enum class Team {
    Neutral,
    Player,
    Enemy
};

// 对象种类。玩家、普通敌人、Boss、道具和特效都走同一套 ActorSnapshot。
enum class ActorKind {
    Player,
    Grunt,
    Boss,
    Prop,
    Effect
};

// 对象当前动作状态。View 用它决定绘制姿势，ViewModel 用它表达规则结果。
enum class ActorState {
    Idle,
    Walk,
    Run,
    LightAttack,
    HeavyAttack,
    ComboFinisher,
    Jump,
    AirAttack,
    Hurt,
    KnockedDown,
    Dead,
    Spawn
};

// 面朝方向。View 可以据此调整角色朝向。
enum class Facing {
    Left,
    Right
};

// 单个角色或对象的可绘制状态。View 只读这些字段，不直接修改游戏对象。
struct ActorSnapshot {
    ActorKind kind = ActorKind::Prop;
    Team team = Team::Neutral;
    WorldPosition position;
    Size drawSize;
    ResourceBar health;
    ResourceBar energy;
    ActorState state = ActorState::Idle;
    Facing facing = Facing::Right;
    bool visible = true;
    bool targetable = true;
    bool invincible = false;
    bool onGround = true;
};

// ============================================================================
// HUD
// ============================================================================

// HUD 专用状态，和角色对象分开，方便 View 统一绘制血条、精力条和连击提示。
struct HudSnapshot {
    ResourceBar playerHealth;
    ResourceBar playerEnergy;
    ResourceBar bossHealth;
    bool showBossHealth = false;
    std::uint32_t comboStep = 0;
    float comboTimeLeftSeconds = 0.0f;
    bool playerExhausted = false;
};

// ============================================================================
// 完整帧快照
// ============================================================================

// ViewModel 每帧输出的完整只读状态。View 理论上只靠它就能完成整帧绘制。
struct GameSnapshot {
    std::uint64_t frameIndex = 0;
    float elapsedSeconds = 0.0f;
    GamePhase phase = GamePhase::Title;
    float progressRatio = 0.0f;    // 玩家在整张地图上的推进比例，View 用它绘制底部进度条。
    MapSnapshot map;
    ActorSnapshot player;
    std::vector<ActorSnapshot> enemies;
    std::vector<ActorSnapshot> effects;
    HudSnapshot hud;
    GameResultSnapshot result;
    std::string screenMessage;
};

} // namespace alleyfist
