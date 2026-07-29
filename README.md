# AkuBot LVGL UI

用自研的 **LVGL C 程序**（`lvgl_aku`）替换 AkuBot 小屏设备出厂显示系统 `sysboot`，
提供时钟主页、音频（UAC/音量）、表情动画、以及局域网音乐遥控（对接
[FlipPanel](https://github.com/jimieguang/FlipPanel) Bridge）四个页面，全部通过
机身三颗按键（Power / Vol+ / Vol-）操作。

![platform](https://img.shields.io/badge/platform-Allwinner%20ARMv7%20%C2%B7%20Ubuntu%2022.04-blue)
![ui](https://img.shields.io/badge/UI-LVGL%20v8%20%2B%20FreeType-green)

## 硬件平台

| 项 | 说明 |
|---|---|
| 设备 | AkuBot（主机名 `akubox`），Allwinner ARMv7 单核，Ubuntu 22.04，gcc 11.4 |
| 屏幕 | ST7735S 162×132，RGB565，`/dev/fb0`（帧缓冲 42768 字节） |
| 按键 | `event0`=Power，`event1`=Vol±（evdev 直读，短按/双击/长按三种手势） |
| 字体 | FreeType 运行时加载 HarmonyOS Sans SC（中文）+ wwDigital（数码管时钟） |

## 功能一览

- **Home**：数码管大时钟、日期、IP、音量/电量条
- **Audio**：UAC 声卡开关 + 音量调节（amixer）
- **Emotion**：出厂表情动画播放（fbdev 让出屏幕）
- **Music**：UDP 自动发现 + WebSocket 遥控 PC 端音乐（播放/暂停/切歌，支持中文歌名）
- 长按呼出系统菜单（Reboot / Back）；systemd 服务开机自启、崩溃自重启，原 sysboot 可一键回滚

## 目录结构

```
lvgl_project/        全部 C 源码 + Makefile + lv_conf.h
├── HANDOFF.md       【首读】交接文档：现状、连接方式、编译部署、协议细节、已知坑
├── DESIGN.md        架构设计契约（状态机/按键语义/驱动 API）
├── OPS.md           运维手册（服务管理、截图调试、回滚、FAQ）
└── drivers/         fbdev / evdev 驱动
tools/               配套工具：press.py 按键注入、fb2png.ps1 截图转 PNG、
                     b64decode.ps1 / md5local.ps1 传输对账、uac_vol_sync.py PC 端音量同步
lvgl_aku.service     systemd 服务单元
wifi-hardener.*      开机 WiFi 省电关闭（oneshot）
wifi_watchdog.sh     WiFi 看门狗（断联自动重连/兜底热点）+ wifi-watchdog.service
legacy_tools/        原 sysboot 体系仍在沿用的组件（见下方"与旧版的关系"）
```

## 与旧版（sysboot / AkuDisplay）的关系

本仓库原为 **AkuDisplay**——sysboot 时代的底层组件源码（`boot.c` 即设备上
`/opt/aku/web/sys_boot` 的源码）。现由本 LVGL 项目整体接替，旧版完整源码保留在
`legacy-sysboot` tag 中。新旧对照：

| 旧组件 | 去向 |
|---|---|
| `boot.c`（sys_boot 主程序） | 被 `lvgl_aku` 替换（状态机 + 全部页面/按键逻辑） |
| `key_monitor.c` / `key_config.json` | 被 `drivers/evdev.c` + `ui_state.c` 手势状态机替换 |
| `show_text.c` / `show_image.c` | 被 LVGL FreeType 文本渲染 / `icons.c` 程序化图像替换 |
| `play_bmp_sequence.c` | **仍在沿用**：开机动画与 Emotion 表情由它播放，源码保留于 `legacy_tools/` |
| `stb_image.h` | **仍在沿用**：`play_bmp_sequence` 的编译依赖 |
| `gif_to_bmp.py` | **仍在沿用**：制作 BMP 序列动画资产的工具 |
| `test.c` / `compile_commands.txt` | 调试产物，已移除 |

## 设备端运行时依赖

以下文件须存在于设备上（出厂镜像自带，缺失时从 `legacy-sysboot` tag 重新编译）：

- `/opt/aku/web/play_bmp_sequence` —— 动画播放器（`gcc play_bmp_sequence.c -o play_bmp_sequence -lm`）
- `/opt/aku/web/booting/`、`/opt/aku/web/emotions/` —— 开机/表情 BMP 序列资产
- `/opt/aku/web/wwDigital.ttf` —— 数码管时钟字体（部署时复制为 `font_digital.ttf`）
- `/usr/local/sbin/reboot-hard` —— 可选；缺失时程序自动回退为直接操作 `/dev/watchdog` 硬复位

## 回滚到 sysboot

原 sysboot 服务在设备上仅停用未删除，随时可一键回退：

```sh
systemctl disable --now lvgl_aku
systemctl enable --now sysboot
```

## 快速开始

1. 文档中的 `<DEVICE_IP>` / `<PASSWORD>` / `<WIFI_CONNECTION_NAME>` 为占位符，替换为你的设备实际值
2. 设备端 `/opt/aku/lvgl/` 需有 LVGL v8 源码树（出厂镜像自带）及 `font_cjk.ttf`、`font_digital.ttf` 两个运行时字体
3. 编译与部署流程、服务切换/回滚步骤见 [lvgl_project/OPS.md](lvgl_project/OPS.md)
4. 接手开发先读 [lvgl_project/HANDOFF.md](lvgl_project/HANDOFF.md)

> `platform-tools/`（adb）与 `sshpass.exe` 为第三方工具，未入库，请自行下载。
