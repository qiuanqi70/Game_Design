# 环境与构建

本项目使用 CMake + vcpkg 管理构建和第三方依赖。当前 `vcpkg.json` 已声明 Qt 依赖，后续如果增加音效库、图片库等，也统一加到 `vcpkg.json`。

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

# 第一轮迭代：基础框架与界面设计
第一轮首先完成Common 层的编写，随后并行开发 view 层和 ViewModel 层，最后在 app 层完成跑动与攻击动画的显示；
- Common 层：存放数据结构。这里在 common 层中放置玩家、小怪、boss 以及背景地图四个类的声明以及定义。在 common 层完成编写后 view 层和 viewmodel 层的编写将根据common 层中定义的数据结构分别独立进行。
- View 层：view 层中隔离地实现了底层的图片管理（Animation 类、ResourceManager 类）与动画播放（draw 方法），同时也实现了键盘事件的响应。
- ViewModel 层：加载和改变需要呈现的各种数据，提供 common 对象集合，提供可用来显示的数据类，提供了一系列 get,set 函数暴露可用来绑定的数据属性。
- App 层：创建 common 层、view 层、ViewModel 层的对象，并实现它们之间的相互绑定。
# 第二轮迭代：游戏逻辑与交互设计
目标：
• 实现主角与敌人（Boss）之间的基本交互和战斗逻辑。
• 增加游戏元素：增加主角的攻击手段、显示血量。
• 添加基本的动画效果和音效支持。
任务：
• 完善角色和敌人的战斗逻辑（ViewModel）。
• 实现攻击动作和反馈效果的动画支持。
• 引入背景音乐和击打音效，增强游戏的沉浸感和交互性。
# 第三轮迭代：优化与扩展
目标：
• 优化游戏性能和用户体验。
• 扩展游戏功能，增加 boss 战斗和更多的角色交互。
• 完善用户界面和视觉效果。
任务：
• 进行代码优化和性能调优，确保游戏在不同平台上的流畅运行。
• 实现 boss 类型角色的战斗逻辑和动画效果。
• 增加游戏界面的交互元素和动态效果，提升用户体验。
优化背景以及击打效果血条，添加游戏胜利失败结算界面，并引入重置按钮清空游戏状态重新开始。
