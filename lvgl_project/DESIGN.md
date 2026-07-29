# AkuBot LVGL App - Design Document

## Project Overview

Replace `sys_boot` (C, single-file, hardcoded logic) with an LVGL-based application on an Allwinner ARMv7 device (162x132 ST7735S display, 3 physical buttons).

**Target**: `/opt/aku/lvgl/`

## Directory Structure

```
/opt/aku/lvgl/
├── lvgl/                  LVGL v8.4.0 (git clone --depth 1 --branch v8.4.0)
├── drivers/
│   ├── fbdev.h/c          Framebuffer driver for /dev/fb0 (ST7735S, 162x132, RGB565)
│   └── evdev.h/c          Input driver for /dev/input/event0 (POWER) and event1 (VOL+/-)
├── lv_conf.h              LVGL configuration (16-bit color, minimal widgets: bar/btn/label only)
├── main.c                 Entry point: boot animation + main loop (5ms tick, event dispatch)
├── sysinfo.c/h            System info: battery/volume/IP/LED/animation paths
├── ui_theme.c/h           Theme init: colors, fonts, styles (dark minimalist)
├── ui_pages.c/h           Page rendering: HOME/AUDIO/EMOTION/MUSIC + refresh logic
├── ui_apps.c/h            App callbacks: audio (UAC toggle) + emotion (animation) + music (FlipPanel)
├── ui_menu.c/h            Menu overlay: Reboot/Back with symbols
├── ui_state.c/h           State machine: ST_PAGES/ST_APP/ST_MENU + event routing (calls ui_app_tick)
├── music_ctrl.c/h         FlipPanel Bridge client (UDP discovery + WebSocket) on a background pthread
└── Makefile               Build system (gcc -j1, -MMD -MP deps, links -lm -lpthread)
```

## Hardware

| Item | Detail |
|------|--------|
| Display | ST7735S, 162x132, 16-bit RGB565, `/dev/fb0` |
| Buttons | Vol+ (left), Vol- (right), Power (below screen) |
| Input devices | `/dev/input/event0` (Power), `/dev/input/event1` (Vol+/-) |
| CPU | ARMv7 single-core, gcc 11.4, cmake 3.22 |

## State Machine

```
┌────────────┐   Power short   ┌──────────┐   Power long    ┌─────────┐
│ ST_PAGES   │ ──────────────→ │ ST_PAGES  │ ──────────────→ │ ST_MENU │
│ browsing   │   (next page)   │           │   (open menu)   │         │
└────────────┘                 └──────────┘                 └─────────┘
      │                              │                           │
      │ Power double                 │ Power double              │ Power short
      ▼                              ▼                           ▼
┌────────────┐                 ┌──────────┐               (execute selected
│ ST_APP     │                 │ ST_PAGES │                menu item, then
│ app mode   │                 │ (exit app)│               hide menu)
└────────────┘                 └──────────┘
      │
      │ Power short
      ▼
  forwarded to app callback (EV_CONFIRM)

Menu can be opened from any state via Power long press.
Menu hides on: Power long again OR selecting an item.
```

## Button Semantics

| Gesture | PAGES mode | APP mode | MENU mode |
|---------|-----------|----------|-----------|
| Power short | Switch to next page (cycle 0→1→2→3→0) | Forward to app as `EV_CONFIRM` | Execute selected menu item |
| Power double | Enter app for current page (page 0 has no app) | Exit app, return to PAGES | (ignored) |
| Power long | Open system menu overlay | Open system menu overlay | Close menu |
| Vol+ short | Volume +1 (repeat 150ms) | Forward to app as `EV_PREV` | Move menu cursor up |
| Vol- short | Volume -1 (repeat 150ms) | Forward to app as `EV_NEXT` | Move menu cursor down |

**Volume is always system volume (amixer 'Power Amplifier', range 0-63).**

## Pages

### Page 0: HOME
Displays: clock (HH:MM), date (MM-DD Mon), WiFi IP, volume bar+number, battery bar+percentage.
No app (double-click shows toast "No app on this page"). Refreshes every 1s (200×5ms).

