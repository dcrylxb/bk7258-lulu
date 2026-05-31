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
            'cJSON_GetObjectItemCaseSensitive(root, "method")',
            'cJSON_GetObjectItemCaseSensitive(root, "path")',
            'cJSON_GetObjectItemCaseSensitive(root, "body")',
            'cJSON_GetObjectItemCaseSensitive(body, "audio_url")',
            "system_play_audio_request_t",
            "SYSTEM_EVENT_PLAY_AUDIO_URL",
            "system_manager_instance()->send_msg(&msg);",
            "play_audio queued title=%s content_id=%s url=%s",
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


if __name__ == "__main__":
    unittest.main()
