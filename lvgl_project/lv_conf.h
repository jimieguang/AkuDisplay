#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH     16
#define LV_HOR_RES_MAX     162
#define LV_VER_RES_MAX     132

#define LV_USE_PERF_MONITOR 0
#define LV_USE_LOG          1
#define LV_LOG_LEVEL        LV_LOG_LEVEL_WARN

#define LV_MEM_SIZE         (48U * 1024U)
#define LV_MEM_ADR          0

#define LV_TICK_CUSTOM      0

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1

/* Features */
#define LV_USE_GPU                 0
#define LV_USE_FILESYSTEM          0
#define LV_USE_LARGE_COORD         0

/* Widgets - only what the app uses (label/bar/btn + plain containers).
 * Everything else off: cuts device-side build time on the single-core ARM. */
#define LV_USE_ARC         0
#define LV_USE_BAR         1
#define LV_USE_BTN         1
#define LV_USE_BTNMATRIX   0
#define LV_USE_CANVAS      0
#define LV_USE_CHECKBOX    0
#define LV_USE_DROPDOWN    0
#define LV_USE_IMG         1
#define LV_USE_LABEL       1
#define LV_USE_LINE        0
#define LV_USE_ROLLER      0
#define LV_USE_SLIDER      0
#define LV_USE_SWITCH      0
#define LV_USE_TEXTAREA    0
#define LV_USE_TABLE       0
#define LV_USE_TABVIEW     0
#define LV_USE_WIN         0
#define LV_USE_SPINNER     0

/* Extra widgets (lvgl/src/extra/widgets) - all off */
#define LV_USE_ANIMIMG     0
#define LV_USE_CALENDAR    0
#define LV_USE_CHART       0
#define LV_USE_COLORWHEEL  0
#define LV_USE_IMGBTN      0
#define LV_USE_KEYBOARD    0
#define LV_USE_LED         0
#define LV_USE_LIST        0
#define LV_USE_MENU        0
#define LV_USE_METER       0
#define LV_USE_MSGBOX      0
#define LV_USE_SPAN        0
#define LV_USE_SPINBOX     0
#define LV_USE_TILEVIEW    0

#define LV_USE_CPU_USAGE   0

/* Extra layouts (flex used by the tab bar) */
#define LV_USE_FLEX        1
#define LV_USE_GRID        0

/* Extra themes */
#define LV_USE_THEME_DEFAULT    1
#define LV_USE_THEME_BASIC      0
#define LV_USE_THEME_MONO       0
#define LV_USE_MONKEY           0

/* Font usage */
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_28   1

#define LV_USE_FREETYPE         1
#if LV_USE_FREETYPE
    /* Memory used by FreeType to cache rendered glyphs [bytes].
     * Glyph bitmaps live in FreeType's own heap (system malloc), NOT LV_MEM_SIZE. */
    #define LV_FREETYPE_CACHE_SIZE (16 * 1024)
    #if LV_FREETYPE_CACHE_SIZE >= 0
        /* 1: bitmap cache uses sbit cache, 0: bitmap cache uses image cache */
        #define LV_FREETYPE_SBIT_CACHE 0
        /* Max number of opened FT_Face / FT_Size objects managed by the cache */
        #define LV_FREETYPE_CACHE_FT_FACES 4
        #define LV_FREETYPE_CACHE_FT_SIZES 4
    #endif
#endif

/* Input device read period (ms) */
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_INDEV_DEF_SCROLL_LIMIT 32

/* Draw buffer */
#define LV_DRAW_BUF_STRIDE_ALIGN 4

#endif
