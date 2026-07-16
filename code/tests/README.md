# 自动化测试说明

本目录使用 Qt Test 编写测试，并通过 CTest 注册三个独立测试程序：

| 测试程序 | 源文件 | 主要职责 |
| --- | --- | --- |
| `alleyfist_viewmodel_test` | `ViewModelTest.cpp` | 验证游戏规则模拟、ViewModel 命令、公共状态映射和完整关卡流程。 |
| `alleyfist_view_test` | `ViewTest.cpp` | 验证输入状态、窗口绑定、资源加载、计时器和离屏渲染。 |
| `alleyfist_app_test` | `AppTest.cpp` | 使用真实应用对象验证 View、ViewModel 与应用组合根的端到端连接。 |

View 和 App 测试使用 Qt 的 `offscreen` 平台，不会显示真实窗口。视觉断言通过 `QWidget::render()` 生成 `QImage`，再比较指定区域的像素变化；这种方式验证的是元素是否出现或改变，不要求不同平台生成完全相同的整张截图。

## ViewModelTest.cpp

### 测试辅助场景

该文件只使用公开的 `GameSimulation` 和 `GameViewModel` 接口搭建场景，不直接修改内部状态：

- `step_until` 和 `step_for`：按固定时间步推进模拟，等待动作、AI 或关卡状态变化。
- `move_player_to`、`move_near_entity` 和 `move_to_safe_attack_edge`：通过四方向输入移动玩家。
- `defeat_entity` 和 `defeat_current_regular_wave`：通过正常轻攻击击败敌人。
- `reach_wave`：依次清理前置波次，进入指定波次。
- `prepare_isolated_ranged_enemy`：只保留一个远程敌人，避免其他敌人干扰投射物测试。

### 测试用例

#### `simulationInitialStateResetAndInvalidDelta`

- 新模拟包含一个玩家和四个敌人，玩家 ID、类型、位置、能量和计时均为预期初值。
- `0`、负数、`NaN` 和无穷大的时间步不会推进时间或移动玩家。
- 正常移动和跳跃会改变水平位置、高度及能量。
- `reset()` 会恢复实体数量、玩家位置、高度、能量、时间和 Idle 行为状态。

#### `viewModelLifecycleCommandsAndNotifications`

- ViewModel 初始公开状态处于 Title，并提供开始提示、敌人和玩家 HUD。
- notification 只统计 `kGameStateChangedEvent`，可验证命令执行后的通知次数。
- 方向键按下本身不立即通知，释放、确认、tick 等同步操作会产生通知。
- Confirm 从 Title 进入 Playing；Pause 冻结模拟；再次 Confirm 恢复游戏。
- 暂停期间移动和 tick 不会推进游戏时间；恢复后有效 tick 会推进时间。
- Reset 恢复 Playing 初态、玩家位置、时间和能量。
- 移除 notification 后不再保留该订阅。

#### `viewModelMapsActionCommandsAndHudState`

- Primary 命令映射为轻攻击，并将能量从 100 降到 88。
- 轻攻击未结束时 Secondary 命令被拒绝，不改变动作和能量。
- 动作计时结束后公共 `ActorState` 回到 Idle。
- Secondary 命令在空闲时映射为重攻击，并将能量降到 75。
- StateToggle 映射为跳跃，并将能量降到 82。
- 跳跃期间 Primary 映射为空中攻击，同时更新高度和 HUD 能量。

#### `movementDirectionsOppositesAndWaveGate`

- 右、左、上、下输入分别按预期改变 `x` 或 `laneY`，相反方向可回到原位。
- 左右和上下同时按住时互相抵消，玩家保持原位和 Idle。
- 第一波未清理前，玩家最多移动到 `x = 760`，继续推进模拟也无法穿过门禁。

#### `lightAndHeavyAttacksUseDistinctHitFrames`

- 轻攻击消耗 12 能量；攻击锁定期间不能插入重攻击或重复扣能量。
- 轻攻击在约 `0.08 s` 的命中帧造成 10 点伤害，并产生 `Light` 冲击。
- 同一次轻攻击不会在后续帧重复伤害，`impactRevision` 只增加一次。
- 重攻击消耗 25 能量，在约 `0.18 s` 的命中帧造成 18 点伤害，并产生 `Heavy` 冲击。
- 同一次重攻击同样只能命中一次。

#### `playerHurtStateRejectsAndInterruptsActions`

- 玩家进入巡逻敌人的接触范围后会受到攻击并进入 Hurt 状态。
- Hurt 状态具有有效硬直计时，期间轻攻击和跳跃均被拒绝，也不会扣除能量。
- 一次受击不会在后续紧邻帧重复增加 `impactRevision`。
- 硬直结束后玩家可以再次请求跳跃。

