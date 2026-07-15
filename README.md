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

## 自动化测试

顶层 `CMakeLists.txt` 通过 `include(CTest)` 启用测试，`BUILD_TESTING` 默认值为 `ON`。首次配置或需要显式开启测试时运行：

```bash
cmake --preset vcpkg -DBUILD_TESTING=ON
cmake --build --preset vcpkg
ctest --test-dir build --output-on-failure
```

只构建并运行本次补充的 View、App 测试：

```bash
cmake --build --preset vcpkg --target alleyfist_view_smoke_test alleyfist_app_smoke_test
ctest --test-dir build -R "^alleyfist_(view|app)_smoke_test$" --output-on-failure
```

也可以只运行某一层：

```bash
ctest --test-dir build -R "^alleyfist_view_smoke_test$" --output-on-failure
ctest --test-dir build -R "^alleyfist_app_smoke_test$" --output-on-failure
```

不需要测试目标的发布构建可在重新配置时传入 `-DBUILD_TESTING=OFF`。

如果 `build` 目录曾经由其他 CMake generator 配置，而 preset 使用的是 Ninja，请先改用空的构建目录或清理旧的生成文件，否则 CMake 会报告 generator 不一致。

### 测试内容

- `alleyfist_view_smoke_test`：覆盖 `MovementIntent`，方向键与 WASD、动作键及其别名，按下/释放、同键去重、自动重复、未知键和空回调；验证 `MainWindow` 对属性与命令的转发、通知触发重绘、定时器停止/恢复与帧号递增。
- View 测试还会把空状态、标题、游戏中、暂停、失败、胜利、普通波次、Boss 入场、所有角色动作、远程投射物、两类掉落物、HUD 及异常数值状态绘制到 `QImage`，检查各分支能够产生有效画面且不会崩溃。
- `alleyfist_app_smoke_test`：构造真实的 `GameApplication`，检查 Qt 应用元数据和窗口对象图，并通过真实按键事件覆盖 App 层绑定的 property、tick、四向移动、攻击、跳跃、重置、确认、暂停和 notification；最后用定时退出验证 `run()` 会显示窗口并正确返回事件循环退出码。

### 测试原理

CTest 根据各层 `CMakeLists.txt` 中的 `add_test` 找到测试程序，每个测试在独立进程中运行，并以退出码判断成功或失败。测试内部使用 Qt Test 提供逐用例断言，因此失败时 `--output-on-failure` 会显示具体用例和断言位置。

View 测试不是直接调用私有处理函数，而是向 `GameWidget` 分派真实 `QKeyEvent`，让事件沿正常的按键处理路径进入已注入的命令回调；绘制测试调用 `QWidget::render` 输出到内存中的 `QImage`，验证状态到画面的转换。App 测试则验证完整链路：

```text
GameWidget 输入 -> command -> GameViewModel -> GameState
                 <- View update <- notification <-
```

两个测试都通过 CTest 设置 `QT_QPA_PLATFORM=offscreen`，无需真实显示器，也不会弹出窗口。测试关注输入映射、分层绑定、生命周期和绘制稳定性，不与容易受操作系统字体或素材缩放影响的固定基准截图做逐像素比较；游戏素材仍由主程序资源目标负责打包，测试目标在素材不可用时覆盖 View 的后备绘制路径。

## 架构说明

跨层绑定参考课程示例中的 properties / commands / notification 模式：

- Properties：ViewModel 暴露 `const GameState*`，View 只读状态并绘制。
- Commands：ViewModel 暴露 `std::function` 命令，View 在键盘事件和定时器中调用。
- Notification：ViewModel 通过 `EventTrigger` 发出状态变化通知，View 收到后触发重绘。

View 不包含 ViewModel 头文件，也不直接修改游戏数据；ViewModel 不依赖 Qt 控件或绘图 API；App 层集中完成对象创建和绑定。
