# Haptic MS32008N1 OUT5 DC Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a safe board-side diagnostic for the CN3 2pin vibration motor on MS32008N1 OUT5A/OUT5B.

**Architecture:** Keep normal `pet_haptic_play()` hardware output disabled. Extend `pet_haptic_probe()` to wake MS32008N1, read `chipFlag=0x08`, then return to sleep. Add explicit CLI-only `pet haptic dc_test [ms] [reverse]` that runs OUT5 for a clipped short interval and always returns OUT5 to HiZ, global standby/reset, and `nSLEEP=0`.

**Tech Stack:** BK7258 Armino SMP, product `pet_haptic`, BK I2C/GPIO drivers, static guard tests, OTA serial validation.

---

### Task 1: Static Guard

**Files:**
- Modify: `/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py`

- [x] Add guard expectations for MS32008N1 DC constants, chipFlag read, clipped `dc_test`, CLI command, and forced shutdown sequence.
- [x] Run the targeted guard and verify it fails before production code changes.

### Task 2: Haptic Driver Diagnostic

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_haptic.h`
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/pet/pet_haptic.c`

- [x] Add `pet_haptic_dc_test()` and result metadata.
- [x] Add MS32008N1 register helpers using `bk_i2c_master_write/read`.
- [x] Make `pet_haptic_probe()` wake, read chipFlag, then sleep.
- [x] Ensure all failure paths call stop/sleep.

### Task 3: CLI

**Files:**
- Modify: `/home/jason/armino1/cmaiw82al_ai_toy/projects/cmaiw82al_ai_toy/ap/main/cli/cli_app_pet.c`

- [x] Extend usage to `pet haptic status|probe|stop|dc_test|<pattern> [duration_ms]`.
- [x] Add `pet haptic dc_test [ms] [reverse]`, default `60ms`, max clipped by driver.
- [x] Log requested duration, actual duration, direction, chipFlag, and result.

### Task 4: Verification And OTA

- [x] Run targeted static guard.
- [x] Run full static guards.
- [x] Build `bk7258`.
- [x] Archive `app_pack.rbl` and `all-app.bin`.
- [x] OTA flash `app_pack.rbl`.
- [x] Run `pet haptic probe`; expect `ack=1 chip=0x08`.
- [x] Run `pet haptic dc_test 60`; user confirms whether the motor vibrates.
- [x] Run `pet haptic status`; expect `awake=0`.

### Result

- OTA log: `logs/serial/20260529-202315-ttyUSB0-115200.log` reached
  `cyg_recvlen_per:(100.00)%` and `write over`.
- Boot confirmation: `logs/serial/20260529-202541-ttyUSB0-115200.log` shows
  software reboot into compile time `May 29 2026 15:41:36`.
- Probe: `logs/serial/20260529-202616-ttyUSB0-115200.log` shows
  `addr=0x10 ack=1 ret=0 chip=0x08`.
- DC test: `logs/serial/20260529-202650-ttyUSB0-115200.log` shows
  `ret=0 requested=60 actual=60 reverse=0 chip=0x08`; user confirmed the
  vibration motor physically vibrated.
- Stop/sleep: `logs/serial/20260529-202733-ttyUSB0-115200.log` shows
  `awake=0`.
