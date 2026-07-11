#pragma once

#include "game_status.h"
#include "level.h"
#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

// GameSnapshot 是 ViewModel 给 View 的只读快照，也是数据绑定的主要载体。
// View 可以按它绘制画面，但不能通过它反向修改游戏规则或内部对象。

// 玩家、敌人、Boss、道具、特效共用的公开分类。
enum class Team {
    Neutral,
    Player,
    Enemy
};

enum class ActorKind {
    Player,
    Grunt,
    Boss,
    Prop,
    Effect
};

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

enum class Facing {
    Left,
    Right
};

// 动画状态只暴露标识和帧信息；这些标识如何映射到具体图片资源由渲染层决定。
struct AnimationSnapshot {
    std::string atlasId;
    std::string animationId;
    std::uint16_t frameIndex = 0;
    float secondsPerFrame = 0.08f;
    bool flipX = false;
    bool loop = true;
};

// 单个角色/对象在 GameSnapshot 中的快照数据。
struct ActorSnapshot {
    ActorId id = kInvalidActorId;
    ActorKind kind = ActorKind::Prop;
    Team team = Team::Neutral;
    WorldPosition position;
    Size drawSize;
    Rect bodyBox;
    ResourceBar health;
    ResourceBar energy;
    ActorState state = ActorState::Idle;
    Facing facing = Facing::Right;
    AnimationSnapshot animation;
    bool visible = true;
    bool targetable = true;
    bool invincible = false;
    bool onGround = true;
    float depthSortY = 0.0f;
};

// HUD 专用状态，和角色数据分开，方便 View 统一绘制状态条。
struct HudSnapshot {
    ResourceBar playerHealth;
    ResourceBar playerEnergy;
    ResourceBar bossHealth;
    bool showBossHealth = false;
    std::uint32_t comboStep = 0;
    float comboTimeLeftSeconds = 0.0f;
    bool playerExhausted = false;
};

// 当前帧完整只读状态。View 理论上只靠它就能完成绘制。
struct GameSnapshot {
    std::uint64_t frameIndex = 0;
    float elapsedSeconds = 0.0f;
    GamePhase phase = GamePhase::Title;
    MapSnapshot map;
    LevelProgressSnapshot progress;
    ActorSnapshot player;
    std::vector<ActorSnapshot> enemies;
    std::vector<ActorSnapshot> effects;
    HudSnapshot hud;
    GameResultSnapshot result;
    std::string screenMessage;
};

} // namespace alleyfist
