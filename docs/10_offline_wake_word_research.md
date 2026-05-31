# 离线唤醒词研究

本轮结论：近期不建议先训练自定义唤醒词模型。当前应先用现有 Beken KWS 或
Wanson 预置词表验证离线唤醒链路、误唤醒率和漏唤醒率；只有当产品必须使用
预置词表之外的品牌唤醒词时，才进入供应商模型/词表生成或替换 KWS 引擎路线。

## 官方依据

本地官方文档镜像和在线 Beken 文档一致：

- `bekencorp_bk7258_v3.1.1_md/01_bk_avdk_smp_ap_doc/pages/developer-guide/audio/asr_service/index.md`
- `bekencorp_bk7258_v3.1.1_md/01_bk_avdk_smp_ap_doc/pages/api-reference/audio/asr_service/index.md`
- `bk_avdk_smp/projects/asr_service_example/README_CN.md`

官方 ASR Service 文档说明：

- ASR Service 提供统一处理框架，支持 Wanson ASR 引擎。
- 输入可以来自麦克风、UAC 或 voice service。
- 不同 ASR 引擎有格式要求；Wanson ASR 要求 16-bit PCM、16 kHz。
- 主要 API 是 `bk_asr_create`、`bk_asr_init`、`bk_asr_init_with_mic`、
  `bk_asr_start`、`bk_asr_stop`、`bk_asr_deinit` 和 audio ASR wrapper。
- 官方示例配置启用 `CONFIG_ASR_SERVICE=y`、`CONFIG_WANSON_ARMINO_ASR=y`、
  `CONFIG_WANSON_ASR_GROUP_VERSION_WORDS_V1=y`，通过串口观察是否识别唤醒词。
- 文档没有给出本地训练唤醒词模型、生成 TFLM 模型或生成 Wanson FST 的公开流程。

在线页面在 2026-05-29 可访问：

- `https://docs.bekencorp.com/arminodoc/bk_avdk_smp/ap_doc/bk7258/zh_CN/v3.1.1/developer-guide/audio/asr_service/index.html`
- `https://docs.bekencorp.com/arminodoc/bk_avdk_smp/ap_doc/bk7258/zh_CN/v3.1.1/api-reference/audio/asr_service/index.html`

## 当前产品实现

当前产品配置：

```text
CONFIG_ASR_SERVICE=y
CONFIG_ASR_SERVICE_USE_PSRAM=y
CONFIG_ASR_SERVICE_NO_MIC=y
CONFIG_BEKEN_KWS=y
# CONFIG_WANSON_ARMINO_ASR is not set
# CONFIG_WANSON_ASR is not set
```

因此 `app_asr_intf_instance()` 当前选择 Beken KWS：

```text
CONFIG_WANSON_ARMINO_ASR -> wanson_asr_instance()
CONFIG_BEKEN_KWS         -> kws_asr_instance()
```

Beken KWS 当前事实：

- 模型来自预编译库
  `ap/components/bk_kws/bk7258/libkws_model.a`。
- TFLM runtime 来自
  `ap/components/bk_kws/bk7258/libtensorflow-microlite.a`。
- 源码中当前标签是 `unknown`、`hello`、`byebye`。
- `bk_tflite_ASR_Recog()` 要求输入长度 `1280` 字节，即 16 kHz、16-bit PCM、
  640 samples、40 ms。
- `dialog_module.c` 已按 `CONFIG_BEKEN_KWS` 设置 `cfg.asr_frame_size = 1280`。
- 成功命中会打印 `Wakeup  %d  %s  %d`，然后 ASR 回调里出现
  `asr result: hello`；当前 `DEV_WAKE_WORD` 是 `hello`。
- 本地没有发现 Beken KWS 的训练脚本、数据集入口或可直接替换的 `.tflite`
  模型文件。

结论：当前 Beken KWS 是“预编译模型 + 硬编码标签/阈值”的集成方式。改标签字符串
不会训练新词，只会改变输出显示文本；如果模型没有学过该词，改标签没有实际意义。

## 2026-05-30 Beken KWS 实测结论

本轮用板端提示音同步口测，分别测试 `hello`、`byebye`、`你好`、`再见`：

