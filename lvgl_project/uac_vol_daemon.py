#!/usr/bin/env python3
"""uac_vol_daemon.py - UAC Volume Sync Daemon (device side)

Listens on UDP port 50580 for volume commands from the PC-side sync script
and applies them to the system mixer ('Power Amplifier', range 0-63).

Protocol (single UDP datagram, ASCII):
  VOL <0-63>      Set volume
  MUTE <0|1>      Mute/unmute (stores last volume, sets 0 or restores)
  GET             → replies "VOL <current>\n" to sender

Started by audio_start.sh when UAC activates; killed by audio_stop.sh.
"""

import socket
import subprocess
import sys
import os
import signal

UDP_PORT = 50580
MIXER = "Power Amplifier"
VOL_MIN = 0
VOL_MAX = 63

last_vol = 40  # default restore level after mute


def get_volume():
    """Read current system volume from amixer."""
    try:
        out = subprocess.check_output(
            f"amixer get '{MIXER}' 2>/dev/null"
            " | sed -n 's/.*: \\([0-9]\\+\\) \\[.*/\\1/p' | head -1",
            shell=True, text=True
        ).strip()
        return int(out) if out else VOL_MAX // 2
    except Exception:
        return VOL_MAX // 2


def set_volume(v):
    """Set system volume via amixer (clamped to 0-63)."""
    v = max(VOL_MIN, min(VOL_MAX, v))
    subprocess.run(
        f"amixer set '{MIXER}' {v} >/dev/null 2>&1",
        shell=True
    )
    return v


def main():
    global last_vol

    # Graceful exit
    def on_term(sig, frame):
        sys.exit(0)
    signal.signal(signal.SIGTERM, on_term)
    signal.signal(signal.SIGINT, on_term)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", UDP_PORT))
    sock.settimeout(1.0)  # allow periodic check for signals

    # Sync initial volume
    last_vol = get_volume()

    while True:
        try:
            data, addr = sock.recvfrom(256)
        except socket.timeout:
            continue
        except OSError:
            break

        msg = data.decode("utf-8", errors="ignore").strip()
        if not msg:
            continue

        parts = msg.split()
        cmd = parts[0].upper()

        if cmd == "VOL" and len(parts) >= 2:
            try:
                # Accept both 0-63 (device scale) and 0-100 (percent)
                val = int(parts[1])
                if val > VOL_MAX:
                    # Assume percentage, map to 0-63
                    val = int(val * VOL_MAX / 100)
                val = set_volume(val)
                last_vol = val if val > 0 else last_vol
            except ValueError:
                pass

        elif cmd == "MUTE" and len(parts) >= 2:
            if parts[1] == "1":
                set_volume(0)
            else:
                set_volume(last_vol)

        elif cmd == "GET":
            cur = get_volume()
            sock.sendto(f"VOL {cur}\n".encode(), addr)

    sock.close()


if __name__ == "__main__":
    main()
