#include <os/str.h>
#include <os/os.h>

#include "cli.h"
#include "pet_brain.h"
#include "pet_action_router.h"
#include "pet_behavior_runtime.h"
#include "pet_haptic.h"
#include "pet_motion.h"
#include "pet_prompt.h"
#include "pet_scene.h"
#include "pet_vision.h"
#include "dialog_module.h"
#include "net_config.h"
#include "system_manager.h"
#if CONFIG_IOT_DEV_CAMERA
#include "iot_camera.h"
#include "mcp_server.h"
#endif

static void cli_pet_help(void)
{
    CLI_RAW_LOGI("pet status|emote|event|action|behavior|scene|prompt|haptic|motion|voice|privacy|idle|ble_pair\r\n");
    CLI_RAW_LOGI("  pet status\r\n");
    CLI_RAW_LOGI("  pet emote <emotion>\r\n");
    CLI_RAW_LOGI("  pet event <event>\r\n");
    CLI_RAW_LOGI("  pet action <json>\r\n");
    CLI_RAW_LOGI("  pet action <json>|<action> [emotion] [duration_ms] [haptic]\r\n");
    CLI_RAW_LOGI("  pet behavior status\r\n");
    CLI_RAW_LOGI("  pet behavior event <event>\r\n");
    CLI_RAW_LOGI("  pet behavior json <json>\r\n");
    CLI_RAW_LOGI("  pet scene status\r\n");
    CLI_RAW_LOGI("  pet scene enter <scene>\r\n");
    CLI_RAW_LOGI("  pet scene event <event>\r\n");
    CLI_RAW_LOGI("  pet prompt play <id>\r\n");
    CLI_RAW_LOGI("  pet haptic status|stop|dc_test|<pattern> [duration_ms]\r\n");
    CLI_RAW_LOGI("  pet haptic probe\r\n");
    CLI_RAW_LOGI("  pet haptic dc_test [duration_ms] [reverse]\r\n");
    CLI_RAW_LOGI("  pet motion status|probe|sample|monitor|route\r\n");
    CLI_RAW_LOGI("  pet motion monitor on|off\r\n");
    CLI_RAW_LOGI("  pet motion route <event>\r\n");
    CLI_RAW_LOGI("  pet vision capture|status|explain [question]\r\n");
    CLI_RAW_LOGI("  pet voice connect|record_start|record_stop|tts_start|tts_stop|abort|idle|status|prompt|volume\r\n");
    CLI_RAW_LOGI("  pet voice status\r\n");
    CLI_RAW_LOGI("  pet voice prompt [file]\r\n");
    CLI_RAW_LOGI("  pet voice volume <0-100>\r\n");
    CLI_RAW_LOGI("  pet privacy on|off\r\n");
    CLI_RAW_LOGI("  pet idle\r\n");
    CLI_RAW_LOGI("  pet ble_pair\r\n");
}

static void cli_pet_status(void)
{
    pet_brain_snapshot_t snapshot = {0};

    pet_brain_get_snapshot(&snapshot);
    CLI_LOGI("pet state=%s privacy=%d mood=%d energy=%d affection=%d security=%d novelty=%d stress=%d last_event=%s last_action=%s last_emotion=%s\r\n",
             pet_brain_state_name(snapshot.state),
             snapshot.privacy,
             snapshot.mood,
             snapshot.energy,
             snapshot.affection,
             snapshot.security,
             snapshot.novelty,
             snapshot.stress,
             pet_brain_event_name(snapshot.last_event),
             pet_action_router_action_name(snapshot.last_action),
             snapshot.last_emotion);
}

static bk_err_t cli_pet_apply_action_request(pet_action_request_t *request)
{
    bk_err_t ret = BK_OK;

    ret = pet_brain_apply_action_request(request);
    if (ret == BK_OK) {
        CLI_LOGI("pet action accepted\r\n");
    } else {
        CLI_LOGE("pet action rejected: %d\r\n", ret);
    }

    return ret;
}

