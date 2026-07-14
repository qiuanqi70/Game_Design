# 1 实验概述

本实验仓库实现的是一款横版街机动作游戏，主题为“双截龙”。项目使用 C++17、Qt6、CMake 与 vcpkg 组织工程，当前代码已经形成了较清晰的 MVVM 分层：`common` 层提供跨层公共状态和通知工具，`viewmodel` 层负责游戏规则和状态推进，`view` 层负责 Qt 窗口、输入和绘制，`app` 层作为组合根管理生命周期以及属性、命令、通知三类绑定。

从玩法目标看，玩家扮演一名格斗角色，在带有纵深的城市街道上从左向右推进。当前实现支持标题画面、移动、跳跃、轻攻击、重攻击、血量、精力、疲劳提示、普通敌人追击、Boss 触发、暂停、失败、胜利和重开等核心流程。普通敌人在初始阶段生成并主动靠近玩家；当玩家推进到关卡后段时，系统生成 Boss，击败 Boss 后进入胜利结算。

## 1.1 组内分工

本组以层次边界作为分工单位，每位成员既负责对应代码，也负责对应分报告：

- 苏易文：负责 App/Common 层以及最终报告的撰写，分报告为 `02_app_common_layer.md`，主要内容包括生命周期、组合根、公共状态、通知工具和三类绑定。
- 陈棋隆：负责 View 层，分报告为 `03_view_layer.md`，主要内容包括窗口、画布、输入、HUD 和覆盖层。
- 邱安琪：负责 ViewModel 层，分报告为 `04_viewmodel_layer.md`，主要内容包括命令回调、状态推进、AI、战斗和胜负判定。

## 1.2 协作与版本管理

为了体现组内协作过程，本项目在开发中持续使用 Git 和 GitHub 记录提交历史。三位成员围绕不同层次并行推进：Common/App 层先确定公共状态和绑定方式，View 层根据公共状态完成输入和绘制，ViewModel 层独立实现规则模拟和状态同步，最后通过主分支合并和冲突处理完成整合。

\begin{figure}[H]
\centering
\includegraphics[width=0.92\linewidth]{Markdown/image.png}
\caption{GitHub Contributors 统计图}
\end{figure}

图中的 GitHub 账号与组员对应关系如下：`wys917` 对应苏易文，`qiuanqi70` 对应邱安琪，`odetta1` 对应陈棋隆。从 GitHub Contributors 页面可以看到三位成员均有提交记录，说明项目并不是由单人集中完成，而是按照分层职责持续推进。

需要补充说明的是，上图中统计的提交数和增删行数不能等同于实际工作量，本次任务中我们小组全程保持顺畅的沟通，每个人都很负责的完成了自己的部分，并在有余力时对队友的工作提出建议或帮助，三人的工作量是一致的。

\begin{figure}[H]
\centering
\includegraphics[width=0.86\linewidth]{Markdown/git-history-combined.png}
\caption{Git 提交历史与协作解耦记录}
\end{figure}

上图合并展示了项目的提交历史。可以看到仓库中存在多次围绕 `viewmodel`、`view`、`common` 和 `app` 的独立提交与分支合并，并不是在同一个大文件中串行修改，而是按照 MVVM 层次拆分任务，并持续进行解耦：把公共契约从具体规则中分离出来，让 View 和 ViewModel 可以围绕稳定的 `GameState`、命令回调和通知机制并行开发，最后再通过 merge 将各层成果整合到主分支。

## 1.3 团队协作情况

本组整体沟通与合作较为顺畅。三位成员按照 MVVM 层次划分职责，在明确各自代码边界的基础上并行开发，并通过线下讨论、文档说明和 Git 提交及时同步接口变化与实现进度。Common/App 层负责确定共享数据结构和绑定方式，View 层围绕稳定的 `GameState` 完成输入与绘制，ViewModel 层独立推进游戏规则和状态同步，最终由 App 层集中完成 properties / commands / notification 的组装与联调。这样的协作方式减少了多人同时修改同一文件的情况，也使各成员能够专注于自己负责的模块。

协作中的主要亮点体现在以下几个方面：

- **分工边界清晰。** 各成员以 App/Common、View、ViewModel 为责任单元，代码文件和分报告均与分工对应，既便于独立推进，也便于出现问题时快速定位负责模块。
- **接口能够及时对齐。** Common 层通过 `GameState`、`EventNotification` 等公共契约统一跨层数据，ViewModel 暴露的命令接口与 View 的 setter 接口保持对应，降低了联调阶段因命名和职责理解不一致产生的沟通成本。
- **成员之间能够互相配合。** 当公共类型、绑定方式或状态字段发生调整时，成员会及时沟通并共同完成适配；在完成各自任务后，也会对其他模块的设计和实现提出建议，而不是只关注个人代码。
- **版本管理较为规范。** 三位成员均通过 Git 提交自己的阶段性成果，并通过分支合并完成整合。提交历史能够反映各层的开发与解耦过程，也为出现问题后的回溯和比较提供了依据。

