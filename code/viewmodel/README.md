# ViewModel Layer

B 负责这里。

ViewModel 层负责游戏逻辑和数据暴露：

- 处理 `alleyfist::GameCommand`。
- 推进 Tick、移动、攻击、跳跃、连招、受击和死亡状态。
- 处理敌人 AI、Boss 行为、碰撞、伤害、精力恢复。
- 处理遭遇战锁屏、清屏解锁、GO 提示和胜负判定。
- 维护并暴露 `alleyfist::GameSnapshot`。
- 在 `SimulationTypes.h` 中维护规则参数、AI 行为、攻击类型、攻击盒和刷怪配置等内部类型。

ViewModel 不依赖 View，也不直接绘制画面。