static void cli_pet_action_json(const char *json)
{
    cJSON *root = NULL;
    pet_action_request_t request = {0};
    bk_err_t ret = BK_OK;

    root = cJSON_Parse(json);
    if (root == NULL) {
        CLI_LOGE("pet action json parse failed\r\n");
        return;
    }

    ret = pet_action_router_request_from_json(root, &request);
    if (ret == BK_OK) {
        ret = cli_pet_apply_action_request(&request);
    } else {
        CLI_LOGE("pet action rejected: %d\r\n", ret);
    }

    cJSON_Delete(root);
}

static void cli_pet_prompt_cmd(int argc, char **argv)
{
    pet_prompt_gate_t gate = {
        .tts_active = false,
        .can_interrupt_tts = true,
    };
    pet_prompt_skip_reason_t skip_reason = PET_PROMPT_SKIP_NONE;
    pet_prompt_result_t result = PET_PROMPT_RESULT_ERROR;

    if (argc != 4 || os_strcmp(argv[2], "play") != 0) {
        CLI_LOGE("pet prompt play <id>\r\n");
        return;
    }

    result = pet_prompt_play(argv[3], &gate, &skip_reason);
    CLI_LOGI("pet prompt result=%s skip=%s id=%s\r\n",
             pet_prompt_result_name(result),
             pet_prompt_skip_reason_name(skip_reason),
             argv[3]);
}

static void cli_pet_behavior_status(void)
{
    pet_behavior_runtime_status_t status = {0};

    pet_behavior_runtime_get_status(&status);
    CLI_LOGI("pet behavior tts_active=%d last_eye_ms=%u last_prompt_ms=%u last_haptic_ms=%u last_emotion=%s last_prompt=%s last_haptic=%s\r\n",
             status.tts_active,
             status.last_eye_ms,
             status.last_prompt_ms,
             status.last_haptic_ms,
             status.last_emotion,
             status.last_prompt_id,
             status.last_haptic);
}

static void cli_pet_behavior_cmd(int argc, char **argv)
{
    if (argc < 3) {
        CLI_LOGE("pet behavior status|event <event>|json <json>\r\n");
        return;
    }

    if (os_strcmp(argv[2], "status") == 0) {
        if (argc != 3) {
            CLI_LOGE("pet behavior status\r\n");
            return;
        }
        cli_pet_behavior_status();
    } else if (os_strcmp(argv[2], "event") == 0) {
        pet_event_type_t event = PET_EVENT_NONE;
        bk_err_t ret = BK_OK;

        if (argc != 4) {
            CLI_LOGE("pet behavior event <event>\r\n");
            return;
        }

        event = pet_brain_event_from_name(argv[3]);
        if (event == PET_EVENT_NONE) {
            CLI_LOGE("unknown pet event: %s\r\n", argv[3]);
            return;
        }

        ret = pet_brain_handle_event(event);
        CLI_LOGI("pet behavior event=%s ret=%d\r\n", argv[3], ret);
    } else if (os_strcmp(argv[2], "json") == 0) {
        if (argc != 4) {
            CLI_LOGE("pet behavior json <json>\r\n");
            return;
        }
        cli_pet_action_json(argv[3]);
    } else {
        CLI_LOGE("pet behavior status|event <event>|json <json>\r\n");
    }
}

static void cli_pet_scene_status(void)
{
    pet_scene_status_t status = {0};

    pet_scene_get_status(&status);
    CLI_LOGI("pet scene current=%s last=%s event=%s last_ms=%u rejected=%u reject=%s online=%d voice=%d vision=%d privacy=%d\r\n",
             pet_scene_type_name(status.current_scene),
             pet_scene_type_name(status.last_scene),
             pet_brain_event_name(status.last_event),
             status.last_enter_ms,
             status.rejected_count,
             pet_scene_reject_reason_name(status.last_reject_reason),
             status.cloud_online,
             status.voice_speaking,
             status.vision_busy,
             status.privacy_active);
}

