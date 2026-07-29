#include "evdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <linux/input.h>

#define SHORT_MS  350
#define LONG_MS   900

static int fd[2] = {-1, -1};
static int vol_dir = 0;

/* power button state machine */
static int  pw_clicks  = 0;       /* completed click count */
static int  pw_held    = 0;       /* currently held? */
static uint64_t pw_dn  = 0;       /* time of last press */
static uint64_t pw_up  = 0;       /* time of last release */
static int  pw_long    = 0;       /* long press already reported */

static uint64_t ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

void evdev_init(void) {
    fd[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    fd[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
}

btn_event_t evdev_poll(void) {
    struct input_event ev;
    btn_event_t res = BTN_NONE;
    uint64_t now = ms();

    for (int i = 0; i < 2; i++) {
        if (fd[i] < 0) continue;
        while (read(fd[i], &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type != EV_KEY) continue;
            int c = ev.code, v = ev.value;

            if (c == KEY_POWER) {
                if (v == 1) {                    /* press */
                    pw_held = 1;
                    pw_dn = now;
                    pw_long = 0;
                } else if (v == 0) {             /* release */
                    pw_held = 0;
                    pw_up = now;
                    if (!pw_long) pw_clicks++;   /* only count if not long-press */
                }
            }

            /* value: 0=release, 1=press, 2=held auto-repeat.
             * Treat repeat as "still held" so volume keeps adjusting. */
            if (c == KEY_VOLUMEUP) {
                if (v >= 1) vol_dir = +1;
                else if (vol_dir == +1) vol_dir = 0;
            }
            if (c == KEY_VOLUMEDOWN) {
                if (v >= 1) vol_dir = -1;
                else if (vol_dir == -1) vol_dir = 0;
            }
        }
    }

    /* ---- gesture detection (runs every poll) ---- */

    /* long press: if held > LONG_MS and not yet reported */
    if (pw_held && !pw_long && pw_dn && now - pw_dn >= LONG_MS) {
        pw_long = 1;
        pw_clicks = 0;     /* discard any pending clicks */
        res = BTN_POWER_LONG;
    }

    /* short / double: check after release silence > SHORT_MS */
    if (!pw_held && pw_clicks > 0 && now - pw_up >= SHORT_MS) {
        int n = pw_clicks;
        pw_clicks = 0;
        res = (n >= 2) ? BTN_POWER_DOUBLE : BTN_POWER_SHORT;
    }

    return res;
}

int evdev_vol_dir(void) {
    return vol_dir;
}
