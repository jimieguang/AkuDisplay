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
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <json-c/json.h>  // 添加JSON支持

// 函数声明
void play_animation(const char *animation_name, int loop_once, int delay);
void stop_animation(void);
void show_battery_info(void);
void update_volume(int change);
void check_battery_status(void);
void handle_random_animation(void);
void cleanup(int signum);
int get_current_volume(void);
void play_random_animation(const char *path);
void handle_idle_state(void);
void handle_key_event(int key_code, int value);
void execute_command(const char *command);
void load_script_config(void);
void cleanup_script_config(void);
void led_on(void);
void led_off(void);
void led_blink(void);
void display_text(const char *text);  // 添加新函数声明

// 全局变量
#define VOLUME_MIN 0
#define VOLUME_MAX 63
#define VOLUME_STEP 1
#define BATTERY_CHECK_INTERVAL 5  // 电池检查间隔(秒)
#define IDLE_TIME_THRESHOLD 5    // 空闲时间阈值(秒)
#define DOUBLE_CLICK_THRESHOLD 300  // 双击时间阈值(毫秒)
#define LONG_PRESS_THRESHOLD 800  // 长按时间阈值(毫秒)

static int current_volume = 0;
static int charging_status = 0;
static time_t last_activity_time = 0;
static int animation_pid = -1;
static int is_idle = 1;  // 是否处于空闲状态  1 为空闲 0 为非空闲
static int animation_enabled = 1;  // 是否允许播放动画 1 为允许 0 为禁止

// 为每个按键设置独立的长按计数器
static struct {
    int power_count;
    int vol_up_count;
    int vol_down_count;
} long_press_counters = {0, 0, 0};

// 配置文件路径
#define CONFIG_FILE "./key_config.json"

// 脚本配置结构
static struct {
    char *power_scripts[2];
    char *volup_scripts[2];
    char *voldown_scripts[2];
    int is_loaded;
} script_config = {
    .power_scripts = {NULL, NULL},
    .volup_scripts = {NULL, NULL},
    .voldown_scripts = {NULL, NULL},
    .is_loaded = 0
};

// 按键状态结构
static struct {
    int key_code;           // 当前按键
    int click_count;        // 点击次数
    int is_pressed;         // 按键是否处于按下状态
    struct timespec last_press_time;    // 最后一次按下时间
    struct timespec last_release_time;  // 最后一次释放时间
} key_state = {0, 0, 0, {0, 0}, {0, 0}};

// LED 控制相关定义
#define LED_PATH "/sys/class/leds/aku-logo"
#define LED_TRIGGER_PATH LED_PATH "/trigger"
#define LED_BRIGHTNESS_PATH LED_PATH "/brightness"

// LED 控制函数
void led_on(void) {
    FILE *fp = fopen(LED_BRIGHTNESS_PATH, "w");
    if (fp) {
        fprintf(fp, "1");
        fclose(fp);
    }
}

void led_off(void) {
    FILE *fp = fopen(LED_BRIGHTNESS_PATH, "w");
    if (fp) {
        fprintf(fp, "0");
        fclose(fp);
    }
}

// LED 闪烁函数
void led_blink(void) {
    // 先设置为 none 触发模式
    FILE *fp = fopen(LED_TRIGGER_PATH, "w");
    if (fp) {
        fprintf(fp, "none");
        fclose(fp);
    }
    
    // 闪烁一次
    led_off();
    usleep(100000);  // 100ms
    led_on();
}

