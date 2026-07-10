#pragma once

#include "actor.h"
#include "game_status.h"
#include "level.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

// GameSnapshot 是 ViewModel 给 View 的只读快照，也是数据绑定的主要载体。
// View 可以按它绘制画面，但不能通过它反向修改游戏规则或内部对象。

// HUD 专用状态，和角色数据分开，方便 View 统一绘制状态条。
struct HudViewData {
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
    MapViewData map;
    LevelProgressViewData progress;
    ActorSnapshot player;
    std::vector<ActorSnapshot> enemies;
    std::vector<ActorSnapshot> effects;
    HudViewData hud;
    GameResultViewData result;
    std::string screenMessage;
};

} // namespace alleyfist
