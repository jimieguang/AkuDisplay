#!/bin/sh
# WiFi watchdog - network connectivity monitor with auto-recovery
# Check interval: 180s (3 minutes)
# After 3 consecutive failures: switch to hotspot mode (SSID/password: see the
# "hotspot" NetworkManager profile pre-configured on the device)

WIFI_IFACE="wlan0"
CON_NAME="<WIFI_CONNECTION_NAME>"   # your NetworkManager WiFi connection name (nmcli con show)
HOTSPOT_NAME="hotspot"
CHECK_INTERVAL=180
MAX_FAILURES=3
LOG_FILE="/var/log/wifi_watchdog.log"

log() {
    local ts="$(date '+%Y-%m-%d %H:%M:%S')"
    echo "[$ts] $1" >> "$LOG_FILE"
    echo "[$ts] $1"
}

# iw is absent on some devices (e.g. brcmfmac unit); guard every call
ps_off() {
    command -v iw >/dev/null 2>&1 && iw dev "$WIFI_IFACE" set power_save off 2>/dev/null
}

check_connectivity() {
    if ! ip addr show "$WIFI_IFACE" | grep -q 'inet '; then
        return 1
    fi
    if ! wpa_cli status 2>/dev/null | grep -q 'wpa_state=COMPLETED'; then
        return 1
    fi
    if curl -s -o /dev/null --connect-timeout 3 http://captive.apple.com; then
        return 0
    fi
    if curl -s -o /dev/null --connect-timeout 3 https://www.baidu.com; then
        return 0
    fi
    return 1
}

reset_interface() {
    log "Full interface reset..."
    ip link set "$WIFI_IFACE" down
    sleep 1
    ip link set "$WIFI_IFACE" up
    sleep 3
    ps_off
    wpa_cli reconfigure 2>/dev/null
    sleep 2
}

restart_nm() {
    log "Restarting NetworkManager..."
    nmcli networking off 2>/dev/null
    sleep 2
    nmcli networking on 2>/dev/null
    sleep 5
    ps_off
}

reconnect() {
    log "Attempting soft reconnect..."
    nmcli con down "$CON_NAME" 2>/dev/null
    sleep 2
    nmcli con up "$CON_NAME" 2>/dev/null
    sleep 5

    if check_connectivity; then
        log "Soft reconnect OK"
        return 0
    fi

    log "Soft reconnect failed, trying hard reset..."
    reset_interface
    nmcli con up "$CON_NAME" 2>/dev/null
    sleep 5

    if check_connectivity; then
        log "Hard reset OK"
        return 0
    fi

    log "Hard reset failed, restarting NetworkManager..."
    restart_nm
    nmcli con up "$CON_NAME" 2>/dev/null
    sleep 5

    if check_connectivity; then
        log "NM restart OK"
        return 0
    fi

    return 1
}

enable_hotspot() {
    if nmcli -t -f NAME,DEVICE con show --active 2>/dev/null | grep -q "$HOTSPOT_NAME"; then
        return 0
    fi
    log "Switching to hotspot mode (192.168.0.1)"
    nmcli con down "$CON_NAME" 2>/dev/null
    nmcli con up "$HOTSPOT_NAME" 2>/dev/null
    sleep 3
    if nmcli -t -f NAME,DEVICE con show --active 2>/dev/null | grep -q "$HOTSPOT_NAME"; then
        log "Hotspot is active"
        ps_off
        return 0
    fi
    log "WARNING: Hotspot failed to start"
    return 1
}

disable_hotspot() {
    nmcli con down "$HOTSPOT_NAME" 2>/dev/null
    log "Exiting hotspot mode"
    sleep 1
}

touch "$LOG_FILE"
log "WiFi watchdog started (interval: ${CHECK_INTERVAL}s, max failures: $MAX_FAILURES)"
ps_off

FAIL_COUNT=0
HOTSPOT_MODE=false

while true; do
    if check_connectivity; then
        if [ "$FAIL_COUNT" -gt 0 ]; then
            log "Connectivity restored"
        fi
        FAIL_COUNT=0
        if $HOTSPOT_MODE; then
            disable_hotspot
            HOTSPOT_MODE=false
        fi
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        log "Connection check failed ($FAIL_COUNT/$MAX_FAILURES)"

        if [ "$FAIL_COUNT" -ge "$MAX_FAILURES" ] && ! $HOTSPOT_MODE; then
            enable_hotspot
            HOTSPOT_MODE=true
        fi

        reconnect
    fi
    sleep "$CHECK_INTERVAL"
done
