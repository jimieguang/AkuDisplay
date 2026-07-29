#ifndef UI_APPS_H
#define UI_APPS_H
#include <lvgl.h>
#include "ui_state.h"   /* app_evt_t */
#include "ui_pages.h"   /* page_id_t */

/* ======================================================================
 * ui_apps.h / ui_apps.c
 * Full-screen apps entered by double-press on a page that has one.
 *   PG_AUDIO   -> Audio app  (toggle UAC)
 *   PG_EMOTION -> Emotion app (play/stop BMP animations via play_bmp_sequence)
 * Home (PG_HOME) has no app.
 *
 * Each app follows the framework in DESIGN.md:
 *   app_init(parent)  / app_btn(event)  / app_deinit()
 * ====================================================================== */

/* Returns 1 if the given page has an app, 0 otherwise. */
int  ui_app_has_app(page_id_t p);

/* Open the app for page p on the given full-screen container. */
void ui_app_open(page_id_t p, lv_obj_t *container);

/* Deliver an input event to the currently-open app. */
void ui_app_event(app_evt_t ev);

/* Periodic (~1s) tick for the open app to refresh live content (e.g. the
 * Music app polls the now-playing snapshot). No-op if no app is open. */
void ui_app_tick(void);

/* Close the current app and release resources. */
void ui_app_close(void);

/* The page the currently-open app belongs to (valid while open). */
page_id_t ui_app_active_page(void);

/* Whether an app is currently open. */
int  ui_app_is_open(void);

/* Whether the currently-open app has paused the framebuffer (Emotion).
 * Used by the menu so it can temporarily resume LVGL drawing to show itself. */
int  ui_app_fb_is_paused(void);

#endif /* UI_APPS_H */
