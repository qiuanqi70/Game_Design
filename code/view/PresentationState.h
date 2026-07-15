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

/// Owns transient presentation-only state observed from the read-only GameState.
class PresentationState {
public:
    void synchronize(const GameState& state);
    std::vector<SoundCue> advance(const GameState* state, float dt);

    float elapsed() const noexcept { return m_elapsed; }
    float display_health_ratio() const noexcept { return m_displayHealthRatio; }
    float screen_shake_remaining() const noexcept { return m_screenShakeTimer; }
    float pickup_effect_remaining() const noexcept { return m_pickupFxTimer; }
    PickupKind last_pickup_kind() const noexcept { return m_lastPickupKind; }

    float actor_animation_elapsed(const ActorState& actor,
                                  float fallbackElapsed) const noexcept;

private:
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
