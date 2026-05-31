# CMAiW82AL AI Toy

本目录是 CMAiW82AL BK7258 AI 玩具产品工程。当前采用方案 A：以
`ai_xiaozhi/projects/xiaozhi` 为应用基线复制出独立项目，再按
CMAiW82AL 已验证硬件约束做板级适配。

## 目录

- `projects/cmaiw82al_ai_toy/`：产品固件工程，包含 AP/CP 代码、Kconfig、
  分区、构建脚本。
- `docs/`：产品开发指引、硬件约束、双屏/AVI 稳定规则、服务后台配置和冲突清单。
- `resources/eyes/`：外部 `/sf0` 眼睛 AVI 资源的源文件和镜像说明。
- `resources/prompts/`：本地提示音、角色提示词等产品资源说明。
- `patch/`：后续必须改 SDK 时沉淀补丁，不直接把 SDK 改动遗忘在工作区。
- `tools/tests/`：产品级静态护栏测试说明。当前实际测试入口在仓库根目录
  `tools/tests/test_cmaiw82al_ai_toy_static_guards.py`。

## 快速入口

1. 先读 `CODEX_DEVICE_DEV_GUIDE.md`。
2. 再读 `docs/README.md` 选择具体主题。
3. 固件构建：

```sh
cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy
SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258
```

输出：

```text
build/bk7258/cmaiw82al_ai_toy/package/all-app.bin
build/bk7258/cmaiw82al_ai_toy/package/app_pack.rbl
```

注意：`all-app.bin` 不包含外部 `/sf0` 眼睛 AVI 资源。