#### `energyExhaustionAndRecovery`

- 连续执行重攻击会持续消耗能量，直到低于 25 点并进入 exhausted 状态。
- exhausted 状态拒绝新的重攻击。
- 推进恢复时间后，能量达到阈值、exhausted 解除，重攻击重新可用。

#### `jumpOwnsHeightAndSupportsAirAttack`

- 跳跃消耗 18 能量，并且跳跃期间不能重复起跳。
- 空中按轻攻击会切换到 AirAttack、再消耗 12 能量；此时不能切换重攻击。
- 跳跃高度随时间上升，空中攻击结束后恢复 Jump 状态。
- 最终高度回到 0，玩家落地并恢复 Idle。

#### `enemyBehaviorStatesAndRangedProjectileAreReachable`

- 第一波巡逻敌人在近距离进入 `MeleeAttack`。
- 第二波会生成远程、伏击和冲锋类型敌人。
- 伏击敌人在有效距离进入 `Ambush`。
- 冲锋敌人在有效距离进入 `Charge`。
- 远程敌人在射程内进入 `RangedAttack`，生成投射物，并携带 `RangedGunner` 视觉变体。

#### `projectileHitsGroundedPlayer`

- 在只保留一个远程敌人的场景中等待一枚新投射物生成。
- 地面玩家被命中后损失 8 点生命。
- 玩家 `impactRevision` 增加一次，`lastImpact` 为 `Heavy`。
- 投射物命中或生命周期结束后会离开模拟集合。

#### `jumpAtProjectileLaunchAvoidsGroundTrajectory`

- 远程敌人按玩家地面位置发射投射物后，玩家立即跳跃。
- 投射物最终离开模拟集合，但玩家生命不变。
- 玩家没有新增受击修订，证明跳跃高度参与投射物碰撞判断。

#### `defeatedEnemiesDropCollectiblePickups`

- 巡逻敌人死亡后生成 Health 补给。
- 玩家移动到补给位置后，补给会被拾取并从模拟集合移除。
- 远程敌人死亡后至少生成一个 Energy 补给。
- 敌人均通过正常攻击击败，测试同时覆盖掉落位置和拾取范围。

#### `wavesGatesBossEncounterAndVictory`

- 初始遭遇为第 1 波，总波数为 3。
- 第一波门禁位于 `x = 760`；清敌后进入第二波。
- 第二波门禁位于 `x = 1540`，远程敌人使用 `RangedGunner` 变体。
- 第三波门禁位于 `x = 2260`，远程敌人使用 `RangedRobot` 变体。
- 三波清理完成后普通遭遇进入 Cleared，玩家可以前往 Boss 区域。
- 进入 Boss 区域后先处于 Intro，Intro 期间 Boss 尚未生成。
- Intro 结束后生成 140 最大生命的 Boss，并进入 Fighting。
- Boss 可以进入 Charge，且只能通过合法攻击流程击败。
- Boss 死亡后遭遇进入 Cleared；死亡展示计时结束前胜利未就绪，结束后 `boss_victory_ready()` 才变为真。

## ViewTest.cpp

### 测试夹具

- `initTestCase`：切换到空临时目录，创建必要的音效目录，并构造 `AssetCatalog` 初始化静态 qrc 资源。
- `cleanupTestCase`：释放素材目录并恢复原工作目录。
- 测试状态生成器默认使用固定的 `960 x 540` 逻辑视口；需要验证响应式布局时另用 `480 x 270`。

### 输入与窗口用例

#### `inputStateTracksAliasesUntilTheLastPhysicalKeyIsReleased`

- `A` 和左方向键同时代表 Left，但只产生一次方向按下边沿。
- 重复按下或重复释放同一个实体键不会产生额外边沿。
- 释放其中一个别名键时 Left 仍保持活动；释放最后一个别名键才产生停止边沿。
- 清理后四个方向均不活动。

#### `inputStateKeepsOppositeDirectionsIndependentAndClearsAtomically`

- Left、Right 和 Up 可以同时记录，互相不会覆盖。
- `clear_movement()` 返回清理前的活动方向快照，并一次清空全部方向。
- 清理后的旧 KeyRelease 不产生边沿，新 KeyPress 可以正常重新激活方向。

#### `inputStateRejectsInvalidDirectionsAndDeduplicatesActions`

- `MovementDirection::Count` 和强制转换的非法枚举值都被拒绝。
- 动作键第一次按下有效，未释放前重复按下无效。
- 释放动作键或调用 `clear_actions()` 后，该键可以再次触发。

