# AlleyFist

`AlleyFist` 是一个使用 C++17、Qt6、CMake 和 vcpkg 实现的横版街机动作游戏原型。项目采用 MVVM 分层：规则、状态映射、Qt 展示和对象装配分别归属独立模块，测试则集中在统一目录中，避免生产层 CMake 混入测试细节。

## 当前功能

- 标题、游戏中、暂停、失败、胜利和重新开始流程。
- A/D/W/S 或方向键控制玩家在带纵深的街道上移动。
- J/Z 轻攻击，K/X 重攻击，Space 跳跃，P/Esc 暂停，Enter 开始或确认，R 重开。
- 玩家生命、精力和疲劳提示，关卡进度、敌人血条及 Boss 血条。
- 多种敌人行为、波次门槛、近战碰撞、远程投射物、掉落物和 Boss 战。

## 目录与职责

```text
code/
  common/       # 跨层基础类型、GameState、EventTrigger
  viewmodel/    # GameSimulation、GameViewModel、内部模拟类型
  view/         # Qt 输入、展示状态、素材目录、绘制、窗口和声音
    Assets/     # 由 alleyfist_view 编入 Qt Resource System 的美术资源
  app/          # GameApplication 组合根与生命周期
  tests/        # ViewModelTest、ViewTest、AppTest 及统一测试构建入口
assets/
  sfx/          # 运行时音效，由主程序的构建后步骤复制到输出目录
report/         # 实验报告及其构建脚本
```

各层的边界如下：

- `GameSimulation` 是游戏规则的唯一负责人，处理移动、动作、精力、敌人 AI、碰撞、遭遇与波次、投射物和掉落物。
- `GameViewModel` 驱动 `GameSimulation`，把内部实体映射为跨层 `GameState`，并向 View 暴露只读 property、command 和 notification。
- View 不依赖 ViewModel 的具体类型。`InputState` 记录物理键与语义方向，`PresentationState` 保存纯展示用动画和音效状态，`AssetCatalog` 集中解析素材，`GameWidget` 负责输入与绘制，`MainWindow` 转发绑定，`SoundManager` 负责音效播放。
- `GameApplication` 是组合根，只创建对象、完成绑定并管理 Qt 应用与窗口生命周期，不承载游戏规则。

美术素材位于 `code/view/Assets`，其 QRC 由 `alleyfist_view` 目标拥有，运行时以 `:/art/...` 路径读取。音效仍位于仓库根目录的 `assets/sfx`，构建 `AlleyFist` 后会复制到可执行文件旁的 `assets` 目录。

## 环境与构建

项目通过 CMake Presets 和 vcpkg manifest 获取 Qt6。`vcpkg.json` 当前声明 `qtbase`，其中包含 Widgets、Gui 和 Test 等本项目使用的模块。

首次准备 vcpkg（Windows 使用对应的 `.bat` 脚本并设置同名环境变量）：

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

配置并构建：

```bash
cmake --preset vcpkg
cmake --build --preset vcpkg
```

支持 CMake Presets 的 IDE 可以直接选择 `vcpkg` preset。若现有 `build` 目录曾由其他 generator 生成，应改用新的构建目录或先处理旧生成文件，避免与 preset 使用的 Ninja 冲突。

## 运行

构建完成后运行输出目录中的 `AlleyFist`。命令行常见路径如下：

```bash
./build/code/AlleyFist
```

Windows 多配置环境中的可执行文件可能位于 `build/code/Debug/AlleyFist.exe` 或对应配置目录。

## 自动化测试

根 CMake 通过 `include(CTest)` 提供 `BUILD_TESTING` 开关，默认开启。测试目标统一定义在 `code/tests/CMakeLists.txt`；关闭测试时不会查找 Qt Test，也不会生成任何测试程序。

每个测试用例的具体场景和断言见 [`code/tests/README.md`](code/tests/README.md)。

配置、构建并运行全部测试：

```bash
cmake --preset vcpkg -DBUILD_TESTING=ON
cmake --build --preset vcpkg
ctest --test-dir build --output-on-failure
```

