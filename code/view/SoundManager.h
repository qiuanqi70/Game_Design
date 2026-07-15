#pragma once

#include <string>
#include <unordered_map>

namespace alleyfist {

/// 简单音效管理器，封装 Windows PlaySound。
/// 异步播放，不阻塞游戏循环。
class SoundManager {
public:
    /// 初始化，assetsPath 指向 assets/sfx/ 目录。
    static void init(const std::string& assetsPath);

    /// 按名称播放音效（异步，不阻塞）。
    /// @param name  文件名（不含路径和 .wav 后缀）
    static void play(const std::string& name);

private:
    static std::string s_assetPath;
    static std::unordered_map<std::string, std::string> s_pathCache;
};

} // namespace alleyfist
