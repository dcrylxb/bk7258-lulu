# 服务后台配置

当前产品固件默认接入自建 Xiaozhi 服务：

```text
OTA:  http://106.55.173.79:8989/xiaozhi/ota/
WS:   ws://106.55.173.79:8989/xiaozhi/v1/
MQTT: 106.55.173.79:2883
UDP:  106.55.173.79:8888
Vision upload: http://106.55.173.79:8989/xiaozhi/api/vision
```

源码入口：

- `ap/main/common/common.h`：产品级后台地址宏。
- `ap/main/ota/ota_module.h`：OTA 版本检查 URL。
- `ap/main/ota/ota_module.c`：HTTP Host 头、OTA JSON 解析、MQTT endpoint 解析。
- `ap/main/protocols/protocol_websocket.c`：WebSocket 优先使用 OTA 下发的 URL 和 token。
- `ap/main/protocols/protocol_mqtt.c`：MQTT 模式下使用 hello 中的 UDP server/port；缺失时回退到产品默认 UDP 地址。

## 当前协议策略

AP 配置仍然使用 WebSocket：

```text
CONFIG_PROTOCOL_USE_WSS=y
# CONFIG_PROTOCOL_USE_MQTT is not set
# CONFIG_WSS_INFO_BY_USER is not set
```

也就是说设备启动后先请求 OTA，然后使用 OTA 返回的 `websocket.url` 连接语音服务。
`CONFIG_WSS_INFO_BY_USER` 默认关闭，避免跳过 OTA 返回的动态字段。

本机探测到当前 OTA 服务会返回：

```text
websocket.url = ws://106.55.173.79:8989/xiaozhi/v1/
mqtt.endpoint = 106.55.173.79:2883
firmware.version = 0.9.9
```

该响应没有 `activation` 字段，现有固件会按“已激活”路径继续初始化协议连接。

## 上板验证日志

烧录新 `all-app.bin` 后重点看：

```text
POST /xiaozhi/ota/ HTTP/1.1
Host: 106.55.173.79:8989
resp_status: 200
rep data: ... "websocket":{"url":"ws://106.55.173.79:8989/xiaozhi/v1/"
ws url: ws://106.55.173.79:8989/xiaozhi/v1/
Connected to WebSocket server
```

同时继续观察双屏稳定性日志：

```text
registered LCD device: cmaiw82al_gc9d01_160
SPI_CTLR open spi_id=0 reset_pin=6 dc_pin=7
SPI_CTLR open spi_id=1 reset_pin=53 dc_pin=5
lcd_spi_init_common[0] is complete
lcd_spi_init_common[1] is complete
LVGL_FLUSH full ... flush_ret=0 wait_ret=0
DUAL_SPI flush ... lcd1_ret=0 lcd0_ret=0
LCD_SPI frame id=0
LCD_SPI frame id=1
```

如果再次出现 LCD2 黑屏且系统仍有语音/网络日志，不要先改云端或音频。按
`03_dual_screen_avi_stability.md` 的顺序排查 LVGL flush、dual SPI、
SPI controller 和低层 LCD frame 日志。

## 视觉识图端点

当前服务端 MCP initialize 会下发 vision capability，但实测 URL 为
`https://dashscope.aliyuncs.com/compatible-mode/v1`。这是模型 API 地址，不是设备
multipart 图片上传地址。产品固件必须拒绝该地址，并 fallback 到自建后端：

```text
http://106.55.173.79:8989/xiaozhi/api/vision
```

2026-05-29 上板验证日志：

```text
logs/serial/20260529-195646-ttyUSB0-115200.log
```

关键闭环：

- WebSocket 连接后收到 MCP `initialize` 和 `capabilities.vision`。
- 设备日志出现 `reject invalid vision url`、`use vision fallback url`、
  `find token len:10`。
- `tools/list` 返回 `self.camera.take_photo`。
- 语音触发后收到 `tools/call self.camera.take_photo`。
- GC2145 抓拍 `640x480` JPEG，POST `/xiaozhi/api/vision`。
- HTTP 返回 `status code: 200`，随后服务端 TTS 返回识图文本。

