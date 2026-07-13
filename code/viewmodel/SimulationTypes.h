#pragma once

#include "../common/types.h"

#include <vector>

namespace alleyfist::viewmodel {

enum class EntityKind {
    Player,
    Grunt,
    Boss
};

// 仅 ViewModel 内部使用的实体快照，不属于共享的 View 状态定义。
struct EntityState {
    alleyfist::WorldPosition pos;
    EntityKind kind = EntityKind::Grunt;
    int hp = 0;
    int maxHp = 0;
    int id = 0;
    bool alive = true;
};

using EntityList = std::vector<EntityState>;

} // namespace alleyfist::viewmodel
