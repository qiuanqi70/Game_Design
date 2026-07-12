#pragma once

namespace alleyfist {

// 基础值类型放在 Common，是因为 View 绘制和 ViewModel 输出快照都会用到这些简单数据形状。
// 这里不放内部编号、碰撞矩形或规则状态，避免 Common 层暴露 View 不需要知道的模拟细节。

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

// 通用资源条：血量、精力、Boss 血条等都可以使用它。
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

// 横版格斗世界坐标：x 表示关卡推进，laneY 表示街道纵深，
// z 表示离地高度，用于跳跃或击飞。
struct WorldPosition {
    float x = 0.0f;
    float laneY = 0.0f;
    float z = 0.0f;
};

} // namespace alleyfist