// 加载配置文件
void load_script_config(void) {
    if (script_config.is_loaded) return;

    json_object *root = json_object_from_file(CONFIG_FILE);
    if (!root) {
        printf("无法加载配置文件: %s\n", CONFIG_FILE);
        return;
    }

    // 加载电源键配置
    json_object *power_obj;
    if (json_object_object_get_ex(root, "power", &power_obj)) {
        script_config.power_scripts[0] = strdup(json_object_get_string(json_object_array_get_idx(power_obj, 0)));
        script_config.power_scripts[1] = strdup(json_object_get_string(json_object_array_get_idx(power_obj, 1)));
    }

    // 加载音量加键配置
    json_object *volup_obj;
    if (json_object_object_get_ex(root, "volup", &volup_obj)) {
        script_config.volup_scripts[0] = strdup(json_object_get_string(json_object_array_get_idx(volup_obj, 0)));
        script_config.volup_scripts[1] = strdup(json_object_get_string(json_object_array_get_idx(volup_obj, 1)));
    }

    // 加载音量减键配置
    json_object *voldown_obj;
    if (json_object_object_get_ex(root, "voldown", &voldown_obj)) {
        script_config.voldown_scripts[0] = strdup(json_object_get_string(json_object_array_get_idx(voldown_obj, 0)));
        script_config.voldown_scripts[1] = strdup(json_object_get_string(json_object_array_get_idx(voldown_obj, 1)));
    }

    json_object_put(root);
    script_config.is_loaded = 1;
}

// 清理配置
void cleanup_script_config(void) {
    for (int i = 0; i < 2; i++) {
        free(script_config.power_scripts[i]);
        free(script_config.volup_scripts[i]);
        free(script_config.voldown_scripts[i]);
    }
}

// 执行命令函数
void execute_command(const char *command) {
    if (!command) return;
    
    // 闪烁LED指示命令开始执行
    led_blink();
    
    pid_t pid = fork();
    if (pid == 0) {
        // 重定向输出到/dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    }
    
    // 等待命令执行完成
    waitpid(pid, NULL, 0);
    
    // 再次闪烁LED指示命令执行完成
    led_blink();
}

// 显示文字的辅助函数
void display_text(const char *text) {
    if (!animation_enabled) return;  // 如果显示被禁用，直接返回
    
    pid_t pid = fork();
    if (pid == 0) {
        execl("./show_text", "./show_text", text, "24", "0xFFFF", "1", "1", NULL);
        exit(1);
    }
    waitpid(pid, NULL, 0);
}

// 处理实际的按键动作
void process_key_action(int key_code, int action_type) {
    if (!script_config.is_loaded) {
        load_script_config();
    }
    // 触发具体事件后，重置按键状态
    key_state.is_pressed = 0;
    key_state.click_count = 0;
    
    // action_type: 1=单击, 2=双击, 3=长按
    // printf("处理按键: %d, 动作类型: %d\n", key_code, action_type);
    
    switch (key_code) {
        case KEY_POWER:
            if (action_type == 1) {
                // 单击：显示电池信息
                show_battery_info();
            } 
            else if (action_type == 2) {
                // 停止当前动画
                stop_animation();
                // 双击：切换空闲动画状态
                animation_enabled = !animation_enabled;
                printf("显示状态: %s\n", animation_enabled ? "启用" : "禁用");
                
                pid_t status_pid = fork();
                if (status_pid == 0) {
                    char text[128];
                    snprintf(text, sizeof(text), "Animation: \n%s", 
                            animation_enabled ? "Enabled" : "Disabled");
                    execl("./show_text", "./show_text", text, "24", "0xFFFF", "1", "1", NULL);
                    exit(1);
                }
                waitpid(status_pid, NULL, 0);
                sleep(1);
                
                pid_t clear_pid = fork();
                if (clear_pid == 0) {
                    execl("./show_text", "./show_text", "", "24", "0xFFFF", "1", "1", NULL);
                    exit(1);
                }
            }
            else if (action_type == 3) {
                // 长按：根据计数器执行不同脚本
                long_press_counters.power_count++;
                int script_idx = (long_press_counters.power_count - 1) % 2;
                char *command = script_config.power_scripts[script_idx];
                // 根据脚本索引设置空闲动画状态
                if (script_idx == 0) {
                    stop_animation();
                    animation_enabled = 0;
                }else{
                    animation_enabled = 1;
                }
                if (command) {
                    printf("电源键长按 - 执行命令: %s\n", command);
                    execute_command(command);
                }
            }
            break;
            
        case KEY_VOLUMEUP:
            if (action_type == 1) {
                update_volume(VOLUME_STEP);
            } else if (action_type == 2) {
                update_volume(3*VOLUME_STEP);
            } else if (action_type == 3) {
                long_press_counters.vol_up_count++;
                int script_idx = (long_press_counters.vol_up_count - 1) % 2;
                char *command = script_config.volup_scripts[script_idx];
                if (command) {
                    printf("音量加长按 - 执行命令: %s\n", command);
                    execute_command(command);
                }
            }
            break;
            
        case KEY_VOLUMEDOWN:
            if (action_type == 1) {
                update_volume(-VOLUME_STEP);
            } else if (action_type == 2) {
                update_volume(-3*VOLUME_STEP);
            } else if (action_type == 3) {
                long_press_counters.vol_down_count++;
                int script_idx = (long_press_counters.vol_down_count - 1) % 2;
                char *command = script_config.voldown_scripts[script_idx];
                if (command) {
                    printf("音量减长按 - 执行命令: %s\n", command);
                    execute_command(command);
                }
            }
            break;
    }
}

// 获取时间差（毫秒）
long get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000 +
           (end.tv_nsec - start.tv_nsec) / 1000000;
}

