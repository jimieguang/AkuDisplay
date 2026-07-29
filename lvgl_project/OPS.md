# AkuBot 操作指南（给另一个 Agent）

## 一、连接方式

### 方案 A：ADB（USB 有线，最可靠）

```
adb 工具: C:\Users\Administrator\Desktop\AkuBot\platform-tools\adb.exe
adb 设备: akubox
```

**重要**：akubox 的 MOTD（启动面板）会在 adb shell 启动时打印，包含终端控制序列，可能导致 `adb shell "command"` 超时。**不要用 `adb shell "command"`，要用管道输入。**

```
❌ 不可靠：adb -s akubox shell "ls"
✅ 可靠：   echo "ls" | adb -s akubox shell
✅ 可靠：   echo "ls" | C:\path\to\adb.exe -s akubox shell
```

多条命令：
```
@"
cd /opt/aku/lvgl
make clean
make -j1
"@ | C:\path\to\adb.exe -s akubox shell
```

### 方案 B：SSH（WiFi 连接时可用）

```
SSH 工具: C:\Users\Administrator\Desktop\AkuBot\sshpass.exe
设备 IP: <DEVICE_IP> (WiFi)
用户名: root
密码:   <PASSWORD>
```

```
sshpass -p <PASSWORD> ssh -o StrictHostKeyChecking=no root@<DEVICE_IP> "command"
```

SSH 不会遇到 MOTD 问题，命令执行稳定。

### 方案 C：ADB Push（文件传输）

```
推文件到设备：
C:\path\to\adb.exe push "C:\path\to\local\file" /opt/aku/lvgl/filename
```

ADB push 是稳定的，可用于推送编译好的文件。

## 二、工作流程

> 现在 `lvgl_aku` 由 **systemd 服务托管**（开机自启 + 崩溃自重启）。sysboot 已 stop+disable 但保留，可一键回滚。

### 修改代码 → 编译 → 重启服务

```
1. 在 C:\Users\Administrator\Desktop\AkuBot\lvgl_project\* 编辑源码
2. 用 ADB push 推送到 /opt/aku/lvgl/
3. 在设备端增量编译（rm 变更的 .o 再 make -j1）
4. systemctl restart lvgl_aku
5. systemctl status lvgl_aku 确认 active
```

### 具体步骤

```powershell
# 1. 推文件
C:\path\to\adb.exe push "C:\path\to\music_ctrl.c" /opt/aku/lvgl/music_ctrl.c
# （每次修改后推变更的文件；新增 .c 记得先加进 Makefile 的 APP_SRC）

# 2. 编译（在设备上，增量）
@"
cd /opt/aku/lvgl
rm -f main.o ui_pages.o ui_apps.o ui_state.o music_ctrl.o lvgl_aku
make -j1
"@ | C:\path\to\adb.exe shell

# 3. 重启服务（systemd 会互斥保证不与 sysboot 抢屏）
echo "systemctl restart lvgl_aku; sleep 1; systemctl is-active lvgl_aku" | C:\path\to\adb.exe shell
```

### 服务管理 / 回滚

```bash
systemctl restart lvgl_aku       # 换二进制后重启
systemctl status  lvgl_aku
systemctl is-active lvgl_aku

# 回滚到出厂显示系统（sysboot 仍在，只是被停用）
systemctl disable --now lvgl_aku
systemctl enable  --now sysboot
```
服务单元 `/etc/systemd/system/lvgl_aku.service`，本地副本
`c:\Users\Administrator\Desktop\AkuBot\lvgl_aku.service`。

> **抢屏教训**：`sys_boot` 与 `lvgl_aku` 都直接写 `/dev/fb0`，**同时运行会互相覆盖**（曾是
> lvgl_aku "启动几秒就消失" 的真因）。二者只能一个在跑，现由服务互斥保证。

### 无人值守截图 / 驱动 UI（调试利器）

设备无法手动按键时，用注入脚本驱动按键 + 抓帧缓冲转 PNG：

```powershell
# 1) 注入 Power 键：<次数> <每次间隔秒>。0.16s≈双击(进/出App)，0.6~0.7s≈多次单击(翻页)
C:\path\to\adb.exe push c:\...\AkuBot\tools\press.py /tmp/press.py
C:\path\to\sshpass.exe -p <PASSWORD> ssh ... 'python3 /tmp/press.py 3 0.7'   # 单击3次翻到 Music 页
C:\path\to\sshpass.exe -p <PASSWORD> ssh ... 'python3 /tmp/press.py 2 0.16'  # 双击进/出 App

# 2) 抓屏并转 PNG（fb0 = 162x132 RGB565 = 42768 字节）
C:\path\to\sshpass.exe -p <PASSWORD> ssh ... 'head -c 42768 /dev/fb0 > /tmp/fb.raw'
C:\path\to\adb.exe pull /tmp/fb.raw c:\...\AkuBot\fb.raw
powershell -File c:\...\AkuBot\tools\fb2png.ps1 -Raw c:\...\AkuBot\fb.raw -Png c:\...\AkuBot\fb.png
```
`press.py`（armhf `input_event` = 16 字节 `llHHi`）与 `fb2png.ps1`（RGB565→PNG，4× 放大）在工程 `tools/` 目录。

> **PowerShell 陷阱**：SSH 远程命令用**单引号**包裹，避免本地 PowerShell 提前展开 `$(...)`/`$VAR`；
> 单引号内传真实单引号用 `''`；多模式 grep 的 `|` 会被拆断，改用单模式 grep 或 `pidof`。

## 三、关键信息

### 显示屏参数

```
分辨率: 162x132
色深: 16-bit RGB565
驱动: ST7735S (SPI)
设备: /dev/fb0
fbset: U:162x132p-0
```

