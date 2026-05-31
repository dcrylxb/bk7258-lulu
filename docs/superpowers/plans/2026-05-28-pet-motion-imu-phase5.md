# Pet Motion IMU Phase 5 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a low-risk BMA253/HXY IMU diagnostic and semantic motion-event layer for the AI pet without touching the protected AVI playback path.

**Architecture:** `pet_motion` owns BMA253/HXY probing, raw sample reads, conservative classification, and optional semantic routing into `pet_brain`. `cli_app_pet` exposes `pet motion status|probe|sample|monitor|route` so hardware can be validated manually before any automatic background behavior is enabled.

**Tech Stack:** BK7258 Armino C firmware, GPIO bitbang I2C on GPIO42/43, `pet_brain`, Python static guard tests, serial OTA/log workflow.

---

### Task 1: Static Guard

**Files:**
- Modify: `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py`

- [x] Add a failing guard requiring `pet_motion.c` registration.
- [x] Lock BMA253/HXY board facts: SCL GPIO42, SDA GPIO43, INT1 GPIO41, INT2 GPIO52, SDO GPIO51, probe addresses `0x18/0x19`, Bosch chip ID `0xfa`, and HXY WHO_AM_I `0x11`.
- [x] Require `CONFIG_SIM_I2C=y`, `CONFIG_SIM_I2C0_SDA_GPIO=43`, `CONFIG_SIM_I2C0_SCL_GPIO=42`, and `# CONFIG_SIM_I2C_HW_BOARD_V3 is not set`.
- [x] Require semantic motion events in `pet_brain`.
- [x] Require `pet_motion` not to call AVI/JPEG/SPI/LVGL/display internals.

### Task 2: Motion API

**Files:**
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_motion.h`
- Create: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_motion.c`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/CMakeLists.txt`

- [x] Define `pet_motion_sample_t`, `pet_motion_event_t`, and probe/status APIs.
- [x] Implement ACK-aware GPIO bitbang probe for normal GPIO42/43 and swapped GPIO43/42 buses.
- [x] Support Bosch BMA253 chip ID `0xfa` and HXY-compatible `WHO_AM_I=0x11`.
- [x] Read six-axis bytes and convert to mg.
- [x] Classify only conservative events: stable, tilt left/right, gentle shake, strong shake, freefall, impact.
- [x] Keep automatic monitoring off unless explicitly enabled by CLI.

### Task 3: Pet Brain Routing

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.h`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_brain.c`

- [x] Add motion events to `pet_event_type_t`.
- [x] Map gentle shake to `happy`, strong shake to `grimacing`, freefall/impact to `afraid`, and tilt to `curious`.
- [x] Keep all visible output through existing `pet_action_router`.

### Task 4: CLI Diagnostics

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/cp/cli_update_forward.c`

- [x] Add `pet motion status|probe|sample|monitor|route`.
- [x] `probe` initializes and reports sensor identity.
- [x] `sample` prints raw/mg/classification.
- [x] `route <event>` sends one semantic motion event into `pet_brain`.
- [x] `monitor on|off` controls background polling manually.

### Task 5: Verification

**Commands:**
- `cd /home/jason/armino1 && python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v`
- `cd /home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy && SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258`

- [x] Static guards pass.
- [x] Firmware builds.
- [x] OTA with current Ubuntu LAN IP.
- [x] Serial logs show `pet motion probe`, `pet motion sample`, and `pet motion route strong_shake` behavior.
- [x] No AVI fallback, crash, or display hot-path regression appears in the validation log.
- [x] Monitor automatic routing is throttled before physical shake stress testing.

### Validation Evidence

- Static guards: `python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v` passed, 58 tests OK.
- Build: `SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258` passed and produced `app_pack.rbl` size 1914992.
- OTA: served from current LAN IP `192.168.31.117`, URL `http://192.168.31.117:8000/2026-05-28-pet-motion-imu-phase5/app_pack.rbl`; serial log reached 100% write, then rebooted into `armino app init: May 28 2026 15:37:56`.
- IMU probe: `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260528-190612-ttyUSB0-115200.log` shows normal bus GPIO42/GPIO43, addr `0x19`, HXY `who_am_i=0x11`, version `0x28`, `pet motion ready=1`.
- IMU sample: `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260528-190629-ttyUSB0-115200.log` shows `raw=(135,-63,-857)`, `mg=(131,-61,-836)`, `mag=1028`, event `stable`.
- Motion route: `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260528-190647-ttyUSB0-115200.log` shows `strong_shake` -> `motion_strong_shake` -> `grimacing`, haptic `alert` accepted with MS32008N1 OUT5 hardware output disabled.
- Monitor retest on the later `May 28 2026 21:14:44` build:
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-090510-ttyUSB0-115200.log`
  shows `pet motion monitor on`, monitor task period `180ms`, and `pet motion ready=1 monitor=1`.
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-090536-ttyUSB0-115200.log`
  captures roughly 70 seconds with monitor enabled and `EYE_AVI_FPS avg=24.98-25.00`,
  `errors=0`, `overruns=0`.
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-090654-ttyUSB0-115200.log`
  shows `monitor disabled`, `pet motion ready=1 monitor=0`, and continued 25fps playback.
  These logs contain no `EYE_FRAME_COLOR_ALERT`, `avi jpeg invalid`, `jpeg decode failed`,
  `short read`, `hw decode failed`, `Assert`, `HardFault`, `rtos_dump`, or `cmd NOT found`.
- Static guard red/green for route throttling: targeted guard first failed on missing
  `PET_MOTION_MONITOR_ROUTE_COOLDOWN_MS`; after adding monitor-only route cooldown,
  `python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards.StaticGuardTests.test_pet_motion_imu_uses_bma253_hxy_without_touching_avi_hot_paths -v`
  passed, and the full static guard suite passed 60 tests.
- Route throttle implementation: `pet_motion.c` now uses
  `PET_MOTION_MONITOR_ROUTE_COOLDOWN_MS=1500` for ordinary motion events and
  `PET_MOTION_MONITOR_ROUTE_SAFETY_COOLDOWN_MS=300` for freefall/impact. The helper
  `pet_motion_monitor_route_allowed()` rejects none/stable, suppresses consecutive
  identical events, and updates `last_route_ms` only on monitor-routed events. Manual
  `pet motion route <event>` remains unthrottled for diagnostics.
- Throttle build/OTA: `SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258`
  passed and produced `app_pack.rbl` size `1916480` and `all-app.bin` size `3343744`.
  Artifacts were archived under
  `/home/jason/armino1/cmaiw82al_ai_toy/output/2026-05-29-pet-motion-route-throttle/`,
  with `app_pack.rbl` sha256
  `6d41738deac1be9c146fb67512afa2c5fdb01f426cba8bd92f0920f5cb1f7ead`.
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-092442-ttyUSB0-115200.log`
  shows `APP_OTA begin`, `cyg_recvlen_per:(100.00)%`, `write over`, and software reboot.
