#include "SoundManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace alleyfist {

std::string SoundManager::s_assetPath;
std::unordered_map<std::string, std::string> SoundManager::s_pathCache;

void SoundManager::init(const std::string& assetsPath)
{
    QString resolvedPath = QDir::cleanPath(QString::fromStdString(assetsPath));
    const QFileInfo requestedPath(resolvedPath);
    if (requestedPath.isRelative()) {
        const QString workingDirectoryPath = QDir::current().absoluteFilePath(resolvedPath);
        if (QDir(workingDirectoryPath).exists()) {
            resolvedPath = workingDirectoryPath;
        } else {
            resolvedPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(resolvedPath);
        }
    }

    s_assetPath = QDir::cleanPath(resolvedPath).toStdString();
    s_pathCache.clear();
}

void SoundManager::play(const std::string& name)
{
    auto it = s_pathCache.find(name);
    if (it == s_pathCache.end()) {
        const QDir assetDirectory(QString::fromStdString(s_assetPath));
        const QString fullPath = assetDirectory.filePath(QString::fromStdString(name) + ".wav");
        if (!QFileInfo::exists(fullPath)) {
            return;
        }

        s_pathCache[name] = QDir::cleanPath(fullPath).toStdString();
        it = s_pathCache.find(name);
    }

    const QString path = QString::fromStdString(it->second);

#ifdef Q_OS_WIN
    PlaySoundW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
               SND_ASYNC | SND_FILENAME | SND_NODEFAULT | SND_NOSTOP);
#elif defined(Q_OS_MACOS)
    QProcess::startDetached("/usr/bin/afplay", QStringList{path});
#else
    if (!QProcess::startDetached("paplay", QStringList{path})) {
        QProcess::startDetached("aplay", QStringList{path});
    }
#endif
}

} // namespace alleyfist
