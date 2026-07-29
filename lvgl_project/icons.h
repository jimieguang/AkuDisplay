#ifndef ICONS_H
#define ICONS_H
#include <lvgl.h>

/* ======================================================================
 * icons.h / icons.c
 * Small 32x32 page icons (audio / emotion / music), generated
 * procedurally at startup instead of shipping binary image assets.
 * Format: LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 + A8 at 16bpp).
 * ====================================================================== */

extern lv_img_dsc_t img_audio;    /* speaker + sound waves  (accent blue) */
extern lv_img_dsc_t img_emotion;  /* smiley face            (warm amber)  */
extern lv_img_dsc_t img_music;    /* beamed note pair       (green)       */

/* Rasterise the icon bitmaps. Idempotent; called from ui_pages_build(). */
void icons_init(void);

#endif /* ICONS_H */
