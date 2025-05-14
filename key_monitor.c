#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

// 全局变量
#define VOLUME_MIN 0
#define VOLUME_MAX 63
#define VOLUME_STEP 1
#define LONG_PRESS_TIME 500000  // 长按判定时间(微秒)
#define BATTERY_CHECK_INTERVAL 5  // 电池检查间隔(秒)
#define VOLUME_REPEAT_INTERVAL 100000  // 音量调节重复间隔(微秒)

static int current_volume = 0;
static int power_key_pressed = 0;
static int charging_status = 0;
static time_t last_activity_time = 0;
static int animation_pid = -1;
static int volume_key_pressed = 0;  // 音量键按下状态
static struct timespec volume_press_time;  // 音量键按下时间
static int volume_key_code = 0;  // 当前按下的音量键代码

// 播放动画
void play_animation(const char *animation_name) {
    if (animation_pid > 0) {
        kill(animation_pid, SIGTERM);
        waitpid(animation_pid, NULL, 0);
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        execlp("play_bmp_sequence", "play_bmp_sequence", "-d", "100", animation_name, NULL);
        exit(1);
    } else if (pid > 0) {
        animation_pid = pid;
    }
}

// 显示电池信息
void show_battery_info() {
    char status[32];
    char capacity[32];
    FILE *fp;
    
    // 读取充电状态
    fp = fopen("/sys/class/power_supply/axp20x-battery/status", "r");
    if (fp) {
        fgets(status, sizeof(status), fp);
        fclose(fp);
    }
    
    // 读取电量
    fp = fopen("/sys/class/power_supply/axp20x-battery/capacity", "r");
    if (fp) {
        fgets(capacity, sizeof(capacity), fp);
        fclose(fp);
    }
    
    // 调用show_text显示
    pid_t pid = fork();
    if (pid == 0) {
        char text[128];
        snprintf(text, sizeof(text), "Battery: %s%% (%s)", capacity, status);
        execlp("show_text", "show_text", text, "24", "0xFFFF", "1", "1", NULL);
        exit(0);
    }
}

// 更新音量
void update_volume(int change) {
    char cmd[128];
    int new_volume = current_volume + change;
    
    // 边界检查
    if (new_volume < VOLUME_MIN) new_volume = VOLUME_MIN;
    if (new_volume > VOLUME_MAX) new_volume = VOLUME_MAX;
    
    if (new_volume != current_volume) {
        snprintf(cmd, sizeof(cmd), "amixer set 'Power Amplifier' %d", new_volume);
        system(cmd);
        current_volume = new_volume;
    }
}

// 检查电池状态
void check_battery_status() {
    static time_t last_check = 0;
    time_t now = time(NULL);
    
    if (now - last_check >= BATTERY_CHECK_INTERVAL) {
        FILE *fp = fopen("/sys/class/power_supply/axp20x-battery/status", "r");
        if (fp) {
            char status[32];
            fgets(status, sizeof(status), fp);
            fclose(fp);
            
            int new_status = strstr(status, "charging") != NULL;
            if (new_status != charging_status) {
                charging_status = new_status;
                if (charging_status) {
                    play_animation("charging");
                }
            }
        }
        last_check = now;
    }
}

// 处理随机动画
void handle_random_animation() {
    static time_t last_animation_time = 0;
    time_t now = time(NULL);
    
    if (now - last_activity_time > 10 && now - last_animation_time > 30) {
        int random_num = rand() % 5 + 1;  // 假设有5个emotion动画
        char animation_name[32];
        snprintf(animation_name, sizeof(animation_name), "emotion%d", random_num);
        play_animation(animation_name);
        last_animation_time = now;
    }
}

// 处理音量键长按
void handle_volume_long_press() {
    if (volume_key_pressed) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - volume_press_time.tv_sec) * 1000000 + 
                      (now.tv_nsec - volume_press_time.tv_nsec) / 1000;
        
        if (elapsed >= VOLUME_REPEAT_INTERVAL) {
            // 根据按键代码调节音量
            if (volume_key_code == KEY_VOLUMEUP) {
                update_volume(VOLUME_STEP);
            } else if (volume_key_code == KEY_VOLUMEDOWN) {
                update_volume(-VOLUME_STEP);
            }
            volume_press_time = now;  // 更新时间，准备下一次调节
        }
    }
}

// 添加调试函数
void print_key_event(const char* device, const struct input_event *ev) {
    printf("Device: %s, Type: %d, Code: %d, Value: %d\n", 
           device, ev->type, ev->code, ev->value);
}

