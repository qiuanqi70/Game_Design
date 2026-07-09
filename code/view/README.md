# View Layer

A 负责这里。

View 层负责 Qt 窗口、双缓冲绘制、键盘事件和界面交互。

View 只能依赖 Common 中的公共契约：

- 输入事件转成 `alleyfist::GameCommand`。
- 每帧读取 `alleyfist::GameSnapshot`。
- 根据 snapshot 绘制地图、角色、敌人、血条、精力条、GO 提示、胜负界面。

View 不依赖 ViewModel，不直接修改游戏数据，不写 AI、碰撞、伤害、刷怪和推图逻辑。
