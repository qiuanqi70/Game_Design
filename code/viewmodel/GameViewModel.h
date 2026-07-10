#pragma once

#include "../common/contracts.h"
#include "GameSimulation.h"
#include <QObject>

namespace alleyfist {

// GameViewModel 是 ViewModel 层对外的门面：
// 对 View 暴露命令入口和只读快照源，对内部委托 GameSimulation 处理玩法逻辑。
// 它不包含 QWidget/GameWidget，也不调用任何绘制代码。
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