目前的协作方式仍有进一步改进的空间。项目初期公共类型的命名和字段组织经历了多次调整，导致依赖这些接口的 View 和 ViewModel 需要重复适配；后续可以在编码前先形成简要的接口文档，并在修改公共契约前由全组确认。当前代码审查主要依靠线下讨论，缺少较系统的 Pull Request 审查和检查清单，部分潜在问题可能只能在最终联调时发现。此外，自动化测试主要集中在 ViewModel，跨层绑定、键盘输入和界面绘制仍以人工验证为主。后续若增加固定的阶段同步、交叉代码审查和基础集成测试，可以进一步降低返工成本，提高团队协作的稳定性与工程质量。

## 1.4 当前完成情况

当前仓库已经完成了游戏的基本可运行骨架和核心流程：

- 工程层面：通过 CMake 分 target 组织 `alleyfist_common`、`alleyfist_viewmodel`、`alleyfist_view`、`alleyfist_app` 和最终可执行文件 `AlleyFist`。
- 公共契约：使用 `types.h` 定义 `Size`、`WorldPosition`、`ResourceBar` 等基础值类型；使用 `game_state.h` 定义 `GameState`、`ActorState`、`MapState`、`HudState` 等只读显示状态；使用 `notification.h` 提供 `EventTrigger` 与 `EventNotification`。
- ViewModel：通过 `GameViewModel` 对外暴露属性和命令，通过 `GameSimulation` 维护实体列表和规则推进，支持玩家移动、跳跃、攻击、精力恢复、普通敌人追击、Boss 生成、失败和胜利判定。
- View：实现 60 FPS 定时器、键盘输入映射、世界坐标到屏幕坐标的投影、街道背景、视差建筑、角色几何绘制、血条、精力条、Boss 血条、进度条、标题画面和胜负覆盖层。
- App：通过 `GameApplication` 创建 `QApplication`、`GameViewModel` 和 `MainWindow`，并集中完成 `properties / commands / notification` 绑定。

## 1.5 阶段性成果展示

当前版本已经完成从标题画面、关卡战斗到胜负结算的主要交互流程。为了直观展示本阶段的开发成果，下面按照实际游戏流程选取六张运行截图，并采用每行三张的方式集中呈现。

\begin{figure}[H]
\centering
\begin{minipage}[t]{0.315\linewidth}
\centering
\includegraphics[width=\linewidth]{游戏运行截图/开始界面.png}
\small （a）开始界面
\end{minipage}\hfill
\begin{minipage}[t]{0.315\linewidth}
\centering
\includegraphics[width=\linewidth]{游戏运行截图/进入游戏之后.png}
\small （b）普通战斗场景
\end{minipage}\hfill
\begin{minipage}[t]{0.315\linewidth}
\centering
\includegraphics[width=\linewidth]{游戏运行截图/遇到boss.png}
\small （c）Boss 战场景
\end{minipage}

\vspace{0.45cm}

\begin{minipage}[t]{0.315\linewidth}
\centering
\includegraphics[width=\linewidth]{游戏运行截图/按P暂停.png}
\small （d）暂停界面
\end{minipage}\hfill
\begin{minipage}[t]{0.315\linewidth}
\centering
\includegraphics[width=\linewidth]{游戏运行截图/游戏失败.png}
\small （e）失败结算界面
\end{minipage}\hfill
\begin{minipage}[t]{0.315\linewidth}
\centering
\includegraphics[width=\linewidth]{游戏运行截图/胜利界面.png}
\small （f）胜利结算界面
\end{minipage}
\caption{AlleyFist 当前阶段主要运行界面}
\end{figure}

各截图反映的阶段性成果如下：

- **开始界面（a）：** 游戏启动后首先进入标题阶段，画面展示项目名称、中文主题、主要操作键位和开始提示。玩家按 Enter 后即可进入正式游戏，说明标题状态与确认命令已经完成联动。
- **普通战斗场景（b）：** 进入游戏后，玩家可以在带纵深的街道中移动并与多个普通敌人战斗。界面顶部显示玩家生命值和精力，底部显示关卡推进进度，体现了角色绘制、敌人 AI、战斗规则、HUD 与镜头跟随等功能的综合效果。
- **Boss 战场景（c）：** 当玩家推进到关卡后段时，系统会在前方生成体型和血量更高的 Boss，并在界面顶部显示独立的 Boss 血条。该画面说明关卡触发条件、Boss 实体生成、特殊角色绘制和首领状态同步已经打通。
- **暂停界面（d）：** 玩家按 P 或 Esc 后，游戏进入暂停阶段并显示半透明覆盖层，同时保留暂停前的场景和 HUD。再次按键即可继续游戏，验证了阶段切换、时间推进控制和覆盖层绘制功能。
- **失败结算界面（e）：** 玩家生命值清空后进入 Game Over 状态，界面显示存活时间和击败敌人数量，并允许玩家重新开始。这表明生命判定、统计数据、失败状态切换和重开流程已经形成完整闭环。
- **胜利结算界面（f）：** 玩家击败 Boss 后进入胜利状态，画面展示通关提示、完成时间和击败数量。该界面直观体现了 Boss 击败判定、胜利结算以及整局游戏流程的最终成果。

以上截图表明，项目已经形成可连续体验的游戏原型，并打通了输入响应、规则推进、界面更新和流程切换，为后续完善动画、音效、连招及关卡内容奠定了基础。