## BLE 配网与设备绑定尾部

2026-05-31 已用 BK 官方 App 跑通 BLE 配网、Wi-Fi 入网和设备绑定闭环。该问题的
关键经验是：App 提示 Wi-Fi 配网成功并不等于用户设备绑定完成，尾部还需要完成
agent/channel 信息交换。

当前产品实现规则：

- 设备主动上报 agent/channel 信息时，使用
  `BOARDING_OP_SET_AGENT_INFO(13)`。
- App/服务器返回 agent 处理结果时，使用 `BOARDING_OP_AGENT_RSP(12)`。
- 设备上报 payload 为：

```json
{"channel":"c7_47_8c_cb_dd_4b"}
```

关键验证日志：

```text
logs/serial/20260531-114906-ttyUSB0-115200.log
BLE provisioning notify source:version_check opcode:13
```

曾经的错误是设备主动通知时使用了 `BOARDING_OP_AGENT_RSP(12)`，导致设备侧已播报
配网成功，但 App 停在最后页面且设备没有出现在绑定列表。后续开发自研小程序
BLE 配网/绑定时，必须把“Wi-Fi 入网成功”和“用户设备绑定成功”拆成两个状态，
并复用上述 opcode 方向，不要把问题误判为 Wi-Fi 扫描或热点能力缺失。

2026-05-31 小程序联调补充经验：`BOARDING_OP_START_WIFI_SCAN(24)` 的 notify
响应可能会分包。设备通知帧格式是
`[opcode_le16][status_u8][payload_len_le16][payload]`，其中 `opcode=24` 时
`status=1` 表示当前包后面还有 Wi-Fi 列表续包，不是失败；`status=0` 表示本次
Wi-Fi 列表发送结束。自研小程序必须累积 `status=1` 包中的 SSID，并等 `status=0`
尾包后再渲染列表。实测日志中设备先发：

```text
notify conn:0 opcode:24 status:1 len:120
notify conn:0 opcode:24 status:0 len:7
```

若小程序在第一包报“设备响应失败，opcode24,status 1”，根因是小程序把续包状态
误判为失败；设备侧当时已经完成扫描并返回了 AP 列表。

## 三端设备鉴权绑定联调进展

2026-05-31 对照 `nicolulu-server-golang-dev-v0.6.3`、`ai-companion-miniprogram-main`
和当前固件做了一轮联动梳理。结论是：服务端鉴权核心已经存在，当前主要缺口在
小程序/BFF 到服务端鉴权 API 的桥接，以及设备端 BLE `auth.sign` 签名命令。

服务端现状：

- 已有 `product_id + device_uid/device_mac + device_secret` 设备身份模型。
- 已有工厂凭证、绑定挑战、绑定确认、运行态 token、OTA/WS/MQTT 鉴权接口。
- App 绑定入口是 `POST /api/app/devices/bind/start` 和
  `POST /api/app/devices/bind/confirm`。
- 绑定挑战签名消息由服务端生成，设备端必须对原文 UTF-8 字节做
  `HMAC-SHA256(device_secret, signing_message)`，输出小写 hex。
- 运行态启用依赖 `auth.enable=true`，OTA 下发短期 WebSocket token，WS/MQTT
  再按 token 和设备状态校验。

BFF/小程序阶段性改动：

- `ai-companion-miniprogram-main/bff` 已新增小程序侧代理：
  `POST /api/devices/bind/start` 和 `POST /api/devices/bind/confirm`。
- BFF 只透出 `bindToken/signingMessage/deviceId/productId/deviceUid/deviceMac` 等绑定
  必需字段，不向小程序返回 `device_secret` 或服务端密文字段。
- `miniprogram/utils/api.js` 已增加 `startDeviceBinding(payload)` 和
  `confirmDeviceBinding(payload)`。
