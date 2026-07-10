#pragma once

#include <memory>

namespace alleyfist {

/// App layer entry point: owns framework lifetime and wires View to ViewModel.
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
