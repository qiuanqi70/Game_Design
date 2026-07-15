#pragma once

#include <string>
#include <unordered_map>

namespace alleyfist {

/// 简单的跨平台异步音效管理器。
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
