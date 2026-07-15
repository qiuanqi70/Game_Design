#pragma once

#include "../common/game_state.h"
#include "../common/types.h"

#include <vector>

namespace alleyfist::viewmodel {

enum class EntityKind {
    Player,
    Patroller,
    Ambusher,
    Charger,
    Ranged,
    Boss
};

enum class EnemyType {
    Patroller,
    Ambusher,
    Charger,
    Ranged,
    Boss
};

enum class EnemyBehaviorState {
    Idle,
    Patrol,
    Ambush,
    Charge,
    RangedAttack,
    Hurt
};

struct ProjectileVm {
    std::uint32_t id = 0;
    int ownerId = 0;
    alleyfist::WorldPosition position;
    alleyfist::Facing facing = alleyfist::Facing::Left;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float lifeSeconds = 0.0f;
    bool active = true;
};

struct PickupVm {
    std::uint32_t id = 0;
    alleyfist::PickupKind kind = alleyfist::PickupKind::Health;
    alleyfist::WorldPosition position;
    float lifeSeconds = 0.0f;
    bool active = true;
};

// 仅 ViewModel 内部使用的实体状态，不属于共享的 Common 状态定义。
struct EntityState {
    alleyfist::WorldPosition pos;
    EntityKind kind = EntityKind::Patroller;
    EnemyType enemyType = EnemyType::Patroller;
    EnemyBehaviorState behaviorState = EnemyBehaviorState::Patrol;
    int hp = 0;
    int maxHp = 0;
    int id = 0;
    bool alive = true;
    float patrolRangeLeft = 0.0f;
    float patrolRangeRight = 0.0f;
    float attackCooldown = 0.0f;
    float chargeTimer = 0.0f;
    float ambushCooldown = 0.0f;
    float hurtTimer = 0.0f;
    std::uint32_t impactRevision = 0;
    alleyfist::ImpactLevel lastImpact = alleyfist::ImpactLevel::None;
    bool pickupDropped = false;
};

using EntityList = std::vector<EntityState>;

} // namespace alleyfist::viewmodel
