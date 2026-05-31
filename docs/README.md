# 文档索引

建议阅读顺序：

1. `01_project_architecture.md`：项目来源、AP/CP 架构和模块职责。
2. `02_hardware_constraints.md`：CMAiW82AL 板级硬件约束和关键 GPIO。
3. `03_dual_screen_avi_stability.md`：双屏、LVGL、AVI 和 `/sf0` 稳定规则。
4. `04_build_flash_resources.md`：构建、产物、烧录和资源镜像规则。
5. `05_upstream_conflicts.md`：官方文档/上游源码与本板冲突点。
6. `06_service_backend.md`：OTA、WebSocket、MQTT 和 UDP 后台地址配置。
7. `07_ai_smp_official_docs_and_new_screen.md`：根目录官方 ai_smp 文档摘要、
   `ZTB071TBIG05` 160x160 GC9D01 新屏规格和下一版点屏顺序。
8. `08_ai_pet_interaction_core_plan.md`：当前 AI 电子宠物交互主线计划、阶段目标
   和 AVI 回归护栏。
9. `09_peripheral_driver_debug_retrospective.md`：截至 2026-05-29 的外设驱动
   调试复盘和后续调试清单。
10. `10_offline_wake_word_research.md`：离线唤醒词官方文档、当前实现、
    Wanson 路线和是否需要训练模型的结论。

这些文档是产品工程的开发指引层。官方 WIKI 和上游工程只作为 API、流程和
组件参考，不能覆盖本板硬件事实。
