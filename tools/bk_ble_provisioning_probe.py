#!/usr/bin/env python3
"""Probe BK BLE provisioning from a Linux host.

The tool intentionally mirrors the mini program's frame format:
EA02 write frame = opcode_le16 + payload_len_le16 + payload.
EA01 notify frame = opcode_le16 + status_u8 + payload_len_le16 + payload.
"""

from __future__ import annotations

import argparse
import binascii
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Iterable

import pexpect


SERVICE_UUID = "0000fa00-0000-1000-8000-00805f9b34fb"
NOTIFY_UUID = "0000ea01-0000-1000-8000-00805f9b34fb"
WRITE_UUID = "0000ea02-0000-1000-8000-00805f9b34fb"
OP_WIFI_SCAN = 24
OP_AUTH_SIGN = 151


@dataclass
class Device:
    address: str
    name: str
    details: str = ""


@dataclass
class Characteristic:
    uuid: str
    handle: str
    value_handle: str


def main() -> int:
    parser = argparse.ArgumentParser(description="BK7258 BLE provisioning probe")
    sub = parser.add_subparsers(dest="command", required=True)

    scan = sub.add_parser("scan", help="scan for BK provisioning advertisements")
    scan.add_argument("--timeout", type=int, default=15)
    scan.add_argument("--all", action="store_true", help="show all discovered devices")
    scan.add_argument("--details", action="store_true", help="include bluetoothctl details for each printed device")

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("address", help="BLE MAC address")
    common.add_argument("--addr-type", choices=["public", "random"], default="public")
    common.add_argument("--timeout", type=int, default=20)

    auth = sub.add_parser("auth-sign", parents=[common], help="send auth.sign opcode 151")
    auth.add_argument("message", help="signing message, hex:... for raw bytes")

    wifi = sub.add_parser("wifi-scan", parents=[common], help="send Wi-Fi scan opcode 24")
    wifi.add_argument("--payload-hex", default="01", help="payload bytes, default 01")

    args = parser.parse_args()
    if args.command == "scan":
        devices = scan_devices(args.timeout)
        for device in devices:
            if args.all or is_candidate(device):
                print(format_scan_device(device, args.details))
        return 0

    session = GattSession(args.address, args.addr_type, args.timeout)
    try:
        session.connect()
        chars = session.characteristics()
        write = find_char(chars, WRITE_UUID)
        notify = find_char(chars, NOTIFY_UUID)
        session.enable_notify(notify)
        if args.command == "auth-sign":
            payload = build_auth_sign_payload(args.message)
            frame = build_write_frame(OP_AUTH_SIGN, payload)
            print(f"write opcode=151 handle={write.value_handle} bytes={frame.hex()}")
            session.write(write.value_handle, frame)
            frames = session.collect_notifications(timeout=args.timeout, target_opcode=OP_AUTH_SIGN)
        else:
            payload = bytes.fromhex(args.payload_hex)
            frame = build_write_frame(OP_WIFI_SCAN, payload)
            print(f"write opcode=24 handle={write.value_handle} bytes={frame.hex()}")
            session.write(write.value_handle, frame)
            frames = session.collect_notifications(timeout=args.timeout, target_opcode=OP_WIFI_SCAN)
        for frame in frames:
            print(format_notify_frame(frame))
        return 0 if frames else 2
    finally:
        session.close()


def scan_devices(timeout: int) -> list[Device]:
    cmd = ["bluetoothctl", "--timeout", str(timeout), "scan", "on"]
    result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    return enrich_devices_with_info(parse_scan_output(result.stdout))


def parse_scan_output(output: str) -> list[Device]:
    devices: dict[str, Device] = {}
    for line in strip_ansi(output).splitlines():
        match = re.search(r"(?:NEW|CHG).*Device\s+([0-9A-F:]{17})\s+(.+)$", line, re.I)
        if not match:
            continue
        address = match.group(1).upper()
        text = match.group(2).strip()
        existing = devices.get(address)
        name = existing.name if existing else text
        if not existing and not looks_like_device_detail(text):
            name = text
        elif not existing:
            name = address
        details = " ".join(part for part in [existing.details if existing else "", text] if part)
        devices[address] = Device(address=address, name=name, details=details)
    return sorted(devices.values(), key=lambda item: item.address)


def is_candidate(device: Device) -> bool:
    text = f"{device.name} {device.details}".upper()
    return device.name.upper().startswith("BK_") or "FE01" in text or "FA00" in text


def looks_like_device_detail(text: str) -> bool:
    return bool(re.match(r"^[A-Za-z ]+:", text))


def format_scan_device(device: Device, details: bool = False) -> str:
    line = f"{device.address} {device.name}"
    if details and device.details:
        collapsed = re.sub(r"\s+", " ", strip_ansi(device.details)).strip()
        line = f"{line} | {collapsed}"
    return line


