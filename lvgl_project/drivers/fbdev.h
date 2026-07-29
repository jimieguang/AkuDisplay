#ifndef FBDEV_H
#define FBDEV_H

#include <lvgl.h>

void fbdev_init(void);
void fbdev_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void fbdev_pause(int on);  /* 1=pause (external process draws), 0=resume */

#endif
