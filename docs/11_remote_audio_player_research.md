# 远程音频播放器研究记录

日期：2026-06-01

## 背景

三端内容下发已经分成三类能力：

- `prompt`：本地短提示音，通过 `prompt_id` 播放 `/if0/prompts/<id>.mp3`。
- `message`：文字内容，服务端注入设备对话通道。
- `remote-audio`：HTTP/HTTPS 音频 URL，目标是后续支持歌曲、故事等长音频推送。

当前设备端已经支持 `prompt_id` 窄通道，但远程 HTTP 音频仍返回
`501 audio player is disabled`。这是正确的门控，不是回归。

## 官方结论

官方 `bk_avdk_smp` 音频播放器文档把 `bk_audio_player` 和
`bk_player_service` 定义为两个独立组件：

- `bk_player_service` 主要面向提示音，不承担完整音乐播放器、播放列表和网络流能力。
- `bk_audio_player` 支持播放列表、播放控制、音量、事件回调、本地 VFS 文件和网络音频流。
- `bk_audio_player` 的网络源依赖 `CONFIG_WEBCLIENT`。
- 默认板载扬声器输出依赖 `CONFIG_ADK_RAW_STREAM` 和
  `CONFIG_ADK_ONBOARD_SPEAKER_STREAM`。
- 播放本地 VFS 文件时，应用需要先挂载文件系统。

因此，不能把 `bk_player_service` 当作 HTTP 音频播放器使用。

## 当前 SDK 事实

当前产品 SDK 路径：

`/home/jason/armino1/bk_avdk_smp`

核对结果：

- 当前 SDK 没有 `ap/components/bk_audio_player` 组件目录。
- 当前产品工程已有 `ap/components/bk_app_audio/app_audio_player.c`、
  `app_audio_player.h`、`bk_audio_player_port_sink.c` 和
  `bk_audio_player_port_sink.h` 适配层。
- 这些适配层包含 `<components/bk_audio_player/...>` 头文件，并在
  `CONFIG_AUDIO_PLAYER` 打开后通过 CMake 依赖 `bk_audio_player`。
- 当前 `ap/config/bk7258_ap/config` 没有开启 `CONFIG_AUDIO_PLAYER`。
- 当前配置已经具备播放器后续可能需要的部分依赖：
  `CONFIG_WEBCLIENT=y`、`CONFIG_ADK_RAW_STREAM=y`、
  `CONFIG_ADK_ONBOARD_SPEAKER_STREAM=y`、`CONFIG_VFS=y`、
  `CONFIG_PSRAM=y`。

结论：当前不能直接开启 CONFIG_AUDIO_PLAYER。直接打开会让工程依赖缺失的
`bk_audio_player` 组件，构建无法成为可验证的稳定能力。

## 可复用资产

当前仓库内已有可继续保留的上层接口：

- WebSocket manager `/api/device/play_audio`。
- `SYSTEM_EVENT_PLAY_AUDIO_URL` 异步播放事件。
- `self.audio_player.stop` MCP 工具。
- `prompt_id` 本地提示音优先分支。
- 服务端和 BFF 的远程音频 501 门控与错误翻译。

当前仓库外可参考但不能直接认定为产品事实的资料：

- 官方文档：`developer-guide/audio/audio_player`。
- 官方文档：`examples/projects/audio_player_example`。
- 上游/历史工程里的 `app_audio_player.c`、`bk_audio_player_port_sink.c`。
- 历史补丁：`08-fix-audio-net-source-seek.patch`、
  `09-fix-audio-player-seek-crash.patch`。

## 移植前置条件

恢复远程音频播放前必须满足以下条件：

1. 补齐与当前 SDK 版本匹配的 `ap/components/bk_audio_player` 源码、Kconfig、
   CMake 和插件目录。
2. 确认 `CONFIG_AUDIO_PLAYER`、`CONFIG_AUDIO_PLAYER_USE_PSRAM` 或
   `CONFIG_AUDIO_PLAYER_USE_SRAM` 的配置项能被 Kconfig 正常解析。
3. 只注册首版需要的插件，优先 MP3/WAV 元数据解析器、MP3/WAV 解码器、
   file/net source 和当前自定义 port sink，避免一次性打开所有格式。
4. 核对网络源是否需要 `CONFIG_WEBCLIENT_TLS`、证书、HTTP range/seek 修复。
5. 核对播放器输出和现有在线 TTS、本地提示音、PA GPIO8 控制之间的互斥关系。
6. 核对 PSRAM、任务栈、解码缓冲和 LCD/AVI/摄像头并发内存余量。
7. 保留远程 HTTP 音频失败时的 501 返回，不允许恢复假成功。

## 建议实现顺序

1. 先复制或引入 `bk_audio_player` 组件，保持 `CONFIG_AUDIO_PLAYER` 关闭。
2. 增加静态测试，确认 Kconfig、CMake、头文件和插件路径完整。
3. 打开 `CONFIG_AUDIO_PLAYER` 和 PSRAM 选项，只构建不烧录。
4. 用 CLI 或本地 VFS MP3 做最小播放测试。
5. 用 HTTP MP3 URL 做最小网络播放测试。
6. 接回 `/api/device/play_audio` 和 `self.audio_player.stop`。
7. 最后再接小程序资源库的 `remote-audio` 真机验收。

## 验收命令

设备端静态护栏：

```sh
python3 -m unittest discover -s tools/tests -p 'test_*.py' -v
```

固件构建：

```sh
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

BLE 和真机远程播放验收需要设备进入对应测试状态后再执行。未完成
`bk_audio_player` 移植前，小程序推送 HTTP 音频预期仍应提示：

`设备暂不支持远程音频播放`
