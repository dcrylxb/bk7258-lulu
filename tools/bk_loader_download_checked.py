#!/usr/bin/env python3
import argparse
from dataclasses import dataclass
import subprocess
import sys


SUCCESS_MARKERS = (
    "Download complete, all pass",
    "Download complete",
)
FAILURE_MARKERS = (
    "Download fail",
    "get bus fail",
    "Get bus failed",
    "Read fail",
)


@dataclass
class DownloadResult:
    ok: bool
    reason: str


def classify_download_result(returncode, output):
    text = output or ""
    for marker in FAILURE_MARKERS:
        if marker in text:
            return DownloadResult(False, marker)
    if returncode != 0:
        return DownloadResult(False, f"bk_loader return code {returncode}")
    if "Download complete" in text and "all pass" in text:
        return DownloadResult(True, "Download complete, all pass")
    return DownloadResult(False, "missing Download complete/all pass success marker")


def build_download_command(loader, port, image, baudrate, link_type, reset_type,
                           reset_baudrate, startaddr, pre_erase, reboot):
    cmd = [
        loader,
        "download",
        "-p", port,
        "-b", str(baudrate),
        "--link_type", str(link_type),
        "--reset_type", str(reset_type),
        "--reset_baudrate", str(reset_baudrate),
        "-i", image,
        "-s", startaddr,
        "-e", str(pre_erase),
    ]
    if reboot:
        cmd.append("-r")
    return cmd


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Run bk_loader download and fail unless its output proves a successful flash."
    )
    parser.add_argument("--loader", default="/home/jason/armino1/bk_loader")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0")
    parser.add_argument("-i", "--image", required=True)
    parser.add_argument("-b", "--baudrate", type=int, default=2000000)
    parser.add_argument("--link-type", type=int, default=4)
    parser.add_argument("--reset-type", type=int, default=3)
    parser.add_argument("--reset-baudrate", type=int, default=115200)
    parser.add_argument("-s", "--startaddr", default="0x0")
    parser.add_argument("-e", "--pre-erase", type=int, choices=(0, 1), default=0)
    parser.add_argument("--no-reboot", action="store_true")
    return parser.parse_args(argv)


def run_download(argv=None):
    args = parse_args(argv)
    cmd = build_download_command(
        loader=args.loader,
        port=args.port,
        image=args.image,
        baudrate=args.baudrate,
        link_type=args.link_type,
        reset_type=args.reset_type,
        reset_baudrate=args.reset_baudrate,
        startaddr=args.startaddr,
        pre_erase=args.pre_erase,
        reboot=not args.no_reboot,
    )
    print(" ".join(cmd), flush=True)
    completed = subprocess.run(cmd, text=True, capture_output=True)
    output = (completed.stdout or "") + (completed.stderr or "")
    if completed.stdout:
        print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, end="", file=sys.stderr)

    result = classify_download_result(completed.returncode, output)
    if result.ok:
        print(f"bk_loader checked success: {result.reason}")
        return 0

    print(f"bk_loader checked failure: {result.reason}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(run_download())
