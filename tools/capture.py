"""Capture a raw audio dump from the Cardputer and save it as a WAV file.

Usage (PlatformIO's Python already has pyserial):
    ~/.platformio/penv/Scripts/python.exe tools/capture.py COM5 bass_low_e [--wait]

Without --wait it asks the device to record straight away. With --wait it
waits for you to press `r` on the Cardputer, so you can pluck first and then
trigger the recording. Saves recordings/<name>.wav. Close the serial monitor
first, since only one program can use the port at a time.
"""

import pathlib
import sys
import time
import wave

import serial

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "--wait"]
    wait = "--wait" in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    port, name = args

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 120 if wait else 10
    # Keep DTR/RTS low so opening the port doesn't reset the ESP32-S3.
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(0.2)
    ser.reset_input_buffer()
    if wait:
        print("Waiting: pluck the string, then press r on the Cardputer...")
    else:
        ser.write(b"r")

    header = None
    while header is None:
        line = ser.readline().decode(errors="replace").strip()
        if not line:
            sys.exit("Timed out waiting for the device. Is the M1 firmware running?")
        if line.startswith("#DUMP_BEGIN"):
            header = dict(kv.split("=") for kv in line.split()[1:])
            ser.timeout = 10

    rate = int(header["rate"])
    expected = int(header["samples"])
    print(f"Recording {expected / rate:.1f} s at {rate} Hz "
          f"(PGA +{int(header['pga']) * 3} dB, HPF {'on' if header['hpf'] == '1' else 'off'})...")

    samples: list[int] = []
    while True:
        line = ser.readline().decode(errors="replace").strip()
        if not line:
            sys.exit("Timed out mid-transfer.")
        if line == "#DUMP_END":
            break
        samples.extend(int(v) for v in line.split(","))
    ser.close()

    if len(samples) != expected:
        print(f"Warning: got {len(samples)} samples, expected {expected}")

    out_dir = ROOT / "recordings"
    out_dir.mkdir(exist_ok=True)
    out = out_dir / f"{name}.wav"
    with wave.open(str(out), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(s.to_bytes(2, "little", signed=True) for s in samples))

    peak = max(abs(s) for s in samples)
    print(f"Saved {out.relative_to(ROOT)} ({len(samples)} samples, peak {peak})")


if __name__ == "__main__":
    main()
