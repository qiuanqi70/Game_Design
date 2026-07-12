#pragma once

// 便捷入口：需要完整 Common 契约时可以直接包含这个头文件。
// 注意：业务实现文件优先包含自己真正需要的具体头，避免无意中扩大层间依赖。
#include "actions.h"
#include "contracts.h"
#include "snapshot.h"
#include "types.h"
