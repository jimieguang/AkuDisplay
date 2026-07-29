#include "sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

/* ======================================================================
 * Device interface wrappers. Paths verified on akubox (recon 2026-07-23).
 * All external assets are referenced by ABSOLUTE path so the program works
 * regardless of the caller's working directory.
 * ====================================================================== */

#define BAT_BASE  "/sys/class/power_supply/axp20x-battery"
#define LED_BASE  "/sys/class/leds/aku-logo"
#define VOL_MIXER "Power Amplifier"
#define VOL_MIN   0
#define VOL_MAX   63
#define EMO_N     9     /* emotion1..emotion9 confirmed present on device */

#define PLAYER_BIN "/opt/aku/web/play_bmp_sequence"
#define BOOT_DIR   "/opt/aku/web/booting"
#define EMO_BASE   "/opt/aku/web/emotions"

/* ---------- tiny helpers ---------- */
static int read_first_line(const char *path, char *buf, int sz)
{
    FILE *f = fopen(path, "r");
    if (!f) { if (sz > 0) buf[0] = 0; return -1; }
    if (!fgets(buf, sz, f)) { buf[0] = 0; fclose(f); return -1; }
    fclose(f);
    for (char *p = buf; *p; p++) if (*p == '\n') { *p = 0; break; }
    return 0;
}

static int read_int(const char *path)
{
    char b[32];
    if (read_first_line(path, b, sizeof b) < 0) return -1;
    return atoi(b);
}

static void write_str(const char *path, const char *val)
{
    FILE *f = fopen(path, "w");
    if (f) { fputs(val, f); fclose(f); }
}

/* ---------- battery ---------- */
int sysinfo_battery_capacity(void)
{
    int c = read_int(BAT_BASE "/capacity");
    if (c < 0) return -1;
    if (c > 100) c = 100;
    return c;
}

void sysinfo_battery_status(char *out, int sz)
{
    if (read_first_line(BAT_BASE "/status", out, sz) < 0)
        snprintf(out, sz, "?");
}

int sysinfo_is_charging(void)
{
    char s[32];
    sysinfo_battery_status(s, sizeof s);
    return strncmp(s, "Charging", 8) == 0;
}

/* ---------- volume ---------- */

/* Cached volume: avoid forking amixer every 1s refresh. The cache is
 * invalidated on set (we know the value we wrote) and refreshed from the
 * hardware at most once every VOL_CACHE_MS. */
#define VOL_CACHE_MS 3000

static int      vol_cached   = -1;
static uint32_t vol_cache_ms = 0;

static uint32_t vol_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int sysinfo_volume_get(void)
{
    /* Return cached value if fresh enough */
    if (vol_cached >= 0 && (vol_now_ms() - vol_cache_ms) < VOL_CACHE_MS)
        return vol_cached;

    /* amixer output has the control index on the header line ("...',0") and the
     * real value on the channel line ("  Mono: 59 [94%] [-4.00dB]"). A naive
     * "first integer" grab returns the index 0, so match the value that is
     * immediately followed by " [" instead. */
    FILE *f = popen("amixer get '" VOL_MIXER "' 2>/dev/null"
                    " | sed -n 's/.*: \\([0-9]\\+\\) \\[.*/\\1/p' | head -1", "r");
    if (!f) return (vol_cached >= 0) ? vol_cached : VOL_MAX / 2;
    int v = -1;
    if (fscanf(f, "%d", &v) != 1) v = -1;
    pclose(f);
    if (v < 0) v = (vol_cached >= 0) ? vol_cached : VOL_MAX / 2;
    if (v < VOL_MIN) v = VOL_MIN;
    if (v > VOL_MAX) v = VOL_MAX;
    vol_cached   = v;
    vol_cache_ms = vol_now_ms();
    return v;
}

void sysinfo_volume_set(int v)
{
    if (v < VOL_MIN) v = VOL_MIN;
    if (v > VOL_MAX) v = VOL_MAX;
    /* Update cache immediately so the UI reflects the new value without
     * waiting for the next popen read. */
    vol_cached   = v;
    vol_cache_ms = vol_now_ms();
    /* Run amixer in the background so the UI thread is never blocked by the
     * shell fork+exec. The trailing & makes system() return immediately. */
    char cmd[80];
    snprintf(cmd, sizeof cmd, "amixer set '" VOL_MIXER "' %d >/dev/null 2>&1 &", v);
    system(cmd);
}

