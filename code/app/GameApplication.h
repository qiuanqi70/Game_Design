#pragma once

#include "ViewAdapter.h"

#include "../view/MainWindow.h"
#include "../viewmodel/GameViewModel.h"

#include <QApplication>

namespace alleyfist {

/// App layer entry point: owns framework lifetime and wires View to ViewModel.
class GameApplication final {
public:
    GameApplication(int& argc, char** argv);

    int run();

    MainWindow& main_window() noexcept { return m_mainWindow; }
    const MainWindow& main_window() const noexcept { return m_mainWindow; }

    GameViewModel& view_model() noexcept { return m_viewModel; }
    const GameViewModel& view_model() const noexcept { return m_viewModel; }

private:
    QApplication m_qtApp;
    GameViewModel m_viewModel;
    MainWindow m_mainWindow;
    ViewAdapter m_adapter;
};

} // namespace alleyfist
