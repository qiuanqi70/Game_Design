#pragma once

#include <memory>

namespace alleyfist {

/// App layer entry point: owns framework lifetime and wires View to ViewModel.
///
/// 这个公开头文件故意只暴露应用生命周期接口。Qt、MainWindow、GameViewModel
/// 都被藏在 Impl 里，避免 app 的公共接口把 View/ViewModel 的具体类型泄漏出去。
class GameApplication final {
public:
    GameApplication(int& argc, char** argv);
    ~GameApplication();

    GameApplication(const GameApplication&) = delete;
    GameApplication& operator=(const GameApplication&) = delete;

    int run();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace alleyfist