### Page 1: AUDIO
Displays: "Audio" title, "UAC: ON/OFF" status, "Tap: UAC  Dbl: back" hint.
App mode: single-click toggles UAC (calls `audio_start.sh` / `audio_stop.sh`), double-click exits.

### Page 2: EMOTION
Displays: "Emotion" title, "Playing..." status, "Tap: next  Dbl: back" hint.
App mode: plays random emotion animation (external `play_bmp_sequence` writes fb0 directly), single-click cycles to next random emotion, double-click kills process & returns to page.

### Page 3: MUSIC
Displays: "MUSIC" title, link status ("Searching..." grey / "Linked" green when a FlipPanel Bridge is discovered).
App mode: shows playback state ("playing"/"paused"/"Offline") + now-playing track (scrolling label).
- OK (Power short) → `music.playPause`
- Vol+ (`EV_PREV`) → `music.previous`
- Vol- (`EV_NEXT`) → `music.next`
- Double-click exits.

See the "Music / FlipPanel Integration" section below for the protocol.

## Menu

2 items with symbol icons:
- "↻ Reboot" → `sysinfo_reboot()`: prefers `/usr/local/sbin/reboot-hard`, falls back to arming `/dev/watchdog` natively, then plain `reboot`; LED heartbeat while rebooting, restored if all paths fail
- "← Back" → close menu

Bottom hint: "Vol: move  OK: select" (muted text).
Menu overlay uses semi-transparent backdrop to show underlying page context.

## Drivers

### fbdev.c

```c
void fbdev_init(void);           // open /dev/fb0, mmap, get screen info
void fbdev_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void fbdev_pause(int on);        // 1=pause (external process draws), 0=resume
```

### evdev.c

```c
void evdev_init(void);           // open /dev/input/event0, event1 (non-blocking)
btn_event_t evdev_poll(void);    // read events, detect gestures, return BTN_NONE / SHORT / DOUBLE / LONG
int evdev_vol_dir(void);         // +1 (vol+ held), -1 (vol- held), 0 (none)

typedef enum {
    BTN_NONE = 0,
    BTN_VOL_UP, BTN_VOL_DOWN,        // not used externally (vol_dir used instead)
    BTN_POWER_SHORT, BTN_POWER_DOUBLE, BTN_POWER_LONG,
} btn_event_t;
```

Timing thresholds for gesture detection:
- SHORT_MS = 350 (release idle time before reporting single click)
- LONG_MS = 900 (hold time before reporting long press)

## App Framework

Each app has 3 callbacks:

```c
void app_init(lv_obj_t *parent);   // create UI on the app container
void app_event(app_evt_t ev);      // handle EV_CONFIRM, EV_BACK, EV_NEXT, EV_PREV
void app_deinit(void);             // cleanup (set pointers to NULL)

typedef enum { EV_CONFIRM, EV_BACK, EV_NEXT, EV_PREV } app_evt_t;
```

To add a new app:
1. Implement the 3 callbacks
2. Register them in the `app_funcs` table (ui_apps.c)
3. Set `has_app = 1` in the page's `page_entry_t` (ui_pages.c)

Apps are rendered on `app_box` (full-screen container). `lv_obj_clean(app_box)` is called before each `app_init`.

`ui_app_tick()` (declared in ui_apps.h, called from `ui_state_refresh` in ui_state.c) lets an open app refresh
periodically (~1s) without a button press. Currently only MUSIC uses it, to poll the network status snapshot.

## Music / FlipPanel Integration

