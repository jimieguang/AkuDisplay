#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void print_usage(const char* program_name) {
    printf("Usage: %s [-d delay_ms] [-l] <directory>\n", program_name);
    printf("Options:\n");
    printf("  -d, --delay  Delay between frames in milliseconds (default: 100)\n");
    printf("  -l, --loop   Play animation once (default: infinite loop)\n");
    printf("Example:\n");
    printf("  %s -d 200 -l bmp_sequence\n", program_name);
}

int compare_filenames(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int main(int argc, char *argv[]) {
    int delay_ms = 100;  // 默认帧延迟
    int loop_once = 0;   // 默认无限循环
    int opt;
    char* directory = NULL;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"delay", required_argument, 0, 'd'},
        {"loop", no_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "d:l", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                delay_ms = atoi(optarg);
                if (delay_ms <= 0) {
                    fprintf(stderr, "Invalid delay value. Must be positive.\n");
                    return 1;
                }
                break;
            case 'l':
                loop_once = 1;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // 获取目录参数
    if (optind < argc) {
        directory = argv[optind];
    } else {
        print_usage(argv[0]);
        return 1;
    }

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

    int fb_width = vinfo.xres;
    int fb_height = vinfo.yres;
    int bpp = vinfo.bits_per_pixel;
    int line_length = finfo.line_length;

    printf("Screen resolution: %dx%d\n", fb_width, fb_height);
    printf("Bits per pixel: %d\n", bpp);
    printf("Line length: %d\n", line_length);

    // 映射帧缓冲到内存
    size_t framebuffer_size = fb_height * line_length;
    printf("Framebuffer size: %zu bytes\n", framebuffer_size);
    
    // 创建双缓冲
    unsigned char* back_buffer = malloc(framebuffer_size);
    if (!back_buffer) {
        perror("Error allocating back buffer");
        close(fb);
        return 1;
    }
    
    unsigned char* front_buffer = mmap(NULL, framebuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (front_buffer == MAP_FAILED) {
        perror("Error mapping framebuffer");
        free(back_buffer);
        close(fb);
        return 1;
    }

    // 扫描目录中的BMP文件
    DIR *dir;
    struct dirent *ent;
    char **bmp_files = NULL;
    int num_files = 0;
    int max_files = 1000;  // 假设最多1000个文件

    bmp_files = malloc(max_files * sizeof(char*));
    if (!bmp_files) {
        perror("Error allocating memory for file list");
        free(back_buffer);
        munmap(front_buffer, framebuffer_size);
        close(fb);
        return 1;
    }

    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        printf("Directory: %s\n", directory);
        free(bmp_files);
        free(back_buffer);
        munmap(front_buffer, framebuffer_size);
        close(fb);
        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (strstr(ent->d_name, ".bmp") != NULL) {
            if (num_files >= max_files) {
                fprintf(stderr, "Too many BMP files in directory\n");
                break;
            }
            bmp_files[num_files] = malloc(strlen(directory) + strlen(ent->d_name) + 2);
            if (!bmp_files[num_files]) {
                perror("Error allocating memory for filename");
                break;
            }
            sprintf(bmp_files[num_files], "%s/%s", directory, ent->d_name);
            num_files++;
        }
    }
    closedir(dir);

    if (num_files == 0) {
        fprintf(stderr, "No BMP files found in directory\n");
        for (int i = 0; i < num_files; i++) {
            free(bmp_files[i]);
        }
        free(bmp_files);
        free(back_buffer);
        munmap(front_buffer, framebuffer_size);
        close(fb);
        return 1;
    }

    // 对文件名进行排序
    qsort(bmp_files, num_files, sizeof(char*), compare_filenames);

    printf("Found %d BMP files\n", num_files);

    // 预计算背景色（黑色）
    unsigned short black_color = 0;
    unsigned short* back_buffer_16 = (unsigned short*)back_buffer;
    for (int i = 0; i < framebuffer_size / 2; i++) {
        back_buffer_16[i] = black_color;
    }

    printf("Animation started. Press Ctrl+C to exit...\n");

    // 动画循环
    do {
        for (int frame = 0; frame < num_files; frame++) {
            int img_width, img_height, img_channels;
            unsigned char* img_data = stbi_load(bmp_files[frame], &img_width, &img_height, &img_channels, 3);
            
            if (!img_data) {
                printf("Error loading image %s: %s\n", bmp_files[frame], stbi_failure_reason());
                continue;
            }

            // 计算显示位置（居中）
            int offset_x = (fb_width - img_width) / 2;
            int offset_y = (fb_height - img_height) / 2;

            // 在后缓冲上绘制当前帧
            for (int y = 0; y < img_height; y++) {
                for (int x = 0; x < img_width; x++) {
                    int src_pos = (y * img_width + x) * 3;
                    unsigned char r = img_data[src_pos];
                    unsigned char g = img_data[src_pos+1];
                    unsigned char b = img_data[src_pos+2];

                    int fb_x = x + offset_x;
                    int fb_y = y + offset_y;
                    
                    if (fb_x >= 0 && fb_x < fb_width && fb_y >= 0 && fb_y < fb_height) {
                        int pixel_offset = fb_y * line_length + fb_x * (bpp/8);
                        if (pixel_offset + 1 < framebuffer_size) {
                            unsigned short color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                            back_buffer_16[pixel_offset/2] = color;
                        }
                    }
                }
            }

            // 使用memcpy逐行更新前缓冲，减少画面割裂
            for (int y = 0; y < fb_height; y++) {
                memcpy(front_buffer + y * line_length,
                       back_buffer + y * line_length,
                       line_length);
            }

            stbi_image_free(img_data);
            usleep(delay_ms * 1000);
        }
    } while (!loop_once);  // 根据参数决定是否循环

    // 清理资源
    for (int i = 0; i < num_files; i++) {
        free(bmp_files[i]);
    }
    free(bmp_files);
    free(back_buffer);
    munmap(front_buffer, framebuffer_size);
    close(fb);

    return 0;
} 