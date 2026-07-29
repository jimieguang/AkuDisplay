#ifndef EVDEV_H
#define EVDEV_H

#include <stdint.h>

typedef enum {
    BTN_NONE = 0,
    BTN_VOL_UP,
    BTN_VOL_DOWN,
    BTN_POWER_SHORT,
    BTN_POWER_DOUBLE,
    BTN_POWER_LONG,
} btn_event_t;

void evdev_init(void);
btn_event_t evdev_poll(void);

/* Returns +1 (vol up pressed), -1 (vol down pressed), 0 (none) */
int evdev_vol_dir(void);

#endif
