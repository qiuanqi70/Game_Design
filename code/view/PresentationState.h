#pragma once

#include "../common/game_state.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace alleyfist::view {

enum class SoundCue {
    PlayerHurt,
    BossIntro,
    GameOver,
    Win
};

/// @brief View 层的临时表现状态。
///
/// 这些状态不属于游戏规则，例如血条缓动、屏幕震动、拾取飘字、音效触发和
/// 动画计时。它们都由只读 GameState 推导出来，因此不会反向影响 ViewModel。
class PresentationState {
public:
    /// 新绑定 GameState 或重开局时调用，重置所有表现时钟。
    void synchronize(const GameState& state);

    /// 每帧推进表现层计时，并返回本帧应播放的一次性音效。
    std::vector<SoundCue> advance(const GameState* state, float dt);

    float elapsed() const noexcept { return m_elapsed; }
    float display_health_ratio() const noexcept { return m_displayHealthRatio; }
    float screen_shake_remaining() const noexcept { return m_screenShakeTimer; }
    float pickup_effect_remaining() const noexcept { return m_pickupFxTimer; }
    PickupKind last_pickup_kind() const noexcept { return m_lastPickupKind; }

    float actor_animation_elapsed(const ActorState& actor,
                                  float fallbackElapsed) const noexcept;

private:
    // 每个 actor 独立计时；动作或受击版本变化时从第 0 帧重新播放。
    struct AnimationClock {
        ActorActionState state = ActorActionState::Idle;
        std::uint32_t impactRevision = 0;
        float elapsed = 0.0f;
        bool initialized = false;
    };

    void reset_round(const GameState& state);
    void update_actor_animation(const ActorState& actor, float dt);
    static float player_health_ratio(const GameState& state) noexcept;

    bool m_initialized = false;
    float m_elapsed = 0.0f;
    float m_displayHealthRatio = 1.0f;
    float m_screenShakeTimer = 0.0f;
    float m_pickupFxTimer = 0.0f;
    std::uint32_t m_lastPlayerImpactRevision = 0;
    int m_observedPlayerHealth = 0;
    int m_observedPlayerEnergy = 0;
    float m_observedGameElapsed = 0.0f;
    PickupKind m_lastPickupKind = PickupKind::Health;
    GamePhase m_observedGamePhase = GamePhase::Title;
    EncounterKind m_observedEncounterKind = EncounterKind::None;
    EncounterPhase m_observedEncounterPhase = EncounterPhase::None;
    std::unordered_map<std::uint32_t, AnimationClock> m_animationClocks;
};

} // namespace alleyfist::view
