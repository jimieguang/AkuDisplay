#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <locale.h>
#include <wchar.h>

// 帧缓冲设备信息
static int fb_fd;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static char *fbp = NULL;
static long int screensize;

// FreeType相关变量
static FT_Library library;
static FT_Face face;
static int font_size = 24;

// 定义对齐方式
#define ALIGN_LEFT   0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT  2
#define ALIGN_TOP    0
#define ALIGN_MIDDLE 1
#define ALIGN_BOTTOM 2

// 初始化帧缓冲
void fb_init(void) {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening framebuffer device");
        exit(1);
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error reading fixed information");
        close(fb_fd);
        exit(1);
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable information");
        close(fb_fd);
        exit(1);
    }

    // printf("Screen info: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    // printf("Red: offset=%d, length=%d\n", vinfo.red.offset, vinfo.red.length);
    // printf("Green: offset=%d, length=%d\n", vinfo.green.offset, vinfo.green.length);
    // printf("Blue: offset=%d, length=%d\n", vinfo.blue.offset, vinfo.blue.length);

    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((int)fbp == -1) {
        perror("Error mapping framebuffer device to memory");
        close(fb_fd);
        exit(1);
    }
}

// 初始化FreeType
void ft_init(const char *font_path) {
    FT_Error error;

    error = FT_Init_FreeType(&library);
    if (error) {
        fprintf(stderr, "Could not initialize FreeType library\n");
        exit(1);
    }

    error = FT_New_Face(library, font_path, 0, &face);
    if (error) {
        fprintf(stderr, "Could not open font file: %s\n", font_path);
        FT_Done_FreeType(library);
        exit(1);
    }

    error = FT_Set_Pixel_Sizes(face, 0, font_size);
    if (error) {
        fprintf(stderr, "Could not set font size\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        exit(1);
    }

    // printf("Font loaded: %s\n", font_path);
    // printf("Font size: %d\n", font_size);
}

// 清屏
void clear_screen(void) {
    memset(fbp, 0, screensize);
}

// 绘制一个像素
void draw_pixel(int x, int y, unsigned short color) {
    if (x >= 0 && x < vinfo.xres && y >= 0 && y < vinfo.yres) {
        long int location = (y * vinfo.xres + x) * (vinfo.bits_per_pixel / 8);
        if (location >= 0 && location < screensize) {
            *((unsigned short*)(fbp + location)) = color;
        }
    }
}

// 绘制一个字符
void draw_char(int x, int y, wchar_t c, unsigned short color) {
    FT_Error error;
    FT_GlyphSlot slot = face->glyph;

    error = FT_Load_Char(face, c, FT_LOAD_RENDER);
    if (error) {
        printf("Error loading character: %lc (0x%x)\n", c, c);
        return;
    }
    // printf("Drawing char: %lc (0x%x), width=%d, height=%d, left=%d, top=%d\n",
    //        c, c, slot->bitmap.width, slot->bitmap.rows, 
    //        slot->bitmap_left, slot->bitmap_top);

    // 更严格的边界检查
    if (x < 0 || y < 0 || 
        x + slot->bitmap_left + slot->bitmap.width > vinfo.xres ||
        y - slot->bitmap_top + slot->bitmap.rows > vinfo.yres) {
        printf("Character out of bounds: x=%d, y=%d, width=%d, height=%d\n",
               x, y, slot->bitmap.width, slot->bitmap.rows);
        return;
    }

    // 修改像素绘制逻辑，添加更安全的边界检查
    for (int i = 0; i < slot->bitmap.rows; i++) {
        for (int j = 0; j < slot->bitmap.width; j++) {
            int pixel_x = x + j + slot->bitmap_left;
            int pixel_y = y + i - slot->bitmap_top;
            
            // 确保像素坐标在屏幕范围内
            if (pixel_x >= 0 && pixel_x < vinfo.xres && 
                pixel_y >= 0 && pixel_y < vinfo.yres) {
                unsigned char alpha = slot->bitmap.buffer[i * slot->bitmap.pitch + j];
                if (alpha > 0) {
                    draw_pixel(pixel_x, pixel_y, color);
                }
            }
        }
    }
}

// 绘制字符串
void draw_string(const wchar_t *str, unsigned short color, int h_align, int v_align) {
    int start_x = 0;
    int line_height = font_size + 2;
    
    // 计算文字总宽度和高度
    int max_width = 0;
    int current_width = 0;
    int lines = 1;
    const wchar_t *temp = str;
    int last_char_left = 0;
    
    while (*temp) {
        if (*temp == L'\n') {
            lines++;
            if (current_width > max_width) {
                max_width = current_width;
            }
            current_width = 0;
        } else {
            FT_Load_Char(face, *temp, FT_LOAD_RENDER);
            current_width += face->glyph->advance.x >> 6;
            last_char_left = face->glyph->bitmap_left;
        }
        temp++;
    }
    if (current_width > max_width) {
        max_width = current_width;
    }
    
    // 根据水平对齐方式调整起始x坐标
    switch (h_align) {
        case ALIGN_CENTER:
            start_x = (vinfo.xres - max_width) / 2;
            break;
        case ALIGN_RIGHT:
            // 考虑最后一个字符的bitmap_left，确保文字完全在屏幕内
            start_x = vinfo.xres - max_width - last_char_left;
            break;
        case ALIGN_LEFT:
        default:
            start_x = 0;  // 左对齐时从x=0开始
            break;
    }
    
    // 根据垂直对齐方式调整起始y坐标
    int total_height = lines * line_height;
    int y;
    switch (v_align) {
        case ALIGN_MIDDLE:
            // 考虑第一个字符的bitmap_top和顶部font_size偏移，确保文字垂直居中
            y = (vinfo.yres - total_height) / 2 + font_size/2;
            break;
        case ALIGN_BOTTOM:
            y = vinfo.yres - total_height;
            break;
        case ALIGN_TOP:
        default:
            y = font_size;  // 顶部对齐时从y=font_size开始
            break;
    }
    
    int x = start_x;
    
    while (*str) {
        if (*str == L'\n') {
            y += line_height;
            x = start_x;
        } else {
            if (y + line_height > vinfo.yres) {
                break;
            }
            
            draw_char(x, y, *str, color);
            x += face->glyph->advance.x >> 6;
            
            // 只有在左对齐时才进行自动换行
            if (h_align == ALIGN_LEFT && x > vinfo.xres - font_size) {
                y += line_height;
                x = start_x;
            }
        }
        str++;
    }
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <text> <font_size> <color> <h_align> <v_align>\n", argv[0]);
        fprintf(stderr, "Example: %s \"Hello World\" 24 0xFFFF 1 1\n", argv[0]);
        fprintf(stderr, "h_align: 0=left, 1=center, 2=right\n");
        fprintf(stderr, "v_align: 0=top, 1=middle, 2=bottom\n");
        return 1;
    }

    // 设置locale以支持中文
    setlocale(LC_ALL, "C.UTF-8");

    // 设置字体大小
    font_size = atoi(argv[2]);
    if (font_size < 8 || font_size > 72) {
        fprintf(stderr, "Font size must be between 8 and 72\n");
        return 1;
    }

    // 解析颜色参数
    unsigned short color;
    if (sscanf(argv[3], "0x%hx", &color) != 1) {
        fprintf(stderr, "Invalid color format. Use 0xRRRRRGGGGGGBBBBB (RGB565)\n");
        return 1;
    }

    // 解析对齐参数
    int h_align = atoi(argv[4]);
    int v_align = atoi(argv[5]);
    if (h_align < 0 || h_align > 2 || v_align < 0 || v_align > 2) {
        fprintf(stderr, "Alignment parameters must be 0, 1, or 2\n");
        return 1;
    }

    // 初始化帧缓冲
    fb_init();

    // 初始化FreeType
    ft_init("/home/aku/xiaozhi/font/HarmonyOS_Sans_SC_Regular.ttf");

    // 清屏
    clear_screen();

    // 转换输入字符串为宽字符
    wchar_t wtext[256];
    // printf("Input text: %s\n", argv[1]);
    // printf("Input length: %zu\n", strlen(argv[1]));
    
    size_t converted = mbstowcs(wtext, argv[1], 256);
    if (converted == (size_t)-1) {
        fprintf(stderr, "Error converting text to wide characters\n");
        perror("mbstowcs");
        // 尝试直接使用宽字符
        swprintf(wtext, 256, L"%s", argv[1]);
        printf("Using swprintf as fallback\n");
    } else {
        // printf("Converted %zu characters\n", converted);
    }

    // 显示用户输入的文字，使用指定的颜色和对齐方式
    draw_string(wtext, color, h_align, v_align);

    // 清理资源
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    munmap(fbp, screensize);
    close(fb_fd);

    return 0;
} 