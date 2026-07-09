#include "ViewAdapter.h"

#include "../view/GameWidget.h"
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
    connect(m_widget, &GameWidget::commandGenerated,
            this, &ViewAdapter::onCommandGenerated);

    connect(m_widget, &GameWidget::tickRequested,
            this, &ViewAdapter::onTickRequested);

    connect(m_viewModel, &GameViewModel::changed,
            this, &ViewAdapter::onViewModelChanged);

    m_widget->updateSnapshot(m_viewModel->snapshot());

    qDebug() << "App: View and ViewModel bindings established.";
}

void ViewAdapter::onCommandGenerated(const GameCommand& command)
{
    m_viewModel->handle_command(command);
}

void ViewAdapter::onTickRequested(float deltaSeconds, std::uint64_t frameIndex)
{
    m_viewModel->handle_command(GameCommand::tick_command(deltaSeconds, frameIndex));
}

void ViewAdapter::onViewModelChanged(ChangeReason reason)
{
    Q_UNUSED(reason);
    m_widget->updateSnapshot(m_viewModel->snapshot());
}

} // namespace alleyfist
