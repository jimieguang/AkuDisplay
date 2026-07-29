/* ======================================================================
 * main.c - AkuBot LVGL application entry point (replaces sys_boot)
 *
 * Boot sequence:
 *   1. lv_init + fbdev + evdev drivers
 *   2. blocking boot animation (external player writes /dev/fb0 directly)
 *   3. register LVGL display driver, build UI (theme -> state machine)
 *   4. LED on, enter main loop
 *
 * Main loop (5 ms tick):
 *   - lv_tick_inc / lv_timer_handler
 *   - every ~1 s: ui_state_refresh() (clock, ip, volume, battery, charge)
 *   - evdev power gestures -> ui_state_on_power_{short,double,long}
 *   - volume direction      -> ui_state_on_volume
 * ====================================================================== */
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "drivers/fbdev.h"
#include "drivers/evdev.h"
#include "ui_theme.h"
#include "ui_state.h"
#include "ui_apps.h"
#include "sysinfo.h"
#include "music_ctrl.h"

/* Graceful shutdown: signal handler sets flag, main loop exits cleanly. */
static volatile sig_atomic_t g_quit = 0;

static void on_term(int sig)
{
    (void)sig;
    g_quit = 1;
}

static void disp_init(void)
{
    static lv_color_t buf[DISP_W * DISP_H];
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, DISP_W * DISP_H);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISP_W;
    disp_drv.ver_res  = DISP_H;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

int main(void)
{
    srand(time(NULL));

    /* Install signal handlers for graceful shutdown (systemctl stop / Ctrl+C) */
    struct sigaction sa;
    sa.sa_handler = on_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    lv_init();
    fbdev_init();
    evdev_init();

    /* Boot animation: plays once and blocks until finished.
     * The external player owns /dev/fb0 during this time (LVGL has not
     * drawn anything yet, so there is no conflict). */
    sysinfo_play_boot_anim();

    disp_init();
    lv_theme_default_init(NULL,
                          lv_palette_main(LV_PALETTE_BLUE),
                          lv_palette_main(LV_PALETTE_RED),
                          1, &lv_font_montserrat_14);

    ui_theme_init();
    ui_fonts_init();             /* load CJK font (FreeType) for e.g. song titles */
    ui_state_init();
    ui_state_refresh();          /* fill clock/ip/volume/battery immediately */

    sysinfo_led_on();            /* aku-logo LED solid on while running */

    /* start the FlipPanel PC-music bridge client (auto-discovers over LAN,
     * used by the Music app; harmless if no PC bridge is present) */
    music_ctrl_init();

    uint32_t cnt = 0;
    unsigned last_gen = music_ctrl_generation();
    while (!g_quit) {
        lv_tick_inc(5);
        lv_timer_handler();

        if (++cnt >= 200) {      /* ~1 s at 5 ms/tick */
            cnt = 0;
            ui_state_refresh();
        }

        /* Instant music status update: when the worker receives a new status
         * frame (play/pause/track change), bump the generation counter and
         * we refresh the Music UI within 5ms instead of waiting up to 1s. */
        unsigned gen = music_ctrl_generation();
        if (gen != last_gen) {
            last_gen = gen;
            ui_app_tick();
        }

        btn_event_t ev = evdev_poll();
        if      (ev == BTN_POWER_LONG)   ui_state_on_power_long();
        else if (ev == BTN_POWER_DOUBLE) ui_state_on_power_double();
        else if (ev == BTN_POWER_SHORT)  ui_state_on_power_short();

        ui_state_on_volume(evdev_vol_dir());

        usleep(5000);
    }

    /* ---- graceful cleanup ---- */
    sysinfo_anim_stop();         /* kill any running animation child process */
    music_ctrl_shutdown();       /* signal worker thread to exit */
    sysinfo_led_off();           /* turn off aku-logo LED */
    return 0;
}
