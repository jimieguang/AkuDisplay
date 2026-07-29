#ifndef UI_THEME_H
#define UI_THEME_H
#include <lvgl.h>

/* ======================================================================
 * ui_theme.h / ui_theme.c
 * Modern dark-minimal visual identity for the 162x132 display.
 *   background  #0d0d0d   text  #F2F4F8   accent (electric blue) #33B1FF
 *   panel #1a1a1a  dim #2A2A2A  sub #9AA7B4 (secondary)  muted #6B6B6B
 * ====================================================================== */

#define DISP_W 162
#define DISP_H 132
#define TAB_H  20                 /* bottom tab-bar height */
#define PAGE_H (DISP_H - TAB_H)   /* usable page area */

/* palette */
#define COL_BG       lv_color_hex(0x0d0d0d)
#define COL_PANEL    lv_color_hex(0x1c1f24)
#define COL_DIM      lv_color_hex(0x30343a)
#define COL_MUTED    lv_color_hex(0x707880)   /* low-priority hints only */
#define COL_SUB      lv_color_hex(0x9AA7B4)   /* secondary text: IP/date/labels */
#define COL_TEXT     lv_color_hex(0xF2F4F8)   /* primary text */
#define COL_ACCENT   lv_color_hex(0x33B1FF)   /* electric blue */
#define COL_GOOD     lv_color_hex(0x2ECC71)
#define COL_WARN     lv_color_hex(0xF2A94A)
#define COL_DANGER   lv_color_hex(0xFF5C5C)

/* font aliases — runtime-loaded FreeType fonts (HarmonyOS Sans SC).
 * Set in ui_fonts_init(); fall back to built-in Montserrat if load fails. */
extern const lv_font_t *F_TITLE;   /* 26px - page titles */
extern const lv_font_t *F_H2;      /* 18px - status */
extern const lv_font_t *F_NUM;     /* 15px - values */
extern const lv_font_t *F_BODY;    /* 13px - body text */
extern const lv_font_t *F_HINT;    /* 11px - smallest */

void ui_theme_init(void);   /* set screen bg + base styles, call once */

/* Load the CJK (Chinese) font via FreeType from an on-device TTF.
 * Call once after lv_init()/ui_theme_init(). Safe to call if FreeType or the
 * font file is unavailable: it just leaves ui_font_cjk() == NULL. */
void ui_fonts_init(void);

/* Runtime CJK font (e.g. for song titles). NULL if unavailable -> caller
 * should fall back to a montserrat F_* font. */
const lv_font_t *ui_font_cjk(void);

/* Digital clock font (wwDigital.ttf from the original sysboot time page).
 * NULL if unavailable -> caller falls back to F_TITLE. */
const lv_font_t *ui_font_digital(void);

/* Style accessors for shared, reusable styles. */
lv_style_t *ui_style_page(void);     /* transparent, no border/pad, dark bg */
lv_style_t *ui_style_bar_bg(void);
lv_style_t *ui_style_bar_ind(void);  /* accent indicator */
lv_style_t *ui_style_menu_btn(void);
lv_style_t *ui_style_menu_btn_sel(void);

/* Shared helper: create a label with given text, font and color.
 * Used by pages/apps/menu/state to avoid duplicating this boilerplate. */
lv_obj_t *ui_make_label(lv_obj_t *parent, const char *txt,
                        const lv_font_t *font, lv_color_t color);

#endif /* UI_THEME_H */
