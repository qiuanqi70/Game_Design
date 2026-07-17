#include "PresentationState.h"

#include <algorithm>

namespace alleyfist::view {

float PresentationState::player_health_ratio(const GameState& state) noexcept
{
    const auto& health = state.hud.playerHealth.maximum > 0
                             ? state.hud.playerHealth
                             : state.player.health;
    return std::clamp(health.ratio(), 0.0f, 1.0f);
}

void PresentationState::synchronize(const GameState& state)
{
    // 新绑定或重置时把表现层完全对齐到当前规则快照。
    m_initialized = true;
    m_displayHealthRatio = player_health_ratio(state);
    m_screenShakeTimer = 0.0f;
    m_pickupFxTimer = 0.0f;
    m_lastPlayerImpactRevision = state.player.impactRevision;
    m_observedPlayerHealth = state.hud.playerHealth.current;
    m_observedPlayerEnergy = state.hud.playerEnergy.current;
    m_observedGameElapsed = state.elapsedSeconds;
    m_observedGamePhase = state.phase;
    m_observedEncounterKind = state.encounter.kind;
    m_observedEncounterPhase = state.encounter.phase;
    m_animationClocks.clear();
    update_actor_animation(state.player, 0.0f);
    for (const auto& enemy : state.enemies) update_actor_animation(enemy, 0.0f);
}

void PresentationState::reset_round(const GameState& state)
{
    m_animationClocks.clear();
    m_displayHealthRatio = player_health_ratio(state);
    m_screenShakeTimer = 0.0f;
    m_pickupFxTimer = 0.0f;
    m_lastPlayerImpactRevision = state.player.impactRevision;
    m_observedPlayerHealth = state.hud.playerHealth.current;
    m_observedPlayerEnergy = state.hud.playerEnergy.current;
    m_observedGameElapsed = state.elapsedSeconds;
    update_actor_animation(state.player, 0.0f);
    for (const auto& enemy : state.enemies) update_actor_animation(enemy, 0.0f);
}

std::vector<SoundCue> PresentationState::advance(const GameState* state, float dt)
{
    std::vector<SoundCue> cues;
    m_elapsed += dt;
    m_screenShakeTimer = std::max(0.0f, m_screenShakeTimer - dt);
    m_pickupFxTimer = std::max(0.0f, m_pickupFxTimer - dt);

    if (state == nullptr) return cues;
    if (!m_initialized) synchronize(*state);

    const bool startingRound = state->phase == GamePhase::Playing &&
                               (m_observedGamePhase == GamePhase::Title ||
                                m_observedGamePhase == GamePhase::GameOver ||
                                m_observedGamePhase == GamePhase::Win);
    const bool stateRestarted = state->elapsedSeconds < m_observedGameElapsed;
    // 通过 phase 或 elapsed 回退识别新一局，避免沿用上一局的动画和特效。
    if (startingRound || stateRestarted) reset_round(*state);

    if (state->phase != m_observedGamePhase) {
        if (state->phase == GamePhase::GameOver) cues.push_back(SoundCue::GameOver);
        if (state->phase == GamePhase::Win) cues.push_back(SoundCue::Win);
        m_observedGamePhase = state->phase;
    }

    const auto& encounter = state->encounter;
    const bool enteringBossIntro = encounter.kind == EncounterKind::Boss &&
                                   encounter.phase == EncounterPhase::Intro &&
                                   (m_observedEncounterKind != EncounterKind::Boss ||
                                    m_observedEncounterPhase != EncounterPhase::Intro);
    if (enteringBossIntro) cues.push_back(SoundCue::BossIntro);
    m_observedEncounterKind = encounter.kind;
    m_observedEncounterPhase = encounter.phase;

    if (state->player.impactRevision != m_lastPlayerImpactRevision) {
        // impactRevision 是规则层给表现层的“一次性事件版本号”。
        m_lastPlayerImpactRevision = state->player.impactRevision;
        if (state->player.lastImpact != ImpactLevel::None) {
            m_screenShakeTimer = state->player.lastImpact == ImpactLevel::Heavy
                                     ? 0.15f
                                     : 0.06f;
            cues.push_back(SoundCue::PlayerHurt);
        }
    }

    constexpr int kMinimumEnergyPickupJump = 10;
    const int health = state->hud.playerHealth.current;
    const int energy = state->hud.playerEnergy.current;
    if (!startingRound && !stateRestarted && health > m_observedPlayerHealth) {
        m_lastPickupKind = PickupKind::Health;
        m_pickupFxTimer = 0.5f;
    } else if (!startingRound && !stateRestarted &&
               energy - m_observedPlayerEnergy >= kMinimumEnergyPickupJump) {
        // Natural regeneration is at most 2.8 per clamped frame; a larger jump
        // is the observable state change that distinguishes an energy pickup.
        m_lastPickupKind = PickupKind::Energy;
        m_pickupFxTimer = 0.5f;
    }
    m_observedPlayerHealth = health;
    m_observedPlayerEnergy = energy;
    m_observedGameElapsed = state->elapsedSeconds;

    const float targetHealth = player_health_ratio(*state);
    // 血条显示值做缓动，真实血量仍以 GameState 为准。
    m_displayHealthRatio += (targetHealth - m_displayHealthRatio) * dt * 4.0f;

    update_actor_animation(state->player, dt);
    for (const auto& enemy : state->enemies) update_actor_animation(enemy, dt);
    return cues;
}

void PresentationState::update_actor_animation(const ActorState& actor, float dt)
{
    auto& clock = m_animationClocks[actor.id];
    if (!clock.initialized || clock.state != actor.actionState ||
        clock.impactRevision != actor.impactRevision) {
        clock.state = actor.actionState;
        clock.impactRevision = actor.impactRevision;
        clock.elapsed = 0.0f;
        clock.initialized = true;
        return;
    }
    clock.elapsed += dt;
}

float PresentationState::actor_animation_elapsed(const ActorState& actor,
                                                  float fallbackElapsed) const noexcept
{
    const auto it = m_animationClocks.find(actor.id);
    return it == m_animationClocks.end() ? fallbackElapsed : it->second.elapsed;
}

} // namespace alleyfist::view
