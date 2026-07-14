# AlleyFist

`AlleyFist` 是一个使用 C++17、Qt6、CMake 与 vcpkg 实现的横版街机动作游戏原型，中文主题为“双截龙/巷战双截龙”。当前版本采用 MVVM 分层组织：Common 提供公共状态和通知工具，ViewModel 负责规则推进，View 负责 Qt 输入和绘制，App 负责生命周期与绑定。

## 当前功能

- 标题画面、游戏中、暂停、失败、胜利和重开流程。
- A/D/W/S 或方向键控制玩家在带纵深的街道上移动。
- J/Z 轻攻击，K/X 重攻击，Space 跳跃，P/Esc 暂停，Enter 开始或确认，R 重开。
- 玩家血量、精力、疲劳提示、关卡进度条、敌人血条和 Boss 血条。
- 普通敌人追踪玩家并近身造成伤害；玩家攻击会命中附近敌人。
- 玩家推进到关卡后段时生成 Boss，击败 Boss 后进入胜利结算。

## 目录结构

```text
code/
  common/       # 基础值类型、GameState、EventTrigger
  viewmodel/    # GameViewModel、GameSimulation、实体规则
  view/         # MainWindow、GameWidget、输入和绘制
  app/          # GameApplication 组合根
report/         # 实验报告 Markdown 与 PDF 构建脚本
```

## 环境与构建

本项目使用 CMake Presets + vcpkg 管理构建和第三方依赖。当前 `vcpkg.json` 声明的依赖为 `qtbase`。

首次配置环境：

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

Windows 下对应设置 `VCPKG_ROOT` 到自己的 vcpkg 目录，并运行 `bootstrap-vcpkg.bat`。

配置和编译：

```bash
cmake --preset vcpkg
cmake --build --preset vcpkg
```

如果 IDE 支持 CMake Presets，直接选择 `vcpkg` preset 即可。

## 架构说明

跨层绑定参考课程示例中的 properties / commands / notification 模式：

- Properties：ViewModel 暴露 `const GameState*`，View 只读状态并绘制。
- Commands：ViewModel 暴露 `std::function` 命令，View 在键盘事件和定时器中调用。
- Notification：ViewModel 通过 `EventTrigger` 发出状态变化通知，View 收到后触发重绘。

View 不包含 ViewModel 头文件，也不直接修改游戏数据；ViewModel 不依赖 Qt 控件或绘图 API；App 层集中完成对象创建和绑定。
