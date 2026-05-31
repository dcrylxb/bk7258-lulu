# BK7258 AI Pet Interaction Core Design

## Goal

Build the AI pet interaction core around a local-first model: BK7258 owns the
pet body, safety state, instant reactions, and eye/haptic output; Xiaozhi cloud
can suggest bounded semantic actions through WebSocket, MCP, or future
`pet_action` payloads.

## Current Baseline

- Eye AVI playback is a protected baseline. Interaction code must use the
  public UI emotion interface and must not modify SDK AVI, JPEG, SPI, LVGL, or
  display hot paths during this phase.
- Firmware OTA and `/sf0` eye resource update remain protected workflows.
- Development firmware keeps shutdown/deep sleep blocked to preserve OTA
  recovery.
- Backend endpoints are fixed in `ap/main/common/common.h`:
  OTA `http://106.55.173.79:8989/xiaozhi/ota/`, WebSocket
  `ws://106.55.173.79:8989/xiaozhi/v1/`, MQTT `106.55.173.79:2883`,
  UDP `106.55.173.79:8888`.

## Architecture

The first implementation adds three product-level modules:

- `pet_brain`: local pet state, hidden variables, privacy flag, and event to
  semantic action mapping.
- `pet_action_router`: whitelist and normalization layer for action names,
  emotion names, durations, and local deltas. It is the only module allowed to
  request visible pet output.
- `cli_app_pet`: serial test entry for deterministic board validation before
  touch, IMU, and cloud actions are connected.

The router talks to existing product APIs only:

- Eyes: `app_ui_display_chat_emotion()`
- Speaker abort: `dialog_module_instance()->speaker_play_abort()`
- Cloud abort, when protocol is active: `protocol_instance()->sendAbortListen()`

The router must not call `bk_avi_player_*`, `bk_display_flush()`, or SDK display
internals.

## Phase Plan

### Phase 1: Pet Brain, Router, CLI

Deliver a local loop that can be tested entirely from serial:

- `pet status` prints current state, privacy flag, variables, last event, and
  last action.
- `pet emote <emotion>` switches eyes through the protected UI emotion API.
- `pet event touch_head_short`, `touch_head_long`, `touch_chin_short`,
  `touch_chin_long`, `touch_chin_very_long`, `audio_listen`,
  `audio_tts_start`, `audio_tts_stop`, `cloud_error`, and `idle` drive local
  state transitions.
- Unknown emotions fall back to `smiling`; error paths fall back to `confused`.
- Durations and variable deltas are clipped.
- Static guard tests lock the new module registration and AVI guardrails.

### Phase 2: Touch Adapter

Map GPIO46 and GPIO44 into pet events using the existing key framework:

- GPIO46: head touch.
- GPIO44: chin touch.
- Short, double, long, and very-long semantics are mapped to `pet_brain`.
- Power key GPIO54 remains a power/recovery key, not a pet interaction key.

### Phase 3: WebSocket Voice State Bridge

Connect the existing dialog/protocol state machine to `pet_brain`:

- `hello` marks cloud connected.
- `listen start/stop`, TTS start/stop, abort, and errors update pet state.
- Cloud `emotion` is routed through `pet_action_router`.
- OTA config failure leaves local pet interactions available.

### Phase 4: `pet_action` and Device MCP

Add bounded cloud control:

- Parse `pet_action` JSON.
- Accept only whitelisted action and emotion values.
- Clip `duration_ms` and `local_delta`.
- Expose semantic MCP tools such as `pet_get_state` and `pet_emit_action`.
- Do not expose raw GPIO, PWM, flash, or memory controls.

### Phase 5: IMU and Haptic

Add posture and haptic hardware after Phase 1-4 are stable:

- Verify IMU identity before thresholds are tuned.
- Implement pickup, putdown, gentle rock, shake, freefall, and impact.
- The current haptic peripheral is a CN3 2pin 直流振动马达 wired to
  MS32008N1 OUT5A/OUT5B, not the old J2/J3 5pin stepper connector;
  不要再按 MS32008N1 五线步进电机设计振动反馈。
- Wrap the 2pin DC vibration motor behind haptic pattern commands before
  product behavior code uses it.
- Keep `PET_HAPTIC_ENABLE_MS32008N1_OUTPUT` disabled until the OUT5 DC output
  register map is confirmed.

## Acceptance Criteria

Phase 1 is accepted when:

1. Static guards pass.
2. Firmware builds.
3. Serial `pet` commands can drive all supported local states.
4. Eye switching continues through existing `EYE_SWITCH` and `EYE_AVI_FPS`
   logs without yellow/blue diagnostic fallback, decode-loop changes, or
   display hot-path regressions.
5. OTA recovery path remains available.
