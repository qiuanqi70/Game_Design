#pragma once

#include "../common/actions.h"
#include "../common/contracts.h"

#include <QObject>

namespace alleyfist {

class GameWidget;
class GameViewModel;

/// @brief ViewAdapter 桥接 View 层（GameWidget）和 ViewModel 层（GameViewModel）。
///
/// 职责：
/// - 将 GameWidget 的 commandGenerated 信号转发到 ViewModel::handle_command
/// - 将 GameWidget 的 tickRequested 信号封装为 TickCommand 发给 ViewModel
/// - 监听 ViewModel::changed 信号，读取最新快照并推送给 GameWidget
///
/// 用法（App 层）：
/// @code
///   GameViewModel viewModel;
///   MainWindow mainWindow;
///   ViewAdapter adapter(mainWindow.gameWidget(), &viewModel);
///   mainWindow.show();
///   app.exec();
/// @endcode
class ViewAdapter : public QObject {
    Q_OBJECT

public:
    explicit ViewAdapter(GameWidget* widget,
                         GameViewModel* viewModel,
                         QObject* parent = nullptr);

private slots:
    /// 收到 GameWidget 的命令后转发给 ViewModel。
    void onCommandGenerated(const GameCommand& command);

    /// 收到 tick 请求后构造 TickCommand 发给 ViewModel。
    void onTickRequested(float deltaSeconds, std::uint64_t frameIndex);

    /// ViewModel 数据更新后，读取快照推送给 GameWidget。
    void onViewModelChanged(ChangeReason reason);

private:
    GameWidget* m_widget = nullptr;
    GameViewModel* m_viewModel = nullptr;
};

} // namespace alleyfist
