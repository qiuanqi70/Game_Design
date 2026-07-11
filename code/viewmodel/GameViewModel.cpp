#include "GameViewModel.h"

#include "GameSimulation.h"

#include <algorithm>

namespace alleyfist {

// Helper functions to compare snapshots for changes
// These functions are used to determine if a specific part of the game snapshot has changed
// and notify the registered callbacks accordingly.
//避免不必要的UI更新

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

bool same_actor(const ActorSnapshot& a, const ActorSnapshot& b)
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
           a.animation.atlasId == b.animation.atlasId &&
           a.animation.animationId == b.animation.animationId &&
           a.animation.frameIndex == b.animation.frameIndex &&
           a.animation.secondsPerFrame == b.animation.secondsPerFrame &&
           a.animation.flipX == b.animation.flipX &&
           a.animation.loop == b.animation.loop &&
           a.visible == b.visible &&
           a.targetable == b.targetable &&
           a.invincible == b.invincible &&
           a.onGround == b.onGround &&
           a.depthSortY == b.depthSortY;
}

bool same_actor_list(const std::vector<ActorSnapshot>& a,
                     const std::vector<ActorSnapshot>& b)
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

// ViewModel 接收命令后推进内部模拟，再通过通知列表发出变化通知。
// 这就是 MVVM 中 ViewModel -> View 的通知绑定，不需要 ViewModel 认识具体 View。
void GameViewModel::handle_command(const GameCommand& command)
{
    const GameSnapshot before = m_snapshot;

    // forward command to simulation
    if (command.type == CommandType::Tick) { //如果是 Tick 命令（每帧更新），推进游戏模拟
        m_sim->step(command.tick.deltaSeconds, command.tick.frameIndex); //通过 GameSimulation 处理 Tick 命令，推进游戏状态
        m_snapshot = m_sim->snapshot(); //更新快照为模拟器的当前状态
        notify_changes(before, m_snapshot); //比较快照的变化，如果有变化，通知所有注册的回调函数
        return;
    }

    m_sim->handle_command(command); //如果是 Input 命令（玩家输入），转发给 GameSimulation 处理
    m_snapshot = m_sim->snapshot();
    notify_changes(before, m_snapshot);
}

const GameSnapshot& GameViewModel::snapshot() const
{
    return m_snapshot;
}

// 注册回调函数，当游戏快照发生变化时通知 View
BindingCookie GameViewModel::add_change_callback(ChangeCallback callback)
{
    if (!callback) return 0;

    const BindingCookie cookie = m_nextCallbackCookie++;
    m_callbacks.emplace_back(cookie, std::move(callback)); //将回调函数和唯一标识符存储在回调列表中
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

// 通知所有注册的回调函数，说明游戏快照发生了变化
void GameViewModel::notify(ChangeReason reason)
{
    const auto callbacks = m_callbacks;
    for (const auto& item : callbacks) {
        if (item.second) {
            item.second(reason); //调用回调函数，传入变化原因
        }
    }
}

void GameViewModel::notify_changes(const GameSnapshot& before,
                                   const GameSnapshot& after)
{
    bool notified = false;
    //通知一次的 lambda 函数，避免重复通知
    const auto notify_once = [this, &notified](ChangeReason reason) {
        notify(reason);
        notified = true;
    };

    if (before.phase != after.phase) { //如果游戏阶段发生变化，通知 Phase 变化
        notify_once(ChangeReason::Phase);
    }
    if (!same_actor(before.player, after.player)) { //如果玩家状态发生变化，通知 Player 变化
        notify_once(ChangeReason::Player);
    }
    if (!same_actor_list(before.enemies, after.enemies) ||
        !same_actor_list(before.effects, after.effects)) { //如果敌人或特效状态发生变化，通知 Enemies 变化
        notify_once(ChangeReason::Enemies);
    }
    if (!same_map(before.map, after.map) ||
        !same_progress(before.progress, after.progress)) { //如果地图或进度状态发生变化，通知 Map 变化
        notify_once(ChangeReason::Map);
    }
    if (!same_hud(before.hud, after.hud)) { //如果 HUD 状态（即屏幕消息）发生变化，通知 Hud 变化
        notify_once(ChangeReason::Hud);
    }
    if (!same_result(before.result, after.result)) { //如果游戏结果状态发生变化，通知 Result 变化
        notify_once(ChangeReason::Result);
    }

    if (!notified &&
        (before.frameIndex != after.frameIndex ||
         before.elapsedSeconds != after.elapsedSeconds ||
         before.screenMessage != after.screenMessage)) { //如果没有其他变化，但帧索引、经过时间或屏幕消息发生变化，通知 Snapshot 变化
        notify_once(ChangeReason::Snapshot);
    }
}

} // namespace alleyfist
