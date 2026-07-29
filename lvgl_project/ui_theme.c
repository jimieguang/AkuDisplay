#include "ui_theme.h"

#if LV_USE_FREETYPE
#include "lvgl/src/extra/libs/freetype/lv_freetype.h"
#endif

/* App-owned copy of a CJK-capable TTF (see OPS.md / HANDOFF.md). Kept inside
 * /opt/aku/lvgl so the UI does not depend on other projects' font files. */
#define CJK_FONT_PATH "/opt/aku/lvgl/font_cjk.ttf"
#define CJK_FONT_PX   15   /* music track line */

#define DIGITAL_FONT_PATH "/opt/aku/lvgl/font_digital.ttf"
#define DIGITAL_FONT_PX   32   /* large clock digits */

#define HARMONY_PATH "/opt/aku/lvgl/font_cjk.ttf"  /* HarmonyOS Sans SC */

/* Static styles, initialised once in ui_theme_init(). */
static lv_style_t st_page, st_bar_bg, st_bar_ind, st_mbtn, st_mbtn_sel;
static int inited = 0;

static const lv_font_t *cjk_font = NULL;
static const lv_font_t *digital_font = NULL;

/* Runtime font pointers (replaces Montserrat bitmap fonts) */
const lv_font_t *F_TITLE = &lv_font_montserrat_28;  /* fallbacks */
const lv_font_t *F_H2    = &lv_font_montserrat_20;
const lv_font_t *F_NUM   = &lv_font_montserrat_16;
const lv_font_t *F_BODY  = &lv_font_montserrat_14;
const lv_font_t *F_HINT  = &lv_font_montserrat_12;

#if LV_USE_FREETYPE
static lv_ft_info_t ft_title, ft_h2, ft_num, ft_body, ft_hint;
static lv_ft_info_t ft_digital, ft_cjk;
#endif

void ui_fonts_init(void)
{
#if LV_USE_FREETYPE
    if (!lv_freetype_init(8, 8, LV_FREETYPE_CACHE_SIZE)) {
        LV_LOG_WARN("ui_fonts_init: lv_freetype_init failed");
        return;
    }

    /* Load HarmonyOS Sans SC at 5 sizes for general UI text
     * Sizes are +2px vs old Montserrat to match visual weight
     * (HarmonyOS has smaller x-height at same pixel size). */
    ft_title.name = HARMONY_PATH; ft_title.mem = NULL;
    ft_title.weight = 28; ft_title.style = FT_FONT_STYLE_NORMAL; ft_title.font = NULL;
    if (lv_ft_font_init(&ft_title) && ft_title.font) F_TITLE = ft_title.font;

    ft_h2.name = HARMONY_PATH; ft_h2.mem = NULL;
    ft_h2.weight = 20; ft_h2.style = FT_FONT_STYLE_NORMAL; ft_h2.font = NULL;
    if (lv_ft_font_init(&ft_h2) && ft_h2.font) F_H2 = ft_h2.font;

    ft_num.name = HARMONY_PATH; ft_num.mem = NULL;
    ft_num.weight = 16; ft_num.style = FT_FONT_STYLE_NORMAL; ft_num.font = NULL;
    if (lv_ft_font_init(&ft_num) && ft_num.font) F_NUM = ft_num.font;

    ft_body.name = HARMONY_PATH; ft_body.mem = NULL;
    ft_body.weight = 14; ft_body.style = FT_FONT_STYLE_NORMAL; ft_body.font = NULL;
    if (lv_ft_font_init(&ft_body) && ft_body.font) F_BODY = ft_body.font;

    ft_hint.name = HARMONY_PATH; ft_hint.mem = NULL;
    ft_hint.weight = 12; ft_hint.style = FT_FONT_STYLE_NORMAL; ft_hint.font = NULL;
    if (lv_ft_font_init(&ft_hint) && ft_hint.font) F_HINT = ft_hint.font;

    /* Digital clock font (wwDigital) */
    ft_digital.name   = DIGITAL_FONT_PATH;
    ft_digital.mem    = NULL;
    ft_digital.weight = DIGITAL_FONT_PX;
    ft_digital.style  = FT_FONT_STYLE_NORMAL;
    ft_digital.font   = NULL;
    if (lv_ft_font_init(&ft_digital) && ft_digital.font)
        digital_font = ft_digital.font;

    /* CJK font for music track (same HarmonyOS, 15px) */
    cjk_font = F_NUM;  /* reuse the 15px HarmonyOS load */

    /* The HarmonyOS TTF has no LV_SYMBOL_* (FontAwesome) glyphs; without a
     * fallback those chars render as an invisible .notdef that still takes
     * advance width, hiding the icons and skewing label centring (menu
     * items, charge bolt). Chain the built-in Montserrat fonts instead. */
    if (ft_title.font)   ft_title.font->fallback   = &lv_font_montserrat_28;
    if (ft_h2.font)      ft_h2.font->fallback      = &lv_font_montserrat_20;
    if (ft_num.font)     ft_num.font->fallback     = &lv_font_montserrat_16;
    if (ft_body.font)    ft_body.font->fallback    = &lv_font_montserrat_14;
    if (ft_hint.font)    ft_hint.font->fallback    = &lv_font_montserrat_12;
    if (ft_digital.font) ft_digital.font->fallback = &lv_font_montserrat_28;
#endif
}

