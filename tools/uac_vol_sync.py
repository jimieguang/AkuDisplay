#!/usr/bin/env python3
"""uac_vol_sync.py - Windows Volume → AkuBot UAC Volume Sync (PC side)

Monitors the Windows master volume (the slider that controls the "AKU USB Speaker"
UAC device) and sends changes to the AkuBot device via UDP.

Requirements:
  pip install pycaw comtypes

Usage:
  python uac_vol_sync.py <device_ip>

The script polls Windows master volume every 200ms. When it detects a change,
it sends "VOL <0-100>" to the device's UDP port 50580. The device daemon
(uac_vol_daemon.py) maps this to its 0-63 range and applies via amixer.

Run this script on the PC while UAC audio is active. Minimize to tray or
run in a background terminal.
"""

import sys
import socket
import time

if len(sys.argv) < 2:
    print("Usage: python uac_vol_sync.py <device_ip>")
    sys.exit(1)
DEVICE_IP = sys.argv[1]
DEVICE_PORT = 50580
POLL_INTERVAL = 0.2  # seconds


def main():
    try:
        from ctypes import cast, POINTER
        from comtypes import CLSCTX_ALL
        from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
    except ImportError:
        print("ERROR: Missing dependencies. Install with:")
        print("  pip install pycaw comtypes")
        sys.exit(1)

    # Get the default audio endpoint volume control
    devices = AudioUtilities.GetSpeakers()
    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
    volume_ctrl = cast(interface, POINTER(IAudioEndpointVolume))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    last_vol = -1
    last_mute = -1

    print(f"AkuBot UAC Volume Sync → {DEVICE_IP}:{DEVICE_PORT}")
    print("Monitoring Windows master volume. Ctrl+C to stop.")
    print("-" * 45)

    try:
        while True:
            # Read Windows master volume (0.0 ~ 1.0)
            scalar = volume_ctrl.GetMasterVolumeLevelScalar()
            vol_pct = int(round(scalar * 100))

            # Read mute state
            mute = volume_ctrl.GetMute()  # True/False

            changed = False

            if mute != last_mute:
                last_mute = mute
                msg = f"MUTE {1 if mute else 0}"
                sock.sendto(msg.encode(), (DEVICE_IP, DEVICE_PORT))
                print(f"  → {msg}")
                changed = True

            if vol_pct != last_vol and not mute:
                last_vol = vol_pct
                msg = f"VOL {vol_pct}"
                sock.sendto(msg.encode(), (DEVICE_IP, DEVICE_PORT))
                print(f"  → {msg}  (Windows {vol_pct}%)")
                changed = True

            if not changed:
                pass  # silent poll

            time.sleep(POLL_INTERVAL)

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