#### `widgetPreservesAliasAndOppositeDirectionEdges`

- 真实 `QKeyEvent` 经过 `GameWidget` 后，同方向别名只向 command 发送一次 `true` 和一次 `false`。
- Left 与 Right 可同时处于按下状态，并分别发送自己的释放边沿。

#### `widgetRejectsAutoRepeatAndDeduplicatesActions`

- 移动键和攻击键的自动重复 KeyPress/KeyRelease 均被忽略。
- 同一实体键未释放前的重复 KeyPress 不重复调用 command。
- 正常释放后再次点击会再次调用动作 command。

#### `focusOutAndHideReleaseMovementAndActionState`

- 控件失去焦点时，已按住的 Left 和 Up 都会自动发送 `false`。
- FocusOut 同时清空动作去重状态，之后 J 键可以重新触发。
- 控件隐藏时，已按住的 Right 和 Down 同样自动释放。
- Hide 同样清空动作状态，重新显示后 K 键可以重新触发。

#### `mainWindowForwardsPropertyEveryCommandAndNotification`

- MainWindow 的中央控件、标题和最小尺寸配置正确。
- `set_game_state` 被转发给 GameWidget，Playing/Paused 状态会绘制不同画面。
- 左右上下四个移动命令都收到按下和释放两次调用。
- Primary、Secondary、StateToggle、Reset、Confirm 和 Pause 各收到一次调用。
- tick 的 `dt` 位于合法范围，frame 大于 0。
- MainWindow 暴露的 notification 会使 GameWidget 产生 Paint 或 UpdateRequest。
- 窗口显示时计时器持续调用 tick，隐藏后 tick 停止。
- `set_game_state(nullptr)` 可解除状态绑定。

#### `timerLifecycleStartsOnShowAndStopsOnHide`

- GameWidget 未显示时不会产生 tick。
- 显示后至少产生两个 tick，帧号严格递增，`dt` 保持合法。
- 隐藏后 tick 数量不再增加。

### 资源与渲染用例

#### `overlayPhasesKeepRepainting`

数据行覆盖 Title、Paused、GameOver 和 Win。每种阶段在没有外部状态通知时仍应产生至少两次 Paint，用于维持覆盖层动画。

#### `qrcImagesExistAndDecode`

数据行覆盖以下关键资源：

- 玩家待机图。
- 普通敌人行走图。
- Boss 攻击图。
- 场景 tileset。
- 生命 HUD。
- 界面边框。
- 机器人投射物。

每个资源都必须存在、可只读打开、文件大小大于 0，并可解码为宽高有效的 `QPixmap`。

#### `qrcFontExists`

- 像素字体的 qrc 路径存在。
- 字体资源可以打开，且内容非空。

#### `phaseOverlaysChangeTheirExpectedRegions`

- Title 相比 Playing 会改变标题横幅和开始按钮区域。
- Paused 会改变中央覆盖层区域。
- GameOver 和 Win 都会改变结果横幅区域。
- Win 与 GameOver 的结果横幅彼此可区分。

#### `actorKindsRenderInsideTheActorRegion`

数据行覆盖 Player、Patroller、Ambusher、Charger、RangedGunner、RangedRobot 和 Boss。每种角色都以对应动作、阵营和视觉变体构造，并必须在角色区域相对空白基线产生足够像素变化。

#### `actorActionsRenderInsideTheActorRegion`

数据行覆盖 Idle、Walk、LightAttack、HeavyAttack、RangedAttack、Ambush、Charge、Jump、AirAttack、Hurt 和 Dead。每个动作都必须产生可见角色绘制；Hurt 行额外设置重击冲击修订，以覆盖受击反馈分支。

#### `projectileKindAndTeamAffectTheTargetRegion`

- `ThrownObject` 投射物会在目标区域出现。
- Team 从 Enemy 改为 Player 时，投射物颜色发生变化。
- `RangedRobot` 视觉变体会改用机器人专属投射物素材。

#### `pickupKindsAffectTheTargetRegion`

- Health 补给会在目标区域出现。
- 将种类改为 Energy 后，目标区域画面发生变化，证明两种补给可区分。

#### `encounterUsesWaveNumbersAndBossIntroProgress`

- 当前波次从 1 改为 2 时，波次 HUD 发生变化。
- 总波数从 3 改为 4 时，波次 HUD 发生变化。
- 遭遇切换为 Boss Intro 后，进度从 0 到 1 会显著改变中央演出区域。

#### `missingActorArtUsesTheProceduralFallback`

