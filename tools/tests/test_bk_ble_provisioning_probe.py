from pathlib import Path
import importlib.util
import sys
import textwrap
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
PROBE_PATH = ROOT / "tools" / "bk_ble_provisioning_probe.py"


def load_probe():
    spec = importlib.util.spec_from_file_location("bk_ble_provisioning_probe", PROBE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class BkBleProvisioningProbeTests(unittest.TestCase):
    def test_candidate_matches_fe01_advertisement_even_when_name_is_not_bk(self):
        probe = load_probe()
        device = probe.Device(
            address="AA:BB:CC:DD:EE:FF",
            name="Unknown",
            details="UUIDs: 0000fe01-0000-1000-8000-00805f9b34fb",
        )

        self.assertTrue(probe.is_candidate(device))

    def test_scan_parser_keeps_bluetoothctl_change_details_for_candidate_matching(self):
        probe = load_probe()
        output = textwrap.dedent(
            """
            [NEW] Device AA:BB:CC:DD:EE:FF Unknown
            [CHG] Device AA:BB:CC:DD:EE:FF UUIDs: 0000fe01-0000-1000-8000-00805f9b34fb
            """
        )

        devices = probe.parse_scan_output(output)

        self.assertEqual(len(devices), 1)
        self.assertEqual(devices[0].name, "Unknown")
        self.assertIn("FE01", devices[0].details.upper())
        self.assertTrue(probe.is_candidate(devices[0]))

    def test_scan_devices_enriches_devices_from_bluetoothctl_info(self):
        probe = load_probe()

        def fake_run(cmd, **kwargs):
            if cmd[:3] == ["bluetoothctl", "--timeout", "1"]:
                return type("Result", (), {
                    "stdout": "[NEW] Device AA:BB:CC:DD:EE:FF Unknown\n",
                    "returncode": 0,
                })()
            if cmd == ["bluetoothctl", "info", "AA:BB:CC:DD:EE:FF"]:
                return type("Result", (), {
                    "stdout": "UUID: Vendor specific (0000fe01-0000-1000-8000-00805f9b34fb)\n",
                    "returncode": 0,
                })()
            raise AssertionError(f"unexpected command: {cmd}")

        with patch.object(probe.subprocess, "run", side_effect=fake_run):
            devices = probe.scan_devices(1)

        self.assertEqual(len(devices), 1)
        self.assertIn("FE01", devices[0].details.upper())
        self.assertTrue(probe.is_candidate(devices[0]))


if __name__ == "__main__":
    unittest.main()
