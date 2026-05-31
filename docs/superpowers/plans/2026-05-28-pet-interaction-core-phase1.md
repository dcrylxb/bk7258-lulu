# Pet Interaction Core Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first local pet interaction loop with a pet brain, action router, and serial CLI while preserving the stable AVI playback baseline.

**Architecture:** `pet_brain` owns local state and converts events into semantic action requests. `pet_action_router` validates and executes those semantic requests through existing product APIs only. `cli_app_pet` gives a deterministic serial surface for testing before touch, IMU, and cloud payloads are connected.

**Tech Stack:** BK7258 Armino SMP C firmware, cJSON-ready product code, existing UI eye API, existing dialog/protocol modules, Python static guard tests.

---

### Task 1: Static Guards

**Files:**
- Modify: `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py`

- [ ] Add tests that fail until `pet_brain`, `pet_action_router`, and `cli_app_pet` exist and are registered in `ap/CMakeLists.txt`.
- [ ] Add tests that require `EYE_AVI_SURPRISED_NAME` to be `surprised.avi` and reject `suprised.avi` as the canonical resource name.
- [ ] Add tests that forbid `pet_action_router` from calling `bk_avi_player`, `bk_display_flush`, or SDK display internals.
- [ ] Run `python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v` and verify the new tests fail for missing implementation.

### Task 2: Pet Brain and Action Router

**Files:**
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_action_router.h`
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_action_router.c`
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.h`
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.c`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/common/common.h`

- [ ] Implement emotion whitelist and action whitelist.
- [ ] Normalize unknown emotions to safe fallbacks.
- [ ] Clip duration to 15000 ms and variable deltas to `[-5, 5]`.
- [ ] Implement pet state transitions for idle, touch, listen, speak, abort, privacy, sleep, and cloud error events.
- [ ] Route eyes only through `app_ui_display_chat_emotion()`.
- [ ] Keep AVI player and display SDK code untouched.

### Task 3: Serial CLI

**Files:**
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_global.h`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app.c`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`

- [ ] Add `pet status`.
- [ ] Add `pet emote <emotion>`.
- [ ] Add `pet event <event_name>`.
- [ ] Add `pet privacy on|off`.
- [ ] Add `pet idle`.
- [ ] Print concise CLI feedback for each accepted or rejected command.

### Task 4: System Startup Wiring

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/system_manager/system_manager.c`

- [ ] Initialize `pet_brain` during `SYSTEM_EVENT_INIT`.
- [ ] Do not auto-start carousel or change default boot AVI.
- [ ] Preserve OTA, power-lock, and deep-sleep guard behavior.

### Task 5: Verification

**Commands:**
- `cd /home/jason/armino1 && python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v`
- `cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy && SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258`

- [ ] Static guards pass.
- [ ] Firmware builds.
- [ ] Prepare OTA command using the current Ubuntu LAN IP before board update.
- [ ] Capture serial logs during OTA and boot.
- [ ] Validate with `pet status`, `pet emote happy`, `pet event touch_head_short`, `pet event touch_chin_long`, `pet privacy on`, `pet privacy off`, and `pet idle`.
- [ ] Confirm eye logs still show normal `EYE_SWITCH` and `EYE_AVI_FPS` behavior.

