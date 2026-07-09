#pragma once

#include "../common/actions.h"
#include "../common/contracts.h"

#include <QObject>

namespace alleyfist {

class GameWidget;
class GameViewModel;

/// App-layer binder between the View and ViewModel modules.
///
/// View emits common-layer commands and receives common-layer snapshots.
/// ViewModel receives commands and owns gameplay state. This adapter is the
/// only place that knows both concrete classes.
class ViewAdapter : public QObject {
    Q_OBJECT

public:
    explicit ViewAdapter(GameWidget* widget,
                         GameViewModel* viewModel,
                         QObject* parent = nullptr);

private slots:
    void onCommandGenerated(const GameCommand& command);
    void onTickRequested(float deltaSeconds, std::uint64_t frameIndex);
    void onViewModelChanged(ChangeReason reason);

private:
    GameWidget* m_widget = nullptr;
    GameViewModel* m_viewModel = nullptr;
};

} // namespace alleyfist
