#pragma once

namespace alleyfist {

// 基础值类型是 View 和 ViewModel 传递属性数据时共用的“小积木”。
// 这里不放输入命令、流程状态、角色状态等规则概念，避免 Common 层承担业务职责。

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

// 通用数值条：具体含义由使用方定义，可以是生命、能量、进度等。
struct ResourceBar {
    int current = 0;
    int maximum = 0;

    float ratio() const noexcept
    {
        return maximum > 0 ? static_cast<float>(current) / static_cast<float>(maximum) : 0.0f;
    }

    bool is_empty() const noexcept
    {
        return maximum <= 0 || current <= 0;
    }
};

// 通用世界坐标：x 表示主轴位置，laneY 表示纵深/行位置，z 表示高度/层次。
// 如果某个模块不需要 laneY 或 z，可以只使用 x，或者在自己的属性类型中重新定义更小的坐标结构。
struct WorldPosition {
    float x = 0.0f;
    float laneY = 0.0f;
    float z = 0.0f;
};

} // namespace alleyfist
