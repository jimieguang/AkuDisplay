#!/usr/bin/env python3
# Inject synthetic button presses into /dev/input/event0 (Power key) so we
# can drive the LVGL UI headless for screenshots. ARMv7 armhf: input_event
# is 16 bytes (sec:4, usec:4, type:2, code:2, value:4) -> struct 'llHHi'.
import struct, time, sys

DEV = "/dev/input/event0"
EV_SYN, EV_KEY = 0, 1
KEY_POWER = 116

def emit(f, typ, code, val):
    f.write(struct.pack('llHHi', 0, 0, typ, code, val))
    f.flush()

def tap(f, hold=0.05):
    emit(f, EV_KEY, KEY_POWER, 1); emit(f, EV_SYN, 0, 0)
    time.sleep(hold)
    emit(f, EV_KEY, KEY_POWER, 0); emit(f, EV_SYN, 0, 0)

def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    gap = float(sys.argv[2]) if len(sys.argv) > 2 else 0.6
    with open(DEV, "wb", buffering=0) as f:
        for _ in range(n):
            tap(f)
            time.sleep(gap)

if __name__ == "__main__":
    main()