def enrich_devices_with_info(devices: list[Device]) -> list[Device]:
    enriched: list[Device] = []
    for device in devices:
        info = bluetoothctl_info(device.address)
        details = " ".join(part for part in [device.details, info] if part)
        enriched.append(Device(address=device.address, name=device.name, details=details))
    return enriched


def bluetoothctl_info(address: str) -> str:
    result = subprocess.run(
        ["bluetoothctl", "info", address],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        return ""
    return strip_ansi(result.stdout)


class GattSession:
    def __init__(self, address: str, addr_type: str, timeout: int) -> None:
        self.timeout = timeout
        self.child = pexpect.spawn(
            "gatttool",
            ["-b", address, "-t", addr_type, "-I"],
            encoding="utf-8",
            timeout=timeout,
        )

    def connect(self) -> None:
        self.child.expect(r"\[.*\]\[LE\]>")
        self.child.sendline("connect")
        self.child.expect("Connection successful")

    def characteristics(self) -> list[Characteristic]:
        self.child.sendline("characteristics")
        self.child.expect(r"\[.*\]\[LE\]>")
        output = self.child.before
        chars: list[Characteristic] = []
        for line in output.splitlines():
            match = re.search(
                r"handle:\s*(0x[0-9a-f]+),\s*char properties:.*char value handle:\s*(0x[0-9a-f]+),\s*uuid:\s*([0-9a-f-]+)",
                line,
                re.I,
            )
            if match:
                chars.append(
                    Characteristic(
                        handle=match.group(1),
                        value_handle=match.group(2),
                        uuid=match.group(3).lower(),
                    )
                )
        return chars

    def enable_notify(self, notify: Characteristic) -> None:
        descriptor = int(notify.value_handle, 16) + 1
        self.write(f"0x{descriptor:04x}", bytes([0x01, 0x00]))

    def write(self, handle: str, payload: bytes) -> None:
        self.child.sendline(f"char-write-req {handle} {payload.hex()}")
        self.child.expect(r"(Characteristic value was written successfully|Notification handle|Indication handle|\[.*\]\[LE\]>)")

    def collect_notifications(self, timeout: int, target_opcode: int) -> list[bytes]:
        frames: list[bytes] = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.child.timeout = max(1, int(deadline - time.time()))
            try:
                index = self.child.expect([r"Notification handle = .*? value:\s*([0-9a-fA-F ]+)", r"\[.*\]\[LE\]>"])
            except pexpect.TIMEOUT:
                break
            if index != 0:
                continue
            data = bytes.fromhex(self.child.match.group(1).replace(" ", ""))
            frames.append(data)
            parsed = parse_notify_frame(data)
            if parsed and parsed[0] == target_opcode and parsed[1] == 0:
                break
        return frames

    def close(self) -> None:
        if not self.child.isalive():
            return
        self.child.sendline("disconnect")
        self.child.close(force=True)


def find_char(chars: Iterable[Characteristic], uuid: str) -> Characteristic:
    target = normalize_uuid(uuid)
    for item in chars:
        if normalize_uuid(item.uuid) == target:
            return item
    available = ", ".join(item.uuid for item in chars)
    raise RuntimeError(f"characteristic not found: {uuid}; available: {available}")


def normalize_uuid(value: str) -> str:
    compact = value.replace("-", "").lower()
    if re.fullmatch(r"[0-9a-f]{4}", compact):
        return f"0000{compact}00001000800000805f9b34fb"
    return compact


def parse_payload_arg(value: str) -> bytes:
    if value.startswith("hex:"):
        return bytes.fromhex(value[4:])
    return value.encode("utf-8")


def build_auth_sign_payload(message: str) -> bytes:
    return parse_payload_arg(message)


def build_write_frame(opcode: int, payload: bytes) -> bytes:
    return opcode.to_bytes(2, "little") + len(payload).to_bytes(2, "little") + payload


def parse_notify_frame(data: bytes) -> tuple[int, int, bytes] | None:
    if len(data) < 5:
        return None
    opcode = int.from_bytes(data[0:2], "little")
    status = data[2]
    length = int.from_bytes(data[3:5], "little")
    if len(data) < 5 + length:
        return None
    return opcode, status, data[5 : 5 + length]


def format_notify_frame(data: bytes) -> str:
    parsed = parse_notify_frame(data)
    if not parsed:
        return f"notify raw={data.hex()}"
    opcode, status, payload = parsed
    text = payload.decode("utf-8", errors="replace")
    return f"notify opcode={opcode} status={status} len={len(payload)} payload_hex={payload.hex()} payload_text={text}"


def strip_ansi(value: str) -> str:
    return re.sub(r"\x1b\[[0-9;]*m|\x01|\x02", "", value)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, binascii.Error, pexpect.ExceptionPexpect) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
