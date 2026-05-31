# Eyes 资源

本目录用于管理双眼动画源资源。运行时资源不从这里直接读取，而是烧录到外部
SPI Flash littlefs 后挂载为 `/sf0`。

当前固件默认查找：

```text
/sf0/neutral.avi
```

建议资源命名：

```text
neutral.avi
curious.avi
pensive.avi
smiling.avi
happy.avi
photo.avi
confused.avi
tired.avi
```

注意：

- `all-app.bin` 不包含这些 AVI。
- AVI 格式、码率、帧尺寸和切换频率会直接影响卡顿和 JPEG decode 稳定性。
- 资源镜像生成和烧录流程确认后，应把脚本和参数补到本目录或 `tools/`。
- BK7258 硬件 JPEG 解码要求每帧 baseline JPEG 带 DQT0 和 DQT1。若日志出现
  `avi jpeg invalid ... reason=missing dqt table`，说明 `/sf0/neutral.avi`
  仍是单量化表资源，需要重刷外部 `/sf0` 资源镜像。

## 当前首选 320x160 动画资源

2026-05-28 早些时候根目录 `/home/jason/armino1/bk_avi` 中的首选 AVI 已核对为：

```text
mjpeg 320x160 yuvj422p 25fps
```

当前首选外部 `/sf0` 镜像：

```text
/home/jason/armino1/cmaiw82al_ai_toy/output/2026-05-26-sf0-yuv422-repack/sf0_bk_avi_320x160_yuv422_repack_15m.bin
```

2026-05-28 上板验证后，硬件 JPEG 解码出现连续 `hw_after_success`，没有
`hw_after_fail`、`software fallback decode success` 或 `jpeg decode failed`，
`EYE_AVI_FPS` 约为 25fps。后续硬解问题复现时，先确认 `/sf0` 是否仍是该
YUV422 资源镜像。

注意：2026-05-28 10:00 用户用 BK 官方 AVI 软件重新生成并覆盖
`/home/jason/armino1/bk_avi` 后，该目录当前文件变为：

```text
mjpeg 320x160 yuvj420p 25fps
```

该资源已写入
`output/2026-05-28-bk-official-avi-yuv420-test/sf0_bk_official_avi_yuv420_15m.bin`
验证，板端硬解成功数为 0，全部进入软解回退，平均 FPS 约 14.56。因此它只能用于
YUV420 兼容性/软解压力测试，不能替代上面的 YUV422 硬解基线。

## 旧 DQT 修复资源

`bk_avi_320x160_sf0_root/` 中的 AVI 是当前更适合硬解的资源集合，第一帧
包含 DQT0 和 DQT1。已生成可直接写入外部 SPI Flash 的 15MiB littlefs 镜像：

```text
/home/jason/armino1/cmaiw82al_ai_toy/output/2026-05-26-sf0-dqt-fixed/sf0_bk_avi_320x160_dqt_fixed_15m.bin
```

上板后期望日志不再出现 `reason=missing dqt table`，而应进入 AVI 播放线程。

## GC9D01 160x160 双屏测试资源

生成 320x160 双屏合成 MJPEG AVI 和 `/sf0/neutral.avi` littlefs 镜像：

```sh
cd /home/jason/armino1/cmaiw82al_ai_toy
python3 tools/generate_gc9d01_test_avi.py
```

默认输出：

```text
resources/eyes/generated_gc9d01_320x160_test/neutral_gc9d01_320x160_test.avi
resources/eyes/generated_gc9d01_320x160_test/sf0/neutral.avi
resources/eyes/generated_gc9d01_320x160_test/sf0_gc9d01_320x160_test_15m.bin
resources/eyes/generated_gc9d01_320x160_test/manifest.json
```

测试图左半区是 `LCD2`，右半区是 `LCD1`；当前固件 `segment_flag=true`
会把左半区映射到第一段、右半区映射到第二段。上板时如果左右、上下或颜色不对，
优先检查分屏映射、RGB565 byte swap、MADCTL 和窗口设置。
