#!/bin/bash
# AlleyFist CI 日常构建脚本
# 用法: bash ci_build.sh [--run]
#   --run  编译后自动启动游戏运行 5 秒
#
# 可通过 Windows 任务计划程序每天自动执行：
#   程序: C:\Program Files\Git\bin\bash.exe
#   参数: /d/mytools/c++_project/Game_Design/ci_build.sh
#   起始于: D:\mytools\c++_project\Game_Design

set -euo pipefail
cd "$(dirname "$0")"

QT_PATH="/d/Qt/6.8.3/mingw_64"
TOOLS_PATH="/d/Qt/Tools/mingw1310_64/bin:/d/Qt/Tools/Ninja"
BUILD_DIR="build"
LOG_FILE="ci_build.log"

echo "========== $(date '+%Y-%m-%d %H:%M:%S') ==========" | tee "$LOG_FILE"

# 1. 检查 Qt 和 MinGW
echo "[1/4] Checking tools..." | tee -a "$LOG_FILE"
if [[ ! -d "$QT_PATH" ]]; then
    echo "ERROR: Qt not found at $QT_PATH" | tee -a "$LOG_FILE"
    exit 1
fi
if [[ ! -f "$QT_PATH/bin/Qt6Core.dll" ]]; then
    echo "ERROR: Qt6Core.dll not found" | tee -a "$LOG_FILE"
    exit 1
fi
echo "  Qt: OK" | tee -a "$LOG_FILE"

# 2. 拉取最新代码
echo "[2/4] Pulling latest code..." | tee -a "$LOG_FILE"
if git rev-parse --git-dir >/dev/null 2>&1; then
    git pull --rebase origin main 2>&1 | tee -a "$LOG_FILE" || echo "  git pull skipped (no network or no changes)" | tee -a "$LOG_FILE"
else
    echo "  Not a git repo, skipping pull" | tee -a "$LOG_FILE"
fi

# 3. 编译
echo "[3/4] Building..." | tee -a "$LOG_FILE"
export PATH="$TOOLS_PATH:$PATH"
rm -rf "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || true
cmake -B "$BUILD_DIR" -G "Ninja" -DCMAKE_PREFIX_PATH="$QT_PATH" \
    -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG_FILE"
cmake --build "$BUILD_DIR" 2>&1 | tee -a "$LOG_FILE"

EXE="$BUILD_DIR/code/AlleyFist.exe"
if [[ ! -f "$EXE" ]]; then
    echo "BUILD FAILED: exe not found at $EXE" | tee -a "$LOG_FILE"
    exit 1
fi
echo "  Build: OK → $EXE" | tee -a "$LOG_FILE"

# 4. 运行测试（可选）
if [[ "${1:-}" == "--run" ]]; then
    echo "[4/4] Running game (5 seconds)..." | tee -a "$LOG_FILE"
    export PATH="$QT_PATH/bin:$PATH"
    timeout 5 "$EXE" 2>&1 | tee -a "$LOG_FILE" || true
    echo "  Run: done" | tee -a "$LOG_FILE"
else
    echo "[4/4] Skipped (use --run to launch game)" | tee -a "$LOG_FILE"
fi

echo "========== DONE ==========" | tee -a "$LOG_FILE"
