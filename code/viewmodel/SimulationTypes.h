#pragma once

#include "../common/game_state.h"
#include "../common/types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace alleyfist::viewmodel {

using Command = std::function<void()>;
using ButtonCommand = std::function<void(bool)>;
using FrameCommand = std::function<void(float, std::uint64_t)>;
using TickCallback = std::function<void(float /*deltaSeconds*/)>;

enum class GamePhase {
    Title,
    Playing,
    EncounterLocked,
    ClearToGo,
    Paused,
    GameOver,
    Win
};

enum class GameOverReason {
    None,
    PlayerDefeated
};

enum class WinReason {
    None,
    BossDefeated
};

struct GameResultViewState {
    GameOverReason gameOverReason = GameOverReason::None;
    WinReason winReason = WinReason::None;
    float elapsedSeconds = 0.0f;
    std::uint32_t defeatedEnemies = 0;
};

// 仅 ViewModel 内部使用的实体快照，不属于共享的 View 状态定义。
struct EntityState {
    alleyfist::WorldPosition pos;
    int hp = 0;
    int maxHp = 0;
    int id = 0;
    bool alive = true;
};

using EntityList = std::vector<EntityState>;

} // namespace alleyfist::viewmodel
