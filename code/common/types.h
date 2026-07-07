#pragma once

#include <cstdint>

namespace alleyfist {

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

struct WorldPosition {
    float x = 0.0f;
    float laneY = 0.0f;
    float z = 0.0f;
};

} // namespace alleyfist
