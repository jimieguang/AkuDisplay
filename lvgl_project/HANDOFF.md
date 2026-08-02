# AkuBot 项目交接文档（HANDOFF）

> 面向"下一个接手的 Agent / 开发者"的唯一入口文档。
> 先读本文了解**当前状态**与**怎么干活**，再按需查阅 `DESIGN.md`（架构契约）与 `OPS.md`（操作细节）。
> 原则：**若文档与代码/设备实际状态冲突，以代码和设备为准**，并回来更新本文。

---

## 0. 一句话现状

用一个自研的 **LVGL C 程序 (`lvgl_aku`)** 替换了出厂的显示系统 `sys_boot`，跑在 Allwinner ARMv7 的
162×132 ST7735S 小屏设备（主机名 `akubox`，WiFi `<DEVICE_IP>`）上，已通过 **systemd 服务
`lvgl_aku.service`** 开机自启并崩溃自重启。出厂的 `sysboot.service` 已被 **暂停（stop+disable）
但未删除**，可随时一键回滚。

当前有 4 个页面：**Home / Audio / Emotion / Music**。最新新增的是 **Music 页**——通过局域网控制
PC 端音乐软件（对接 [FlipPanel](https://github.com/jimieguang/FlipPanel) Bridge）。

---

## 1. 设备与连接

| 项 | 值 |
|----|----|
| 设备 | `akubox`，Allwinner ARMv7 单核，Ubuntu 22.04，gcc 11.4 |
| 屏幕 | ST7735S 162×132，RGB565，`/dev/fb0`，帧缓冲 42768 字节 |
| 按键 | `/dev/input/event0`=Power(116)，`/dev/input/event1`=Vol+(115)/Vol-(114) |
| WiFi IP | `<DEVICE_IP>` |
| 代码目标目录 | `/opt/aku/lvgl/` |
| 本地工程 | `c:\Users\Administrator\Desktop\AkuBot\lvgl_project\` |

**两条通道（都可用）：**

```powershell
# ADB（USB，最稳）—— 注意 MOTD 问题，务必用管道，不要 adb shell "cmd"
C:\Users\Administrator\Desktop\AkuBot\platform-tools\adb.exe push <本地文件> /opt/aku/lvgl/<名>
echo "命令" | C:\Users\Administrator\Desktop\AkuBot\platform-tools\adb.exe -s akubox shell

# SSH（WiFi，命令执行最稳，推荐跑复杂命令）
C:\Users\Administrator\Desktop\AkuBot\sshpass.exe -p <PASSWORD> ssh -o StrictHostKeyChecking=no root@<DEVICE_IP> "命令"
```

**PowerShell 传参陷阱（务必记住）：**
- SSH 远程命令用 **单引号** 包裹，避免本地 PowerShell 把 `$(...)`、`$VAR` 提前展开。
- 单引号串里要传"真实单引号"给远程时，用 `''`（两个单引号=一个字面单引号），如 `amixer sget ''Power Amplifier''`。
- 多模式 grep 里的 `|` 会被 SSH/PowerShell 拆断（报 `xxx: command not found`）；用**单模式 grep** 或 `pidof`。
- PowerShell 用 `;` 分隔语句，**不支持 `&&`**。

---

## 2. 编译 / 部署 / 回滚

### 编译（在设备上，ARM 单核）
```bash
cd /opt/aku/lvgl
# 只改了少数 .c/.h 时，删掉相关 .o 让它增量编译（几十秒）：
rm -f main.o ui_pages.o ui_apps.o ui_state.o music_ctrl.o lvgl_aku
make -j1
# 全量 clean 编译要 2-3 分钟（LVGL ~100+ 文件）
```
> `-MMD -MP` 自动依赖已开启：改了头文件，依赖它的 .o 会自动重编，但**新增 .c 首次要保证在 Makefile 的 `APP_SRC` 里**。

### 部署（当前是 systemd 服务托管）
```bash
systemctl restart lvgl_aku       # 换了二进制后重启即可
systemctl status  lvgl_aku
systemctl is-active lvgl_aku
```
服务单元：`/etc/systemd/system/lvgl_aku.service`
（本地副本：`c:\Users\Administrator\Desktop\AkuBot\lvgl_aku.service`）
```ini
[Unit]
Description=AkuBot LVGL UI (replaces sysboot)
After=network.target
[Service]
Type=simple
WorkingDirectory=/opt/aku/lvgl
ExecStart=/opt/aku/lvgl/lvgl_aku
Restart=on-failure
RestartSec=3
[Install]
WantedBy=multi-user.target
```

### 回滚到出厂显示系统（sysboot 仍在，只是被停用）
```bash
systemctl disable --now lvgl_aku
systemctl enable  --now sysboot     # 恢复出厂 sys_boot
```
> **重要历史教训**：`sys_boot`（sysboot.service 拉起）和 `lvgl_aku` 都直接写 `/dev/fb0`，
> **同时运行会互相抢屏**。这曾是 `lvgl_aku` 反复"启动几秒就消失"的真正原因。
> 二者只能有一个在跑。现在由服务互斥保证。

---

## 3. 截图 / 无人值守驱动 UI（调试利器）

设备无法手动按键时，用注入脚本驱动 + 抓帧缓冲：

```powershell
# 1) 注入 Power 键（press.py 已在本地，可 adb push 到 /tmp/）
#    参数： <次数> <每次间隔秒>。间隔 0.16s≈双击(进/出App)，0.6s≈多次单击(翻页)
C:\...\adb.exe push c:\...\AkuBot\tools\press.py /tmp/press.py
C:\...\sshpass.exe -p <PASSWORD> ssh ... 'python3 /tmp/press.py 3 0.7'   # 单击3次：Home→Audio→Emotion→Music
C:\...\sshpass.exe -p <PASSWORD> ssh ... 'python3 /tmp/press.py 2 0.16'  # 双击：进入/退出 App

# 2) 抓屏并转 PNG
C:\...\sshpass.exe -p <PASSWORD> ssh ... 'head -c 42768 /dev/fb0 > /tmp/fb.raw'
C:\...\adb.exe pull /tmp/fb.raw c:\...\AkuBot\fb.raw
powershell -File c:\...\AkuBot\tools\fb2png.ps1 -Raw c:\...\AkuBot\fb.raw -Png c:\...\AkuBot\fb.png
```
`press.py`（`input_event` 在 armhf 上是 16 字节 `llHHi`）与 `fb2png.ps1`（RGB565→PNG，4× 放大）都在工程 `tools/` 目录。

---

## 4. 代码结构（`/opt/aku/lvgl/`）

```
main.c          入口：驱动初始化 → 开机动画(阻塞) → 建 UI → music_ctrl_init → 5ms 主循环
drivers/fbdev.c fb0 显示驱动（fbdev_pause 供 Emotion 让出屏幕）
drivers/evdev.c 按键+手势（SHORT/DOUBLE/LONG + vol_dir），不走 LVGL indev
ui_theme.c/h    暗色主题：颜色 COL_*、字体 F_*（FreeType 运行时加载 HarmonyOS Sans SC，支持中文；
                加载失败回退 montserrat）。ui_font_digital()=数字钟字体 wwDigital
ui_pages.c/h    4 个浏览页 + 底部 tab 圆点；page_id_t / N_PAGES
ui_apps.c/h     全屏 App（双击进入）：Audio(UAC+音量) / Emotion(动画) / Music(音乐控制)
ui_menu.c/h     长按 Power 呼出的系统菜单（Reboot / Back）
ui_state.c/h    三态状态机 ST_PAGES/ST_APP/ST_MENU + 事件路由
sysinfo.c/h     电池/音量(amixer 'Power Amplifier' 0-63)/IP/LED/动画 的 shell 封装
music_ctrl.c/h  FlipPanel Bridge 客户端（UDP 发现 + WebSocket）后台线程
icons.c/h       32x32 页面图标（audio/emotion/music），启动时程序化绘制，无二进制资源
Makefile        新增 .c 记得加进 APP_SRC（-lfreetype 已链接）
lv_conf.h       LVGL 配置（16bpp；LV_USE_FREETYPE=1、LV_USE_IMG=1）
```

**交互语义（关键，改动前先确认用户）：**
- Power 短按：PAGES=翻页(0→1→2→3→0)；APP=转发 `EV_CONFIRM`；MENU=选中项
- Power 双击：PAGES=进入当前页 App(Home 无 App)；APP=退出
- Power 长按：任意态呼出/关闭系统菜单
- Vol+ / Vol-：PAGES=系统音量±1；APP=`EV_PREV`/`EV_NEXT`；MENU=光标上/下

---

## 5. Music 页 / FlipPanel 集成（本次新增，重点）

**目的**：局域网遥控 PC 端音乐软件的 播放/暂停/上一首/下一首。
**对端**：PC 上运行 [FlipPanel](https://github.com/jimieguang/FlipPanel) 的 Windows Agent（"FlipPanel Bridge"）。

**协议（已从 FlipPanel 源码核实）：**
- **发现**：Bridge 每 ~1s 向 UDP **50570** 广播 JSON：
  `{"hostAddress":"<PC-IP>","hostPort":50571,"endpoint":"ws://<PC-IP>:50571/ws","deviceName":...}`
- **连接**：`ws://<hostAddress>:50571/ws`（WebSocket）
- **收**：Bridge 持续推送 `status` 消息，含 `musicTitle` / `musicArtist` / `musicPlaybackState`（"Playing"/"Paused"/...）
- **发**：控制命令 `{"messageType":"command","actionId":"<id>","value":null}`
  - 可用 actionId：`music.playPause` `music.pause` `music.resume` `music.next` `music.previous`
    `music.stop` `music.like` `music.dislike` `music.setVolume`(需 value) `music.launch`

**本端实现（`music_ctrl.c`）：**
- 一个**后台 pthread** 负责全部网络（UI 主线程从不阻塞）：
  UDP 发现 → TCP 连接 → **极简 RFC6455 WebSocket 握手**（随机 Sec-WebSocket-Key，**不校验** server Accept，够用）
  → 循环：收帧(解析 status，回 ping→pong) + 发命令（客户端帧**必须掩码**）。断线自动回到发现重连。
- 命令走一个小环形队列（`music_ctrl_action("music.playPause")`），UI 线程投递，worker 线程发送。
- 现状快照 `music_ctrl_get(&mc_status_t)` 受互斥锁保护，UI 每 ~1s（`ui_app_tick`）读取刷新。

**Music 页/App 的按键映射（App 内）：**
- OK(Power 短按) → `music.playPause`
- Vol+（`EV_PREV`）→ `music.previous`
- Vol-（`EV_NEXT`）→ `music.next`
- Power 双击 → 退出 App

**已知限制 / 坑：**
- **中文歌名已支持**：FreeType 运行时加载 `/opt/aku/lvgl/font_cjk.ttf`（HarmonyOS Sans SC，拷自
  `/opt/aku/xiaozhi/font/`）。数字钟用 `/opt/aku/lvgl/font_digital.ttf`（wwDigital，拷自 `/opt/aku/web/`）。
  **这两个 ttf 是运行时依赖**：文件丢失时静默回退 montserrat（中文又会变"□"方块），重装设备时记得拷回。
  System.Text.Json 的 `\uXXXX` 转义已在 `music_ctrl.c` 解码为 UTF-8。
- 未做 setVolume/like 的 UI（协议支持，按需扩展 `music_event` + 加个可视控件即可）。
- Bridge 不在线时，Music 页显示 `Searching...`，App 内显示 `Offline`，不影响其它功能。

**如何测试（需 PC 端跑 FlipPanel Bridge，且与设备同一 LAN）：**
1. PC 启动 FlipPanel Bridge（托盘常驻，默认端口 50570/50571）。
2. 设备翻到 Music 页，应显示绿色 `Linked`（否则查防火墙/是否同网段）。
3. 双击进入 App，按 OK 应看到 `paused`↔`playing` 切换（并实际控制 PC 播放器）。

> 本次交付时已在真实 Bridge 上验证：发现→连接→收状态→发命令→UI 联动 全链路通过（截图 `fb_music*.png` / `fb_play.png`）。

---

## 6. 其它已知事实 / 历史坑

- **音量解析**：`amixer get 'Power Amplifier'` 头行有控件索引 `,0`，真实值在 `Mono: 59 [..]` 行。
  早期用 `grep -oE '[0-9]+' | head -1` 抓到的是索引 0（恒 0 bug）。已改用
  `sed -n 's/.*: \([0-9]\+\) \[.*/\1/p'` 精确取值（见 `sysinfo.c`）。
- **UAC（Audio 页）**：`audio_start.sh` 用相对路径调 `./uacFunc.sh`，必须在 `/opt/aku/web` 下运行，
  故 `ui_apps.c` 里是 `cd /opt/aku/web && ./audio_start.sh &`。
  该脚本在无 USB host 拉流时有 **200ms 忙等待循环打满单核 CPU** 的固有隐患（曾导致设备重启），
  用户明确要求**保留**（为延迟最优化），勿改。
- **UAC 音量（v0.8，2026-08-02 实测定案）**：`uac_bridge` 在数据通路应用 Windows 滑块音量。
  机制：Windows 滑块 → USB Feature Unit 控件镜像（`PCM Capture Volume`，0..100 → −100..0 dB）
  → bridge 每 ~100ms 读一次 → 采样增益/静音。**内核 v6.1 不应用增益，Windows 也不缩放
  USB 数据**，所以必须在 bridge 里做；设备侧 `amixer -c 1` 改音量只是状态镜像、不影响声音。
  实测：Windows 100% → 增益 1.0；50% → 0.2818（−11dB）；0%/静音 → 0。UI 音量键仍只控
  `Power Amplifier`（本地），符合"不需要音量同步"的要求。
- **musb 热解绑大坑**：多次 unbind/rebind UDC（UAC↔ADB 切换、UAC 开关反复操作）会把
  USB OUT 端弄坏——Windows 播放端点失效（`AUDCLNT_E_DEVICE_INVALIDATED`，采集/控件正常、
  唯独播放打不开），**只能重启设备恢复**。UAC 开关勿设计成高频操作；出问题先重启。
- **Emotion 页**：外部 `play_bmp_sequence` 直接写 fb0，进入前 `fbdev_pause(1)` 让 LVGL 停画，退出恢复。
- 相关系统服务：`wifi-watchdog` / `btstart` / `wifi-hardener` / `akuweb`(端口80) 保持运行。

---

## 7. 待办 / 可扩展方向

- [x] Music：中文歌名（FreeType + HarmonyOS Sans SC 运行时加载，已上屏验证）。
- [ ] Music：可加音量条(`music.setVolume`) / 喜欢(`music.like`) 控件。
- [ ] Music：连接目标 PC 目前全靠 UDP 自动发现；如需固定 PC，可加个 host 配置文件回退。
- [ ] Emotion：LVGL↔外部进程切换时偶发闪烁（待测）。
- [ ] 充电动画未实现（电量状态可读）。
- [ ] 菜单可扩展 WiFi/蓝牙开关、系统信息。
