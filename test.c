#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>

// 颜色结构体
typedef struct {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} Color;

// 获取颜色值函数
Color get_color_from_name(const char* color_name) {
    Color color = {0, 0, 0};
    
    if (strcmp(color_name, "red") == 0) {
        color.red = 255;
    } else if (strcmp(color_name, "green") == 0) {
        color.green = 255;
    } else if (strcmp(color_name, "blue") == 0) {
        color.blue = 255;
    } else if (strcmp(color_name, "white") == 0) {
        color.red = 255;
        color.green = 255;
        color.blue = 255;
    }
    
    return color;
}

int main() {
    // 打开帧缓冲设备
    int fb = open("/dev/fb0", O_RDWR);
    if (fb == -1) {
        perror("Error opening /dev/fb0");
        return 1;
    }

    // 获取屏幕信息
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable information");
        close(fb);
        return 1;
    }
    
    if (ioctl(fb, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error reading fixed information");
        close(fb);
        return 1;
    }

    // 使用实际分辨率
    int fb_width = vinfo.xres;
    int fb_height = vinfo.yres;
    int bpp = vinfo.bits_per_pixel;
    int line_length = finfo.line_length;

    printf("Screen resolution: %dx%d\n", fb_width, fb_height);
    printf("Bits per pixel: %d\n", bpp);
    printf("Color format details:\n");
    printf("Red:   offset=%d, length=%d, msb_right=%d\n", vinfo.red.offset, vinfo.red.length, vinfo.red.msb_right);
    printf("Green: offset=%d, length=%d, msb_right=%d\n", vinfo.green.offset, vinfo.green.length, vinfo.green.msb_right);
    printf("Blue:  offset=%d, length=%d, msb_right=%d\n", vinfo.blue.offset, vinfo.blue.length, vinfo.blue.msb_right);

    // 映射帧缓冲到内存
    size_t framebuffer_size = fb_height * line_length;
    unsigned char* framebuffer = mmap(NULL, framebuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (framebuffer == MAP_FAILED) {
        perror("Error mapping framebuffer");
        close(fb);
        return 1;
    }

    // 清空屏幕（设置为黑色背景）
    memset(framebuffer, 0, framebuffer_size);

    // 测试红色
    printf("Testing RED...\n");
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < fb_width; x++) {
            int pixel_offset = y * line_length + x * (bpp/8);
            unsigned short color = (0x1F << 11); // R=31, G=0, B=0
            *(unsigned short*)(framebuffer + pixel_offset) = color;
        }
    }

    // 测试绿色
    printf("Testing GREEN...\n");
    for (int y = 40; y < 80; y++) {
        for (int x = 0; x < fb_width; x++) {
            int pixel_offset = y * line_length + x * (bpp/8);
            unsigned short color = (0x3F << 5); // R=0, G=63, B=0
            *(unsigned short*)(framebuffer + pixel_offset) = color;
        }
    }

    // 测试蓝色
    printf("Testing BLUE...\n");
    for (int y = 80; y < 120; y++) {
        for (int x = 0; x < fb_width; x++) {
            int pixel_offset = y * line_length + x * (bpp/8);
            unsigned short color = 0x1F; // R=0, G=0, B=31
            *(unsigned short*)(framebuffer + pixel_offset) = color;
        }
    }

    printf("Color test completed. Press Enter to exit...");
    getchar();

    // 清理资源
    munmap(framebuffer, framebuffer_size);
    close(fb);

    return 0;
} 