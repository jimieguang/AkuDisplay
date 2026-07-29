#include "ui_state.h"
#include "ui_theme.h"
#include "ui_pages.h"
#include "ui_apps.h"
#include "ui_menu.h"
#include "sysinfo.h"
#include <stdio.h>

/* ---- module state ---- */
static ui_state_t state = ST_PAGES;
static lv_obj_t *app_box;       /* full-screen container apps render into */

/* transient overlays: volume toast + "no app" hint */
static lv_obj_t *vol_toast;
static lv_obj_t *noapp_lbl;
static int toast_hide_ticks = 0;     /* refresh-calls remaining before hide */
static int noapp_hide_ticks = 0;

/* volume / menu navigation throttling (ms, monotonic) */
static uint32_t vol_last_ms = 0;
#define VOL_REPEAT_MS  150

static uint32_t now_ms(void)
{
    return lv_tick_get();
}

/* ---------- transient overlays ---------- */
static void toast_show(const char *txt)
{
    if (!vol_toast) return;
    lv_label_set_text(vol_toast, txt);
    lv_obj_clear_flag(vol_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(vol_toast);
    toast_hide_ticks = 1;     /* ~1s at 1 refresh/s; main loop also times it */
}

static void show_noapp_hint(void)
{
    if (!noapp_lbl) return;
    lv_obj_clear_flag(noapp_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(noapp_lbl);
    noapp_hide_ticks = 1;
}

/* ---------- state transitions ---------- */
static void enter_app(page_id_t p)
{
    if (!ui_app_has_app(p)) { show_noapp_hint(); return; }
    lv_obj_clean(app_box);
    ui_app_open(p, app_box);
    ui_pages_tab_hide(1);
    lv_obj_clear_flag(app_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(app_box);
    state = ST_APP;
}

static void exit_app(void)
{
    page_id_t p = ui_app_active_page();
    ui_app_close();
    lv_obj_clean(app_box);
    lv_obj_add_flag(app_box, LV_OBJ_FLAG_HIDDEN);
    ui_pages_tab_hide(0);
    ui_pages_show(p);        /* return to the page the app was on */
    /* ensure a clean repaint after the Emotion app released the framebuffer */
    lv_obj_invalidate(lv_scr_act());
    state = ST_PAGES;
}

/* ---------- public API ---------- */
void ui_state_init(void)
{
    lv_obj_t *sc = lv_scr_act();

    ui_pages_build(sc);

    /* apps container (full screen, hidden until an app opens) */
    app_box = lv_obj_create(sc);
    lv_obj_remove_style_all(app_box);
    lv_obj_add_style(app_box, ui_style_page(), 0);
    lv_obj_set_size(app_box, DISP_W, DISP_H);
    lv_obj_set_pos(app_box, 0, 0);
    lv_obj_clear_flag(app_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(app_box, LV_OBJ_FLAG_HIDDEN);

    ui_menu_build(sc);

    /* volume toast (centered pill) */
    vol_toast = ui_make_label(sc, "", F_H2, COL_TEXT);
    lv_obj_set_style_bg_color(vol_toast, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(vol_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(vol_toast, 10, 0);
    lv_obj_set_style_pad_ver(vol_toast, 4, 0);
    lv_obj_set_style_radius(vol_toast, 8, 0);
    lv_obj_align(vol_toast, LV_ALIGN_BOTTOM_MID, 0, -26);
    lv_obj_add_flag(vol_toast, LV_OBJ_FLAG_HIDDEN);

    /* "No app" hint (centered) */
    noapp_lbl = ui_make_label(sc, "No app", F_BODY, COL_MUTED);
    lv_obj_align(noapp_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(noapp_lbl, LV_OBJ_FLAG_HIDDEN);

    state = ST_PAGES;
}

ui_state_t ui_state_get(void) { return state; }

void ui_state_on_power_short(void)
{
    if (state == ST_PAGES) {
        ui_pages_show((ui_pages_current() + 1) % N_PAGES);
    } else if (state == ST_APP) {
        ui_app_event(EV_CONFIRM);
    } else if (state == ST_MENU) {
        ui_menu_select();
        /* menu_select closes the overlay; return to whatever we came from
         * (Reboot never returns). Restore APP vs PAGES so the next gesture
         * is routed correctly. */
        state = ui_app_is_open() ? ST_APP : ST_PAGES;
    }
}

void ui_state_on_power_double(void)
{
    if (state == ST_MENU) return;            /* ignored in menu */
    if (state == ST_PAGES) {
        enter_app(ui_pages_current());       /* Home -> no-app hint */
    } else if (state == ST_APP) {
        exit_app();
    }
}

void ui_state_on_power_long(void)
{
    if (state == ST_MENU) {
        ui_menu_close();
        state = ui_app_is_open() ? ST_APP : ST_PAGES;
    } else {
        ui_menu_open();
        state = ST_MENU;
    }
}

void ui_state_on_volume(int dir)
{
    if (dir == 0) return;
    /* single throttle for all states so a held key repeats at ~6.7 Hz
     * instead of firing every main-loop iteration */
    if (now_ms() - vol_last_ms < VOL_REPEAT_MS) return;
    vol_last_ms = now_ms();

    if (state == ST_PAGES) {
        int v = ui_pages_get_volume() + dir;
        if (v < 0) v = 0; if (v > 63) v = 63;
        sysinfo_volume_set(v);
        ui_pages_set_volume(v);
        /* Home already shows a live VOL bar + number, so the toast would just
         * duplicate it; only pop the toast on pages without a volume widget. */
        if (ui_pages_current() != PG_HOME) {
            char t[16]; snprintf(t, sizeof t, "VOL  %d", v);
            toast_show(t);
        }
    } else if (state == ST_APP) {
        /* Vol+ = PREV, Vol- = NEXT (matches menu: Vol+ moves up) */
        ui_app_event(dir > 0 ? EV_PREV : EV_NEXT);
    } else if (state == ST_MENU) {
        /* Vol+ moves the cursor up, Vol- down (per DESIGN.md) */
        ui_menu_move(dir > 0 ? -1 : 1);
    }
}

/* called by main loop every ~1s */
void ui_state_refresh(void)
{
    ui_pages_refresh();
    ui_app_tick();               /* let the open app refresh live content */
    /* auto-hide transient overlays */
    if (toast_hide_ticks > 0 && --toast_hide_ticks == 0 && vol_toast)
        lv_obj_add_flag(vol_toast, LV_OBJ_FLAG_HIDDEN);
    if (noapp_hide_ticks > 0 && --noapp_hide_ticks == 0 && noapp_lbl)
        lv_obj_add_flag(noapp_lbl, LV_OBJ_FLAG_HIDDEN);
}
