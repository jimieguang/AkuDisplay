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

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEP)

clean:
	rm -f $(TARGET) $(OBJ) $(DEP)

distclean: clean
	rm -rf lvgl

run: $(TARGET)
	./$(TARGET)
