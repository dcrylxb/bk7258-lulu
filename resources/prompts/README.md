# Prompts 资源

本目录用于沉淀 AI 玩具的人设提示词、本地提示音清单和后续离线语音资源说明。

## Runtime Path

Firmware resolves prompt IDs to:

```text
/if0/prompts/pet/<prompt_id>.mp3
```

## Initial Prompt Manifest

| ID | Purpose | Target duration |
| --- | --- | --- |
| `boot` | startup / wake from sleep | 500-900 ms |
| `net_ok` | cloud connected | 300-700 ms |
| `net_lost` | cloud disconnected or degraded | 400-900 ms |
| `low_power` | low battery warning | 600-1200 ms |
| `error` | recoverable error | 400-900 ms |
| `listen_start` | about to listen | 250-600 ms |
| `cancel` | cancel / abort acknowledgement | 250-600 ms |
| `photo` | camera capture started | 300-800 ms |
| `done` | local action completed | 250-600 ms |
| `happy_chirp` | happy local reaction | 200-600 ms |
| `curious` | curious local reaction | 250-700 ms |
| `afraid` | afraid reaction | 400-900 ms |
| `comforted` | soothe completed | 400-900 ms |
| `impact` | impact/freefall alert | 300-800 ms |
| `privacy_on` | privacy enabled | 300-700 ms |
| `privacy_off` | privacy disabled | 300-700 ms |
| `stop` | hard stop | 200-500 ms |
| `test_wake_prompt` | operator cue before user speech/action tests | 500-1200 ms |

## 资源要求

- 优先使用 16 kHz、mono、短时长音频。
- 交互提示音控制在 200 ms 到 1200 ms。
- 当前扬声器响度较高，资源峰值音量应保守归一化，避免加剧背光闪烁。
- 资源缺失时固件只能记录 `pet_prompt miss` 或播放路径日志，不能阻塞眼神或语音链路。
