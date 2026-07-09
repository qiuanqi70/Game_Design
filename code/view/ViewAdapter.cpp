#include "ViewAdapter.h"
#include "GameWidget.h"
#include "../viewmodel/GameViewModel.h"

#include <QDebug>

namespace alleyfist {

ViewAdapter::ViewAdapter(GameWidget* widget,
                         GameViewModel* viewModel,
                         QObject* parent)
    : QObject(parent)
    , m_widget(widget)
    , m_viewModel(viewModel)
{
    // ---- 输入：GameWidget → ViewModel ----
    // 键盘/游戏动作命令
    connect(m_widget, &GameWidget::commandGenerated,
            this, &ViewAdapter::onCommandGenerated);

    // 定时器 tick
    connect(m_widget, &GameWidget::tickRequested,
            this, &ViewAdapter::onTickRequested);

    // ---- 输出：ViewModel → GameWidget ----
    // ViewModel 数据变更后，推送最新快照给 GameWidget 重绘
    connect(m_viewModel, &GameViewModel::changed,
            this, &ViewAdapter::onViewModelChanged);

    // 初始快照
    m_widget->updateSnapshot(m_viewModel->snapshot());

    qDebug() << "ViewAdapter: View ↔ ViewModel bindings established.";
}

void ViewAdapter::onCommandGenerated(const GameCommand& command)
{
    m_viewModel->handle_command(command);
}

void ViewAdapter::onTickRequested(float deltaSeconds, std::uint64_t frameIndex)
{
    // 每帧驱动 ViewModel 更新逻辑
    m_viewModel->handle_command(GameCommand::tick_command(deltaSeconds, frameIndex));
}

void ViewAdapter::onViewModelChanged(ChangeReason reason)
{
    Q_UNUSED(reason);
    m_widget->updateSnapshot(m_viewModel->snapshot());
}

} // namespace alleyfist
