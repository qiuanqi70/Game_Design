# View Layer

陈棋隆负责这里。

View 层负责 Qt 窗口、输入事件、展示状态、定时刷新、素材加载、声音提示和界面绘制。它只依赖 Common 中的跨层契约，不依赖 ViewModel 的具体类型，也不直接修改游戏规则数据。

## 文件与职责

- `InputDefs.h`：`InputState` 按语义方向记录实际按下的物理键。同一方向的别名键（例如 A 和 Left）全部释放后才发送方向松开，动作键也会去重；失焦或隐藏时统一清空，避免粘键。
- `PresentationState.h/.cpp`：保存仅影响画面的瞬时状态，包括角色动画时钟、平滑血条、震屏、拾取反馈和声音线索。规则状态仍以只读 `GameState` 为唯一来源。
- `AssetCatalog.h/.cpp`：集中初始化 QRC、加载贴图和字体，并根据角色种类、动作及外观变体选择动画片段；素材缺失时由 `GameWidget` 使用程序化图形兜底。
- `GameWidget.h/.cpp`：接收只读 `GameState`，把键盘事件转换为命令，驱动 tick，计算世界坐标投影，并绘制背景、角色、投射物、掉落物、粒子、HUD、遭遇提示和各阶段覆盖层。
- `MainWindow.h/.cpp`：View 对 App 的入口，持有唯一的 `GameWidget`，转发 property、command 和 notification，不包含规则逻辑。
- `SoundManager.h/.cpp`：播放 View 接受到的展示声音线索；音效文件仍从可执行文件旁的 `assets/sfx` 读取。
- `Assets/`：View 自己拥有的美术素材。更详细的运行时资源与创作源素材边界见 `Assets/README.md`。

## 分层与数据流

View 持有 `const alleyfist::GameState*`，绘制期间只读该快照；空指针会落到内部空状态，因此窗口在绑定前也可以安全绘制。输入与定时 tick 通过 App 注入的 `std::function` 命令发给 ViewModel；`EventNotification` 只请求重绘，不携带或复制规则状态。

```text
GameWidget 输入/tick -> command -> ViewModel -> GameSimulation -> GameState
                       <- 重绘请求 <- notification <----------+
```

因此 AI、碰撞、伤害、刷怪、推图和胜负判断不属于本层。动画帧、震屏、平滑血条等展示细节也不会反向写入规则层。

## 定时器生命周期

`GameWidget` 默认希望持续运行，但只有控件可见时才启动约 60 FPS 的 Qt 定时器；`hideEvent` 会停止定时器并释放当前输入，重新显示后按运行意图恢复。这样隐藏窗口不会继续产生 tick，也不会留下按键状态。

`setRunning(bool)` 是显式生命周期控制：App 的嵌入场景可以暂停或恢复 View tick，测试也可以用它固定画面。`showEvent`、`hideEvent` 和 `setRunning` 共同决定定时器是否实际工作，而不是维护第二套游戏暂停状态。

## 美术资源

`code/view/CMakeLists.txt` 中 `qt_add_resources(... FILES ...)` 的显式清单决定哪些文件会编入 `alleyfist_view`，运行时统一使用 `:/art/...` 路径。`AssetCatalog` 显式初始化该静态库拥有的资源，所以主程序和单独链接 View 的测试都能读取素材。

`Assets/Aseprite`、`Assets/界面图/PSD`、逐帧拆图等未列入 `FILES` 的目录是创作源文件，只保存在仓库中，不会扩大可执行文件。增加或替换运行时图片、字体时，必须同时更新 CMake 的 `FILES` 清单和 `AssetCatalog` 中的资源路径。

## View 测试

View 的测试模块位于 `code/tests/ViewTest.cpp`，目标名为 `alleyfist_view_test`。从仓库根目录配置、构建并只运行该模块：

```bash
cmake --preset vcpkg -DBUILD_TESTING=ON
cmake --build --preset vcpkg --target alleyfist_view_test
ctest --test-dir build -R '^alleyfist_view_test$' --output-on-failure
```

也可以直接运行测试程序；无图形桌面的环境需指定 Qt 离屏平台：

```bash
QT_QPA_PLATFORM=offscreen ./build/code/tests/alleyfist_view_test
```

Windows 多配置构建的可执行文件通常位于 `build/code/tests/Debug/alleyfist_view_test.exe` 等配置目录。CTest 已为该测试设置 `QT_QPA_PLATFORM=offscreen`，正常使用上面的 CTest 命令时无需额外设置。

测试覆盖以下边界：

- WASD/方向键别名、相反方向同时按下、最后一个物理键释放、动作去重、自动重复、未知键、失焦和隐藏清理。
- `MainWindow` 的 property、全部 command、notification 转发，以及 show/hide/`setRunning` 的定时器生命周期和持续重绘。
- QRC 文件存在性与 `QPixmap` 解码，防止资源只在源码目录存在却未编入测试目标。
- 空状态、非法视口、所有游戏阶段、全部角色种类与动作、外观变体、无素材兜底、投射物、掉落物、遭遇提示和 HUD 绘制分支。

输入测试向真实 `QWidget` 分派 `QKeyEvent`，验证正式事件处理和回调路径。绘制测试通过 `QWidget::render()` 得到内存中的 `QImage`，再比较角色、HUD 或覆盖层等目标区域的像素变化；限定区域能证明预期元素确实改变，同时比整张截图基线更少受字体、平台抗锯齿和缩放差异影响。数据驱动用例复用同一套断言覆盖枚举分支，避免为每种角色或阶段复制近似测试代码。