// 处理按键事件
void handle_key_event(int key_code, int value) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    if (value == 1) {  // 按下
        key_state.is_pressed = 1;
        key_state.last_press_time = current_time;
        
        if (key_code != key_state.key_code || 
            get_time_diff(key_state.last_release_time, current_time) >= DOUBLE_CLICK_THRESHOLD) {
            // 新的按键序列
            key_state.key_code = key_code;
            key_state.click_count = 1;
        } else {
            // 可能的双击
            key_state.click_count++;
            if (key_state.click_count == 2) {
                process_key_action(key_code, 2);  // 立即处理双击
                key_state.click_count = 0;
            }
        }
    }
    else if (value == 0) {  // 释放
        key_state.is_pressed = 0;
        key_state.last_release_time = current_time;
    }
}

// 在主循环中检查事件
void check_pending_clicks(void) {
    if (key_state.is_pressed) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        // 检查长按
        long press_duration = get_time_diff(key_state.last_press_time, current_time);
        if (press_duration >= LONG_PRESS_THRESHOLD) {
            process_key_action(key_state.key_code, 3);  // 处理长按
            key_state.click_count = 0;     // 清除点击计数
            return;  // 长按触发后直接返回
        }
    }
    
    // 检查单击（未处于按下状态且有一次点击）
    if (!key_state.is_pressed && key_state.click_count == 1) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        long release_diff = get_time_diff(key_state.last_release_time, current_time);
        if (release_diff >= DOUBLE_CLICK_THRESHOLD) {
            process_key_action(key_state.key_code, 1);  // 处理单击
            key_state.click_count = 0;
        }
    }
}

// 播放动画
void play_animation(const char *animation_name, int loop_once, int delay) {
    if (!animation_name) {
        printf("错误：动画名称为空\n");
        return;
    }

    // 停止当前动画
    if (animation_pid > 0) {
        kill(animation_pid, SIGTERM);
        waitpid(animation_pid, NULL, 0);
    }
    
    // 创建新进程
    pid_t pid = fork();
    if (pid < 0) {
        printf("错误：无法创建动画进程\n");
        return;
    }
    
    if (pid == 0) {
        // 子进程
        // 重定向输出到/dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        char delay_str[16];
        snprintf(delay_str, sizeof(delay_str), "%d", delay);
        
        if (loop_once) {
            execlp("./play_bmp_sequence", "./play_bmp_sequence", "-d", delay_str, "-l", animation_name, NULL);
        } else {
            execlp("./play_bmp_sequence", "./play_bmp_sequence", "-d", delay_str, animation_name, NULL);
        }
        exit(1);
    } else {
        // 父进程
        animation_pid = pid;
        printf("启动动画进程，PID: %d\n", pid);
        // 如果是只播放一次的动画，等待它结束并重置 PID
        if (loop_once) {
            waitpid(pid, NULL, 0);
            animation_pid = -1;
            printf("动画播放结束，PID: %d\n", pid);
        }
    }
}