只构建三个测试目标：

```bash
cmake --build --preset vcpkg --target \
  alleyfist_viewmodel_test alleyfist_view_test alleyfist_app_test
```

只运行某一层：

```bash
ctest --test-dir build -R '^alleyfist_viewmodel_test$' --output-on-failure
ctest --test-dir build -R '^alleyfist_view_test$' --output-on-failure
ctest --test-dir build -R '^alleyfist_app_test$' --output-on-failure
```

不生成测试目标的构建方式：

```bash
cmake --preset vcpkg -DBUILD_TESTING=OFF
cmake --build --preset vcpkg
```

### 覆盖范围

- `alleyfist_viewmodel_test` 覆盖初始化与重置、非法时间步、命令与通知、四向及相反方向输入、轻重攻击帧、受击打断、精力耗尽与恢复、跳跃和空中攻击、各类敌人行为、远程投射物、掉落物、波次门槛、Boss 与胜利流程。
- `alleyfist_view_test` 覆盖输入意图、方向键与 WASD、动作键别名、按下/释放、去重、自动重复、未知键和空回调；同时验证窗口绑定、通知重绘、定时器，以及空状态、所有游戏阶段、角色动作、投射物、掉落物和 HUD 的绘制分支。
- `alleyfist_app_test` 构造真实 `GameApplication`，验证应用元数据、唯一窗口对象图、property/command/notification 完整绑定链、四向移动、各动作、暂停/确认/重置、tick 推进和 `run()` 事件循环。

### 测试原理

CTest 负责发现 `add_test()` 注册的三个可执行程序、隔离进程并根据退出码汇总结果；程序内部使用 Qt Test 的用例发现、数据驱动测试和断言，失败详情可由 `--output-on-failure` 输出。

View 和 App 测试向真实 QWidget 分派 `QKeyEvent`，使事件经过正式输入和绑定路径，而不是直接调用私有函数。绘制测试使用 `QWidget::render()` 输出到内存 `QImage`：基础分支检查图像非空且包含有效内容，关键交互则比较玩家、精力条或覆盖层等限定区域内的像素变化。区域检查既能确认预期部分确实更新，又避免整张基准截图因字体、平台渲染或素材缩放差异产生大量误报。

`alleyfist_view_test` 和 `alleyfist_app_test` 显式导入 Qt 的 offscreen 平台插件，并由 CTest 设置 `QT_QPA_PLATFORM=offscreen`，因此持续集成或无显示器环境也能创建、派发事件和渲染 QWidget，不会弹出真实窗口。

完整绑定方向为：

```text
GameWidget 输入 -> command -> GameViewModel -> GameSimulation -> GameState
                 <- repaint  <- notification <---------------+
```

测试强调规则边界、输入映射、跨层绑定、生命周期和绘制稳定性；展示动画等只影响画面的状态保留在 View 内，不反向污染规则层。
### 持续集成

项目提供两层 CI，本地和云端独立运行。

**本地日常构建 `ci_build.sh`**

```bash
# 拉取最新代码 → 检查 Qt 工具链 → 编译 Release
bash ci_build.sh

# 编译完成后自动启动游戏运行 5 秒
bash ci_build.sh --run
```

流程：git pull → 验证 Qt/MinGW/Ninja 可用 → cmake configure → cmake build → 可选运行。日志输出到 `ci_build.log`。

通过 Windows 任务计划程序可设为每天定时执行：程序填 `C:\Program Files\Git\bin\bash.exe`，参数填 `/d/mytools/c++_project/Game_Design/ci_build.sh --run`，起始于项目根目录。

**GitHub Actions 云端 CI `.github/workflows/ci.yml`**

每次 push、PR 以及每天凌晨 2 点自动触发：

- 使用 `jurplel/install-qt-action` 自动安装 Qt 6.8 + MinGW 工具链
- cmake configure + build（Release）
- 运行 `alleyfist_app_smoke_test` 验证绑定链路完整

推送到 GitHub 仓库即自动生效，无需额外配置。
