# 外设驱动调试复盘

截至 2026-05-29，本项目已经闭合或基本闭合双屏/AVI、语音链路、摄像头识图、
IMU 姿态、振动马达等关键外设调试。屏幕闪烁和供电余量评估暂不在本轮处理，
留到交互功能稳定后统一调优。

## 核心方法

1. 先定板级事实，再谈驱动策略。官方 SDK、上游 `ai_xiaozhi` 和旧工程只用于
   API、生命周期和可运行路径参考；GPIO、总线、资源路径、屏幕尺寸和外设型号
   以当前板原理图、网表、产品文档和上板日志为准。
2. 先做最小诊断闭环，再接业务状态机。每个外设先有 CLI 或日志可单独验证，
   确认硬件响应后才映射到 `pet_brain`、MCP 或 UI 表情。
3. 保护热路径。显示刷新、AVI/JPEG 解码、音频采集回调不能塞入阻塞式网络、
   文件、摄像头或复杂业务逻辑；动作路由只发语义事件。
4. 每次闭合必须留下证据。有效证据包括启动配置、外设 ID、关键 API 成功返回、
   资源尺寸、HTTP 状态、FPS、错误计数、OTA 日志和用户体感确认。
5. 失败先分层，不横向乱猜。典型顺序是能力声明、配置、硬件采集、数据格式、
   传输、上层业务响应；不要在未确认底层输入时调阈值或改算法。

## 已验证模块

### 双屏和 AVI

- 当前双屏目标是 GC9D01 `160x160 x2`，LVGL 逻辑尺寸 `160x320`。
- 眼神资源是 `/sf0/*.avi`，资源不在 `all-app.bin` 中，必须独立更新外部 SPI
  Flash。
- AVI 资源合同是 `320x160`、YUV422 MJPEG；错误尺寸会导致 JPEG/AVI 失败、
  反复 reopen、PSRAM 压力甚至异常 dump。
- 已形成 manifest cache-required 保护：manifest 已知 `/sf0/*.avi` cache/hash
  失败时拒绝打开，不允许退回低 FPS 直接流式播放。
- 关键验收日志是 `EYE_AVI_FPS avg=24.9x-25.0x`、`errors=0`、`overruns=0`。

经验：显示问题先分离静态色块、LCD 初始化、SPI 帧完成、AVI 文件格式和
JPEG 解码；不要一边改 AI/语音一边调屏。

### 音频和语音对话

- 主麦是 MIC2/R，`CONFIG_CMAIW82AL_AUDIO_MAIN_MIC=2`；PA/MUTE 是 GPIO8。
- 语音对话链路已经跑通，服务端 ASR/LLM/TTS 可用；扬声器音量已调大过，
  后续功率和背光闪烁留到整机调优阶段。
- 调音频时必须先确认 PCM 幅度和音频 pipeline，而不是直接调 KWS 阈值或 VAD。
- WSS 实时模式需要连续 Opus 上行，不能让本地 VAD 在服务端仍等待音频时饿死
  voice read 任务。

经验：音频链路最容易被“格式看起来对”误导。要同时看采样率、帧长、编码类型、
PA 状态、MIC 选择、回调计数、服务端事件和 TTS 播放状态。

### 摄像头和拍照识图

- GC2145 使用 I2C1 GPIO0/1，PWR GPIO9，RST GPIO28。
- 单帧抓拍已经验证：`640x480` JPEG，SOI `ff d8 ff e0` 正常。
- MCP 识图闭环已经验证：服务端下发 `self.camera.take_photo`，设备抓拍后 POST
  到 `http://106.55.173.79:8989/xiaozhi/api/vision`，HTTP `200` 后 TTS 回复结果。
- 服务端曾下发模型 API 地址作为 vision URL，设备侧必须拒绝非本后端上传端点，
  fallback 到 `/xiaozhi/api/vision`。

经验：摄像头不要只测 I2C ID。真实验收要覆盖 MCP capability、fallback
URL/token、camera open/capture、JPEG magic、HTTP upload、TTS result。

### IMU 姿态

- IMU 是 HXY 兼容器件，I2C GPIO42/43，地址 `0x19`，`WHO_AM_I=0x11`，
  version `0x28`。
- 平放、左侧垂直、右侧垂直已经通过上板日志闭合。
- 真实右侧垂直手持时，轻微抖动曾被 `gentle_shake` 抢先误判；已调整分类顺序：
  freefall/impact/strong_shake 安全事件优先，然后先判 X/Y dominant tilt，
  最后再判 gentle_shake。
- monitor 自动路由已经加节流：普通姿态事件 `1500ms`，安全事件 `300ms`。

经验：姿态分类不能只看单轴阈值，要用真实手持姿态日志校正优先级；自动路由
必须节流，避免按采样周期刷表情拖垮 AVI。

### 振动马达

- 当前振动外设是 CN3 2pin 直流振动马达，走 MS32008N1 OUT5A/OUT5B；
  不是 J2/J3 5pin 步进电机，也不是旧 `bk_motor` PWM demo。
- MS32008N1 I2C 地址按 `0x10` 使用，`chipFlag=0x08` 已确认。
- `pet haptic probe` 可读芯片，`pet haptic dc_test 60` 已产生用户确认的 60ms
  体感短振，测试后回到 `awake=0`。
- `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` 仍保持关闭；业务 haptic pattern 等
  功耗、背光闪烁和交互节流评估后再默认接硬件。

经验：电机类外设先确认“接口类型和线序”，再讨论波形。把硬件短脉冲留在显式
诊断命令里，不要在业务状态机中散写寄存器。

## 尚未闭合或后置项

- 触摸适配还未进入同等深度闭合，需要后续按 GPIO46/GPIO44 分别验证短触、
  双击、长按和超长按。
- 离线唤醒词需要先验证当前 KWS/Wanson 方案选择，不能直接假设要训练模型。
- 屏幕闪烁、扬声器功率和振动功耗耦合放到最后做整机供电余量评估。

## 后续外设调试清单

- 写代码前先更新或查看 `docs/02_hardware_constraints.md`。
- 每个外设先有 `probe/status/test` 级 CLI 或等价日志。
- 日志至少覆盖：外设地址/ID、关键配置、一次成功动作、一次停止/释放。
- 业务集成前必须确认不会阻塞显示、音频或网络热路径。
- 上板验证完成后，把证据日志和残余风险写回计划或经验文档。
