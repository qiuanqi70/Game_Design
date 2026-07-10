#pragma once

#include "types.h"

#include <cstdint>
#include <string>

namespace alleyfist {

// 这里只保留单个场上对象在一帧快照中的公开状态。
// AI 行为、攻击盒、伤害类型等玩法内部细节已经放到 ViewModel 层，避免 Common 变成“大杂烩”。

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

} // namespace alleyfist