- 为 Player 构造没有对应素材的 RangedAttack，确认素材目录返回空精灵图。
- 即使精灵图缺失，角色区域仍有可见像素变化，证明程序化后备绘制生效。

#### `invalidViewportFallsBackAndKeepsCriticalLayoutStable`

- 逻辑视口宽度为 0、高度为负数时使用后备尺寸，不发生无效计算。
- 生命和能量条最大值为 0 或负数时仍可渲染。
- 进度值从 0 改为越界的 5 时会按合法范围钳制并更新底部进度条。
- Playing/Paused 覆盖层在 `960 x 540` 和 `480 x 270` 下都可区分。
- 两种输出图片保持请求的实际尺寸，关键布局在缩放后仍稳定。

`overlayPhasesKeepRepainting_data`、`qrcImagesExistAndDecode_data`、`actorKindsRenderInsideTheActorRegion_data` 和 `actorActionsRenderInsideTheActorRegion_data` 是 Qt Test 的数据提供函数，不单独执行断言；它们分别为紧随其后的同名测试提供上述数据行。

## AppTest.cpp

### 测试夹具

- `initTestCase`：从真实 `GameApplication` 创建的顶层控件中确认只有一个 MainWindow，并取得其 GameWidget。
- `cleanup`：每个用例结束后停止计时器、隐藏窗口并清理刷新事件。
- `main`：使用 offscreen 平台和空临时工作目录运行测试，验证程序不依赖工作目录中的外部素材。

### 测试用例

#### `applicationMetadataIsConfigured`

- 当前 Qt 应用实例就是 `qApp`。
- 应用名为 `Alley Fist`，版本为 `0.1.0`，组织名为 `AlleyFist`。
- `GameApplication` 不允许复制构造、复制赋值、移动构造或移动赋值。

#### `compositionRootCreatesTheViewObjectGraph`

- 组合根创建的 MainWindow 和 GameWidget 均存在，测试开始时窗口不可见。
- 窗口标题、窗口最小尺寸、GameWidget 最小尺寸和强焦点策略正确。
- GameWidget 是 MainWindow 的中央控件，父子对象关系正确。

#### `propertyCommandAndNotificationFormACompleteLoop`

- 初始 Title 画面通过真实 GameWidget 渲染。
- Return 键经过 Confirm command 使公共 GameState 进入 Playing。
- 状态变化通过 notification 触发 MainWindow 刷新请求。
- Title 与 Playing 的整帧画面有显著差异，证明 property、command 和 notification 构成完整闭环。

#### `fourDirectionBindingsMoveThePlayerInTheExpectedDirection`

- 分别按 A、D、W、S 并让真实 View 计时器短暂推进模拟。
- 四个方向都必须在玩家区域产生可见变化。
- Left 的变化中心位于 Right 左侧，Up 的变化中心位于 Down 上侧，排除命令缺失或方向接反。

#### `actionBindingsProduceDistinctActionsAndEnergyCosts`

- J、K 和 Space 分别触发轻攻击、重攻击和跳跃。
- 三种动作都会改变能量条，证明都按规则消耗能量。
- 三组动作后的玩家区域两两不同，证明公共动作状态及其渲染没有混用。

#### `pauseConfirmAndResetBindingsRestoreStableStates`

- P 键从 Playing 进入 Paused，并出现明显暂停覆盖层。
- Paused 时按 Return 可恢复，恢复后的覆盖层接近原 Playing 画面。
- K 键重攻击会改变能量条。
- R 键重置后能量条与初始 Playing 状态一致。

#### `timerTickDrivesTheBoundSimulation`

- 按住 D 后启动 GameWidget 的真实计时器。
- 短暂等待再停止计时器，玩家区域必须发生明显变化。
- 变化包围盒位于预期右移位置，证明 View 的 tick command 实际推进了绑定的 Simulation。

#### `runShowsTheWindowAndReturnsTheEventLoopCode`

- 调用 `GameApplication::run()` 前 MainWindow 处于隐藏状态。
- Qt 事件循环启动后可以观察到窗口已经显示。
- 测试主动以状态码 23 退出，`run()` 必须原样返回 23。

## 构建与运行

在项目根目录执行：

```bash
cmake --preset vcpkg -DBUILD_TESTING=ON
cmake --build --preset vcpkg
ctest --test-dir build --output-on-failure
```

只运行某一组测试：

```bash
ctest --test-dir build -R '^alleyfist_viewmodel_test$' --output-on-failure
ctest --test-dir build -R '^alleyfist_view_test$' --output-on-failure
ctest --test-dir build -R '^alleyfist_app_test$' --output-on-failure
```
