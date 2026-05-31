#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import io
import json
import shutil
import struct
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


WIDTH = 320
HEIGHT = 160
PANEL = 160
FPS = 10
FRAME_COUNT = 60
JPEG_QUALITY = 84
LFS_BLOCK_SIZE = 4096
LFS_PAGE_SIZE = 256
LFS_IMAGE_SIZE = 0xF00000


def le32(value: int) -> bytes:
    return struct.pack("<I", value)


def riff_chunk(fourcc: bytes, payload: bytes) -> bytes:
    pad = b"\0" if len(payload) & 1 else b""
    return fourcc + le32(len(payload)) + payload + pad


def riff_list(list_type: bytes, payload: bytes) -> bytes:
    body = list_type + payload
    pad = b"\0" if len(body) & 1 else b""
    return b"LIST" + le32(len(body)) + body + pad


def load_font(size: int) -> ImageFont.ImageFont:
    candidates = [
        Path("/home/jason/armino1/bk_avdk_smp/ap/components/lvgl/lvgl_v8/src/extra/libs/freetype/arial.ttf"),
        Path("/home/jason/armino1/bk_avdk_smp/ap/components/lvgl/lvgl_v9/src/libs/freetype/arial.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size=size)
    return ImageFont.load_default()


FONT_BIG = load_font(32)
FONT_MED = load_font(16)
FONT_SMALL = load_font(11)


def draw_centered(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], text: str,
                  font: ImageFont.ImageFont, fill: tuple[int, int, int]) -> None:
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    x = box[0] + ((box[2] - box[0] - text_w) // 2)
    y = box[1] + ((box[3] - box[1] - text_h) // 2) - 2
    draw.text((x, y), text, font=font, fill=fill)


def draw_panel(draw: ImageDraw.ImageDraw, x0: int, label: str, frame: int,
               accent: tuple[int, int, int], sweep_offset: int) -> None:
    x1 = x0 + PANEL - 1
    draw.rectangle((x0, 0, x1, HEIGHT - 1), fill=(10, 12, 14))

    for p in range(0, PANEL + 1, 20):
        color = (42, 48, 54) if p % 40 else (68, 76, 84)
        draw.line((x0 + p, 0, x0 + p, HEIGHT - 1), fill=color)
        draw.line((x0, p, x1, p), fill=color)

    draw.rectangle((x0, 0, x1, HEIGHT - 1), outline=accent, width=2)
    draw.line((x0 + 79, 0, x0 + 79, HEIGHT - 1), fill=(160, 160, 160), width=1)
    draw.line((x0, 79, x1, 79), fill=(160, 160, 160), width=1)

    swatches = [
        ((255, 0, 0), (x0 + 5, 5, x0 + 25, 25)),
        ((0, 255, 0), (x0 + 134, 5, x0 + 154, 25)),
        ((0, 0, 255), (x0 + 5, 134, x0 + 25, 154)),
        ((255, 255, 255), (x0 + 134, 134, x0 + 154, 154)),
    ]
    for color, rect in swatches:
        draw.rectangle(rect, fill=color, outline=(0, 0, 0), width=1)

    sx = x0 + sweep_offset
    sy = (frame * 3) % PANEL
    draw.rectangle((sx, 0, min(sx + 4, x1), HEIGHT - 1), fill=(255, 255, 0))
    draw.rectangle((x0, sy, x1, min(sy + 3, HEIGHT - 1)), fill=(0, 220, 255))

    cx = x0 + 80 + int(38 * ((frame % 30) - 15) / 15)
    cy = 80 + int(20 * (((frame * 2) % 30) - 15) / 15)
    draw.ellipse((cx - 28, cy - 18, cx + 28, cy + 18), fill=(22, 28, 36), outline=(230, 230, 230), width=2)
    draw.ellipse((cx - 11, cy - 11, cx + 11, cy + 11), fill=accent)
    draw.ellipse((cx - 4, cy - 4, cx + 4, cy + 4), fill=(0, 0, 0))

    draw_centered(draw, (x0, 38, x1, 72), label, FONT_BIG, (255, 255, 255))
    draw_centered(draw, (x0, 106, x1, 126), "160x160", FONT_MED, (230, 230, 230))
    draw.text((x0 + 34, 140), f"F{frame:02d}", font=FONT_SMALL, fill=(235, 235, 235))


def make_frame(frame: int) -> Image.Image:
    image = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    sweep = (frame * 5) % PANEL

    draw_panel(draw, 0, "LCD2", frame, (0, 122, 255), sweep)
    draw_panel(draw, PANEL, "LCD1", frame, (255, 196, 0), (PANEL - sweep - 4) % PANEL)

    draw.rectangle((158, 0, 161, HEIGHT - 1), fill=(255, 255, 255))
    draw.text((92, 2), "AVI 320x160", font=FONT_SMALL, fill=(255, 255, 255))
    draw.text((204, 2), "GC9D01 TEST", font=FONT_SMALL, fill=(255, 255, 255))
    return image


def encode_jpeg(image: Image.Image) -> bytes:
    buf = io.BytesIO()
    image.save(
        buf,
        format="JPEG",
        quality=JPEG_QUALITY,
        subsampling=2,
        optimize=False,
        progressive=False,
    )
    data = buf.getvalue()
    if not data.startswith(b"\xff\xd8") or not data.endswith(b"\xff\xd9"):
        raise ValueError("PIL did not emit a baseline JPEG frame")
    return data


def write_mjpeg_avi(path: Path, frames: list[bytes]) -> dict[str, int]:
    max_frame = max(len(frame) for frame in frames)
    us_per_frame = int(round(1_000_000 / FPS))

    avih = struct.pack(
        "<IIIIIIIIII4I",
        us_per_frame,
        max_frame * FPS,
        0,
        0x10,
        len(frames),
        0,
        1,
        max_frame,
        WIDTH,
        HEIGHT,
        0,
        0,
        0,
        0,
    )
    strh = struct.pack(
        "<4s4sIHHIIIIIIIIhhhh",
        b"vids",
        b"MJPG",
        0,
        0,
        0,
        0,
        1,
        FPS,
        0,
        len(frames),
        max_frame,
        JPEG_QUALITY * 100,
        0,
        0,
        0,
        WIDTH,
        HEIGHT,
    )
    strf = struct.pack(
        "<IiiHH4sIiiII",
        40,
        WIDTH,
        HEIGHT,
        1,
        24,
        b"MJPG",
        WIDTH * HEIGHT * 3,
        0,
        0,
        0,
        0,
    )

    hdrl = riff_list(
        b"hdrl",
        riff_chunk(b"avih", avih)
        + riff_list(b"strl", riff_chunk(b"strh", strh) + riff_chunk(b"strf", strf)),
    )

    movi_payload = bytearray()
    idx_entries = bytearray()
    cumulative = 0
    for frame in frames:
        idx_entries += b"00dc" + le32(0x10) + le32(cumulative + 4) + le32(len(frame))
        chunk = riff_chunk(b"00dc", frame)
        movi_payload += chunk
        cumulative += len(chunk)

    movi = riff_list(b"movi", bytes(movi_payload))
    idx1 = riff_chunk(b"idx1", bytes(idx_entries))
    riff_body = b"AVI " + hdrl + movi + idx1
    path.write_bytes(b"RIFF" + le32(len(riff_body)) + riff_body)

    return {
        "max_jpeg_frame_bytes": max_frame,
        "movi_payload_bytes": len(movi_payload),
        "idx_entries": len(frames),
    }


def verify_avi(path: Path, frame_count: int) -> dict[str, int | str]:
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"AVI ":
        raise ValueError("not a RIFF AVI file")
    riff_size = struct.unpack_from("<I", data, 4)[0]
    if riff_size + 8 != len(data):
        raise ValueError(f"RIFF size mismatch: header={riff_size + 8}, actual={len(data)}")

    strf_pos = data.find(b"strf")
    if strf_pos < 0:
        raise ValueError("missing strf chunk")
    width = struct.unpack_from("<i", data, strf_pos + 8 + 4)[0]
    height = struct.unpack_from("<i", data, strf_pos + 8 + 8)[0]
    if (width, height) != (WIDTH, HEIGHT):
        raise ValueError(f"unexpected AVI size {width}x{height}")

    movi_tag = data.find(b"movi")
    idx_pos = data.rfind(b"idx1")
    if movi_tag < 0 or idx_pos < 0:
        raise ValueError("missing movi or idx1")
    idx_size = struct.unpack_from("<I", data, idx_pos + 4)[0]
    if idx_size % 16 != 0:
        raise ValueError("idx1 is not a multiple of 16 bytes")
    entries = idx_size // 16
    if entries != frame_count:
        raise ValueError(f"idx1 frame count mismatch: {entries} != {frame_count}")

    beken_idx_base = movi_tag + 8
    min_frame = 1 << 30
    max_frame = 0
    for i in range(entries):
        entry = idx_pos + 8 + i * 16
        tag, flags, offset, size = struct.unpack_from("<4sIII", data, entry)
        if tag != b"00dc" or flags != 0x10:
            raise ValueError(f"unexpected idx1 entry {i}: {tag!r} flags=0x{flags:x}")
        frame_start = beken_idx_base + offset
        frame_end = frame_start + size
        if data[frame_start:frame_start + 2] != b"\xff\xd8":
            raise ValueError(f"frame {i} does not start with JPEG SOI at 0x{frame_start:x}")
        if data[frame_end - 2:frame_end] != b"\xff\xd9":
            raise ValueError(f"frame {i} does not end with JPEG EOI at 0x{frame_end:x}")
        min_frame = min(min_frame, size)
        max_frame = max(max_frame, size)

    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "file_bytes": len(data),
        "width": width,
        "height": height,
        "frames": entries,
        "min_jpeg_frame_bytes": min_frame,
        "max_jpeg_frame_bytes": max_frame,
    }


def build_littlefs_image(sf0_dir: Path, image_path: Path, mklittlefs: Path) -> bool:
    if not mklittlefs.exists():
        return False
    cmd = [
        str(mklittlefs),
        "-c",
        str(sf0_dir),
        "-b",
        str(LFS_BLOCK_SIZE),
        "-p",
        str(LFS_PAGE_SIZE),
        "-s",
        str(LFS_IMAGE_SIZE),
        str(image_path),
    ]
    subprocess.run(cmd, check=True)
    return True


def main() -> None:
    repo = Path(__file__).resolve().parents[1]
    default_output = repo / "resources" / "eyes" / "generated_gc9d01_320x160_test"
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=default_output)
    parser.add_argument("--mklittlefs", type=Path,
                        default=repo / "projects" / "cmaiw82al_ai_toy" / "ap" / "main" / "fs" / "mklittlefs")
    args = parser.parse_args()

    out_dir = args.output_dir
    preview_dir = out_dir / "preview_frames"
    sf0_dir = out_dir / "sf0"
    preview_dir.mkdir(parents=True, exist_ok=True)
    sf0_dir.mkdir(parents=True, exist_ok=True)

    images = [make_frame(i) for i in range(FRAME_COUNT)]
    for i in [0, 15, 30, 45]:
        images[i].save(preview_dir / f"frame_{i:03d}.png")

    frames = [encode_jpeg(image) for image in images]
    avi_path = out_dir / "neutral_gc9d01_320x160_test.avi"
    writer_stats = write_mjpeg_avi(avi_path, frames)
    verify_stats = verify_avi(avi_path, FRAME_COUNT)

    sf0_avi_path = sf0_dir / "neutral.avi"
    shutil.copy2(avi_path, sf0_avi_path)
    sf0_bin_path = out_dir / "sf0_gc9d01_320x160_test_15m.bin"
    lfs_created = build_littlefs_image(sf0_dir, sf0_bin_path, args.mklittlefs)

    manifest = {
        "purpose": "GC9D01 dual 160x160 dynamic LCD resolution test",
        "avi": str(avi_path),
        "sf0_neutral_avi": str(sf0_avi_path),
        "sf0_littlefs_bin": str(sf0_bin_path) if lfs_created else None,
        "runtime_path": "/sf0/neutral.avi",
        "geometry": {
            "avi_width": WIDTH,
            "avi_height": HEIGHT,
            "panel_width": PANEL,
            "panel_height": PANEL,
            "left_half": "LCD2 / first LVGL segment",
            "right_half": "LCD1 / second LVGL segment",
        },
        "video": {
            "codec": "MJPEG",
            "fps": FPS,
            "frames": FRAME_COUNT,
            "jpeg_quality": JPEG_QUALITY,
            "jpeg_subsampling": "4:2:0",
            "idx1_offsets": "Beken avilib data offsets: chunk_data - (movi_start + 4)",
        },
        "littlefs": {
            "created": lfs_created,
            "size_bytes": LFS_IMAGE_SIZE if lfs_created else None,
            "block_size": LFS_BLOCK_SIZE,
            "page_size": LFS_PAGE_SIZE,
        },
        "writer": writer_stats,
        "verification": verify_stats,
    }
    manifest_path = out_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
