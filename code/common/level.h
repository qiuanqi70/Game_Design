#pragma once

#include "game_status.h"
#include "types.h"

#include <cstdint>
#include <vector>

namespace alleyfist {

// 关卡相关的 Common 类型只保留可展示状态。
// 刷怪配置、遭遇战生成规则等由 ViewModel 拥有，View 只根据这些 ViewData 画进度和提示。

// 遭遇战运行时状态，用于界面、调试和 GO 提示判断。
struct EncounterViewData {
    EncounterId id = kInvalidEncounterId;
    EncounterState state = EncounterState::Inactive;
    ScrollLockState lockState = ScrollLockState::Free;
    float triggerX = 0.0f;
    std::uint32_t spawnedCount = 0;
    std::uint32_t defeatedCount = 0;
    std::uint32_t remainingCount = 0;
    bool bossEncounter = false;
};

// 地图镜头、街道边界、推图锁和可见遭遇战状态。
struct MapViewData {
    float worldWidth = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float cameraX = 0.0f;
    float streetTopY = 0.0f;
    float streetBottomY = 0.0f;
    float leftBoundaryX = 0.0f;
    float rightBoundaryX = 0.0f;
    ScrollLockState scrollLock = ScrollLockState::Free;
    bool showGoIndicator = false;
    std::vector<EncounterViewData> encounters;
};

// 简洁的关卡进度数据，可用于 HUD 或结算界面。
struct LevelProgressViewData {
    std::uint32_t stageIndex = 0;
    EncounterId activeEncounterId = kInvalidEncounterId;
    float progressRatio = 0.0f;
    bool bossSpawned = false;
    bool bossDefeated = false;
};

} // namespace alleyfist
