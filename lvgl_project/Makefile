CC = gcc
CFLAGS = -Wall -Wextra -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard
CFLAGS += -I. -Idrivers -Ilvgl
CFLAGS += -I/usr/include/freetype2
CFLAGS += -MMD -MP
LDFLAGS = -lm -lpthread -lfreetype

LVGL_DIR = lvgl
LVGL_ALL = $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_SRC = $(filter-out $(LVGL_DIR)/src/draw/nxp/% $(LVGL_DIR)/src/draw/sdl/% \
                        $(LVGL_DIR)/src/draw/renesas/% $(LVGL_DIR)/src/draw/arm2d/% \
                        $(LVGL_DIR)/src/draw/stm32_dma2d/% $(LVGL_DIR)/src/draw/swm341_dma2d/%, \
                        $(LVGL_ALL))

APP_SRC = main.c sysinfo.c ui_theme.c ui_pages.c ui_apps.c ui_menu.c ui_state.c \
          music_ctrl.c icons.c \
          drivers/fbdev.c drivers/evdev.c
SRC = $(APP_SRC) $(LVGL_SRC)
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)
TARGET = lvgl_aku
UAC_BRIDGE = uac_bridge

all: $(TARGET) $(UAC_BRIDGE)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# uac_bridge: 独立音频桥进程（UAC gadget -> sun4i codec）。
# v0.8 起在数据通路应用 Windows 滑块音量（读 card1 'PCM Capture Volume' 镜像，
# 0..100 -> -100..0 dB，静音由 'PCM Capture Switch' 控制）。
# 与 lvgl_aku 分开编译；需要 libasound2-dev（设备端已装）。
$(UAC_BRIDGE): uac_bridge.c
	$(CC) -O2 -Wall -o $@ $< -lasound -lpthread -lm

# 部署到 audio_start.sh 期望的运行时路径（生产 BRIDGE=/opt/aku/lvgl/uac_bridge）
bridge-install: $(UAC_BRIDGE)
	install -m 0755 $(UAC_BRIDGE) /opt/aku/lvgl/$(UAC_BRIDGE)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEP)

clean:
	rm -f $(TARGET) $(OBJ) $(DEP) $(UAC_BRIDGE)

distclean: clean
	rm -rf lvgl

run: $(TARGET)
	./$(TARGET)