const lv_font_t *ui_font_cjk(void)
{
    return cjk_font;
}

const lv_font_t *ui_font_digital(void)
{
    return digital_font;
}

void ui_theme_init(void)
{
    if (inited) return;
    inited = 1;

    /* screen background */
    lv_obj_t *sc = lv_scr_act();
    lv_obj_set_style_bg_color(sc, COL_BG, 0);
    lv_obj_set_style_bg_opa(sc, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(sc, 0, 0);
    lv_obj_set_style_border_width(sc, 0, 0);
    lv_obj_set_style_pad_gap(sc, 0, 0);

    /* page container: dark, no border, no padding */
    lv_style_init(&st_page);
    lv_style_set_bg_color(&st_page, COL_BG);
    lv_style_set_bg_opa(&st_page, LV_OPA_COVER);
    lv_style_set_border_width(&st_page, 0);
    lv_style_set_pad_all(&st_page, 0);
    lv_style_set_radius(&st_page, 0);

    /* progress bar */
    lv_style_init(&st_bar_bg);
    lv_style_set_bg_color(&st_bar_bg, COL_DIM);
    lv_style_set_bg_opa(&st_bar_bg, LV_OPA_COVER);
    lv_style_set_radius(&st_bar_bg, 3);
    lv_style_set_border_width(&st_bar_bg, 0);

    lv_style_init(&st_bar_ind);
    lv_style_set_bg_color(&st_bar_ind, COL_ACCENT);
    lv_style_set_bg_opa(&st_bar_ind, LV_OPA_COVER);
    lv_style_set_radius(&st_bar_ind, 3);

    /* menu buttons */
    lv_style_init(&st_mbtn);
    lv_style_set_bg_color(&st_mbtn, COL_PANEL);
    lv_style_set_bg_opa(&st_mbtn, LV_OPA_COVER);
    lv_style_set_radius(&st_mbtn, 3);
    lv_style_set_border_width(&st_mbtn, 0);
    lv_style_set_text_color(&st_mbtn, COL_TEXT);
    lv_style_set_pad_ver(&st_mbtn, 2);

    lv_style_init(&st_mbtn_sel);
    lv_style_set_bg_color(&st_mbtn_sel, COL_ACCENT);
    lv_style_set_bg_opa(&st_mbtn_sel, LV_OPA_COVER);
    lv_style_set_text_color(&st_mbtn_sel, lv_color_white());
}

lv_style_t *ui_style_page(void)       { return &st_page; }
lv_style_t *ui_style_bar_bg(void)     { return &st_bar_bg; }
lv_style_t *ui_style_bar_ind(void)    { return &st_bar_ind; }
lv_style_t *ui_style_menu_btn(void)   { return &st_mbtn; }
lv_style_t *ui_style_menu_btn_sel(void) { return &st_mbtn_sel; }

lv_obj_t *ui_make_label(lv_obj_t *parent, const char *txt,
                        const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}
