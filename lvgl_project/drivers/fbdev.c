#include "fbdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

static int fbfd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static uint8_t *fb_mem = NULL;
static uint32_t fb_size = 0;
static int paused = 0;

void fbdev_init(void)
{
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) return;

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0) { close(fbfd); fbfd=-1; return; }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) { close(fbfd); fbfd=-1; return; }

    fb_size = finfo.smem_len;
    fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fb_mem == MAP_FAILED) { fb_mem=NULL; close(fbfd); fbfd=-1; }
}

void fbdev_pause(int on) { paused = on; }

void fbdev_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    if (!fb_mem || paused) { lv_disp_flush_ready(disp); return; }

    uint16_t *fb = (uint16_t *)fb_mem;
    uint32_t line_len = finfo.line_length / 2;   /* pixels per line */
    lv_coord_t w = area->x2 - area->x1 + 1;

    for (lv_coord_t y = area->y1; y <= area->y2; y++) {
        memcpy(&fb[y * line_len + area->x1], color_p, w * sizeof(uint16_t));
        color_p += w;
    }

    lv_disp_flush_ready(disp);
}