static void cli_pet_scene_cmd(int argc, char **argv)
{
    if (argc < 3) {
        CLI_LOGE("pet scene status|enter <scene>|event <event>\r\n");
        return;
    }

    if (os_strcmp(argv[2], "status") == 0) {
        if (argc != 3) {
            CLI_LOGE("pet scene status\r\n");
            return;
        }
        cli_pet_scene_status();
    } else if (os_strcmp(argv[2], "enter") == 0) {
        pet_scene_type_t scene = PET_SCENE_NONE;
        bk_err_t ret = BK_OK;

        if (argc != 4) {
            CLI_LOGE("pet scene enter <scene>\r\n");
            return;
        }

        scene = pet_scene_from_name(argv[3]);
        if (scene == PET_SCENE_NONE) {
            CLI_LOGE("unknown pet scene: %s\r\n", argv[3]);
            return;
        }

        ret = pet_scene_enter(scene);
        CLI_LOGI("pet scene enter=%s ret=%d\r\n", argv[3], ret);
    } else if (os_strcmp(argv[2], "event") == 0) {
        pet_event_type_t event = PET_EVENT_NONE;
        bk_err_t ret = BK_OK;

        if (argc != 4) {
            CLI_LOGE("pet scene event <event>\r\n");
            return;
        }

        event = pet_brain_event_from_name(argv[3]);
        if (event == PET_EVENT_NONE) {
            CLI_LOGE("unknown pet event: %s\r\n", argv[3]);
            return;
        }

        ret = pet_scene_handle_event(event);
        CLI_LOGI("pet scene event=%s ret=%d\r\n", argv[3], ret);
    } else {
        CLI_LOGE("pet scene status|enter <scene>|event <event>\r\n");
    }
}

static void cli_pet_motion_status(void)
{
    pet_motion_status_t status = {0};

    pet_motion_get_status(&status);
    CLI_LOGI("pet motion ready=%d monitor=%d variant=%s bus=%s addr=0x%02x last=%s\r\n",
             status.ready,
             status.monitor_enabled,
             status.variant_name,
             status.bus_name,
             status.addr,
             pet_motion_event_name(status.last_event));
}

static void cli_pet_motion_sample(void)
{
    pet_motion_sample_t sample = {0};
    bk_err_t ret = BK_OK;

    ret = pet_motion_read_sample(&sample);
    if (ret != BK_OK) {
        CLI_LOGE("pet motion sample failed: %d\r\n", ret);
        return;
    }

    CLI_LOGI("pet motion sample variant=%s bus=%s addr=0x%02x raw=(%d,%d,%d) mg=(%d,%d,%d) mag=%u event=%s\r\n",
             sample.variant_name,
             sample.bus_name,
             sample.addr,
             sample.x_raw,
             sample.y_raw,
             sample.z_raw,
             sample.x_mg,
             sample.y_mg,
             sample.z_mg,
             sample.magnitude_mg,
             pet_motion_event_name(pet_motion_classify_sample(&sample)));
}

