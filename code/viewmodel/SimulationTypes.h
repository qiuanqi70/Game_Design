#pragma once

#include "../common/types.h"
#include <functional>
#include <cstdint>
#include <vector>

namespace alleyfist::viewmodel {

using Command = std::function<void()>;
using TickCallback = std::function<void(float /*deltaSeconds*/)>;

// 简单的实体状态：位置与生命值。View 层通过这些只读快照来绘制。
struct EntityState {
    alleyfist::WorldPosition pos;
    int hp = 0;
    int id = 0;
    bool alive = true;
};

using EntityList = std::vector<EntityState>;

} // namespace alleyfist::viewmodel
