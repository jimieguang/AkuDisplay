#include "ui_pages.h"
#include "ui_theme.h"
#include "sysinfo.h"
#include "music_ctrl.h"
#include "icons.h"
#include <time.h>
#include <stdio.h>

/* ---- module state ---- */
static lv_obj_t *page[N_PAGES];      /* page containers */
static lv_obj_t *tab_bar;            /* bottom bar */
static lv_obj_t *dot[N_PAGES];       /* tab indicator dots */
static page_id_t cur_page = PG_HOME;

static int vol_val = 50;             /* last known system volume */
static int uac_on  = 0;

/* Home widgets */
static lv_obj_t *h_clock, *h_date, *h_ip, *h_vol_bar, *h_vol_num, *h_bat_bar, *h_bat_txt, *h_charge;
/* Audio widgets */
static lv_obj_t *a_title, *a_uac_lab, *a_hint;
/* Emotion widgets */
static lv_obj_t *e_title, *e_anim_lab, *e_hint;
/* Music widgets */
static lv_obj_t *m_title, *m_status_lab, *m_hint;

/* ---------- helpers ---------- */
#define make_label ui_make_label

/* ---------- HOME ---------- */
static void build_home(lv_obj_t *p)
{
    /* Hero: large clock, vertically centered slightly above middle */
    const lv_font_t *clk_font = ui_font_digital();
    h_clock = make_label(p, "--:--", clk_font ? clk_font : F_TITLE, COL_TEXT);
    lv_obj_align(h_clock, LV_ALIGN_CENTER, 0, -18);

    /* charging bolt icon, right of clock (hidden unless charging) */
    h_charge = lv_label_create(p);
    lv_label_set_text(h_charge, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(h_charge, COL_WARN, 0);
    lv_obj_set_style_text_font(h_charge, F_BODY, 0);
    lv_obj_align_to(h_charge, h_clock, LV_ALIGN_OUT_RIGHT_TOP, 4, 4);
    lv_obj_add_flag(h_charge, LV_OBJ_FLAG_HIDDEN);

    /* date: subtle, below clock */
    h_date = make_label(p, "--", F_HINT, COL_MUTED);
    lv_obj_align(h_date, LV_ALIGN_CENTER, 0, 8);

    /* IP: small, below date (plain text, no symbol to avoid font-mix width issues) */
    h_ip = make_label(p, "--", F_HINT, COL_MUTED);
    lv_obj_align(h_ip, LV_ALIGN_CENTER, 0, 24);

    /* Bottom status: two ultra-thin bars (volume + battery), no text labels.
     * The bars themselves communicate the state through fill level + color. */
    h_vol_bar = lv_bar_create(p);
    lv_obj_remove_style_all(h_vol_bar);
    lv_obj_add_style(h_vol_bar, ui_style_bar_bg(), LV_PART_MAIN);
    lv_obj_add_style(h_vol_bar, ui_style_bar_ind(), LV_PART_INDICATOR);
    lv_obj_set_size(h_vol_bar, DISP_W - 24, 3);
    lv_bar_set_range(h_vol_bar, 0, 63);
    lv_obj_align(h_vol_bar, LV_ALIGN_BOTTOM_MID, 0, -14);

    h_bat_bar = lv_bar_create(p);
    lv_obj_remove_style_all(h_bat_bar);
    lv_obj_add_style(h_bat_bar, ui_style_bar_bg(), LV_PART_MAIN);
    lv_obj_add_style(h_bat_bar, ui_style_bar_ind(), LV_PART_INDICATOR);
    lv_obj_set_size(h_bat_bar, DISP_W - 24, 3);
    lv_bar_set_range(h_bat_bar, 0, 100);
    lv_obj_align(h_bat_bar, LV_ALIGN_BOTTOM_MID, 0, -7);

    /* Hidden number labels (kept for programmatic access, not displayed) */
    h_vol_num = make_label(p, "0", F_HINT, COL_MUTED);
    lv_obj_add_flag(h_vol_num, LV_OBJ_FLAG_HIDDEN);
    h_bat_txt = make_label(p, "0%", F_HINT, COL_MUTED);
    lv_obj_add_flag(h_bat_txt, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- AUDIO ---------- */
static void build_audio(lv_obj_t *p)
{
    lv_obj_t *icon = lv_img_create(p);
    lv_img_set_src(icon, &img_audio);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -24);

    a_title = make_label(p, "AUDIO", F_BODY, COL_SUB);
    lv_obj_align(a_title, LV_ALIGN_CENTER, 0, 8);
    a_uac_lab = make_label(p, "UAC: OFF", F_HINT, COL_MUTED);
    lv_obj_align(a_uac_lab, LV_ALIGN_CENTER, 0, 26);
}

/* ---------- EMOTION ---------- */
static void build_emotion(lv_obj_t *p)
{
    lv_obj_t *icon = lv_img_create(p);
    lv_img_set_src(icon, &img_emotion);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -24);

    e_title = make_label(p, "EMOTION", F_BODY, COL_SUB);
    lv_obj_align(e_title, LV_ALIGN_CENTER, 0, 8);
    e_anim_lab = make_label(p, "idle", F_HINT, COL_MUTED);
    lv_obj_align(e_anim_lab, LV_ALIGN_CENTER, 0, 26);
}

/* ---------- MUSIC (FlipPanel PC control) ---------- */
static void build_music(lv_obj_t *p)
{
    lv_obj_t *icon = lv_img_create(p);
    lv_img_set_src(icon, &img_music);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -24);

    m_title = make_label(p, "MUSIC", F_BODY, COL_SUB);
    lv_obj_align(m_title, LV_ALIGN_CENTER, 0, 8);
    /* link state */
    m_status_lab = make_label(p, "Searching...", F_HINT, COL_MUTED);
    lv_obj_align(m_status_lab, LV_ALIGN_CENTER, 0, 26);
}