// 显示电池信息
void show_battery_info() {
    // 停止动画
    stop_animation();

    char status[32];
    char capacity[32];
    FILE *fp;
    
    printf("开始读取电池信息...\n");
    
    // 读取充电状态
    fp = fopen("/sys/class/power_supply/axp20x-battery/status", "r");
    if (fp) {
        fgets(status, sizeof(status), fp);
        // 移除换行符
        status[strcspn(status, "\n")] = 0;
        fclose(fp);
        printf("读取到充电状态: %s\n", status);
    } else {
        printf("无法打开充电状态文件\n");
        return;
    }
    
    // 读取电量
    fp = fopen("/sys/class/power_supply/axp20x-battery/capacity", "r");
    if (fp) {
        fgets(capacity, sizeof(capacity), fp);
        // 移除换行符
        capacity[strcspn(capacity, "\n")] = 0;
        fclose(fp);
        printf("读取到电池电量: %s\n", capacity);
    } else {
        printf("无法打开电池电量文件\n");
        return;
    }
    
    char text[128];
    snprintf(text, sizeof(text), "Battery: %s%%\n(%s)", capacity, status);
    display_text(text);
}

// 更新音量
void update_volume(int change) {
    // 停止动画
    stop_animation();

    char cmd[128];
    
    // 先获取当前实际音量
    current_volume = get_current_volume();
    
    int new_volume = current_volume + change;
    
    // 边界检查
    if (new_volume < VOLUME_MIN) new_volume = VOLUME_MIN;
    if (new_volume > VOLUME_MAX) new_volume = VOLUME_MAX;
    
    if (new_volume != current_volume) {
        // 重定向标准输出和错误输出到/dev/null
        snprintf(cmd, sizeof(cmd), "amixer set 'Power Amplifier' %d > /dev/null 2>&1", new_volume);
        system(cmd);
        current_volume = new_volume;
        
        char text[128];
        snprintf(text, sizeof(text), "Volume: %d", current_volume);
        display_text(text);
    }
    printf("当前音量: %d\n", new_volume);
}

// 检查电池状态
void check_battery_status() {
    FILE *fp = fopen("/sys/class/power_supply/axp20x-battery/status", "r");
    if (fp) {
        char status[32];
        fgets(status, sizeof(status), fp);
        fclose(fp);
        
        int new_status = strstr(status, "Charging") != NULL;
        if (new_status != charging_status) {
            charging_status = new_status;
            if (charging_status&&animation_enabled) {
                play_animation("charging", 0, 100);  // 充电动画无限循环
            } else {
                stop_animation();  // 停止充电动画
                handle_idle_state(); // 处理空闲状态
            }
        } else if (charging_status && animation_pid <= 0) {
            // 如果正在充电但没有动画在运行，重新启动动画
            play_animation("charging", 0, 100);
        }
    }
}

// 停止动画
void stop_animation(void) {
    if (animation_pid > 0) {
        kill(animation_pid, SIGTERM);
        waitpid(animation_pid, NULL, 0);
        animation_pid = -1;
    }
}

// 清理函数
void cleanup(int signum) {
    if (animation_pid > 0) {
        kill(animation_pid, SIGTERM);
        waitpid(animation_pid, NULL, 0);
    }
    cleanup_script_config();
    exit(0);
}

// 获取当前音量
int get_current_volume(void) {
    FILE *fp;
    char cmd[128];
    char result[32];
    int volume = 0;
    
    // 使用amixer命令获取当前音量
    snprintf(cmd, sizeof(cmd), "amixer get 'Power Amplifier' | grep 'Mono:' | awk '{print $2}'");
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(result, sizeof(result), fp)) {
            volume = atoi(result);
        }
        pclose(fp);
    }
    
    // 确保音量在有效范围内
    if (volume < VOLUME_MIN) volume = VOLUME_MIN;
    if (volume > VOLUME_MAX) volume = VOLUME_MAX;

    return volume;
}

