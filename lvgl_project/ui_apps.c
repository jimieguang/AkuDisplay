#include "ui_apps.h"
#include "ui_theme.h"
#include "ui_pages.h"
#include "sysinfo.h"
#include "music_ctrl.h"
#include "drivers/fbdev.h"
#include <stdio.h>

/* ---- module state ---- */
static page_id_t  active_page = PG_HOME;   /* only valid while open */
static int        is_open = 0;
static int        fb_paused = 0;           /* set by Emotion while animating */

/* per-app widget handles */
static lv_obj_t *au_title, *au_lab, *au_hint;
static lv_obj_t *au_vol_bar, *au_vol_num;   /* volume bar + number in Audio app */
static lv_obj_t *em_title, *em_lab, *em_hint;
static lv_obj_t *mu_state, *mu_track, *mu_hint;

#define make_label ui_make_label

int ui_app_has_app(page_id_t p)
{
    return p == PG_AUDIO || p == PG_EMOTION || p == PG_MUSIC;
}

/* ---------- AUDIO app (UAC toggle + volume control) ---------- */
static int uac_on = 0;

static void audio_vol_update(void)
{
    int v = sysinfo_volume_get();
    if (au_vol_bar) lv_bar_set_value(au_vol_bar, v, LV_ANIM_OFF);
    if (au_vol_num) lv_label_set_text_fmt(au_vol_num, "%d", v);
}

static void audio_init(lv_obj_t *p)
{
    /* Hero: large volume number, centered (digital font) */
    const lv_font_t *dig = ui_font_digital();
    au_vol_num = make_label(p, "0", dig ? dig : F_TITLE, COL_TEXT);
    lv_obj_align(au_vol_num, LV_ALIGN_CENTER, 0, -14);

    /* Thin full-width volume bar below the number */
    au_vol_bar = lv_bar_create(p);
    lv_obj_remove_style_all(au_vol_bar);
    lv_obj_add_style(au_vol_bar, ui_style_bar_bg(), LV_PART_MAIN);
    lv_obj_add_style(au_vol_bar, ui_style_bar_ind(), LV_PART_INDICATOR);
    lv_obj_set_size(au_vol_bar, DISP_W - 32, 4);
    lv_bar_set_range(au_vol_bar, 0, 63);
    lv_obj_align(au_vol_bar, LV_ALIGN_CENTER, 0, 14);

    /* UAC status: small, at bottom */
    au_lab = make_label(p, uac_on ? "UAC ON" : "UAC OFF", F_HINT,
                        uac_on ? COL_GOOD : COL_MUTED);
    lv_obj_align(au_lab, LV_ALIGN_BOTTOM_MID, 0, -8);

    au_title = NULL;  /* no separate title needed - the number IS the page */
    au_hint = NULL;
    audio_vol_update();
}

static void audio_event(app_evt_t ev)
{
    if (ev == EV_CONFIRM) {
        uac_on = !uac_on;
        if (au_lab) {
            lv_label_set_text(au_lab, uac_on ? "UAC ON" : "UAC OFF");
            lv_obj_set_style_text_color(au_lab, uac_on ? COL_GOOD : COL_MUTED, 0);
        }
        ui_pages_set_uac(uac_on);
        system(uac_on ? "cd /opt/aku/web && ./audio_start.sh &"
                      : "cd /opt/aku/web && ./audio_stop.sh &");
    } else if (ev == EV_PREV) {          /* Vol+ → volume up */
        int v = ui_pages_get_volume() + 1;
        if (v > 63) v = 63;
        sysinfo_volume_set(v);
        ui_pages_set_volume(v);
        audio_vol_update();
    } else if (ev == EV_NEXT) {          /* Vol- → volume down */
        int v = ui_pages_get_volume() - 1;
        if (v < 0) v = 0;
        sysinfo_volume_set(v);
        ui_pages_set_volume(v);
        audio_vol_update();
    }
}

static void audio_deinit(void)
{
    au_title = au_lab = au_hint = NULL;
    au_vol_bar = au_vol_num = NULL;
}

/* ---------- EMOTION app (external BMP animation) ---------- */
static void emotion_init(lv_obj_t *p)
{
    em_title = make_label(p, "Emotion", F_TITLE, COL_TEXT);
    lv_obj_align(em_title, LV_ALIGN_CENTER, 0, -14);
    em_lab = make_label(p, "Playing...", F_BODY, COL_MUTED);
    lv_obj_align(em_lab, LV_ALIGN_CENTER, 0, 14);
    em_hint = NULL;

    /* start a random emotion and let it write /dev/fb0 directly */
    sysinfo_anim_play_random();
    fbdev_pause(1);
    fb_paused = 1;
    if (em_lab) lv_label_set_text_fmt(em_lab, "Play: %s", sysinfo_anim_name());
}