- Throttle board retest:
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-093008-ttyUSB0-115200.log`
  reconfirms HXY-compatible IMU on normal GPIO42/GPIO43 bus, addr `0x19`,
  `who_am_i=0x11`, version `0x28`, and `pet motion ready=1`.
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-093055-ttyUSB0-115200.log`
  shows monitor task start at `180ms`; during the idle monitor run, `happy.avi` stayed at
  `EYE_AVI_FPS avg=24.96-25.00`, `errors=0`, `overruns=0`, and did not emit automatic
  `route motion=` flooding. `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-093417-ttyUSB0-115200.log`
  shows `monitor disabled` and continued 25fps playback.
- Tilt priority bugfix: hand-held right-vertical testing showed samples such as
  `mg=(4,1007,332)` were classified as `gentle_shake` because gentle-shake ran before
  dominant X/Y tilt. A red/green static guard now requires X/Y tilt returns to precede
  `PET_MOTION_EVENT_GENTLE_SHAKE` in `pet_motion_classify_sample()`. The implementation
  keeps `freefall`, `impact`, and `strong_shake` first, then classifies X/Y dominant tilt
  before gentle shake. `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-102602-ttyUSB0-115200.log`
  confirms `mg=(20,1019,318) mag=1357 event=tilt_right`.
- Static orientation closure: `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-131259-ttyUSB0-115200.log`
  confirms left-vertical as `raw=(42,-1000,178)`, `mg=(41,-976,173)`, `mag=1190`,
  event `tilt_left`. `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-131358-ttyUSB0-115200.log`
  confirms flat as `raw=(-25,65,-913)`, `mg=(-24,63,-891)`, `mag=978`, event
  `stable`. Right-vertical, left-vertical, and flat static orientation mapping are now
  closed; automatic behavior still requires shake stress evidence before being enabled
  by default.
- AVI cache-required guardrail: `caring.avi` is large (`947834`) and repeatedly exposed
  `/sf0` short-read/hash-mismatch behavior. Before the guardrail, cache failure could fall
  through to direct POSIX `/sf0` streaming and sustain `5-11fps`. The new SDK guard returns
  `VIDEO_OSI_AVI_CACHE_REQUIRED` for manifest-known `/sf0/*.avi` when cache load/hash
  fails, while non-manifest files still keep direct POSIX fallback. Static guard TDD passed,
  full static guard suite passed 60 tests, and
  `SDK_DIR=/home/jason/armino1/bk_avdk_smp ./dbuild.sh make bk7258` passed. Artifacts:
  `/home/jason/armino1/cmaiw82al_ai_toy/output/2026-05-29-avi-cache-required/app_pack.rbl`
  sha256 `5f5ddb47760766db65c41a8b9002e6f91a2904740a2cca146ed85d8330ef92a4`,
  `all-app.bin` sha256 `871343bc8d9a72a77999fa8bddc3f4211506fedc1ef47a1efdddf1caa9eea37d`.
  OTA used `http://192.168.31.117:8000/2026-05-29-avi-cache-required/app_pack.rbl`;
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-125012-ttyUSB0-115200.log`
  shows software reboot and `happy.avi` at `EYE_AVI_FPS avg=25.07/24.99/25.00`.
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-125130-ttyUSB0-115200.log`
  shows two `VIDEO_OSI avi cache required path=/sf0/caring.avi reason=cache required`
  failures instead of direct streaming, and
  `/home/jason/armino1/cmaiw82al_ai_toy/logs/serial/20260529-125231-ttyUSB0-115200.log`
  shows `caring.avi` recovered to continuous `EYE_AVI_FPS avg=24.99-25.00`,
  `errors=0`, `overruns=0`.

### Residual Risk

- The original stream-read validation exposed the AVI performance guardrail gap and must stay in the history above. The current manifest-verified PSRAM cache baseline plus cache-required refusal closes the low-FPS fallback path for manifest-known AVI files, but `/sf0` read instability itself still exists and should be reduced separately.
- Automatic pet behavior is still not enabled by default. The monitor route cooldown closes the obvious alternating-event flooding risk, and right/left/flat static orientation mapping is verified, but a separate physical motion stress test with real gentle/strong shake events is still required before enabling default automatic behavior. Do not use drop/freefall testing as a routine manual validation step.
