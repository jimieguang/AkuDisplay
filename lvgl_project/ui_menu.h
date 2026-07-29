#ifndef UI_MENU_H
#define UI_MENU_H
#include <lvgl.h>

/* ======================================================================
 * ui_menu.h / ui_menu.c
 * Quick menu overlay (Reboot / Back). Opened by long-press from any state.
 *
 * IMPORTANT: when the menu opens while an app has paused the framebuffer
 * (Emotion), we temporarily resume LVGL drawing so the menu is actually
 * visible, and restore the prior pause state when it closes.
 * ====================================================================== */

#define MENU_N 2   /* Reboot, Back */

void ui_menu_build(lv_obj_t *screen);   /* create (hidden) overlay, call once */

void ui_menu_open(void);                /* show overlay; resumes fb if needed */
void ui_menu_close(void);               /* hide overlay; restores fb pause */
int  ui_menu_is_open(void);

/* Move the selection cursor. dir: +1 down, -1 up (wraps). */
void ui_menu_move(int dir);

/* Execute the currently-selected item, then close. */
void ui_menu_select(void);

#endif /* UI_MENU_H */
