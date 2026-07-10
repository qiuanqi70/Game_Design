#include "GameViewModel.h"

#include "GameSimulation.h"

#include <algorithm>

namespace alleyfist {

// ViewModel 接收命令后推进内部模拟，再通过通知列表发出变化通知。
// 这就是 MVVM 中 ViewModel -> View 的通知绑定，不需要 ViewModel 认识具体 View。

namespace {

bool same_resource(const ResourceBar& a, const ResourceBar& b) noexcept
{
    return a.current == b.current && a.maximum == b.maximum;
}

bool same_position(const WorldPosition& a, const WorldPosition& b) noexcept
{
    return a.x == b.x && a.laneY == b.laneY && a.z == b.z;
}

bool same_size(const Size& a, const Size& b) noexcept
{
    return a.width == b.width && a.height == b.height;
}

bool same_rect(const Rect& a, const Rect& b) noexcept
{
    return a.x == b.x && a.y == b.y &&
           a.width == b.width && a.height == b.height;
}

bool same_actor(const ActorViewData& a, const ActorViewData& b)
{
    return a.id == b.id &&
           a.kind == b.kind &&
           a.team == b.team &&
           same_position(a.position, b.position) &&
           same_size(a.drawSize, b.drawSize) &&
           same_rect(a.bodyBox, b.bodyBox) &&
           same_resource(a.health, b.health) &&
           same_resource(a.energy, b.energy) &&
           a.state == b.state &&
           a.facing == b.facing &&
           a.sprite.atlasId == b.sprite.atlasId &&
           a.sprite.animationId == b.sprite.animationId &&
           a.sprite.frameIndex == b.sprite.frameIndex &&
           a.sprite.secondsPerFrame == b.sprite.secondsPerFrame &&
           a.sprite.flipX == b.sprite.flipX &&
           a.sprite.loop == b.sprite.loop &&
           a.visible == b.visible &&
           a.targetable == b.targetable &&
           a.invincible == b.invincible &&
           a.onGround == b.onGround &&
           a.depthSortY == b.depthSortY;
}

bool same_actor_list(const std::vector<ActorViewData>& a,
                     const std::vector<ActorViewData>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!same_actor(a[i], b[i])) return false;
    }
    return true;
}

bool same_encounter(const EncounterViewData& a, const EncounterViewData& b) noexcept
{
    return a.id == b.id &&
           a.state == b.state &&
           a.lockState == b.lockState &&
           a.triggerX == b.triggerX &&
           a.spawnedCount == b.spawnedCount &&
           a.defeatedCount == b.defeatedCount &&
           a.remainingCount == b.remainingCount &&
           a.bossEncounter == b.bossEncounter;
}

bool same_encounters(const std::vector<EncounterViewData>& a,
                     const std::vector<EncounterViewData>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!same_encounter(a[i], b[i])) return false;
    }
    return true;
}

bool same_map(const MapViewData& a, const MapViewData& b)
{
    return a.worldWidth == b.worldWidth &&
           a.viewportWidth == b.viewportWidth &&
           a.viewportHeight == b.viewportHeight &&
           a.cameraX == b.cameraX &&
           a.streetTopY == b.streetTopY &&
           a.streetBottomY == b.streetBottomY &&
           a.leftBoundaryX == b.leftBoundaryX &&
           a.rightBoundaryX == b.rightBoundaryX &&
           a.scrollLock == b.scrollLock &&
           a.showGoIndicator == b.showGoIndicator &&
           same_encounters(a.encounters, b.encounters);
}

bool same_progress(const LevelProgressViewData& a,
                   const LevelProgressViewData& b) noexcept
{
    return a.stageIndex == b.stageIndex &&
           a.activeEncounterId == b.activeEncounterId &&
           a.progressRatio == b.progressRatio &&
           a.bossSpawned == b.bossSpawned &&
           a.bossDefeated == b.bossDefeated;
}

bool same_hud(const HudViewData& a, const HudViewData& b) noexcept
{
    return same_resource(a.playerHealth, b.playerHealth) &&
           same_resource(a.playerEnergy, b.playerEnergy) &&
           same_resource(a.bossHealth, b.bossHealth) &&
           a.showBossHealth == b.showBossHealth &&
           a.comboStep == b.comboStep &&
           a.comboTimeLeftSeconds == b.comboTimeLeftSeconds &&
           a.playerExhausted == b.playerExhausted;
}

bool same_result(const GameResultViewData& a, const GameResultViewData& b) noexcept
{
    return a.gameOverReason == b.gameOverReason &&
           a.winReason == b.winReason &&
           a.elapsedSeconds == b.elapsedSeconds &&
           a.defeatedEnemies == b.defeatedEnemies;
}

} // namespace

GameViewModel::GameViewModel()
    : m_sim(std::make_unique<GameSimulation>())
{
    // initialize snapshot from simulation
    m_snapshot = m_sim->snapshot();
}

GameViewModel::~GameViewModel() = default;

void GameViewModel::handle_command(const GameCommand& command)
{
    const GameSnapshot before = m_snapshot;

    // forward command to simulation
    if (command.type == CommandType::Tick) {
        m_sim->step(command.tick.deltaSeconds, command.tick.frameIndex);
        m_snapshot = m_sim->snapshot();
        notify_changes(before, m_snapshot);
        return;
    }

    m_sim->handle_command(command);
    m_snapshot = m_sim->snapshot();
    notify_changes(before, m_snapshot);
}

const GameSnapshot& GameViewModel::snapshot() const
{
    return m_snapshot;
}

BindingCookie GameViewModel::add_change_callback(ChangeCallback callback)
{
    if (!callback) return 0;

    const BindingCookie cookie = m_nextCallbackCookie++;
    m_callbacks.emplace_back(cookie, std::move(callback));
    return cookie;
}

void GameViewModel::remove_change_callback(BindingCookie cookie)
{
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
                       [cookie](const auto& item) {
                           return item.first == cookie;
                       }),
        m_callbacks.end());
}

void GameViewModel::notify(ChangeReason reason)
{
    const auto callbacks = m_callbacks;
    for (const auto& item : callbacks) {
        if (item.second) {
            item.second(reason);
        }
    }
}

void GameViewModel::notify_changes(const GameSnapshot& before,
                                   const GameSnapshot& after)
{
    bool notified = false;
    const auto notify_once = [this, &notified](ChangeReason reason) {
        notify(reason);
        notified = true;
    };

    if (before.phase != after.phase) {
        notify_once(ChangeReason::Phase);
    }
    if (!same_actor(before.player, after.player)) {
        notify_once(ChangeReason::Player);
    }
    if (!same_actor_list(before.enemies, after.enemies) ||
        !same_actor_list(before.effects, after.effects)) {
        notify_once(ChangeReason::Enemies);
    }
    if (!same_map(before.map, after.map) ||
        !same_progress(before.progress, after.progress)) {
        notify_once(ChangeReason::Map);
    }
    if (!same_hud(before.hud, after.hud)) {
        notify_once(ChangeReason::Hud);
    }
    if (!same_result(before.result, after.result)) {
        notify_once(ChangeReason::Result);
    }

    if (!notified &&
        (before.frameIndex != after.frameIndex ||
         before.elapsedSeconds != after.elapsedSeconds ||
         before.screenMessage != after.screenMessage)) {
        notify_once(ChangeReason::Snapshot);
    }
}

} // namespace alleyfist
