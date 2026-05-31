# 构建、烧录和资源

## 构建

```sh
cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

输出：

```text
build/bk7258/cmaiw82al_ai_toy/package/all-app.bin
build/bk7258/cmaiw82al_ai_toy/package/app_pack.rbl
build/bk7258/cmaiw82al_ai_toy/package/build_summary.txt
```

当前一次构建摘要：

```text
CP FLASH used 872580 B / 0x000f0000, 88.76%
AP FLASH used 2070336 B / 0x002a0000, 75.22%
OTA app size 3053376 B, compressed/encrypted 1900768 B
```

2026-05-27 救援固件：

```text
output/2026-05-27-avi-boot-static-rescue/app_pack.rbl
  size: 1900864
  sha256: baa23f3277a2ed50416778a7cc50340d5a1652d4ad891d5a34671c134fb7208d
output/2026-05-27-avi-boot-static-rescue/all-app.bin
  size: 3313888
  sha256: 65dfa095ea40657c6b78f516e4ee85cfb7fa9ffb2b2cf337808888f06ab2570b
```

该固件默认不启动 AVI 播放线程，也不自动启动 `eye_carousel`，用于恢复稳定串口、
OTA 和后续显示调试窗口。若旧固件已经反复 AP assert/CP watchdog 或无串口输出，
不要继续在异常 dump 阶段发送 `update ota`；先用 BK 烧录工具刷
`all-app.bin` 一次。

## 静态护栏

```sh
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

护栏覆盖：

- LCD 尺寸、设备名、DCX/reset、背光。
- `/sf0` 资源路径，避免回到 `/sd0`。
- AP GPIO 复用，避免恢复 SDIO/RGB LCD 默认映射。
- 主麦 MIC2/R 和 PA GPIO8。
- 旧双屏 AVI 组件不能恢复 `/sd0`、错误 GPIO 或共享 reset 假设。

换到 `ZTB071TBIG05` 160x160 GC9D01 新屏时，必须同步更新静态护栏。护栏应从
旧的 `LCD_WIDTH=240`、`LCD_HEIGHT=2*240`、`cmaiw82al_gc9a01_240` 切到
新的 `LCD_WIDTH=160`、`LCD_HEIGHT=2*160`、产品化 GC9D01 设备名，并继续禁止
`/sd0`、错误 GPIO、错误 reset 和 MIC/PA 回退。

## 烧录和资源注意

- `all-app.bin` 只包含主 flash 固件。
- 眼睛 AVI 不会自动打进 `all-app.bin`。
- 外部资源根目录是 `/sf0`，默认需要 `neutral.avi`。
- 当前 VFS 会尝试按 15MiB、16MiB、旧 1MiB 三种 littlefs 布局挂载外部 Flash。
- 量产时需要单独准备 `/sf0` littlefs 镜像或资源升级流程。
- 新 160x160 双屏接入 AVI 后，视频资源应按官方要求转换为双屏合成
  `320x160` AVI；本板文件位置仍是 `/sf0/neutral.avi`，不是官方 SD NAND
  `/sd0`。
- 当前优先使用根目录 `bk_avi` 派生的 YUV422 资源镜像：
  `output/2026-05-26-sf0-yuv422-repack/sf0_bk_avi_320x160_yuv422_repack_15m.bin`。
  它已上板验证为 `mjpeg 320x160 yuvj422p 25fps`，硬解成功且播放约 25fps。
  不要再把旧 YUV420 `/sf0` 镜像作为硬解调试基线。

## 资源目录约定

```text
resources/eyes/      AVI 源文件、导出说明、sf0 镜像说明
resources/prompts/   提示音、角色提示词、离线语音资源说明
```

后续可以把资源生成脚本放到 `tools/`，但不要假设 SDK stock BKFIL 会自动写入
外部 `/sf0`。

## AP/CPU1 启动超时恢复

若冷启动日志只剩 CP 心跳，例如：

```text
reset_cpu1_core at: 02100000, start=1
cp0 boot cp1[0] time out, boot cp1 fail!!!
IPC retry to start core1
[CMAI][CP] alive tick=... POWER_LOCK(GPIO19)=1
```

且没有任何 `ap0:bk_ipc_core_init` 或 AP 初始化日志，说明 AP/CPU1 在运行 AP
应用前就没有完成启动握手。此时不要继续排查 `/sf0`、AVI、JPEG 或 LVGL；这些
模块还没有运行机会。

处理顺序：

1. 先刷已知能进入 AP 日志的归档主固件，例如
   `output/2026-05-26-avi-guard-update/all-app.bin`
   （SHA256: `0681ccfb5693955532b983e40334e71bd5ad0f94495384e3c3de237189ef16c6`）。
2. 若归档包可以进入 AP，再隔离当前 SDK 脏改动，重新用干净 SDK 全量构建。
   2026-05-27 的干净 SDK 恢复包在
   `recovery_builds/2026-05-27-clean-sdk/package/all-app-clean-sdk.bin`
   （SHA256: `5e43c89b1bf8026f668d73c0d2c2e6eebb868c3efad31b993c91ff8bcbcb84e1`）。
3. 若归档包也不能进入 AP，优先怀疑烧录文件或板端 internal flash 内容不对。
   读取并比对 AP 分区 `0x110000` 起始向量，期望前 16 字节类似
   `00 40 06 28 55 92 11 02 0f 91 11 02 fd 90 11 02`。

`reset_cpu1_core at: 02100000` 本身不是异常；这是 AP 镜像链接地址/flash 映射
后的正常启动地址。异常点是 AP 没有回传 CPU1 boot success 事件。