// 随机播放动画
void play_random_animation(const char *path) {
    DIR *dir;
    struct dirent *entry;
    char **folders = NULL;
    int folder_count = 0;
    int max_folders = 100;  // 预分配空间

    // 分配初始空间
    folders = malloc(max_folders * sizeof(char *));
    if (!folders) {
        printf("Failed to allocate memory for folders\n");
        return;
    }

    // 打开指定目录
    dir = opendir(path);
    if (!dir) {
        printf("Failed to open directory: %s\n", path);
        free(folders);
        return;
    }

    // 读取所有子文件夹
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && 
            strcmp(entry->d_name, ".") != 0 && 
            strcmp(entry->d_name, "..") != 0) {
            
            // 如果空间不足，重新分配
            if (folder_count >= max_folders) {
                max_folders *= 2;
                char **new_folders = realloc(folders, max_folders * sizeof(char *));
                if (!new_folders) {
                    printf("Failed to reallocate memory\n");
                    closedir(dir);
                    for (int i = 0; i < folder_count; i++) {
                        free(folders[i]);
                    }
                    free(folders);
                    return;
                }
                folders = new_folders;
            }

            // 复制文件夹名
            folders[folder_count] = strdup(entry->d_name);
            if (!folders[folder_count]) {
                printf("Failed to allocate memory for folder name\n");
                closedir(dir);
                for (int i = 0; i < folder_count; i++) {
                    free(folders[i]);
                }
                free(folders);
                return;
            }
            folder_count++;
        }
    }
    closedir(dir);

    // 如果有找到文件夹，随机选择一个播放
    if (folder_count > 0) {
        srand(time(NULL));
        int random_index = rand() % folder_count;
        printf("Playing random animation: %s\n", folders[random_index]);
        
        // 构建完整的动画路径
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, folders[random_index]);
        play_animation(full_path, 0, 100);
    } else {
        printf("No animation folders found in %s\n", path);
    }

    // 清理内存
    for (int i = 0; i < folder_count; i++) {
        free(folders[i]);
    }
    free(folders);
}

// 检查空闲状态并处理随机动画
void handle_idle_state(void) {
    time_t now = time(NULL);
    
    // 如果当前有动画在运行，不处理空闲状态
    if (animation_pid > 0) {
        return;
    }
    
    // 检查是否处于空闲状态（限制检测频率）
    if (now - last_activity_time >= IDLE_TIME_THRESHOLD) {
        if (is_idle) {
            if (animation_enabled) {
                play_random_animation("./emotions");
                printf("设备进入空闲状态，开始播放随机动画\n");
            }else{
                // 输出可能会造成性能问题，实际运行时注释掉
                // printf("设备进入空闲状态，但空闲动画已被禁用\n");
            }
            
        }
        else{
            is_idle = 1;
            printf("设备进入空闲状态\n");
        }
    }
}

int main(int argc, char *argv[]) {
    int fd0, fd1;
    struct input_event ev;
    struct pollfd fds[2];
    char *device0 = "/dev/input/event0";
    char *device1 = "/dev/input/event1";
    
    // 设置信号处理
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);
    
    // 初始化随机数生成器
    srand(time(NULL));
    
    // 获取当前音量
    current_volume = get_current_volume();
    printf("当前音量: %d\n", current_volume);
    
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
    play_animation("booting", 1, 20);  // 开机动画只播放一次
    // 等待开机动画结束
    while (animation_pid > 0) {
        usleep(100000);  // 等待 100ms
    }
    
    printf("开始监控按键事件...\n");
    
    // 循环读取输入事件
    while (1) {
        // 处理空闲状态
        handle_idle_state();
        
        // 检查待处理的点击事件
        check_pending_clicks();
        
        // 如果处于空闲状态，检查电池状态
        if (is_idle){
            check_battery_status();
        }
        
        // 等待事件
        int ret = poll(fds, 2, 100);  // 100毫秒超时
        if (ret > 0) {
            // 检查event0 ： 电源键事件在event0
            if (fds[0].revents & POLLIN) {
                if (read(fd0, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
                    if (ev.type == EV_KEY) {
                        last_activity_time = time(NULL);
                        is_idle = 0;  // 重置为非空闲状态
                        
                        if (ev.code == KEY_POWER) {
                            // printf("检测到电源键事件，value = %d\n", ev.value);
                            handle_key_event(KEY_POWER, ev.value);
                        }
                    }
                }
            }
            
            // 检查event1 音量键在event1
            if (fds[1].revents & POLLIN) {
                if (read(fd1, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
                    if (ev.type == EV_KEY) {
                        last_activity_time = time(NULL);
                        is_idle = 0;  // 重置为非空闲状态
                        handle_key_event(ev.code, ev.value);
                    }
                }
            }
        }
    }
    
    // 清理资源
    cleanup(0);
    return 0;
}
