#pragma once

#include "types.h"

#include <cstdint>
#include <string>

namespace alleyfist {

// 这里只保留 View 绘制角色必须知道的公共显示数据。
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

// 精灵动画只暴露名字；这些名字如何映射到图片由 View 决定。
struct SpriteViewData {
    std::string atlasId;
    std::string animationId;
    std::uint16_t frameIndex = 0;
    float secondsPerFrame = 0.08f;
    bool flipX = false;
    bool loop = true;
};

// ViewModel 暴露给 View 的角色绘制状态。
struct ActorViewData {
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
    SpriteViewData sprite;
    bool visible = true;
    bool targetable = true;
    bool invincible = false;
    bool onGround = true;
    float depthSortY = 0.0f;
};

} // namespace alleyfist