static void emotion_event(app_evt_t ev)
{
    if (ev == EV_CONFIRM) {        /* short press: pick another random emotion */
        sysinfo_anim_play_random();
        if (em_lab) lv_label_set_text_fmt(em_lab, "Play: %s", sysinfo_anim_name());
    }
}

static void emotion_deinit(void)
{
    sysinfo_anim_stop();
    fbdev_pause(0);
    fb_paused = 0;
    em_title = em_lab = em_hint = NULL;
}

/* ---------- MUSIC app (FlipPanel PC control) ---------- */
static void music_refresh(void)
{
    mc_status_t ms;
    music_ctrl_get(&ms);

    if (!ms.connected) {
        if (mu_state) {
            lv_label_set_text(mu_state, "Offline");
            lv_obj_set_style_text_color(mu_state, COL_MUTED, 0);
        }
        if (mu_track) lv_label_set_text(mu_track, "no PC bridge");
        return;
    }

    if (mu_state) {
        /* PlaybackState is ASCII from the bridge ("Playing"/"Paused"/...) */
        int playing = (ms.state[0] &&
                       (ms.state[0] == 'P' || ms.state[0] == 'p') &&
                       (ms.state[1] == 'l' || ms.state[1] == 'L'));
        lv_label_set_text(mu_state, ms.state[0] ? ms.state : "Linked");
        lv_obj_set_style_text_color(mu_state, playing ? COL_ACCENT : COL_SUB, 0);
    }
    if (mu_track) {
        /* title - artist; CJK renders via the FreeType font (ui_font_cjk) */
        if (ms.title[0] && ms.artist[0]) {
            lv_label_set_text_fmt(mu_track, "%s - %s", ms.title, ms.artist);
        } else if (ms.title[0]) {
            lv_label_set_text(mu_track, ms.title);
        } else {
            lv_label_set_text(mu_track, "--");
        }
    }
}

static void music_init(lv_obj_t *p)
{
    /* Playback state as hero text */
    mu_state = make_label(p, "...", F_H2, COL_SUB);
    lv_obj_align(mu_state, LV_ALIGN_CENTER, 0, -16);

    /* scrolling track line */
    const lv_font_t *track_font = ui_font_cjk();
    mu_track = make_label(p, "--", track_font ? track_font : F_HINT, COL_MUTED);
    lv_obj_set_width(mu_track, DISP_W - 20);
    lv_label_set_long_mode(mu_track, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(mu_track, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(mu_track, LV_ALIGN_CENTER, 0, 10);

    mu_hint = NULL;
    music_refresh();
}

static void music_event(app_evt_t ev)
{
    if (ev == EV_CONFIRM)   music_ctrl_action("music.playPause");
    else if (ev == EV_PREV) music_ctrl_action("music.previous");
    else if (ev == EV_NEXT) music_ctrl_action("music.next");
}

static void music_deinit(void)
{
    mu_state = mu_track = mu_hint = NULL;
}

/* ---------- public API ---------- */
void ui_app_open(page_id_t p, lv_obj_t *container)
{
    if (!ui_app_has_app(p)) return;
    active_page = p;
    is_open = 1;
    fb_paused = 0;
    if (p == PG_AUDIO)   audio_init(container);
    if (p == PG_EMOTION) emotion_init(container);
    if (p == PG_MUSIC)   music_init(container);
}

void ui_app_event(app_evt_t ev)
{
    if (!is_open) return;
    if (active_page == PG_AUDIO)   audio_event(ev);
    if (active_page == PG_EMOTION) emotion_event(ev);
    if (active_page == PG_MUSIC)   music_event(ev);
}

void ui_app_tick(void)
{
    if (!is_open) return;
    if (active_page == PG_MUSIC) music_refresh();
}

void ui_app_close(void)
{
    if (!is_open) return;
    if (active_page == PG_AUDIO)   audio_deinit();
    if (active_page == PG_EMOTION) emotion_deinit();
    if (active_page == PG_MUSIC)   music_deinit();
    is_open = 0;
    fb_paused = 0;
}

int ui_app_is_open(void)      { return is_open; }
int ui_app_fb_is_paused(void) { return fb_paused; }

page_id_t ui_app_active_page(void) { return active_page; }