**Goal**: remote-control a PC music player (play/pause/prev/next) over LAN, targeting the
[FlipPanel](https://github.com/jimieguang/FlipPanel) Windows Agent ("FlipPanel Bridge").

**Protocol (verified against FlipPanel source):**
- **Discovery**: Bridge broadcasts JSON to UDP **50570** ~1×/s:
  `{"hostAddress":"<PC-IP>","hostPort":50571,"endpoint":"ws://<PC-IP>:50571/ws","deviceName":...}`
- **Connect**: `ws://<hostAddress>:50571/ws` (WebSocket).
- **Receive**: Bridge pushes `status` messages with `musicTitle` / `musicArtist` / `musicPlaybackState`.
- **Send**: `{"messageType":"command","actionId":"<id>","value":null}`
  - actionId: `music.playPause` `music.pause` `music.resume` `music.next` `music.previous`
    `music.stop` `music.like` `music.dislike` `music.setVolume`(needs value) `music.launch`

**Client implementation (`music_ctrl.c`):** a single background pthread owns all networking
(UDP discover → TCP connect → minimal RFC6455 handshake → recv status / send masked command frames,
auto-reconnect on drop). UI thread posts commands via a small ring queue; a mutex-protected snapshot
is read by `ui_app_tick`. Public API in `music_ctrl.h`: `music_ctrl_init/action/get`.

**Known limit**: `montserrat` has no CJK glyphs, so Chinese track titles render as "□" boxes
(`\uXXXX` is already decoded to UTF-8, but LVGL lacks the font). Requires adding a CJK font to lv_conf.h.

## Key Design Decisions

1. **No LVGL indev** — evdev.c handles raw input + gesture detection, main loop dispatches. This bypasses LVGL's group/focus system for maximum control.
2. **External animation** — EMOTION app pauses LVGL framebuffer writes and runs `play_bmp_sequence` externally. On exit, kills it and resumes.
3. **Full-screen page layout** — Each page fills the entire display (no bottom badge bar). Widgets positioned with `lv_obj_align` relative to anchors.
4. **Toast notifications** — Volume changes and "No app" feedback shown as centered overlay text (auto-hide after 1.5s).
5. **LED feedback** — Boot: solid on. Reboot: heartbeat. Idle: off. Controlled via `/sys/class/leds/aku-logo/`.
6. **Minimal widget set** — lv_conf.h enables only LV_USE_BAR/BTN/LABEL to reduce binary size (371KB vs 517KB full).

## Build & Run

Deployment is now managed by **systemd** (`lvgl_aku.service`). sysboot is stopped+disabled but kept for rollback.

```bash
cd /opt/aku/lvgl
make -j1                          # incremental: rm the changed .o first, then make

systemctl restart lvgl_aku        # after replacing the binary
systemctl status  lvgl_aku

# Rollback to factory display system (sysboot is still installed, only disabled):
systemctl disable --now lvgl_aku
systemctl enable  --now sysboot
```
> `lvgl_aku` and factory `sys_boot` both write `/dev/fb0` directly — only one may run at a time
> (systemd mutual exclusion handles this now). Running both causes framebuffer contention.


## Current Issues / TODO

### Completed
- [x] Volume indicator on all pages (toast + bar联动)
- [x] LED control (boot solid, reboot heartbeat, idle off)
- [x] Page 0 double-click feedback (toast "No app on this page")
- [x] Menu symbol icons (LV_SYMBOL_REFRESH/LEFT)
- [x] App hint text fits 162px width ("Tap: UAC  Dbl: back")
- [x] evdev auto-repeat handling (value=2 treated as held)
- [x] Menu navigation direction (Vol+ = up, Vol- = down)
- [x] Volume/menu 150ms throttle (prevents rapid cycling)
- [x] Animation paths absolute (no WorkingDirectory dependency)
- [x] Deploy as systemd service (`lvgl_aku.service`, Restart=on-failure; sysboot stopped+disabled, reversible)
- [x] MUSIC page: FlipPanel Bridge remote control (UDP discovery + WebSocket, verified end-to-end)

### Pending
- [ ] MUSIC: CJK font for Chinese track titles (currently boxes)
- [ ] MUSIC: setVolume/like/dislike UI controls (protocol supports, not wired)
- [ ] MUSIC: fixed-host fallback config (currently UDP auto-discovery only)
- [ ] EMOTION app: framebuffer flicker when switching between LVGL and external process (needs testing)
- [ ] Charging animation (battery status readable, animation not implemented)
- [ ] Menu expansion (WiFi/Bluetooth toggles, system info)
- [ ] Config file hot-reload (SIGHUP)
- [ ] App system fully generic (page-to-app mapping still hardcoded in ui_pages.c)
