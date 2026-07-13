#pragma once

namespace alleyfist {

// 基础值类型放在 Common，是因为 View 绘制和 ViewModel 输出快照都会用到这些简单数据形状。
// 这里不放内部编号、碰撞矩形或规则状态，避免 Common 层暴露具体业务细节。

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

// 通用数值条：生命、能量、进度等由使用方定义含义。
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
struct WorldPosition {
    float x = 0.0f;
    float laneY = 0.0f;
    float z = 0.0f;
};

} // namespace alleyfist
