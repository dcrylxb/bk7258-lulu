from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "projects" / "cmaiw82al_ai_toy"


def read(relpath: str) -> str:
    return (PROJECT / relpath).read_text(encoding="utf-8", errors="ignore")


class DeviceAudioPushStaticTests(unittest.TestCase):
    def test_websocket_play_audio_request_is_queued_not_played_inline(self):
        websocket = read("ap/main/protocols/protocol_websocket.c")
        system_h = read("ap/main/system_manager/system_manager.h")
        system_c = read("ap/main/system_manager/system_manager.c")

        for marker in [
            '#define WS_DEVICE_PLAY_AUDIO_PATH "/api/device/play_audio"',
            "static void send_manager_response",
            'cJSON_GetObjectItemCaseSensitive(root, "method")',
            'cJSON_GetObjectItemCaseSensitive(root, "path")',
            'cJSON_GetObjectItemCaseSensitive(root, "body")',
            'cJSON_GetObjectItemCaseSensitive(body, "audio_url")',
            "system_play_audio_request_t",
            "SYSTEM_EVENT_PLAY_AUDIO_URL",
            "system_manager_instance()->send_msg(&msg);",
            "play_audio queued title=%s content_id=%s url=%s",
            'send_manager_response(request_id, 202, "queued", "play_audio queued");',
        ]:
            self.assertIn(marker, websocket + system_h)

        self.assertNotIn("app_audio_player_start(", websocket)
        self.assertNotIn("app_audio_player_add_music(", websocket)
        self.assertNotIn("webclient_get(", websocket)

        play_case_start = system_c.index("case SYSTEM_EVENT_PLAY_AUDIO_URL:")
        play_case = system_c[
            play_case_start:
            system_c.index("break;", play_case_start)
        ]
        self.assertIn("_play_remote_audio_url((system_play_audio_request_t *)msg.param);", play_case)

        play_helper_start = system_c.index("static void _play_remote_audio_url")
        play_helper = system_c[
            play_helper_start:
            system_c.index("static void _manager_task", play_helper_start)
        ]
        for marker in [
            "system_play_audio_request_t *request",
            "app_audio_player_stop();",
            "app_audio_player_clear_music_list();",
            "app_audio_player_add_music(request->title, request->audio_url);",
            "app_audio_player_start();",
            "remote audio start title=%s content_id=%s url=%s",
        ]:
            self.assertIn(marker, play_helper)

    def test_websocket_manager_mcp_call_is_bridged_to_device_mcp_server(self):
        websocket = read("ap/main/protocols/protocol_websocket.c")
        websocket_h = read("ap/main/protocols/protocol_websocket.h")
        mcp_h = read("ap/main/protocols/mcp_server.h")
        mcp_c = read("ap/main/protocols/mcp_server.c")

        for marker in [
            '#define WS_MCP_CALL_PATH "/api/mcp/call"',
            "mcp_server_instance()->set_manager_request_id",
            "mcp_server_instance()->recv_msg_cb(mcp_request);",
            'cJSON_AddStringToObject(mcp_request, "jsonrpc", "2.0");',
            'cJSON_AddStringToObject(mcp_request, "method", "tools/call");',
            'cJSON_AddStringToObject(params, "name", tool_name->valuestring);',
            'cJSON_AddItemReferenceToObject(params, "arguments", arguments);',
        ]:
            self.assertIn(marker, websocket)

        for marker in [
            "void (*sendManagerResponse)(uint8_t*);",
            ".sendManagerResponse = _protocol_websocket_send_text",
        ]:
            self.assertIn(marker, websocket_h + websocket)

        for marker in [
            "char manager_request_id[64];",
            "void (*set_manager_request_id)(int, const char *);",
            "static void mcp_server_set_manager_request_id",
            "static bool mcp_server_take_manager_request_id",
            "if (!mcp_server_reply_manager_response(id, 200, result, NULL))",
            "if (!mcp_server_reply_manager_response(id, 500, NULL, message))",
            'cJSON_AddStringToObject(response, "id", request_id);',
            'cJSON_AddNumberToObject(response, "status", status);',
            'cJSON_AddStringToObject(response, "error", error_message);',
            "protocol_websocket_instance()->sendManagerResponse((uint8_t *)response_str);",
            '"{\\"jsonrpc\\":\\"2.0\\",\\"id\\":%d,\\"error\\":{\\"message\\":\\"%s\\"}}"',
        ]:
            self.assertIn(marker, mcp_h + mcp_c)

    def test_ble_linux_probe_usage_is_documented(self):
        service_doc = (ROOT / "docs" / "06_service_backend.md").read_text(encoding="utf-8", errors="ignore")
        probe = (ROOT / "tools" / "bk_ble_provisioning_probe.py").read_text(encoding="utf-8", errors="ignore")

        for marker in [
            "电脑蓝牙调试经验",
            "python3 tools/bk_ble_provisioning_probe.py scan --timeout 20 --all",
            "python3 tools/bk_ble_provisioning_probe.py wifi-scan <BLE_MAC> --addr-type public",
            "python3 tools/bk_ble_provisioning_probe.py auth-sign <BLE_MAC> \"signing_message\"",
            "App 写入 EA02",
            "设备通知 EA01",
            "opcode=24",
            "opcode=151",
        ]:
            self.assertIn(marker, service_doc)

        for marker in [
            'SERVICE_UUID = "0000fa00-0000-1000-8000-00805f9b34fb"',
            'NOTIFY_UUID = "0000ea01-0000-1000-8000-00805f9b34fb"',
            'WRITE_UUID = "0000ea02-0000-1000-8000-00805f9b34fb"',
            "OP_WIFI_SCAN = 24",
            "OP_AUTH_SIGN = 151",
        ]:
            self.assertIn(marker, probe)

    def test_power_key_double_press_requires_confirmation_before_ble_pairing(self):
        common_h = read("ap/main/common/common.h")
        app_key_c = read("ap/main/key/app_key.c")

        for marker in [
            "APP_EVENT_BLE_PAIR",
            ".double_event = APP_EVENT_BLE_PAIR",
            "#define APP_KEY_BLE_PAIR_CONFIRM_WINDOW_MS 10000",
            "#include \"net_config.h\"",
            "s_ble_pair_confirm_until_ms",
            "BLE pairing first double press, waiting confirm",
            "dialog_module_instance()->speaker_play_prompt_tone(PROMPT_NETWORK_PROVISION);",
            "BLE pairing confirmed by power key double press",
            "net_config_instance()->start_ble_provisioning();",
        ]:
            self.assertIn(marker, common_h + app_key_c)

    def test_audio_player_stop_is_exposed_as_device_mcp_tool(self):
        devices_h = read("ap/main/iot/iot_devices.h")
        devices_c = read("ap/main/iot/iot_devices.c")
        player_c = read("ap/main/iot/iot_audio_player.c")
        cmake = read("ap/CMakeLists.txt")

        for marker in [
            "int iot_audio_player_tool_init(void);",
            "iot_audio_player_tool_init();",
            "./main/iot/iot_audio_player.c",
            "#if (CONFIG_AUDIO_PLAYER)",
            "bk_err_t app_audio_player_stop(void);",
            "static void audio_player_stop_func_cb(mcp_cf_t *msg)",
            "app_audio_player_stop();",
            "audio player is disabled",
            'stop_tool->name = "self.audio_player.stop";',
            "stop_tool->func_cb = audio_player_stop_func_cb;",
            "mcp_server_instance()->reply_result(msg->id, json_string);",
        ]:
            self.assertIn(marker, devices_h + devices_c + player_c + cmake)
        self.assertNotIn('#include "app_audio_player.h"', player_c)


if __name__ == "__main__":
    unittest.main()
