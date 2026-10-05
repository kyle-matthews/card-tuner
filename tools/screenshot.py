"""Save a screenshot of the Cardputer's screen as a PNG.

Usage (PlatformIO's Python already has pyserial):
    ~/.platformio/penv/Scripts/python.exe tools/screenshot.py COM5 [out.png] [--scale N]

Sends `s` to the device, which streams its screen buffer over serial.
Defaults to screenshots/latest.png at 3x scale. Close the serial monitor first.
"""

import base64
import pathlib
import struct
import sys
import time
import zlib

import serial

ROOT = pathlib.Path(__file__).resolve().parent.parent


def write_png(path: pathlib.Path, width: int, height: int, rgb: bytes) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\x00" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(rows, 9))
           + chunk(b"IEND", b""))
    path.write_bytes(png)


def main() -> None:
    args = sys.argv[1:]
    scale = 3
    if "--scale" in args:
        i = args.index("--scale")
        scale = int(args[i + 1])
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    port = args[0]
    out = pathlib.Path(args[1]) if len(args) > 1 else ROOT / "screenshots" / "latest.png"

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 10
    ser.dtr = False  # don't reset the ESP32-S3 on open
    ser.rts = False
    ser.open()
    time.sleep(0.2)
    ser.reset_input_buffer()
    ser.write(b"s")

    raw = bytearray()
    while b"#SHOT_END" not in raw or b"\n" not in raw[raw.index(b"#SHOT_END"):]:
        chunk = ser.read(65536)
        if not chunk:
            sys.exit("Timed out. Is M4 or later firmware running?")
        raw += chunk
    ser.close()

    raw = bytes(raw[raw.index(b"#SHOT_BEGIN"):])
    header, _, rest = raw.partition(b"\n")
    fields = dict(kv.split(b"=") for kv in header.split()[1:])
    width, height = int(fields[b"w"]), int(fields[b"h"])
    body, _, tail = rest.partition(b"#SHOT_END")
    pixels = base64.b64decode(b"".join(body.split()))
    expected_sum = int(tail.split(b"sum=")[1].split()[0])
    if len(pixels) != width * height * 2 or sum(pixels) & 0xFFFFFFFF != expected_sum:
        sys.exit("Transfer error, try again.")

    # Big-endian RGB565 -> RGB888, nearest-neighbour upscaled.
    rgb = bytearray()
    for y in range(height):
        row = bytearray()
        for x in range(width):
            v = (pixels[(y * width + x) * 2] << 8) | pixels[(y * width + x) * 2 + 1]
            r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
            row += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))) * scale
        rgb += bytes(row) * scale

    out.parent.mkdir(parents=True, exist_ok=True)
    write_png(out, width * scale, height * scale, bytes(rgb))
    print(f"Saved {out}")


if __name__ == "__main__":
    main()