/* ---------- network ---------- */
const char *sysinfo_wifi_ip(void)
{
    static char ip[32] = "N/A";
    FILE *f = popen("ip -4 -br addr show wlan0 2>/dev/null | awk '{print $3}' | cut -d/ -f1", "r");
    if (f) {
        char tmp[32];
        if (fgets(tmp, sizeof tmp, f)) {
            for (char *p = tmp; *p; p++) if (*p == '\n') { *p = 0; break; }
            if (tmp[0]) snprintf(ip, sizeof ip, "%s", tmp);
            else        snprintf(ip, sizeof ip, "N/A");
        }
        pclose(f);
    }
    return ip;
}

/* ---------- LED ---------- */
void sysinfo_led_on(void)        { write_str(LED_BASE "/trigger", "default-on"); write_str(LED_BASE "/brightness", "1"); }
void sysinfo_led_off(void)       { write_str(LED_BASE "/trigger", "none");       write_str(LED_BASE "/brightness", "0"); }
void sysinfo_led_heartbeat(void) { write_str(LED_BASE "/trigger", "heartbeat"); }
void sysinfo_led_charging(void)  { write_str(LED_BASE "/trigger", "axp20x-battery-charging-or-full"); }

/* ---------- animations ---------- */
static int  anim_pid = -1;
static char anim_cur[32] = "idle";

static void anim_launch(const char *path, const char *name, int delay_ms, int once)
{
    sysinfo_anim_stop();   /* never overlap two players */
    pid_t pid = fork();
    if (pid == 0) {
        /* child: run play_bmp_sequence writing directly to /dev/fb0 */
        char darg[16];
        snprintf(darg, sizeof darg, "%d", delay_ms);
        if (once) execl(PLAYER_BIN, "play_bmp_sequence", "-d", darg, "-l", path, (char *)NULL);
        else      execl(PLAYER_BIN, "play_bmp_sequence", "-d", darg, path, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) { anim_pid = pid; snprintf(anim_cur, sizeof anim_cur, "%s", name); }
}

int sysinfo_play_boot_anim(void)
{
    if (access(PLAYER_BIN, X_OK) != 0) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        execl(PLAYER_BIN, "play_bmp_sequence", "-d", "20", "-l", BOOT_DIR, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) { int st; waitpid(pid, &st, 0); return 0; }   /* block until done */
    return -1;
}

void sysinfo_anim_play_random(void)
{
    char name[16], path[48];
    snprintf(name, sizeof name, "emotion%d", 1 + rand() % EMO_N);
    snprintf(path, sizeof path, EMO_BASE "/%s", name);
    anim_launch(path, name, 100, 0);   /* loop */
}

void sysinfo_anim_stop(void)
{
    if (anim_pid > 0) { kill(anim_pid, SIGTERM); int st; waitpid(anim_pid, &st, 0); anim_pid = -1; }
    /* belt + suspenders: kill any stray player by name */
    system("killall play_bmp_sequence 2>/dev/null");
}

void sysinfo_anim_pause(int on)
{
    if (anim_pid <= 0) return;
    /* SIGSTOP freezes the player so it stops writing /dev/fb0; SIGCONT resumes
     * it, and its next frame naturally repaints over whatever drew on top. */
    kill(anim_pid, on ? SIGSTOP : SIGCONT);
}

const char *sysinfo_anim_name(void) { return anim_cur; }

/* ---------- reboot ---------- */
#define REBOOT_HARD "/usr/local/sbin/reboot-hard"

/* Soft reboot (reboot/systemctl reboot) cannot reset the PMIC on this SoC,
 * so the device-side reboot-hard script arms /dev/watchdog and stops feeding
 * it. Replicate that here as a fallback so reboot still works on units that
 * lack the script (it silently did nothing on the second device). */
int sysinfo_reboot(void)
{
    sync();

    /* 1) preferred: the ops-provided script (matches sys_boot behaviour) */
    if (access(REBOOT_HARD, X_OK) == 0) {
        system("sync; " REBOOT_HARD " &");
        return 0;
    }

    /* 2) native fallback: arm the hardware watchdog and never feed it */
    int fd = open("/dev/watchdog", O_RDWR);
    if (fd >= 0) {
        int timeout = 5;
        ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
        /* keep fd open, don't write: board resets in ~timeout seconds */
        return 0;
    }

    /* 3) last resort: soft reboot (may hang on this SoC, better than nothing) */
    system("reboot &");
    return -1;
}