static void cli_pet_motion_cmd(int argc, char **argv)
{
    if (argc < 3) {
        CLI_LOGE("pet motion status|probe|sample|monitor on|off|route <event>\r\n");
        return;
    }

    if (os_strcmp(argv[2], "status") == 0) {
        cli_pet_motion_status();
    } else if (os_strcmp(argv[2], "probe") == 0) {
        bk_err_t ret = pet_motion_probe();
        if (ret == BK_OK) {
            CLI_LOGI("pet motion probe ok\r\n");
        } else {
            CLI_LOGE("pet motion probe failed: %d\r\n", ret);
        }
        cli_pet_motion_status();
    } else if (os_strcmp(argv[2], "sample") == 0) {
        cli_pet_motion_sample();
    } else if (os_strcmp(argv[2], "monitor") == 0) {
        bk_err_t ret = BK_OK;

        if (argc != 4) {
            CLI_LOGE("pet motion monitor on|off\r\n");
            return;
        }

        if (os_strcmp(argv[3], "on") == 0) {
            ret = pet_motion_set_monitor_enabled(true);
        } else if (os_strcmp(argv[3], "off") == 0) {
            ret = pet_motion_set_monitor_enabled(false);
        } else {
            CLI_LOGE("pet motion monitor on|off\r\n");
            return;
        }

        if (ret == BK_OK) {
            CLI_LOGI("pet motion monitor %s\r\n", argv[3]);
        } else {
            CLI_LOGE("pet motion monitor failed: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[2], "route") == 0) {
        pet_motion_event_t event = PET_MOTION_EVENT_NONE;
        bk_err_t ret = BK_OK;

        if (argc != 4) {
            CLI_LOGE("pet motion route <event>\r\n");
            return;
        }

        event = pet_motion_event_from_name(argv[3]);
        if (event == PET_MOTION_EVENT_NONE) {
            CLI_LOGE("unknown motion event: %s\r\n", argv[3]);
            return;
        }

        ret = pet_motion_route_event(event);
        if (ret == BK_OK) {
            CLI_LOGI("pet motion route accepted: %s\r\n", argv[3]);
        } else {
            CLI_LOGE("pet motion route rejected: %d\r\n", ret);
        }
    } else {
        CLI_LOGE("Unknown pet motion command: %s\r\n", argv[2]);
    }
}

static void cli_pet_haptic_status(void)
{
    pet_haptic_status_t status = {0};

    pet_haptic_get_status(&status);
    CLI_LOGI("pet haptic status init=%d hw=%d awake=%d nsleep_gpio=%u i2c=%u addr=0x%02x last=%s duration=%u\r\n",
             status.initialized,
             status.hardware_output_enabled,
             status.driver_awake,
             status.nsleep_gpio,
             status.i2c_id,
             status.i2c_addr,
             status.last_pattern,
             status.last_duration_ms);
}

static void cli_pet_vision_capture(void)
{
    pet_vision_capture_t capture = {0};
    bk_err_t ret = pet_vision_capture_once(&capture);

    if (ret != BK_OK) {
        CLI_LOGE("pet vision capture failed: %d\r\n", ret);
        return;
    }

    CLI_LOGI("pet vision capture ok width=%u height=%u size=%u length=%u jpeg_soi=%d prefix=%02x %02x %02x %02x\r\n",
             capture.width,
             capture.height,
             capture.size,
             capture.length,
             capture.jpeg_soi,
             capture.prefix[0],
             capture.prefix[1],
             capture.prefix[2],
             capture.prefix[3]);
}

static void cli_pet_vision_status(void)
{
#if CONFIG_IOT_DEV_CAMERA
    char *url = mcp_server_instance()->get_explain_url();
    char *token = mcp_server_instance()->get_explain_token();

    CLI_LOGI("pet vision status ready=%d url=%s token_len=%u\r\n",
             iot_camera_vision_ready(),
             (url != NULL && url[0] != '\0') ? url : "(empty)",
             (token != NULL) ? os_strlen(token) : 0);
#else
    CLI_LOGE("pet vision not supported\r\n");
#endif
}

static void cli_pet_vision_explain(int argc, char **argv)
{
#if CONFIG_IOT_DEV_CAMERA
    const char *question = "请描述你现在看到的画面。";
    char *result = NULL;
    bk_err_t ret = BK_OK;

    if (argc > 4) {
        CLI_LOGE("pet vision explain [question]\r\n");
        return;
    }

    if (argc == 4) {
        question = argv[3];
    }

    ret = iot_camera_take_photo_and_explain(question, &result);
    if (ret != BK_OK) {
        CLI_LOGE("pet vision explain failed: %d\r\n", ret);
        return;
    }

    CLI_LOGI("pet vision explain ok: %s\r\n", result ? result : "");
    if (result) {
        psram_free(result);
    }
#else
    (void)argc;
    (void)argv;
    CLI_LOGE("pet vision not supported\r\n");
#endif
}

static void cli_pet_voice_status(void)
{
    dialog_voice_diag_t diag = {0};
    system_manager_module_t *system_module = system_manager_instance();
    system_status_e status = system_module->get_system_status();
    uint32_t now_ms = rtos_get_time();
    uint32_t cb_age_ms = 0;

    dialog_voice_diag_get(&diag);
    if (diag.mic_last_cb_ms > 0) {
        cb_age_ms = now_ms - diag.mic_last_cb_ms;
    }

    CLI_LOGI("pet voice diag cb=%u rec=%u fwd=%u drop_status=%u bytes=%u fwd_bytes=%u last_status=%d last_len=%u last_cb_ms=%u cb_age_ms=%u now_ms=%u ws_count=%u ws_bytes=%u ws_fail=%u ws_last_len=%d ws_ret=%d status=%s\r\n",
             diag.mic_cb_total,
             diag.mic_cb_recording,
             diag.mic_cb_forwarded,
             diag.mic_cb_dropped_status,
             diag.mic_cb_bytes,
             diag.mic_forwarded_bytes,
             diag.mic_last_status,
             diag.mic_last_len,
             diag.mic_last_cb_ms,
             cb_age_ms,
             now_ms,
             diag.ws_audio_tx_count,
             diag.ws_audio_tx_bytes,
             diag.ws_audio_tx_fail,
             diag.ws_audio_last_len,
             diag.ws_audio_last_ret,
             system_module->get_system_status_string_fmt(status));
}

static void cli_pet_voice_prompt(int argc, char **argv)
{
    char *prompt_path = PROMPT_NETWORK_CONNECTED;

    if (argc > 4) {
        CLI_LOGE("pet voice prompt [file]\r\n");
        return;
    }

    if (argc == 4) {
        prompt_path = argv[3];
    }

    dialog_module_instance()->speaker_play_prompt_tone(prompt_path);
    CLI_LOGI("pet voice prompt played: %s\r\n", prompt_path);
}

static void cli_pet_voice_volume(int argc, char **argv)
{
    uint32_t volume = 0;

    if (argc != 4) {
        CLI_LOGE("pet voice volume <0-100>\r\n");
        return;
    }

    volume = os_strtoul(argv[3], NULL, 10);
    if (volume > 100) {
        CLI_LOGE("pet voice volume out of range: %u\r\n", volume);
        return;
    }

    dialog_module_instance()->speaker_set_volume((u8)volume);
    CLI_LOGI("pet voice volume set: %u\r\n", volume);
}

static void cli_pet_voice_cmd(int argc, char **argv)
{
    system_event_e event = SYSTEM_EVENT_NULL;

    if (argc < 3) {
        CLI_LOGE("pet voice connect|record_start|record_stop|tts_start|tts_stop|abort|idle|status|prompt|volume\r\n");
        return;
    }

    if (os_strcmp(argv[2], "status") == 0) {
        if (argc != 3) {
            CLI_LOGE("pet voice status\r\n");
            return;
        }
        cli_pet_voice_status();
        return;
    } else if (os_strcmp(argv[2], "prompt") == 0) {
        cli_pet_voice_prompt(argc, argv);
        return;
    } else if (os_strcmp(argv[2], "volume") == 0) {
        cli_pet_voice_volume(argc, argv);
        return;
    } else if (os_strcmp(argv[2], "connect") == 0) {
        event = SYSTEM_EVENT_DIALOG_START;
    } else if (os_strcmp(argv[2], "record_start") == 0) {
        event = SYSTEM_EVENT_RECORD_START;
    } else if (os_strcmp(argv[2], "record_stop") == 0) {
        event = SYSTEM_EVENT_RECORD_END;
    } else if (os_strcmp(argv[2], "tts_start") == 0) {
        event = SYSTEM_EVENT_PLAY_START;
    } else if (os_strcmp(argv[2], "tts_stop") == 0) {
        event = SYSTEM_EVENT_PLAY_END;
    } else if (os_strcmp(argv[2], "abort") == 0) {
        event = SYSTEM_EVENT_DIALOG_ABORT;
    } else if (os_strcmp(argv[2], "idle") == 0) {
        event = SYSTEM_EVENT_IDLE;
    } else {
        CLI_LOGE("Unknown pet voice command: %s\r\n", argv[2]);
        return;
    }

    if (argc != 3) {
        CLI_LOGE("pet voice %s\r\n", argv[2]);
        return;
    }

    system_manager_instance()->send_msg_by_event(event);
    CLI_LOGI("pet voice event sent: %s\r\n", argv[2]);
}

static void cli_pet_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    (void)pcWriteBuffer;
    (void)xWriteBufferLen;

    if (argc < 2 || argv == NULL) {
        cli_pet_help();
        return;
    }

    if (os_strcmp(argv[1], "status") == 0) {
        cli_pet_status();
    } else if (os_strcmp(argv[1], "emote") == 0) {
        if (argc != 3) {
            CLI_LOGE("pet emote <emotion>\r\n");
            return;
        }
        if (pet_brain_emit_emotion(argv[2]) == BK_OK) {
            CLI_LOGI("pet emote accepted: %s\r\n", argv[2]);
        } else {
            CLI_LOGE("pet emote rejected: %s\r\n", argv[2]);
        }
    } else if (os_strcmp(argv[1], "event") == 0) {
        pet_event_type_t event = PET_EVENT_NONE;

        if (argc != 3) {
            CLI_LOGE("pet event <event>\r\n");
            return;
        }

        event = pet_brain_event_from_name(argv[2]);
        if (event == PET_EVENT_NONE) {
            CLI_LOGE("unknown pet event: %s\r\n", argv[2]);
            return;
        }

        if (pet_brain_handle_event(event) == BK_OK) {
            CLI_LOGI("pet event accepted: %s\r\n", argv[2]);
        } else {
            CLI_LOGE("pet event rejected: %s\r\n", argv[2]);
        }
    } else if (os_strcmp(argv[1], "action") == 0) {
        pet_action_request_t request = {0};

        if (argc < 3) {
            CLI_LOGE("pet action <json>|<action> [emotion] [duration_ms]\r\n");
            return;
        }

        if (argv[2][0] == '{') {
            if (argc != 3) {
                CLI_LOGE("pet action <json>\r\n");
                return;
            }

            cli_pet_action_json(argv[2]);
            return;
        }

        request.action = pet_action_router_action_from_name(argv[2]);
        request.emotion = (argc >= 4) ? argv[3] : "smiling";
        request.duration_ms = (argc >= 5) ? pet_action_router_clip_duration(os_strtoul(argv[4], NULL, 10)) : 2000;
        request.haptic = (argc >= 6) ? argv[5] : NULL;
        (void)cli_pet_apply_action_request(&request);
    } else if (os_strcmp(argv[1], "behavior") == 0) {
        cli_pet_behavior_cmd(argc, argv);
    } else if (os_strcmp(argv[1], "scene") == 0) {
        cli_pet_scene_cmd(argc, argv);
    } else if (os_strcmp(argv[1], "prompt") == 0) {
        cli_pet_prompt_cmd(argc, argv);
    } else if (os_strcmp(argv[1], "haptic") == 0) {
        uint32_t duration_ms = 0;
        bk_err_t ret = BK_OK;

        if (argc < 3 || argc > 5) {
            CLI_LOGE("pet haptic status|probe|stop|dc_test|<pattern> [duration_ms]\r\n");
            return;
        }

        if (os_strcmp(argv[2], "status") == 0) {
            if (argc != 3) {
                CLI_LOGE("pet haptic status\r\n");
                return;
            }
            cli_pet_haptic_status();
            return;
        }

        if (os_strcmp(argv[2], "probe") == 0) {
            pet_haptic_probe_result_t probe = {0};

            if (argc != 3) {
                CLI_LOGE("pet haptic probe\r\n");
                return;
            }

            ret = pet_haptic_probe(&probe);
            CLI_LOGI("pet haptic probe addr=0x%02x ack=%d ret=%d i2c=%u chip=0x%02x\r\n",
                     probe.i2c_addr,
                     probe.ack,
                     probe.ret,
                     probe.i2c_id,
                     probe.chip_flag);
            if (ret != BK_OK) {
                CLI_LOGE("pet haptic probe failed: %d\r\n", ret);
            }
            return;
        }

        if (os_strcmp(argv[2], "dc_test") == 0) {
            pet_haptic_dc_test_result_t dc_result = {0};
            bool reverse = false;

            if (argc > 5) {
                CLI_LOGE("pet haptic dc_test [duration_ms] [reverse]\r\n");
                return;
            }

            duration_ms = (argc >= 4) ? os_strtoul(argv[3], NULL, 10) : PET_HAPTIC_DC_TEST_DEFAULT_MS;
            reverse = (argc >= 5 && os_strcmp(argv[4], "reverse") == 0);

            ret = pet_haptic_dc_test(duration_ms, reverse, &dc_result);
            CLI_LOGI("pet haptic dc_test ret=%d requested=%u actual=%u reverse=%d chip=0x%02x\r\n",
                     dc_result.ret,
                     dc_result.requested_duration_ms,
                     dc_result.actual_duration_ms,
                     dc_result.reverse,
                     dc_result.chip_flag);
            if (ret != BK_OK) {
                CLI_LOGE("pet haptic dc_test failed: %d\r\n", ret);
            }
            return;
        }

        if (os_strcmp(argv[2], "stop") == 0) {
            if (argc != 3) {
                CLI_LOGE("pet haptic stop\r\n");
                return;
            }
            ret = pet_haptic_stop();
            if (ret == BK_OK) {
                CLI_LOGI("pet haptic stopped\r\n");
            } else {
                CLI_LOGE("pet haptic stop rejected: %d\r\n", ret);
            }
            return;
        }

        duration_ms = (argc >= 4) ? os_strtoul(argv[3], NULL, 10) : 0;
        ret = pet_haptic_play(argv[2], duration_ms);
        if (ret == BK_OK) {
            CLI_LOGI("pet haptic accepted pattern=%s duration=%u\r\n", argv[2], duration_ms);
        } else {
            CLI_LOGE("pet haptic rejected: %d\r\n", ret);
        }
    } else if (os_strcmp(argv[1], "motion") == 0) {
        cli_pet_motion_cmd(argc, argv);
    } else if (os_strcmp(argv[1], "vision") == 0) {
        if (argc == 3 && os_strcmp(argv[2], "capture") == 0) {
            cli_pet_vision_capture();
        } else if (argc == 3 && os_strcmp(argv[2], "status") == 0) {
            cli_pet_vision_status();
        } else if ((argc == 3 || argc == 4) && os_strcmp(argv[2], "explain") == 0) {
            cli_pet_vision_explain(argc, argv);
        } else {
            CLI_LOGE("pet vision capture|status|explain [question]\r\n");
        }
    } else if (os_strcmp(argv[1], "voice") == 0) {
        cli_pet_voice_cmd(argc, argv);
    } else if (os_strcmp(argv[1], "privacy") == 0) {
        if (argc != 3) {
            CLI_LOGE("pet privacy on|off\r\n");
            return;
        }
        if (os_strcmp(argv[2], "on") == 0) {
            bk_err_t ret = pet_scene_handle_event(PET_EVENT_PRIVACY_ON);
            CLI_LOGI("pet privacy on ret=%d\r\n", ret);
        } else if (os_strcmp(argv[2], "off") == 0) {
            bk_err_t ret = pet_scene_handle_event(PET_EVENT_PRIVACY_OFF);
            CLI_LOGI("pet privacy off ret=%d\r\n", ret);
        } else {
            CLI_LOGE("pet privacy on|off\r\n");
        }
    } else if (os_strcmp(argv[1], "idle") == 0) {
        (void)pet_brain_handle_event(PET_EVENT_IDLE);
        CLI_LOGI("pet idle\r\n");
    } else if (os_strcmp(argv[1], "ble_pair") == 0) {
        int ret = BK_FAIL;

        if (argc != 2) {
            CLI_LOGE("pet ble_pair\r\n");
            return;
        }

        ret = net_config_instance()->start_ble_provisioning();
        CLI_LOGI("pet BLE pairing start ret=%d\r\n", ret);
    } else if (os_strcmp(argv[1], "help") == 0) {
        cli_pet_help();
    } else {
        CLI_LOGE("Unknown pet command: %s\r\n", argv[1]);
        cli_pet_help();
    }
}

static const struct cli_command s_pet_commands[] = {
    {"pet", "pet status|emote|event|action|behavior|prompt|haptic|motion|voice|privacy|idle|ble_pair", cli_pet_cmd},
};

int cli_app_pet_init(void)
{
    return cli_register_commands(s_pet_commands, sizeof(s_pet_commands) / sizeof(struct cli_command));
}
