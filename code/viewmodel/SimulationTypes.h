#pragma once

#include "../common/game_state.h"
#include "../common/types.h"

#include <vector>

namespace alleyfist::viewmodel {

// 内部规则层的实体分类，比 Common 的 ActorKind 更贴近 AI 行为。
enum class EntityKind {
    Player,
    Patroller,
    Ambusher,
    Charger,
    Ranged,
    Boss
};

// View 发来的动作语义会先落到这里，再由 GameSimulation 判断能否执行。
enum class PlayerActionType {
    None,
    Jump,
    LightAttack,
    HeavyAttack
};

// 玩家行为状态用于映射成 Common::ActorActionState，供 View 播放正确动画。
enum class PlayerBehaviorState {
    Idle,
    Walk,
    Jump,
    LightAttack,
    HeavyAttack,
    AirAttack,
    Hurt,
    Dead
};

// 敌人行为状态保留 AI 语义，再在 GameViewModel 中转换为显示状态。
enum class EnemyBehaviorState {
    Idle,
    Patrol,
    MeleeAttack,
    Ambush,
    Charge,
    RangedAttack,
    Hurt
};

// 投射物属于模拟规则状态，GameViewModel 会转换为 Common::ProjectileState。
struct ProjectileVm {
    std::uint32_t id = 0;
    alleyfist::ActorVisualVariant visualVariant = alleyfist::ActorVisualVariant::Default;
    alleyfist::WorldPosition position;
    alleyfist::Facing facing = alleyfist::Facing::Left;
    float velocityX = 0.0f;
    float velocityLaneY = 0.0f;
    float velocityZ = 0.0f;
    float lifeSeconds = 0.0f;
    bool active = true;
};

// 拾取物属于模拟规则状态，View 只看到转换后的 Common::PickupState。
struct PickupVm {
    std::uint32_t id = 0;
    alleyfist::PickupKind kind = alleyfist::PickupKind::Health;
    alleyfist::WorldPosition position;
    float lifeSeconds = 0.0f;
    bool active = true;
};

// 仅 ViewModel 内部使用的实体状态，不属于共享的 Common 状态定义。
// 这里保存规则需要的血量、冷却、AI 计时器等字段，避免污染 Common 层 DTO。
struct EntityState {
    alleyfist::WorldPosition pos;
    EntityKind kind = EntityKind::Patroller;
    alleyfist::ActorVisualVariant visualVariant = alleyfist::ActorVisualVariant::Default;
    alleyfist::Facing facing = alleyfist::Facing::Left;
    EnemyBehaviorState behaviorState = EnemyBehaviorState::Patrol;
    int hp = 0;
    int maxHp = 0;
    int id = 0;
    bool alive = true;
    float patrolRangeLeft = 0.0f;
    float patrolRangeRight = 0.0f;
    float attackCooldown = 0.0f;
    float attackTimer = 0.0f;
    float chargeTimer = 0.0f;
    float ambushCooldown = 0.0f;
    float hurtTimer = 0.0f;
    float deathTimer = 0.0f;
    std::uint32_t impactRevision = 0;
    alleyfist::ImpactLevel lastImpact = alleyfist::ImpactLevel::None;
    bool attackHitApplied = false;
    bool pickupDropped = false;
};

using EntityList = std::vector<EntityState>;

} // namespace alleyfist::viewmodel
