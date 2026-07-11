#pragma once

#include "../common/snapshot.h"

#include <cstdint>
#include <vector>

namespace alleyfist {

// 这些类型只服务于 GameSimulation 的规则、AI 和战斗计算，
// 不属于 View 与 ViewModel 之间的公共显示契约。
// 老师检查 common 层时，可以看到这些内部类型已经留在 ViewModel 层内。

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

enum class SpawnSide {
    Left,
    Right,
    Both,
    BossGate
};

struct GameRules {
    float levelLength = 3000.0f;
    float viewportWidth = 960.0f;
    float viewportHeight = 540.0f;
    float streetTopY = 300.0f;
    float streetBottomY = 500.0f;
    float playerMoveSpeed = 260.0f;
    float enemyMoveSpeed = 140.0f;
    float bossMoveSpeed = 180.0f;
    float jumpSeconds = 0.55f;
    float jumpHeight = 90.0f;
    float energyRecoveryPerSecond = 18.0f;
    float runEnergyCostPerSecond = 8.0f;
    int lightAttackEnergyCost = 8;
    int heavyAttackEnergyCost = 18;
    int jumpEnergyCost = 12;
};

struct CombatBox {
    Rect localBounds;
    AttackKind attack = AttackKind::None;
    DamageType damageType = DamageType::Light;
    int damage = 0;
    float stunSeconds = 0.0f;
    ActorId ownerId = kInvalidActorId;
};

struct SpawnSpec {
    ActorKind actorKind = ActorKind::Grunt;
    EnemyBehavior behavior = EnemyBehavior::Surround;
    SpawnSide side = SpawnSide::Both;
    std::uint32_t count = 0;
    int maxHealth = 1;
};

struct EncounterSpec {
    EncounterId id = kInvalidEncounterId;
    float triggerX = 0.0f;
    bool bossEncounter = false;
    std::vector<SpawnSpec> spawns;
};

} // namespace alleyfist
