#include "icons.h"
#include "ui_theme.h"
#include <math.h>
#include <string.h>

/* 32x32, LV_IMG_CF_TRUE_COLOR_ALPHA: at 16bpp each pixel is
 * [color low byte][color high byte][alpha]. Drawn once at startup with
 * simple signed-distance shapes (soft 1px edge for anti-aliasing). */
#define ICON_W 32
#define ICON_H 32
#define PX_BYTES 3

static uint8_t map_audio  [ICON_W * ICON_H * PX_BYTES];
static uint8_t map_emotion[ICON_W * ICON_H * PX_BYTES];
static uint8_t map_music  [ICON_W * ICON_H * PX_BYTES];

#define ICON_DSC(m) {                                   \
    .header = { .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,       \
                .always_zero = 0, .reserved = 0,        \
                .w = ICON_W, .h = ICON_H },             \
    .data_size = sizeof(m),                             \
    .data = (m),                                        \
}

lv_img_dsc_t img_audio   = ICON_DSC(map_audio);
lv_img_dsc_t img_emotion = ICON_DSC(map_emotion);
lv_img_dsc_t img_music   = ICON_DSC(map_music);

/* Blend a coverage value into the map (keep the max alpha at overlaps). */
static void put_px(uint8_t *map, int x, int y, lv_color_t c, float cov)
{
    if (x < 0 || x >= ICON_W || y < 0 || y >= ICON_H) return;
    if (cov <= 0.0f) return;
    if (cov > 1.0f) cov = 1.0f;
    uint8_t *p = map + (y * ICON_W + x) * PX_BYTES;
    uint8_t a = (uint8_t)(cov * 255.0f + 0.5f);
    if (a <= p[2]) return;
    p[0] = c.full & 0xFF;
    p[1] = (c.full >> 8) & 0xFF;
    p[2] = a;
}

/* coverage helpers: 1 inside, 0 outside, ~1px soft edge */
static float cov_edge(float d)              /* d = signed dist, <0 inside */
{
    return 0.5f - d;                        /* clamped by put_px */
}

/* filled circle */
static void fill_circle(uint8_t *m, lv_color_t c, float cx, float cy, float r)
{
    for (int y = 0; y < ICON_H; y++)
        for (int x = 0; x < ICON_W; x++) {
            float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r;
            put_px(m, x, y, c, cov_edge(d));
        }
}

/* circle outline (ring), thickness t */
static void ring(uint8_t *m, lv_color_t c, float cx, float cy, float r, float t)
{
    for (int y = 0; y < ICON_H; y++)
        for (int x = 0; x < ICON_W; x++) {
            float d = fabsf(sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r) - t * 0.5f;
            put_px(m, x, y, c, cov_edge(d));
        }
}

/* arc of a ring, limited to angle range [a0,a1] (degrees, 0=+x, CCW) */
static void arc(uint8_t *m, lv_color_t c, float cx, float cy, float r, float t,
                float a0, float a1)
{
    for (int y = 0; y < ICON_H; y++)
        for (int x = 0; x < ICON_W; x++) {
            float ang = atan2f(y - cy, x - cx) * 57.29578f;
            if (ang < a0 || ang > a1) continue;
            float d = fabsf(sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r) - t * 0.5f;
            put_px(m, x, y, c, cov_edge(d));
        }
}

/* axis-aligned filled rectangle (float edges for AA) */
static void rect(uint8_t *m, lv_color_t c, float x0, float y0, float x1, float y1)
{
    for (int y = 0; y < ICON_H; y++)
        for (int x = 0; x < ICON_W; x++) {
            float dx = fmaxf(x0 - x, x - x1);
            float dy = fmaxf(y0 - y, y - y1);
            float d = fmaxf(dx, dy);
            put_px(m, x, y, c, cov_edge(d));
        }
}

/* thick line segment from (x0,y0) to (x1,y1), width w */
static void seg(uint8_t *m, lv_color_t c, float x0, float y0, float x1, float y1, float w)
{
    float vx = x1 - x0, vy = y1 - y0;
    float len2 = vx * vx + vy * vy;
    for (int y = 0; y < ICON_H; y++)
        for (int x = 0; x < ICON_W; x++) {
            float t = len2 > 0 ? ((x - x0) * vx + (y - y0) * vy) / len2 : 0;
            if (t < 0) t = 0; if (t > 1) t = 1;
            float px = x0 + t * vx, py = y0 + t * vy;
            float d = sqrtf((x - px) * (x - px) + (y - py) * (y - py)) - w * 0.5f;
            put_px(m, x, y, c, cov_edge(d));
        }
}

/* ---- icon painters ---- */

/* speaker box + cone + two sound-wave arcs */
static void paint_audio(void)
{
    lv_color_t c = COL_ACCENT;
    rect(map_audio, c, 3, 12, 8, 20);                       /* box       */
    seg(map_audio, c, 8, 16, 15, 7, 2.0f);                  /* cone top  */
    seg(map_audio, c, 8, 16, 15, 25, 2.0f);                 /* cone bot  */
    seg(map_audio, c, 15, 7, 15, 25, 2.0f);                 /* cone back */
    arc(map_audio, c, 15, 16, 8,  2.2f, -55, 55);           /* wave 1    */
    arc(map_audio, c, 15, 16, 13, 2.2f, -45, 45);           /* wave 2    */
}

/* smiley: face ring, two eyes, smile arc */
static void paint_emotion(void)
{
    lv_color_t c = COL_WARN;
    ring(map_emotion, c, 16, 16, 13, 2.4f);                 /* face      */
    fill_circle(map_emotion, c, 11, 12.5f, 2.0f);           /* left eye  */
    fill_circle(map_emotion, c, 21, 12.5f, 2.0f);           /* right eye */
    arc(map_emotion, c, 16, 14, 8.5f, 2.4f, 25, 155);       /* smile     */
}

/* beamed pair of eighth notes */
static void paint_music(void)
{
    lv_color_t c = COL_GOOD;
    fill_circle(map_music, c, 9.5f, 24.5f, 3.6f);           /* head L    */
    fill_circle(map_music, c, 23.5f, 22.5f, 3.6f);          /* head R    */
    seg(map_music, c, 12.5f, 24, 12.5f, 7, 2.0f);           /* stem L    */
    seg(map_music, c, 26.5f, 22, 26.5f, 5, 2.0f);           /* stem R    */
    seg(map_music, c, 12.5f, 7, 26.5f, 5, 3.0f);            /* beam      */
}

void icons_init(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    memset(map_audio,   0, sizeof map_audio);
    memset(map_emotion, 0, sizeof map_emotion);
    memset(map_music,   0, sizeof map_music);
    paint_audio();
    paint_emotion();
    paint_music();
}
