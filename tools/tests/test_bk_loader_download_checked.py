from pathlib import Path
import io
import importlib.util
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = ROOT / "tools" / "bk_loader_download_checked.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("bk_loader_download_checked", TOOL_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class BkLoaderDownloadCheckedTests(unittest.TestCase):
    def test_failed_output_overrides_zero_return_code(self):
        tool = load_tool()

        result = tool.classify_download_result(0, "Download fail.\nElapse time: 10.9s\n")

        self.assertFalse(result.ok)
        self.assertIn("Download fail", result.reason)

    def test_success_requires_download_complete_and_all_pass(self):
        tool = load_tool()

        success = tool.classify_download_result(0, "Download complete, all pass\n")
        missing_marker = tool.classify_download_result(0, "do reboot from bootrom\n")

        self.assertTrue(success.ok)
        self.assertFalse(missing_marker.ok)
        self.assertIn("success marker", missing_marker.reason)

    def test_build_download_command_uses_safe_defaults(self):
        tool = load_tool()

        cmd = tool.build_download_command(
            loader="/home/jason/armino1/bk_loader",
            port="/dev/ttyUSB0",
            image="/tmp/all-app.bin",
            baudrate=2000000,
            link_type=4,
            reset_type=3,
            reset_baudrate=115200,
            startaddr="0x0",
            pre_erase=0,
            reboot=True,
        )

        self.assertEqual(cmd[:2], ["/home/jason/armino1/bk_loader", "download"])
        self.assertIn("--link_type", cmd)
        self.assertIn("4", cmd)
        self.assertIn("-e", cmd)
        self.assertIn("0", cmd)
        self.assertIn("-r", cmd)
        self.assertIn("/tmp/all-app.bin", cmd)

    def test_run_download_returns_failure_when_loader_prints_get_bus_failed(self):
        tool = load_tool()

        completed = type("Completed", (), {
            "returncode": 0,
            "stdout": "Get bus failed\nDownload fail.\n",
            "stderr": "",
        })()
        with patch.object(tool.subprocess, "run", return_value=completed), \
                patch("sys.stdout", new_callable=io.StringIO), \
                patch("sys.stderr", new_callable=io.StringIO):
            rc = tool.run_download([
                "--loader", "/home/jason/armino1/bk_loader",
                "--port", "/dev/ttyUSB0",
                "--image", "/tmp/all-app.bin",
            ])

        self.assertEqual(rc, 1)

    def test_run_download_accepts_bk_loader_style_short_options(self):
        tool = load_tool()

        completed = type("Completed", (), {
            "returncode": 0,
            "stdout": "Download complete, all pass\n",
            "stderr": "",
        })()
        with patch.object(tool.subprocess, "run", return_value=completed) as run, \
                patch("sys.stdout", new_callable=io.StringIO), \
                patch("sys.stderr", new_callable=io.StringIO):
            rc = tool.run_download([
                "--loader", "/home/jason/armino1/bk_loader",
                "-p", "/dev/ttyUSB0",
                "-i", "/tmp/all-app.bin",
                "-b", "2000000",
                "-s", "0x0",
                "-e", "0",
            ])

        self.assertEqual(rc, 0)
        self.assertIn("-p", run.call_args.args[0])
        self.assertIn("/tmp/all-app.bin", run.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