// 清理函数
void cleanup(int signum) {
    if (animation_pid > 0) {
        kill(animation_pid, SIGTERM);
        waitpid(animation_pid, NULL, 0);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    int fd0, fd1;
    struct input_event ev;
    struct pollfd fds[2];
    char *device0 = "/dev/input/event0";
    char *device1 = "/dev/input/event1";
    struct timespec press_time;
    
    // 设置信号处理
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    
    // 初始化随机数生成器
    srand(time(NULL));
    
    // 打开输入设备0
    fd0 = open(device0, O_RDONLY);
    if (fd0 == -1) {
        printf("无法打开输入设备 %s\n", device0);
        return -1;
    }
    printf("成功打开设备 %s\n", device0);
    
    // 打开输入设备1
    fd1 = open(device1, O_RDONLY);
    if (fd1 == -1) {
        printf("无法打开输入设备 %s\n", device1);
        close(fd0);
        return -1;
    }
    printf("成功打开设备 %s\n", device1);
    
    // 设置poll结构
    fds[0].fd = fd0;
    fds[0].events = POLLIN;
    fds[1].fd = fd1;
    fds[1].events = POLLIN;
    
    // 播放开机动画
    play_animation("boot");
    
    printf("开始监控按键事件...\n");
    
    // 循环读取输入事件
    while (1) {
        // 检查电池状态
        check_battery_status();
        
        // 处理随机动画
        handle_random_animation();
        
        // 处理音量键长按
        handle_volume_long_press();
        
        // 等待事件
        int ret = poll(fds, 2, 1000);  // 1秒超时
        if (ret > 0) {
            printf("收到事件，ret = %d\n", ret);
            
            // 检查event0
            if (fds[0].revents & POLLIN) {
                printf("event0 有事件\n");
                if (read(fd0, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
                    print_key_event("event0", &ev);
                    if (ev.type == EV_KEY) {
                        last_activity_time = time(NULL);
                        
                        if (ev.code == KEY_POWER) {
                            printf("检测到电源键事件，value = %d\n", ev.value);
                            if (ev.value == 1) {  // 按下
                                power_key_pressed = 1;
                                clock_gettime(CLOCK_MONOTONIC, &press_time);
                                // 立即显示电池信息
                                show_battery_info();
                            } else if (ev.value == 0) {  // 释放
                                power_key_pressed = 0;
                                system("pkill show_text");
                            }
                        } else if (ev.code == KEY_VOLUMEUP) {
                            if (ev.value == 1) {  // 按下
                                volume_key_pressed = 1;
                                volume_key_code = KEY_VOLUMEUP;
                                clock_gettime(CLOCK_MONOTONIC, &volume_press_time);
                                update_volume(VOLUME_STEP);
                            } else if (ev.value == 0) {  // 释放
                                volume_key_pressed = 0;
                            }
                        } else if (ev.code == KEY_VOLUMEDOWN) {
                            if (ev.value == 1) {  // 按下
                                volume_key_pressed = 1;
                                volume_key_code = KEY_VOLUMEDOWN;
                                clock_gettime(CLOCK_MONOTONIC, &volume_press_time);
                                update_volume(-VOLUME_STEP);
                            } else if (ev.value == 0) {  // 释放
                                volume_key_pressed = 0;
                            }
                        }
                    }
                }
            }
            
            // 检查event1
            if (fds[1].revents & POLLIN) {
                printf("event1 有事件\n");
                if (read(fd1, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
                    print_key_event("event1", &ev);
                    if (ev.type == EV_KEY) {
                        last_activity_time = time(NULL);
                        
                        if (ev.code == KEY_POWER) {
                            printf("检测到电源键事件，value = %d\n", ev.value);
                            if (ev.value == 1) {  // 按下
                                power_key_pressed = 1;
                                clock_gettime(CLOCK_MONOTONIC, &press_time);
                                // 立即显示电池信息
                                show_battery_info();
                            } else if (ev.value == 0) {  // 释放
                                power_key_pressed = 0;
                                system("pkill show_text");
                            }
                        } else if (ev.code == KEY_VOLUMEUP) {
                            if (ev.value == 1) {  // 按下
                                volume_key_pressed = 1;
                                volume_key_code = KEY_VOLUMEUP;
                                clock_gettime(CLOCK_MONOTONIC, &volume_press_time);
                                update_volume(VOLUME_STEP);
                            } else if (ev.value == 0) {  // 释放
                                volume_key_pressed = 0;
                            }
                        } else if (ev.code == KEY_VOLUMEDOWN) {
                            if (ev.value == 1) {  // 按下
                                volume_key_pressed = 1;
                                volume_key_code = KEY_VOLUMEDOWN;
                                clock_gettime(CLOCK_MONOTONIC, &volume_press_time);
                                update_volume(-VOLUME_STEP);
                            } else if (ev.value == 0) {  // 释放
                                volume_key_pressed = 0;
                            }
                        }
                    }
                }
            }
        }
        
        // 处理电源键长按事件
        if (power_key_pressed) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed = (now.tv_sec - press_time.tv_sec) * 1000000 + 
                          (now.tv_nsec - press_time.tv_nsec) / 1000;
            
            if (elapsed >= LONG_PRESS_TIME) {
                printf("电源键长按触发\n");
                show_battery_info();
            }
        }
    }
    
    // 清理资源
    cleanup(0);
    return 0;
}