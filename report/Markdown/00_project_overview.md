# 1 实验概述

本实验仓库实现的是一款横版街机动作游戏，主题为“双截龙”。项目使用 C++17、Qt6、CMake 与 vcpkg 组织工程，当前代码已经形成了较清晰的 MVVM 分层：`common` 层提供跨层公共状态和通知工具，`viewmodel` 层负责游戏规则和状态推进，`view` 层负责 Qt 窗口、输入和绘制，`app` 层作为组合根管理生命周期以及属性、命令、通知三类绑定。

从玩法目标看，玩家扮演一名格斗角色，在带有纵深的城市街道上从左向右推进。当前实现支持标题画面、移动、跳跃、轻攻击、重攻击、血量、精力、疲劳提示、普通敌人追击、Boss 触发、暂停、失败、胜利和重开等核心流程。普通敌人在初始阶段生成并主动靠近玩家；当玩家推进到关卡后段时，系统生成 Boss，击败 Boss 后进入胜利结算。

## 1.1 组内分工

本组以层次边界作为分工单位，每位成员既负责对应代码，也负责对应分报告：

- 苏易文：负责 App/Common 层，分报告为 `02_app_common_layer.md`，主要内容包括生命周期、组合根、公共状态、通知工具和三类绑定。
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

## 1.3 当前完成情况

当前仓库已经完成了游戏的基本可运行骨架和核心流程：

- 工程层面：通过 CMake 分 target 组织 `alleyfist_common`、`alleyfist_viewmodel`、`alleyfist_view`、`alleyfist_app` 和最终可执行文件 `AlleyFist`。
- 公共契约：使用 `types.h` 定义 `Size`、`WorldPosition`、`ResourceBar` 等基础值类型；使用 `game_state.h` 定义 `GameState`、`ActorState`、`MapState`、`HudState` 等只读显示状态；使用 `notification.h` 提供 `EventTrigger` 与 `EventNotification`。
- ViewModel：通过 `GameViewModel` 对外暴露属性和命令，通过 `GameSimulation` 维护实体列表和规则推进，支持玩家移动、跳跃、攻击、精力恢复、普通敌人追击、Boss 生成、失败和胜利判定。
- View：实现 60 FPS 定时器、键盘输入映射、世界坐标到屏幕坐标的投影、街道背景、视差建筑、角色几何绘制、血条、精力条、Boss 血条、进度条、标题画面和胜负覆盖层。
- App：通过 `GameApplication` 创建 `QApplication`、`GameViewModel` 和 `MainWindow`，并集中完成 `properties / commands / notification` 绑定。
