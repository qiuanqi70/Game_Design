#pragma once

#include <QMainWindow>

namespace alleyfist {

class GameWidget;

/// @brief 游戏主窗口，承载 GameWidget 作为中心控件。
///
/// MainWindow 是 View 层对外的入口之一。
/// App 层创建 MainWindow 后调用 show() 即可显示游戏窗口。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /// 获取内部的 GameWidget，供 App 层绑定使用。
    GameWidget* gameWidget() const { return m_gameWidget; }

private:
    GameWidget* m_gameWidget = nullptr;
};

} // namespace alleyfist