- `miniprogram/utils/ble-auth.js` 已对齐官方 BLE provisioning 帧格式：发现设备、
  连接 GATT、寻找可写和可通知特征，通过私有 opcode `151` 发送
  `{"cmd":"auth.sign","signing_message":"..."}`，再从 notify 响应中解析
  `signature`。
- `pages/add-device` 已支持录入 `productId/deviceUid/deviceMac` 并传入 pairing 页面；
  旧二维码/绑定码流程保留。
- `pages/pairing` 已按“申请挑战 -> BLE 请求设备签名 -> 确认绑定”的状态流转接入，
  失败时显示明确错误，不影响旧绑定码流程。

当前必须注意的边界：

- 现有 BFF 微信登录只生成本地 session，上游 xiaozhi 访问仍使用管理员 JWT。也就是说，
  目前新绑定代理能打通链路，但如果服务端没有 openid 到 xiaozhi 用户的映射，设备会
  绑定到 BFF 使用的上游账号。正式用户体系需要补微信用户和服务端用户的映射/登录态。
- 小程序 BLE 工具目前采用自动发现可写/通知特征，GATT UUID 仍未固定；承载方式已经
  固定为官方 provisioning operation characteristic：
  - App 写入：`[opcode_le16][payload_len_le16][payload]`
  - 设备通知：`[opcode_le16][status_u8][payload_len_le16][payload]`
  - 鉴权 opcode：`BOARDING_OP_AUTH_SIGN = 151`
- 设备端不要另起一套 BLE 服务抢占官方配网链路。当前产品固件已经通过
  `bk_ble_provisioning_set_msg_handle_cb(net_config_ble_msg_handler)` 接入官方
  provisioning 消息队列，后续应在 `ap/main/net_config/net_config.c` 的
  `net_config_ble_msg_handler()` 中增加鉴权签名处理。

设备端下一步推荐实现：

1. 私有鉴权 opcode 已定为 `151`，避开现有 `1/10/12/13/15/24/25/26/500`。
2. 在 `net_config_ble_msg_handler()` 中解析 `auth.sign` 请求，只读取
   `signing_message`，不打印完整密钥、不通过 BLE 返回 `device_secret`。
3. 设备凭证读取先走最小可测实现：`device_secret` 存入 EasyFlash key
   `device_secret`。串口调试/工厂写入命令为 `auth secret <device_secret>`；
   `auth status` 只打印是否已写入和长度，不能打印密钥原文。
4. HMAC 实现复用当前已链接的 `hmac_sha_256` 能力；输出必须是 64 字符小写 hex。
5. 通过 `bk_ble_provisioning_event_notify_with_data()` 返回：

```json
{"cmd":"auth.sign.result","signature":"0123abcd..."}
```

6. 上板验证顺序：小程序拿挑战 -> 串口确认设备收到鉴权请求 -> 串口只打印摘要和长度
   -> 小程序收到 signature -> BFF confirm -> 服务端设备 `activated=true/status=active`。

2026-05-31 设备端落地状态：

- `ap/main/auth/device_auth.c` 已实现：
  `hex(HMAC-SHA256(device_secret, signing_message))`。
- `ap/main/net_config/net_config.c` 已接入 `BOARDING_OP_AUTH_SIGN(151)`，缺少密钥时返回
  失败状态并打印 `BLE auth.sign failed: device secret unavailable`。
- `ap/main/cli/cli_app_auth.c` 已新增：
  - `auth status`
  - `auth secret <device_secret>`
  - `auth sign <signing_message>`
- 注意：当前还是工程化最小写入链路。商业量产需要把 `device_secret` 写入从串口调试
  命令收敛到受控工厂烧录/后台发放流程，并保证设备端不会通过日志、BLE、云端接口泄露
  密钥。

验证记录：

- `nicolulu-server-golang-dev-v0.6.3/manager/backend`：
  `go test ./services/deviceauth ./controllers ./test/device_auth_e2e` 通过。
- `ai-companion-miniprogram-main/bff`：
  `npm test` 24 项通过，`npm run build` 通过。
