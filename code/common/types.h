#pragma once

#include <cstdint>

namespace alleyfist {

// 稳定编号：View 和 ViewModel 用它指向同一个游戏对象。
using ActorId = std::uint32_t;
using EncounterId = std::uint32_t;

constexpr ActorId kInvalidActorId = 0;
constexpr ActorId kPlayerActorId = 1;
constexpr EncounterId kInvalidEncounterId = 0;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool is_empty() const noexcept
    {
        return width <= 0.0f || height <= 0.0f;
    }
};

// 通用资源条：血量、精力、Boss 血条等都可以使用它。
struct ResourceBar {
    int current = 0;
    int maximum = 0;

    float ratio() const noexcept
    {
        return maximum > 0 ? static_cast<float>(current) / static_cast<float>(maximum) : 0.0f;
    }

    bool is_empty() const noexcept
    {
        return maximum <= 0 || current <= 0;
    }
};

// 横版格斗世界坐标：x 表示关卡推进，laneY 表示街道纵深，
// z 表示离地高度，用于跳跃或击飞。
struct WorldPosition {
    float x = 0.0f;
    float laneY = 0.0f;
    float z = 0.0f;
};

} // namespace alleyfist
