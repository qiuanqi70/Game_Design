#pragma once

#include "../common/contracts.h"

#include <QMainWindow>
#include <memory>

namespace alleyfist {

class GameWidget;

/// @brief 游戏主窗口，承载 GameWidget 作为中心控件。
///
/// MainWindow 是 View 层对外的入口之一。
/// App 层创建 MainWindow 后调用 show() 即可显示游戏窗口。
/// App 层不能再直接拿内部 GameWidget，而是通过 bind() 传入公共接口完成 MVVM 绑定。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /// 将 View 暴露的输入和 ViewModel 暴露的状态源绑定起来。
    void bind(IGameCommandSink& commandSink,
              IGameSnapshotSource& snapshotSource);

private:
    GameWidget* m_gameWidget = nullptr;
    std::unique_ptr<QObject> m_binding;
};

} // namespace alleyfist
