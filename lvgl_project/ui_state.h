#ifndef UI_STATE_H
#define UI_STATE_H
#include <lvgl.h>

/* ======================================================================
 * ui_state.h / ui_state.c
 * Top-level state machine + input dispatch + transient volume toast.
 *
 * States:  PAGES (browsing) | APP (full-screen app) | MENU (quick menu overlay)
 *
 * Inputs (from evdev, see drivers/evdev.h):
 *   BTN_POWER_SHORT / _DOUBLE / _LONG  +  volume direction (+1/-1/0)
 *
 * Interaction contract (preserved from the original design):
 *   short   : PAGES next page / APP confirm / MENU execute selection
 *   double  : PAGES enter app (Home=hint) / APP exit to pages / MENU ignored
 *   long    : any state -> open menu / MENU -> close menu
 *   vol +/- : PAGES change volume (150ms repeat) / APP EV_PREV|EV_NEXT / MENU move cursor
 * ====================================================================== */

typedef enum { ST_PAGES, ST_APP, ST_MENU } ui_state_t;

typedef enum { EV_CONFIRM, EV_BACK, EV_NEXT, EV_PREV } app_evt_t;

/* Build everything (pages, apps container, menu, toast) onto the screen.
 * Sets initial state ST_PAGES. Call once after drivers are registered. */
void ui_state_init(void);

/* Current top-level state. */
ui_state_t ui_state_get(void);

/* Input entry points, called from the main loop. */
void ui_state_on_power_short(void);
void ui_state_on_power_double(void);
void ui_state_on_power_long(void);
/* dir: +1 vol up, -1 vol down, 0 none. Throttling handled here. */
void ui_state_on_volume(int dir);

/* Periodic refresh (clock/battery/etc.). Called ~1/s from main loop. */
void ui_state_refresh(void);

#endif /* UI_STATE_H */
