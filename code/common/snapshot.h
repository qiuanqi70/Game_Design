#pragma once

#include "types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alleyfist {

using SnapshotId = std::uint32_t;

constexpr SnapshotId kInvalidSnapshotId = 0;

// Camera and world data needed by the view to place objects on screen.
struct ViewportSnapshot {
    Size worldSize;
    Size viewportSize;
    WorldPosition camera;
};

// Optional visible play/display bounds. The owner of the snapshot defines the meaning.
struct BoundarySnapshot {
    float leftX = 0.0f;
    float rightX = 0.0f;
    float topY = 0.0f;
    float bottomY = 0.0f;
    bool visible = false;
};

// A renderable object. visualId, poseId and layerId are ids defined outside common.
struct ObjectSnapshot {
    SnapshotId id = kInvalidSnapshotId;
    SnapshotId visualId = kInvalidSnapshotId;
    SnapshotId poseId = kInvalidSnapshotId;
    SnapshotId layerId = kInvalidSnapshotId;

    WorldPosition position;
    Size drawSize;

    ResourceBar primaryBar;
    ResourceBar secondaryBar;
    bool showPrimaryBar = false;
    bool showSecondaryBar = false;

    bool visible = true;
    bool flipX = false;
};

// Generic HUD/status meter. Its id decides whether it is health, energy, progress, etc.
struct MeterSnapshot {
    SnapshotId id = kInvalidSnapshotId;
    ResourceBar value;
    bool visible = true;
};

// Generic text or overlay message. Its id decides the semantic meaning.
struct TextSnapshot {
    SnapshotId id = kInvalidSnapshotId;
    std::string text;
    bool visible = true;
};

// A complete display frame. It describes what can be shown, not why it happened.
struct FrameSnapshot {
    std::uint64_t frameIndex = 0;
    float elapsedSeconds = 0.0f;

    ViewportSnapshot viewport;
    BoundarySnapshot boundary;
    std::vector<ObjectSnapshot> objects;
    std::vector<MeterSnapshot> meters;
    std::vector<TextSnapshot> texts;
};

} // namespace alleyfist
