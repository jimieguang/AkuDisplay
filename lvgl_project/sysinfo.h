#ifndef SYSINFO_H
#define SYSINFO_H
#include <lvgl.h>

/* ======================================================================
 * sysinfo.h / sysinfo.c
 * Thin wrappers around the device's sysfs / shell interfaces.
 *
 * All hardware facts below were verified on the live akubox device:
 *   - Battery PMIC : /sys/class/power_supply/axp20x-battery/{capacity,status,...}
 *   - LED          : /sys/class/leds/aku-logo/{brightness,trigger}  (max=1)
 *   - Audio mixer  : amixer 'Power Amplifier'  (range 0-63)
 *   - WiFi IP      : wlan0
 * ====================================================================== */

/* ---- Battery / charging ---- */
/* capacity: 0..100, or -1 if unreadable */
int  sysinfo_battery_capacity(void);
/* status string ("Charging"/"Discharging"/"Full"/"Not charging"/"?") */
void sysinfo_battery_status(char *out, int sz);
/* convenience: true if currently charging (or full while plugged) */
int  sysinfo_is_charging(void);

/* ---- Audio volume (system 'Power Amplifier', 0..63) ---- */
int  sysinfo_volume_get(void);          /* 0..63, default 50 on error */
void sysinfo_volume_set(int v);         /* clamps to 0..63, applies via amixer */

/* ---- Network ---- */
/* Returns a static buffer with wlan0 IPv4 ("N/A" if none). */
const char *sysinfo_wifi_ip(void);

/* ---- LED control (aku-logo, binary on/off + trigger modes) ---- */
void sysinfo_led_on(void);
void sysinfo_led_off(void);
void sysinfo_led_heartbeat(void);       /* use kernel heartbeat trigger */
void sysinfo_led_charging(void);        /* blink while charging, solid when full */

/* ---- Animation helpers (external binaries in CWD = /opt/aku/web) ---- */
/* Play the boot animation once and block until it finishes.
 * Returns 0 on success, -1 if the binary is missing. */
int  sysinfo_play_boot_anim(void);
/* Launch a looping emotion animation in the background (non-blocking).
 * Stores child info internally; kill with sysinfo_anim_stop(). */
void sysinfo_anim_play_random(void);
void sysinfo_anim_stop(void);
/* Freeze (on=1, SIGSTOP) or resume (on=0, SIGCONT) the running animation
 * process. Used when an overlay (menu) must draw over the emotion app:
 * freezing stops the external player from overwriting the framebuffer. */
void sysinfo_anim_pause(int on);
/* Name of the currently/last-played emotion dir (static buffer). */
const char *sysinfo_anim_name(void);

/* ---- Reboot ---- */
/* Hard-reboot the device. Prefers /usr/local/sbin/reboot-hard, falls back to
 * arming /dev/watchdog natively, then plain `reboot`. Returns 0 when a hard
 * reboot path was armed (board resets within seconds), -1 if only the soft
 * fallback was issued (may not work on this SoC). */
int sysinfo_reboot(void);

#endif /* SYSINFO_H */
