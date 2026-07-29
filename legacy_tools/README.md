# legacy_tools

来自原 [AkuDisplay](https://github.com/jimieguang/AkuDisplay)（sysboot 时代组件仓库，
完整源码见 `legacy-sysboot` tag）中**仍被本 LVGL 项目沿用**的三个文件，内容未做任何修改：

| 文件 | 用途 |
|---|---|
| `play_bmp_sequence.c` | 设备端动画播放器源码。`lvgl_aku` 通过 `/opt/aku/web/play_bmp_sequence` 播放开机动画与 Emotion 表情。编译：`gcc play_bmp_sequence.c -o play_bmp_sequence -lm` |
| `stb_image.h` | `play_bmp_sequence.c` 的图像解码依赖（[stb](https://github.com/nothings/stb) 单头文件库） |
| `gif_to_bmp.py` | GIF → BMP 序列转换工具，用于制作 `booting/`、`emotions/` 动画资产 |
