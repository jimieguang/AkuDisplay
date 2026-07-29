#ifndef UI_PAGES_H
#define UI_PAGES_H
#include <lvgl.h>

/* ======================================================================
 * ui_pages.h / ui_pages.c
 * The three browsing pages (Home / Audio / Emotion) and the bottom tab.
 * Pages are full-width panels stacked on the screen; only one visible.
 * ====================================================================== */

#define N_PAGES 4

typedef enum { PG_HOME = 0, PG_AUDIO = 1, PG_EMOTION = 2, PG_MUSIC = 3 } page_id_t;

/* Build all pages + tab bar onto the given screen. Call once. */
void ui_pages_build(lv_obj_t *screen);

/* Show a specific page (hides the others), updates the tab indicator. */
void ui_pages_show(page_id_t p);
page_id_t ui_pages_current(void);

/* Tab bar visibility (hidden while an app or menu is open). */
void ui_pages_tab_hide(int hide);

/* Refresh the dynamic content of all pages (clock, ip, volume, battery,
 * charge icon, emotion name, uac state). Called every ~1s and on demand. */
void ui_pages_refresh(void);

/* Set the volume value displayed on the Home page (does NOT change system
 * volume). Used by the volume-toast path and normal refresh. */
void ui_pages_set_volume(int v);
int  ui_pages_get_volume(void);

/* UAC state shown on the Audio page. */
void ui_pages_set_uac(int on);

#endif /* UI_PAGES_H */
