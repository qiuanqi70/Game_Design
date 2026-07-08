#pragma once

#include "../common/contracts.h"
#include "GameSimulation.h"
#include <QObject>

namespace alleyfist {

class GameViewModel : public QObject, public IGameCommandSink, public IGameSnapshotSource {
    Q_OBJECT
public:
    explicit GameViewModel(QObject* parent = nullptr);

    // IGameCommandSink
    void handle_command(const GameCommand& command) override;

    // IGameSnapshotSource
    const GameSnapshot& snapshot() const override;
    void set_change_callback(ChangeCallback callback) override;

signals:
    void changed(ChangeReason reason);

private:
    GameSnapshot m_snapshot;
    ChangeCallback m_callback;
    GameSimulation m_sim;
};

} // namespace alleyfist
