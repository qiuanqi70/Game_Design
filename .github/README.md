# 持续集成说明

本目录保存 AlleyFist 工程的持续集成配置。当前使用 GitHub Actions，在 Ubuntu 24.04 和 Windows 2022 上验证 Debug 测试和 Release 打包流程。

## 文件结构

```text
.github/
├── README.md
└── workflows/
    └── ci.yml
```

## 工作流触发条件

工作流名称为 `AlleyFist CI`，配置文件为 `.github/workflows/ci.yml`，会在以下情况下运行：

- 推送到 `main` 分支
- 推送到 `master` 分支
- 创建或更新 Pull Request
- 在 GitHub Actions 页面手动执行

## 跨平台 Job

工作流目前包含四个 Job：

| Job | Runner | 内容 |
|---|---|---|
| `debug-test` | Ubuntu 24.04 | Linux Debug 构建和 CTest |
| `release-package` | Ubuntu 24.04 | Linux Release 构建和 TGZ 打包 |
| `windows-debug-test` | Windows 2022 | Windows Debug 构建和 CTest |
| `windows-release-package` | Windows 2022 | Windows Release 构建和 ZIP 打包 |

Linux Job 使用 `apt-get` 安装 Qt 所需的 X11/XCB 开发包；Windows Runner 使用 PowerShell 初始化 vcpkg，不执行 Linux 专用的系统依赖安装命令。

Windows Release 安装阶段会调用 Qt 的部署脚本，将 Qt DLL 和平台插件一起复制到 CPack 安装目录中。

## Debug 测试流程

`debug-test` Job 执行以下步骤：

1. 检出源代码。
2. 安装 Ninja、Autotools、Bison、Flex，以及 Qt 需要的 X11/XCB 开发库。
3. 下载并初始化 vcpkg。
4. 恢复 vcpkg 下载缓存和二进制缓存。
5. 使用 `vcpkg-debug` preset 配置 Debug 构建。
6. 编译工程。
7. 使用 CTest 执行所有已注册测试。

对应的构建目录为 `build-debug/`，CTest 命令为：

```shell
ctest --test-dir build-debug --output-on-failure --timeout 120
```

Qt 图形界面测试运行在无窗口环境中，工作流设置了：

```text
QT_QPA_PLATFORM=offscreen
```

因此 `ViewTest` 和 `AppTest` 不依赖 CI 机器上的桌面环境。

## Release 打包流程

`release-package` Job 和 `windows-release-package` Job 执行以下步骤：

1. 安装构建依赖并初始化 vcpkg。
2. 使用 `vcpkg-release` preset 配置 Release 构建。
3. 编译工程。
4. 使用 CPack 生成安装包。
5. 将生成的安装包上传为 GitHub Actions Artifact。

对应的构建目录为 `build-release/`，打包命令为：

```shell
cpack --config build-release/CPackConfig.cmake --verbose
```

Linux 下当前生成 TGZ 压缩包，文件名类似：

```text
AlleyFist-0.1.0-Linux.tar.gz
```

Windows 下生成 ZIP 压缩包，文件名类似：

```text
AlleyFist-0.1.0-Windows.zip
```

安装规则位于根目录的 `CMakeLists.txt`，包括：

- 安装 `AlleyFist` 可执行文件
- 安装运行所需的 `assets/` 目录

## 本地复现 CI

### 环境变量

需要先准备 vcpkg，并设置 `VCPKG_ROOT`：

```shell
export VCPKG_ROOT=/home/qqq/vcpkg
```

如果 vcpkg 位于其他目录，请替换为实际路径。

### Debug 构建和测试

```shell
cmake --preset vcpkg-debug
cmake --build --preset debug --parallel
QT_QPA_PLATFORM=offscreen ctest \
  --test-dir build-debug \
  --output-on-failure \
  --timeout 120
```

### Windows Debug 构建和测试

在 Windows PowerShell 中设置 vcpkg 路径后执行：

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset vcpkg-debug
cmake --build --preset debug --parallel
$env:QT_QPA_PLATFORM = "offscreen"
ctest --test-dir build-debug -C Debug --output-on-failure --timeout 120
```

### Windows Release 构建和打包

```powershell
cmake --preset vcpkg-release
cmake --build --preset release --parallel
cpack --config build-release\CPackConfig.cmake --verbose
```

建议分别使用 `build-debug/` 和 `build-release/`，不要在同一个构建目录中反复切换 Debug 和 Release。

## 依赖缓存

工作流缓存以下目录：

```text
vcpkg/downloads/
.vcpkg-binary-cache/
```

缓存键由操作系统以及 `vcpkg.json`、`vcpkg-configuration.json` 的内容决定。修改 vcpkg 依赖清单后，缓存键会变化，CI 会自动建立新的缓存。

## 常见问题

### CTest 显示 `No tests were found`

通常表示 CMake 配置没有成功完成，或者使用了 `BUILD_TESTING=OFF` 的构建目录。请确认使用的是 Debug preset：

```shell
cmake --preset vcpkg-debug
cmake --build --preset debug --parallel
ctest --test-dir build-debug --output-on-failure
```

### QtBase 配置失败：`X11_SM_FOUND = ""`

如果日志出现以下错误：

```text
ERROR: Feature "xcb_sm": Forcing to "ON" breaks its condition
X11_SM_FOUND = ""
```

说明缺少 X11 Session Management 开发库。安装 QtBase 常用的 X11/XCB 开发依赖：

```shell
sudo apt-get update
sudo apt-get install -y \
  libegl1-mesa-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libice-dev \
  libsm-dev \
  libx11-xcb-dev \
  libxi-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libxrender-dev
```

安装完成后，删除失败的构建目录并重新配置：

```shell
rm -rf build-debug
cmake --preset vcpkg-debug
```

`CMake Error: CMAKE_MAKE_PROGRAM is not set` 通常是 QtBase 配置失败后的连带错误。确认 `/usr/bin/ninja` 存在即可，不要只根据这条错误判断 Ninja 缺失。如果确实没有 Ninja，可以安装：

```shell
sudo apt-get install ninja-build
```

### vcpkg 安装依赖失败

先检查网络和 vcpkg 路径：

```shell
echo "$VCPKG_ROOT"
ls "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

如果错误来自 GitHub 源码下载，应检查网络代理或重新执行配置。CI 使用 vcpkg 下载缓存，可以减少重复下载，但首次构建仍需要访问依赖源。

### Qt 测试启动失败

在无桌面环境下运行 Qt 测试时设置：

```shell
export QT_QPA_PLATFORM=offscreen
```

工作流已经默认设置该变量。

## 修改 CI 配置的注意事项

- 修改构建命令时，应同步更新本 README 和 `CMakePresets.json`。
- 新增 vcpkg 依赖后，应确认 CI 的系统构建工具是否满足该依赖要求。
- 新增测试后，应确保测试通过 `add_test()` 注册到 CTest。
- 修改安装目录或安装文件后，应检查 CPack 压缩包内容。
- 不要提交 `build-debug/`、`build-release/`、`vcpkg_installed/` 或 vcpkg 缓存目录；这些目录已在 `.gitignore` 中排除。