- 标签映射没有整体写反：`score0/id0` 对应 `hello`，`score1/id1` 对应 `byebye`。
- `score2` 不是第三个唤醒词，而是 noise/gate 累积分数；后续诊断日志命名为
  `noise_gate` / `peak_noise_gate`，避免误读。
- 临时诊断阈值 `20000` 只适合观察分数分布，不适合作为发布阈值。
- 中文干扰项会明显推高 `byebye` 类：`你好` 曾触发一次 `Wakeup 1 byebye`，
  `再见` 最高接近但未越过 `byebye` 分类阈值。
- 因误触发风险，当前默认关闭 `byebye` 的业务动作：命中后只打印
  `offline kws matched disabled bye word`，不再发送 `SYSTEM_EVENT_LAST_SENTENCE`。

因此当前可保留 `hello` 作为离线唤醒链路验证词；`byebye` 暂时只能作为诊断分类，
不能作为可靠的业务控制词。

## Wanson 路线

项目中已有 Wanson Armino 组件，但当前未启用。相关事实：

- Kconfig 支持 `WANSON_ASR_WORDS_VERSION_V1/V2` 和
  `WANSON_ASR_GROUP_VERSION_WORDS_V1/V2`。
- 预编译库包括 `libasrbase.a`、`libasrfst.a`、`libasrfst_with_auth.a`。
- FST 头文件中存在不少内置中文候选词，包括 `小智小智`、`小艾小艾`、
  `你好阿米诺`、`嗨阿米诺`、`拜拜阿米诺` 等。
- 官方示例的 Wanson 回调根据识别出的中文字符串做业务判断。
- 组件也没有公开训练流程；FST 和库是预生成交付物。

结论：如果我们希望离线唤醒词是 `小智小智`，优先评估 Wanson 预置词表是否已经可用，
比自行训练模型更现实。但启用 Wanson 会改变帧长、配置、识别字符串和授权路径，
必须单独做一轮最小固件实验。

## 是否需要训练模型

短期：不需要，也不应该先训练。

理由：

- 当前第一目标是验证离线唤醒链路，而不是品牌词最终形态。
- Beken KWS 已能用 `hello` 验证离线唤醒事件、服务端 wake detect、录音启动和
  后续对话链路。
- Wanson 预置词表已经包含部分中文唤醒候选，可能覆盖 `小智小智`。
- 官方文档和 SDK 没有提供可直接执行的训练工具链，盲目训练会引入模型格式、
  量化、算子、内存、阈值、授权和误唤醒评估风险。

中期：只有满足以下任一条件才需要进入训练或供应商定制：

- 产品必须使用预置词表之外的固定中文/品牌唤醒词。
- Beken KWS `hello` 和 Wanson 预置中文词的误唤醒/漏唤醒无法接受。
- 需要多唤醒词、多命令词或特定儿童声线/噪声场景优化。
- 供应商能提供匹配 BK7258/TFLM 或 Wanson FST 的模型生成交付物和授权说明。

## 推荐路线

1. 保持当前 Beken KWS 配置，先补离线唤醒测试脚本和日志指标。
2. 验证 `hello` 在安静、播放 TTS、近场、远场和轻噪声场景下的命中率。
3. 不把 `byebye` 接入业务控制，直到误触发率评估通过并重新确认阈值。
4. 单独开实验分支评估 Wanson Armino 预置 `小智小智` 或 `嗨阿米诺`：
   调整 Kconfig、帧长 `960`、回调匹配字符串和授权路径。
5. 若 Wanson 预置词可用且误唤醒可接受，优先采用预置词，不训练。
6. 若产品必须定制词，向 Beken/Wanson 获取官方模型/FST 生成流程或交付物；
   在拿到工具链前，不把训练模型作为当前开发阻塞项。

## 后续验收指标

- 启动日志：ASR service、KWS/Wanson init、模型/授权、`bk_asr_start`、
  `bk_aud_asr_start`。
- 音频输入：MIC2/R、PCM 非零幅度、帧长正确。
- 真命中：`Wakeup` 或 Wanson 识别字符串、`asr result`、`SYSTEM_EVENT_DIALOG_START`。
- 链路：设备发送 wake detect、服务端进入 listen、后续语音上行和 TTS 回复。
- 可靠性：固定测试语料下统计命中次数、漏唤醒、误唤醒、平均唤醒延迟。
