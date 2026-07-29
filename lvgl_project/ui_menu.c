#include "ui_menu.h"
#include "ui_theme.h"
#include "ui_apps.h"
#include "sysinfo.h"
#include "drivers/fbdev.h"
#include <lvgl.h>

static const char *items[MENU_N] = { LV_SYMBOL_REFRESH " Reboot", LV_SYMBOL_LEFT " Back" };

static lv_obj_t *box;                 /* full-screen overlay */
static lv_obj_t *btn[MENU_N];
static int       sel = 0;             /* selected index */
static int       open_flag = 0;
static int       fb_was_paused = 0;   /* pause state we must restore on close */

#define make_label ui_make_label

static void paint_sel(void)
{
    for (int i = 0; i < MENU_N; i++) {
        if (i == sel) lv_obj_add_style(btn[i], ui_style_menu_btn_sel(), 0);
        else          lv_obj_remove_style(btn[i], ui_style_menu_btn_sel(), 0);
    }
}

void ui_menu_build(lv_obj_t *screen)
{
    box = lv_obj_create(screen);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, DISP_W, DISP_H);
    lv_obj_set_pos(box, 0, 0);
    lv_obj_set_style_bg_color(box, COL_BG, 0);
    lv_obj_set_style_bg_opa(box, 230, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = make_label(box, "Menu", F_H2, COL_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    int y = 36;
    for (int i = 0; i < MENU_N; i++) {
        btn[i] = lv_obj_create(box);
        lv_obj_remove_style_all(btn[i]);
        lv_obj_add_style(btn[i], ui_style_menu_btn(), 0);
        lv_obj_set_size(btn[i], DISP_W - 24, 22);
        lv_obj_set_pos(btn[i], 12, y);
        lv_obj_clear_flag(btn[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *l = make_label(btn[i], items[i], F_BODY, COL_TEXT);
        lv_obj_center(l);
        y += 26;
    }

    sel = 0;
    paint_sel();
}

void ui_menu_open(void)
{
    if (open_flag) return;
    sel = 0;
    paint_sel();

    /* If an app paused the framebuffer (Emotion), LVGL can't draw the menu, and
     * the external player keeps overwriting fb0. Freeze that player AND resume
     * LVGL's fb writes so the overlay stays visible; both are restored on close. */
    fb_was_paused = ui_app_fb_is_paused();
    if (fb_was_paused) {
        sysinfo_anim_pause(1);          /* stop the player overwriting fb0 */
        fbdev_pause(0);
        /* force a full redraw so the menu appears over the last BMP frame */
        lv_obj_invalidate(lv_scr_act());
    }

    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(box);
    open_flag = 1;
}

void ui_menu_close(void)
{
    if (!open_flag) return;
    lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
    open_flag = 0;

    /* restore the app's framebuffer pause if we lifted it */
    if (fb_was_paused) {
        fbdev_pause(1);                 /* LVGL stops writing fb0 again */
        sysinfo_anim_pause(0);          /* player resumes, repaints over menu */
        fb_was_paused = 0;
    }
}

int ui_menu_is_open(void) { return open_flag; }

void ui_menu_move(int dir)
{
    if (!open_flag) return;
    sel = (sel + dir + MENU_N) % MENU_N;
    paint_sel();
}

void ui_menu_select(void)
{
    int chosen = sel;
    ui_menu_close();
    if (chosen == 0) {
        /* Reboot: blink LED, then hard reboot (matches sys_boot behaviour).
         * If no hard-reboot path could be armed, restore the LED so a failed
         * attempt doesn't leave it blinking forever. */
        sysinfo_led_heartbeat();
        if (sysinfo_reboot() != 0)
            sysinfo_led_on();
    }
    /* chosen == 1 (Back): just closes, nothing else to do */
}