/* ---------- tab bar ---------- */
static void build_tab(lv_obj_t *screen)
{
    tab_bar = lv_obj_create(screen);
    lv_obj_remove_style_all(tab_bar);
    lv_obj_set_size(tab_bar, DISP_W, TAB_H);
    lv_obj_set_pos(tab_bar, 0, PAGE_H);
    lv_obj_set_style_bg_color(tab_bar, COL_BG, 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab_bar, 0, 0);
    lv_obj_set_style_pad_all(tab_bar, 0, 0);
    lv_obj_set_flex_flow(tab_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < N_PAGES; i++) {
        dot[i] = lv_obj_create(tab_bar);
        lv_obj_set_size(dot[i], 7, 7);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot[i], 0, 0);
        lv_obj_set_style_bg_opa(dot[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot[i], COL_DIM, 0);
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void upd_tab(void)
{
    for (int i = 0; i < N_PAGES; i++)
        lv_obj_set_style_bg_color(dot[i], (i == cur_page) ? COL_ACCENT : COL_DIM, 0);
}

/* ---------- public API ---------- */
void ui_pages_build(lv_obj_t *screen)
{
    icons_init();   /* rasterise page icons before lv_img_set_src uses them */
    for (int i = 0; i < N_PAGES; i++) {
        page[i] = lv_obj_create(screen);
        lv_obj_remove_style_all(page[i]);
        lv_obj_add_style(page[i], ui_style_page(), 0);
        lv_obj_set_size(page[i], DISP_W, PAGE_H);
        lv_obj_set_pos(page[i], 0, 0);
        lv_obj_clear_flag(page[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    build_home(page[PG_HOME]);
    build_audio(page[PG_AUDIO]);
    build_emotion(page[PG_EMOTION]);
    build_music(page[PG_MUSIC]);
    build_tab(screen);
    ui_pages_show(PG_HOME);
}

void ui_pages_show(page_id_t p)
{
    if (p < 0 || p >= N_PAGES) return;
    for (int i = 0; i < N_PAGES; i++) lv_obj_add_flag(page[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(page[p], LV_OBJ_FLAG_HIDDEN);
    cur_page = p;
    upd_tab();
}

page_id_t ui_pages_current(void) { return cur_page; }

void ui_pages_tab_hide(int hide)
{
    if (hide) lv_obj_add_flag(tab_bar, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_HIDDEN);
}

void ui_pages_set_volume(int v)
{
    if (v < 0) v = 0; if (v > 63) v = 63;
    vol_val = v;
    lv_bar_set_value(h_vol_bar, v, LV_ANIM_OFF);
    lv_label_set_text_fmt(h_vol_num, "%d", v);
}

int ui_pages_get_volume(void) { return vol_val; }

void ui_pages_set_uac(int on)
{
    uac_on = on;
    if (a_uac_lab) lv_label_set_text(a_uac_lab, on ? "UAC: ON" : "UAC: OFF");
}

void ui_pages_refresh(void)
{
    /* clock + date */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char b[16];
    strftime(b, sizeof b, "%H:%M", tm);
    if (h_clock) lv_label_set_text(h_clock, b);
    strftime(b, sizeof b, "%m-%d %a", tm);
    if (h_date) lv_label_set_text(h_date, b);

    /* ip */
    if (h_ip) lv_label_set_text(h_ip, sysinfo_wifi_ip());

    /* volume: keep displayed value in sync with system once per refresh */
    ui_pages_set_volume(sysinfo_volume_get());

    /* battery + charging icon */
    int bc = sysinfo_battery_capacity();
    if (bc < 0) bc = 0;
    if (h_bat_bar) {
        lv_bar_set_value(h_bat_bar, bc, LV_ANIM_OFF);
        /* colour-code the indicator: green healthy, amber low, red critical */
        lv_color_t c = (bc <= 15) ? COL_DANGER : (bc <= 35) ? COL_WARN : COL_GOOD;
        lv_obj_set_style_bg_color(h_bat_bar, c, LV_PART_INDICATOR);
    }
    if (h_bat_txt) lv_label_set_text_fmt(h_bat_txt, "%d%%", bc);
    if (h_charge) {
        if (sysinfo_is_charging()) lv_obj_clear_flag(h_charge, LV_OBJ_FLAG_HIDDEN);
        else                       lv_obj_add_flag(h_charge, LV_OBJ_FLAG_HIDDEN);
    }

    /* audio / emotion status from modules that own them */
    if (a_uac_lab) lv_label_set_text(a_uac_lab, uac_on ? "UAC: ON" : "UAC: OFF");
    if (e_anim_lab) lv_label_set_text_fmt(e_anim_lab, "%s", sysinfo_anim_name());

    /* music link state (browsing page) */
    if (m_status_lab) {
        mc_status_t ms;
        music_ctrl_get(&ms);
        if (ms.connected) {
            lv_label_set_text(m_status_lab, "Linked");
            lv_obj_set_style_text_color(m_status_lab, COL_GOOD, 0);
        } else {
            lv_label_set_text(m_status_lab, "Searching...");
            lv_obj_set_style_text_color(m_status_lab, COL_MUTED, 0);
        }
    }
}
