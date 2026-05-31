# 项目架构

## 方案 A 基线

当前产品工程由 `ai_xiaozhi/projects/xiaozhi` 复制而来，目录为：

```text
/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
```

选择复制工程而不是直接在上游示例里改，是为了把产品板级约束、资源路径、
AI 玩具业务和后续 SDK 补丁隔离出来。

## 源码角色

| 路径 | 角色 |
| --- | --- |
| `cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy` | 我们自己的产品工程 |
| `bk_avdk_smp` | 官方底层 SDK 和驱动组件 |
| `ai_xiaozhi` | Xiaozhi AI 语音对话参考工程 |
| `WIKI` | 官方教程和框架说明 |
| `原理图` | 当前开发板硬件来源 |
| `CMAiW82AL_开发板硬件配置说明_已验证.md` | 已交叉验证的板级事实 |
| `/home/jason/armino/cmaiw82al_xiaozhi` | 旧适配工程，只作只读参考 |

## AP/CP 分工

BK7258 使用 AP + CP 架构。当前产品工程保持官方 SMP 分工：

- CP 负责 Wi-Fi/日志/IPC/基础系统，并在启动后拉高 GPIO19 `POWER_LOCK`。
- AP 负责 UI、音频、协议、文件系统、摄像头和产品业务。
- AP 日志经 mailbox 转发到 CP，再从 UART0 输出。
- CP UART0 使用 GPIO10/11，AP 不直接占用 UART0 物理管脚。

## 主要模块

| 模块 | 入口 |
| --- | --- |
| AP 入口 | `ap/ap_main.c` |
| CP 入口 | `cp/cp_main.c` |
| 系统状态 | `ap/main/system_manager/` |
| AI 对话 | `ap/main/dialog/` |
| 协议 | `ap/main/protocols/` |
| 音频 | `ap/components/bk_app_audio/` |
| 双屏 UI | `ap/main/ui/` |
| 本板 LCD 设备 | `ap/main/boards/cmaiw82al_lcd.*` |
| VFS 和 `/sf0` | `ap/main/fs/app_vfs.c` |
| 摄像头 MCP | `ap/main/iot/iot_camera.c` |

## 当前工程策略

- 产品代码优先通过 AP/CP 工程内模块实现。
- SDK 改动尽量避免；必须改 SDK 时，在 `patch/` 记录原因、补丁和回放步骤。
- 从上游同步功能时，先对照 `05_upstream_conflicts.md`，尤其检查 GPIO、LCD、
  资源路径、MIC 和 Kconfig。
