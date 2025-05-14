#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>
#include <getopt.h>

// 定义 STB_IMAGE_IMPLEMENTATION 来包含完整的 stb_image 实现
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 旋转类型枚举
typedef enum {
    ROTATE_0 = 0,    // 不旋转
    ROTATE_90 = 90,  // 顺时针旋转90度
    ROTATE_180 = 180,// 旋转180度
    ROTATE_270 = 270 // 顺时针旋转270度
} Rotation;

void print_usage(const char* program_name) {
    printf("Usage: %s [-r rotation] <image_path>\n", program_name);
    printf("Options:\n");
    printf("  -r, --rotate  Rotation angle (0, 90, 180, or 270)\n");
    printf("Example:\n");
    printf("  %s -r 90 image.jpg\n", program_name);
}

// 根据旋转角度获取目标位置的像素
void get_rotated_pixel(int x, int y, int width, int height, Rotation rotation,
                      int *out_x, int *out_y) {
    switch (rotation) {
        case ROTATE_90:
            *out_x = height - 1 - y;
            *out_y = x;
            break;
        case ROTATE_180:
            *out_x = width - 1 - x;
            *out_y = height - 1 - y;
            break;
        case ROTATE_270:
            *out_x = y;
            *out_y = width - 1 - x;
            break;
        default: // ROTATE_0
            *out_x = x;
            *out_y = y;
            break;
    }
}

int main(int argc, char *argv[]) {
    Rotation rotation = ROTATE_0;
    int opt;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"rotate", required_argument, 0, 'r'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "r:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                rotation = atoi(optarg);
                if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
                    fprintf(stderr, "Invalid rotation angle. Must be 0, 90, 180, or 270.\n");
                    return 1;
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char* image_path = argv[optind];

    // 加载图像
    int img_width, img_height, img_channels;
    unsigned char *img_data = stbi_load(image_path, &img_width, &img_height, &img_channels, 3);
    if (!img_data) {
        printf("Error loading image: %s\n", stbi_failure_reason());
        return 1;
    }

    printf("Image loaded: %dx%d with %d channels\n", img_width, img_height, img_channels);

    // 打开帧缓冲设备
    int fb = open("/dev/fb0", O_RDWR);
    if (fb == -1) {
        perror("Error opening /dev/fb0");
        stbi_image_free(img_data);
        return 1;
    }

    // 获取屏幕信息
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable information");
        close(fb);
        stbi_image_free(img_data);
        return 1;
    }
    
    if (ioctl(fb, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error reading fixed information");
        close(fb);
        stbi_image_free(img_data);
        return 1;
    }

    int fb_width = vinfo.xres;
    int fb_height = vinfo.yres;
    int bpp = vinfo.bits_per_pixel;
    int line_length = finfo.line_length;

    printf("Screen resolution: %dx%d\n", fb_width, fb_height);
    printf("Bits per pixel: %d\n", bpp);
    printf("Red: offset=%d, length=%d\n", vinfo.red.offset, vinfo.red.length);
    printf("Green: offset=%d, length=%d\n", vinfo.green.offset, vinfo.green.length);
    printf("Blue: offset=%d, length=%d\n", vinfo.blue.offset, vinfo.blue.length);

    // 根据旋转角度调整目标尺寸
    int target_width = (rotation == ROTATE_90 || rotation == ROTATE_270) ? img_height : img_width;
    int target_height = (rotation == ROTATE_90 || rotation == ROTATE_270) ? img_width : img_height;

    // 计算缩放比例
    float scale_x = (float)fb_width / target_width;
    float scale_y = (float)fb_height / target_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    // 计算显示尺寸和偏移量
    int display_width = (int)(target_width * scale);
    int display_height = (int)(target_height * scale);
    int offset_x = (fb_width - display_width) / 2;
    int offset_y = (fb_height - display_height) / 2;

    // 映射帧缓冲到内存
    size_t framebuffer_size = fb_height * line_length;
    unsigned char* framebuffer = mmap(NULL, framebuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (framebuffer == MAP_FAILED) {
        perror("Error mapping framebuffer");
        close(fb);
        stbi_image_free(img_data);
        return 1;
    }

    // 清空屏幕（设置为黑色背景）
    memset(framebuffer, 0, framebuffer_size);

    // 显示图像
    for (int y = 0; y < display_height; y++) {
        for (int x = 0; x < display_width; x++) {
            // 计算源图像中的对应像素位置
            float src_x = x / scale;
            float src_y = y / scale;

            // 根据旋转角度获取实际的源像素位置
            int rotated_x, rotated_y;
            get_rotated_pixel((int)src_x, (int)src_y, img_width, img_height, rotation,
                            &rotated_x, &rotated_y);

            // 获取源像素颜色
            int src_pos = (rotated_y * img_width + rotated_x) * 3;
            unsigned char r = img_data[src_pos];
            unsigned char g = img_data[src_pos+1];
            unsigned char b = img_data[src_pos+2];

            // 计算目标帧缓冲区中的位置
            int fb_x = x + offset_x;
            int fb_y = y + offset_y;
            int pixel_offset = fb_y * line_length + fb_x * (bpp/8);

            // 将24位RGB转换为16位RGB565
            // R: 5位 (31/255 * r)
            // G: 6位 (63/255 * g)
            // B: 5位 (31/255 * b)
            unsigned short color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            *(unsigned short*)(framebuffer + pixel_offset) = color;
        }
    }

    printf("Image displayed successfully! (Rotation: %d degrees)\n", rotation);
    printf("Press Enter to exit...");
    getchar();

    // 清理资源
    stbi_image_free(img_data);
    munmap(framebuffer, framebuffer_size);
    close(fb);

    return 0;
} 