### 按键输入

```
/dev/input/event0 → KEY_POWER（电源键）
/dev/input/event1 → KEY_VOLUMEUP / KEY_VOLUMEDOWN（音量+音量-）

物理布局：Vol+ (左)  Vol- (右)  Power (下方)
```

**按键事件代码**：
```c
#define KEY_POWER      116
#define KEY_VOLUMEUP   115
#define KEY_VOLUMEDOWN 114
```

### 已有脚本路径

```
UAC 启动:  /opt/aku/web/audio_start.sh
UAC 停止:  /opt/aku/web/audio_stop.sh
表情动画:  /opt/aku/web/play_bmp_sequence (直接写 /dev/fb0)
表情目录:  /opt/aku/web/emotions/ (emotion1 ~ emotion10)
音量控制:  amixer set 'Power Amplifier' N (N=0~63)
重启命令:  /usr/local/sbin/reboot-hard
```

### 硬件辅助

```
电池电量:  /sys/class/power_supply/axp20x-battery/capacity
电池状态:  /sys/class/power_supply/axp20x-battery/status
LED 控制:  /sys/class/leds/aku-logo/brightness (写 0/1)
WiFi 状态: ip -br addr show wlan0 | awk '{print $3}'
```

### 已有系统服务

```
sysboot.service           → 出厂显示系统（已 stop+disable，保留可回滚，与 lvgl_aku 互斥）
lvgl_aku.service          → 【当前】LVGL UI，开机自启 + 崩溃自重启（Restart=on-failure）
wifi-watchdog.service     → WiFi 看门狗
btstart.service           → 蓝牙启动
wifi-hardener.service     → WiFi 省电禁用（开机自启）
akuweb.service            → Web 服务器（端口 80，保持运行）
```

### FlipPanel（Music 页）测试

前提：PC 与设备同一 LAN，PC 上运行 FlipPanel Bridge（默认端口 UDP 50570 发现 / 50571 WebSocket）。

```
1. PC 启动 FlipPanel Bridge（托盘常驻）。
2. 设备翻到 Music 页 → 应显示绿色 "Linked"（否则查防火墙 / 是否同网段）。
3. 双击进入 App，按 OK → 状态在 paused↔playing 切换，并实际控制 PC 播放器。
   Vol+ = 上一首(music.previous)，Vol- = 下一首(music.next)。
```
协议细节见 `DESIGN.md` 的 "Music / FlipPanel Integration" 章节与 `HANDOFF.md` 第 5 节。

## 四、常见问题

### Q: 编译太慢怎么办？
原因：ARM 单核编译 LVGL 约 100+ 个 .c 文件，耗时 2-3 分钟。
解决办法：只修改了一个文件时，只重新编译该文件的 .o 再链接：

```makefile
# 如果只改了 main.c：
rm -f main.o lvgl_aku
make -j1

# 结果：只编译 main.c → 链接（几十秒）
```

### Q: `adb shell "command"` 超时怎么办？
原因：MOTD 的终端控制序列（`<2004hroot@akubox:~#`）让解析器误认为 shell 仍在等待输入。
解决办法：改用 `echo "command" | adb shell`。

极端情况：如果 echo 模式也卡住，可以用 SSH（如果 WiFi 通）：

```powershell
C:\path\to\sshpass.exe -p <PASSWORD> ssh -o StrictHostKeyChecking=no root@<DEVICE_IP> "command"
```

### Q: 如何恢复出厂显示系统？
sysboot 仍安装，只是被停用。回滚：
```
systemctl disable --now lvgl_aku
systemctl enable  --now sysboot
```

### Q: framebuffer 冲突怎么办？
当 LVGL 和 play_bmp_sequence 同时写 `/dev/fb0` 时内容互相覆盖。
解决方案：进入 EMOTION app 前调用 `fbdev_pause(1)`（LVGL 停止刷新），退出时调用 `fbdev_pause(0)`（恢复）。

### Q: 按键检测不到怎么办？
在 evdev.c 里加日志检查：
```c
fprintf(stderr, "KEY: code=%d val=%d\n", ev.code, ev.value);
```
然后 `adb shell` 按按钮看输出。

## 五、文件清单

```
C:\Users\Administrator\Desktop\AkuBot\lvgl_project\
├── lv_conf.h          LVGL 配置
├── main.c             主程序（含 music_ctrl_init）
├── Makefile           编译脚本（APP_SRC 含 music_ctrl.c）
├── drivers/
│   ├── fbdev.h/c      framebuffer 显示驱动
│   └── evdev.h/c      按键输入驱动
├── ui_pages.c/h       浏览页（HOME/AUDIO/EMOTION/MUSIC）
├── ui_apps.c/h        全屏 App 回调（含 Music）
├── ui_state.c/h       状态机
├── ui_menu.c/h        系统菜单
├── ui_theme.c/h       主题/颜色/字体
├── sysinfo.c/h        系统信息封装
├── music_ctrl.c/h     FlipPanel Bridge 客户端（UDP 发现 + WebSocket）
├── HANDOFF.md         【首读】交接文档（现状/连接/编译/回滚/协议/坑）
├── DESIGN.md          架构设计文档
└── OPS.md             本操作指南
```

> 工程 `tools/` 目录另有调试工具：`press.py`（按键注入）、`fb2png.ps1`（截图转 PNG）、`b64decode.ps1`
> （base64 下载解码）、`md5local.ps1`（源码 md5 对账）、`uac_vol_sync.py`（PC 端 UAC 音量同步）；
> 根目录有 `lvgl_aku.service`（服务单元本地副本）与 WiFi 加固/看门狗脚本。
