#!/bin/sh
# Boot-time WiFi hardening
# Disable power save at interface and connection level

WIFI_IFACE="wlan0"

# Wait for interface to appear
for i in $(seq 1 30); do
    if ip link show "$WIFI_IFACE" >/dev/null 2>&1; then
        break
    fi
    sleep 1
done

# Disable power save at interface level (iw may be absent on some devices)
command -v iw >/dev/null 2>&1 && iw dev "$WIFI_IFACE" set power_save off 2>/dev/null

# Disable power save at connection level (for all known WiFi connections)
nmcli -t -f NAME,TYPE con show 2>/dev/null | grep ':802-11-wireless' | while IFS=: read -r name type; do
    nmcli con modify "$name" 802-11-wireless.powersave 2 2>/dev/null
done

logger "WiFi hardening complete (power save disabled)"
