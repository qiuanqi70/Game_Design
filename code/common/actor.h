#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

// 玩家、敌人、Boss、道具、特效共用的显示和玩法分类。
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

enum class EnemyBehavior {
    None,
    Surround,
    Chase,
    BossChase
};

enum class AttackKind {
    None,
    LightPunch,
    HeavyStrike,
    ComboFinisher,
    JumpKick,
    EnemyStrike,
    BossStrike
};

enum class DamageType {
    Light,
    Heavy,
    Air,
    Contact,
    Boss
};

// 精灵动画只暴露名字；这些名字如何映射到图片由 View 决定。
struct SpriteViewData {
    std::string atlasId;
    std::string animationId;
    std::uint16_t frameIndex = 0;
    float secondsPerFrame = 0.08f;
    bool flipX = false;
    bool loop = true;
};

// 当前帧生效的攻击盒，坐标相对于攻击发起者。
struct CombatBoxViewData {
    Rect localBounds;
    AttackKind attack = AttackKind::None;
    DamageType damageType = DamageType::Light;
    int damage = 0;
    float stunSeconds = 0.0f;
    ActorId ownerId = kInvalidActorId;
};

// ViewModel 暴露给 View 的角色绘制状态。
struct ActorViewData {
    ActorId id = kInvalidActorId;
    ActorKind kind = ActorKind::Prop;
    Team team = Team::Neutral;
    EnemyBehavior behavior = EnemyBehavior::None;
    WorldPosition position;
    Size drawSize;
    Rect bodyBox;
    ResourceBar health;
    ResourceBar energy;
    ActorState state = ActorState::Idle;
    Facing facing = Facing::Right;
    SpriteViewData sprite;
    std::vector<CombatBoxViewData> activeHitBoxes;
    bool visible = true;
    bool targetable = true;
    bool invincible = false;
    bool onGround = true;
    float depthSortY = 0.0f;
    std::string debugName;
};

} // namespace alleyfist
