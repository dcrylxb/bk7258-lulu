#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


IMAGE_SIZE = 1048576
BLOCK_SIZE = 4096
PAGE_SIZE = 256

REQUIRED_PET_PROMPTS = [
    "boot",
    "net_ok",
    "net_lost",
    "low_power",
    "error",
    "listen_start",
    "cancel",
    "photo",
    "done",
    "happy_chirp",
    "curious",
    "afraid",
    "comforted",
    "impact",
    "privacy_on",
    "privacy_off",
    "stop",
    "sleep",
    "thinking",
    "processing",
    "success",
    "fail",
    "wake_confirm",
    "test_wake_prompt",
]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def project_root() -> Path:
    return repo_root() / "projects" / "cmaiw82al_ai_toy"


def rel(path: Path) -> str:
    return path.relative_to(repo_root()).as_posix()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_source(source: Path) -> None:
    missing: list[str] = []
    empty: list[str] = []

    for prompt_id in REQUIRED_PET_PROMPTS:
        path = source / "prompts" / "pet" / f"{prompt_id}.mp3"
        if not path.exists():
            missing.append(path.relative_to(source).as_posix())
        elif path.stat().st_size <= 0:
            empty.append(path.relative_to(source).as_posix())

    if missing or empty:
        for item in missing:
            print(f"IF0_PACK missing={item}")
        for item in empty:
            print(f"IF0_PACK empty={item}")
        raise SystemExit(2)


def stage_source(source: Path) -> tempfile.TemporaryDirectory[str]:
    temp = tempfile.TemporaryDirectory(prefix="if0_pack_")
    staged = Path(temp.name) / "bk"
    shutil.copytree(source, staged)

    prompt_root = staged / "prompts"
    pet_root = prompt_root / "pet"
    for prompt_id in REQUIRED_PET_PROMPTS:
        src = pet_root / f"{prompt_id}.mp3"
        dst = prompt_root / f"{prompt_id}.mp3"
        shutil.copy2(src, dst)

    return temp


def run_pack(mklittlefs: Path, source: Path, output: Path) -> str:
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(mklittlefs),
        "-c",
        str(source),
        "-b",
        str(BLOCK_SIZE),
        "-p",
        str(PAGE_SIZE),
        "-s",
        str(IMAGE_SIZE),
        str(output),
    ]
    result = subprocess.run(
        cmd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=60,
        check=False,
    )
    if result.returncode != 0:
        print(result.stdout, end="")
        raise SystemExit(result.returncode)
    return result.stdout


def print_manifest(source: Path, pack_stdout: str, output: Path) -> None:
    print(f"IF0_PACK source={rel(source)}")
    print(f"IF0_PACK output={output}")
    print(f"IF0_PACK image_size={output.stat().st_size}")
    print(f"IF0_PACK sha256={sha256(output)}")

    for line in pack_stdout.splitlines():
        item = line.strip()
        if item:
            print(f"IF0_PACK file={item}")


def parse_args() -> argparse.Namespace:
    root = repo_root()
    default_source = project_root() / "ap" / "main" / "fs" / "bk"
    default_mklittlefs = project_root() / "ap" / "main" / "fs" / "mklittlefs"
    default_output = project_root() / "ap" / "main" / "fs" / "bk_0x6f9000.bin"

    parser = argparse.ArgumentParser(description="Pack CMAiW82AL internal /if0 littlefs resources.")
    parser.add_argument("--source", type=Path, default=default_source)
    parser.add_argument("--mklittlefs", type=Path, default=default_mklittlefs)
    parser.add_argument("--output", type=Path, default=default_output)
    parser.add_argument("--check", action="store_true", help="Validate prompt manifest and generated image.")
    parser.add_argument("--quiet", action="store_true", help="Suppress manifest output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    mklittlefs = args.mklittlefs.resolve()
    output = args.output.resolve()

    if not source.is_dir():
        print(f"IF0_PACK source_not_found={source}", file=sys.stderr)
        return 2
    if not mklittlefs.exists():
        print(f"IF0_PACK mklittlefs_not_found={mklittlefs}", file=sys.stderr)
        return 2

    validate_source(source)

    with stage_source(source) as staged_dir:
        staged_source = Path(staged_dir) / "bk"
        pack_stdout = run_pack(mklittlefs, staged_source, output)

    if output.stat().st_size != IMAGE_SIZE:
        print(f"IF0_PACK bad_image_size={output.stat().st_size}", file=sys.stderr)
        return 2

    if args.check:
        for prompt_id in REQUIRED_PET_PROMPTS:
            for marker in (
                f"/prompts/pet/{prompt_id}.mp3",
                f"/prompts/{prompt_id}.mp3",
            ):
                if marker not in pack_stdout:
                    print(f"IF0_PACK pack_log_missing={marker}", file=sys.stderr)
                    return 2

    if not args.quiet:
        print_manifest(source, pack_stdout, output)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
