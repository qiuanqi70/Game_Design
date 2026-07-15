#include "SoundManager.h"

#include <windows.h>
#include <mmsystem.h>

namespace alleyfist {

std::string SoundManager::s_assetPath;
std::unordered_map<std::string, std::string> SoundManager::s_pathCache;

void SoundManager::init(const std::string& assetsPath)
{
    s_assetPath = assetsPath;
    if (s_assetPath.back() != '/' && s_assetPath.back() != '\\') {
        s_assetPath += '/';
    }
    s_pathCache.clear();
}

void SoundManager::play(const std::string& name)
{
    // 缓存查找
    auto it = s_pathCache.find(name);
    if (it == s_pathCache.end()) {
        std::string fullPath = s_assetPath + name + ".wav";
        // 检查文件是否存在
        DWORD attrs = GetFileAttributesA(fullPath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return; // 文件不存在，静默跳过
        }
        s_pathCache[name] = fullPath;
        it = s_pathCache.find(name);
    }

    PlaySoundA(it->second.c_str(), nullptr,
               SND_ASYNC | SND_NODEFAULT | SND_NOSTOP);
}

} // namespace alleyfist
