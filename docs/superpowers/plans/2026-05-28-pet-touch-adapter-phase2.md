# Pet Touch Adapter Phase 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Map the physical CMAiW82AL touch inputs on GPIO46 and GPIO44 into local `pet_brain` events without touching the AVI playback path.

**Architecture:** Reuse the existing AP `app_key` wrapper over the SDK `multi_button` framework. Add semantic pet touch APP events, register GPIO46/GPIO44 as low-active inputs, and route those events directly to `pet_brain_handle_event()`.

**Tech Stack:** BK7258 Armino SMP C firmware, existing `multi_button` key framework, `pet_brain`, Python static guard tests.

---

### Task 1: Static Guards

**Files:**
- Modify: `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py`

- [ ] Add a guard requiring `APP_EVENT_TOUCH_HEAD_SHORT`, `APP_EVENT_TOUCH_HEAD_DOUBLE`, `APP_EVENT_TOUCH_HEAD_LONG`, `APP_EVENT_TOUCH_CHIN_SHORT`, `APP_EVENT_TOUCH_CHIN_DOUBLE`, `APP_EVENT_TOUCH_CHIN_LONG`, and `APP_EVENT_TOUCH_CHIN_VERY_LONG`.
- [ ] Add a guard requiring GPIO46 to map to head touch and GPIO44 to map to chin touch in `APP_KEY_CONFIG_TABLE`.
- [ ] Add a guard requiring `app_key.c` to include `pet_brain.h` and route touch events through `pet_brain_handle_event()`.
- [ ] Add a guard requiring GPIO54 to remain the shutdown/recovery key only.
- [ ] Run `python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards.StaticGuardTests.test_pet_touch_adapter_maps_gpio46_gpio44_to_pet_events -v` and verify it fails before implementation.

### Task 2: Touch Events and Key Table

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/common/common.h`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/key/app_key.h`

- [ ] Add pet touch APP events after existing product key events.
- [ ] Extend `app_key_config_t` with `very_long_event` so chin long and chin very-long remain distinct.
- [ ] Register GPIO46 low-active as head short/double/long.
- [ ] Register GPIO44 low-active as chin short/double/long/very-long.
- [ ] Keep GPIO12 as dialog and GPIO54 as shutdown/recovery.

### Task 3: Touch Event Routing

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/key/app_key.c`

- [ ] Include `pet_brain.h`.
- [ ] Map head touch events to `PET_EVENT_TOUCH_HEAD_SHORT`, `PET_EVENT_TOUCH_HEAD_DOUBLE`, and `PET_EVENT_TOUCH_HEAD_LONG`.
- [ ] Map chin touch events to `PET_EVENT_TOUCH_CHIN_SHORT`, `PET_EVENT_TOUCH_CHIN_DOUBLE`, `PET_EVENT_TOUCH_CHIN_LONG`, and `PET_EVENT_TOUCH_CHIN_VERY_LONG`.
- [ ] Use SDK `LONG_PRESS_HOLD` to emit very-long once per press when configured.
- [ ] Register hold and long-up callbacks in `key_item_configure()`.

### Task 4: Verification

**Commands:**
- `cd /home/jason/armino1 && python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v`
- `cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy && SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258`

- [ ] Static guards pass.
- [ ] Firmware builds.
- [ ] OTA using the current Ubuntu LAN IP.
- [ ] Physically tap GPIO46/44 touch pads and capture serial logs.
- [ ] Confirm logs show key event -> pet event -> `EYE_SWITCH` without AVI fallback or crash.

