#include "GameViewModel.h"

#include <QDebug>

namespace alleyfist {

GameViewModel::GameViewModel(QObject* parent)
    : QObject(parent)
    , m_sim()
{
    // initialize snapshot from simulation
    m_snapshot = m_sim.snapshot();
}

void GameViewModel::handle_command(const GameCommand& command)
{
    // forward command to simulation
    if (command.type == CommandType::Tick) {
        m_sim.step(command.tick.deltaSeconds, command.tick.frameIndex);
        m_snapshot = m_sim.snapshot();
        if (m_callback) m_callback(ChangeReason::Snapshot);
        emit changed(ChangeReason::Snapshot);
        return;
    }

    m_sim.handle_command(command);
    m_snapshot = m_sim.snapshot();
    if (m_callback) m_callback(ChangeReason::Snapshot);
    emit changed(ChangeReason::Snapshot);
}

const GameSnapshot& GameViewModel::snapshot() const
{
    return m_snapshot;
}

void GameViewModel::set_change_callback(ChangeCallback callback)
{
    m_callback = std::move(callback);
}

} // namespace alleyfist
