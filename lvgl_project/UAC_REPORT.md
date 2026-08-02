# UAC 音频通路优化报告（alsaloop → 阻塞式桥接）

日期：2026-07-31（设备 18:27-18:44 实测）
设备：akubox，ARMv7 单核，Ubuntu 22.04，ST7735S

## 〇、最终结论（v0.7 定稿）

**纯阻塞 I/O 双线程桥接器**：
- CPU：~2-4%（对比 alsaloop 45-100%）
- 延迟：2.7ms（period 可调低至 1.3ms）
- **音质：与 v0.2 一致（纯净）**——v0.3-v0.6 的非阻塞/累积器/FIFO/重采样层全部引入可闻底噪，已全部移除
- 退出：`snd_pcm_drop` 解阻塞，SIGTERM 后 <100ms 退出（89ms 实测）
- 完整启停：解绑 uac + 恢复 ADB 安全（218ms 实测），无设备冻结

## 一、背景

原 UAC 音频链路使用 `audio_start.sh` + **alsaloop** 将 USB 音频（f_uac1 gadget, hw:1,0）
搬运到 sun4i codec（hw:0,0，功放输出）。实测 alsaloop 存在严重 CPU 问题：
- 空闲（host 连接未播放）：~45% 单核
- 播放中：~100% 单核（曾导致设备重启）
- 另有 200ms fork+exec 重启循环隐患

根因：alsaloop 使用 **poll 轮询架构**，而 UAC 等时传输使 capture 侧数据"总是就绪"，
poll 每次返回立即处理，等于高频忙轮询。加大缓冲收益有限（47%→36%）。

## 二、验证过程

### 1. alsaloop 行为实测（对照）

| 状态 | CPU | 表现 |
|------|-----|------|
| 空闲 | ~45% | `do_sys_poll` 睡眠但高频唤醒 |
| 播放中 | ~100% | 单核饱和 |
| 停止播放瞬间 | 尖峰 | 3 次 `underrun for playback hw:0,0`（可恢复，不退出） |

### 2. 阻塞 read 可行性验证（probe 程序）

`probe_block.c`：`snd_pcm_readi` 阻塞读 gadget capture PCM：

| period | 等待时间 | 阻塞判定 | CPU（2000次read） |
|--------|---------|---------|-------------------|
| 64 (1.3ms) | ~1.3ms | 真阻塞 | 2.6% |
| 128 (2.7ms) | ~2.7ms | 真阻塞 | 1.5% |
| 256 (5.3ms) | ~5.3ms | 真阻塞 | 15.7%（初始化占比大） |

**结论：read 由中断驱动唤醒，等待时间精确等于 period 时长，无忙等待。**

### 3. v0.1 单线程实机验证

单线程阻塞 read→write（capture hw:1,0 → playback hw:0,0，48kHz/2ch/S16，period 128）

**运行统计（52 秒，含用户播放/暂停/切歌全程）：**

| 指标 | 数值 | 对比 alsaloop |
|------|------|--------------|
| 空闲 CPU | **2.0%** | 45%（↓22倍） |
| 平均 CPU（含播放） | **4.4%** | ~100%（↓22倍） |
| 数据延迟 | **2.7ms**（period） | 80ms（↓30倍） |
| 系统负载 | **0.21** | 1.08-4.44 |
| 进程状态 | S（睡眠） | R/S 高频切换 |
| 读次数 | 15498 | - |
| xrun | 2503 | - |

**用户实听结论：播放/暂停/恢复/切歌全程正常，无异常。**

### 4. v0.2 双线程+环形缓冲实机验证

双线程（capture 阻塞读 → 16 周期环形缓冲 → playback 阻塞写），
环形缓冲吸收两端速率抖动，水位监控处理时钟漂移。

**运行统计（111 秒，用户播放/暂停/切歌全程）：**

| 指标 | v0.1 单线程 | v0.2 双线程+ring |
|------|------------|-----------------|
| xrun | 2503 / 52s | **6 / 111s**（↓99.8%） |
| 平均 CPU | 4.4% | **2.9%** |
| 有效播放时长 | - | ~12s（reads=4471） |
| 静音填充(dups) | - | 42463（空闲期保持流） |
| 丢弃(drops) | - | 0（16 周期缓冲充足） |
| 系统负载 | 0.21 | **0.16** |

**行为修正**：PC 未播放时 capture 侧**无数据**（read 阻塞，0 CPU），
playback 侧持续写静音保持 codec 流活跃（dups 计数），
播放时数据正常流过环形缓冲。这比预期更好——空闲时是真正 0 负载。

## 三、结论

阻塞式桥接器完全可行，效果远超 alsaloop：
- **CPU 降低约 15-35 倍**（解决单核饱和/重启隐患）
- **延迟降低约 30 倍**（80ms → 2.7ms，满足"低延迟最优"）
- **xrun 近乎消除**（双线程环形缓冲，2503 → 6）
- 空闲时由内核中断驱动，无任何忙等待

## 四、版本演进与教训

| 版本 | 架构 | 音质 | 问题 |
|------|------|------|------|
| v0.1 | 单线程阻塞 | 正常 | xrun 2503/52s |
| v0.2 | 双线程阻塞+ring | **正常（基准）** | 退出卡死（阻塞 I/O 无法被信号解除） |
| v0.3 | 非阻塞+wait | 无声 | wait 在 PREPARED 态不返回（死锁） |
| v0.4 | +帧累积器+pend | 很差 | 时钟漂移 drop/dup 咔哒 |
| v0.5 | +重采样直写 | 嘈杂混乱 | frac 与写入进度耦合 → 采样点错位 |
| v0.6 | 重采样+FIFO 解耦 | 底噪 | 非阻塞通路时序抖动 + 自适应 bug |
| v0.6.1 | 自适应修复 | 仍差 | 非阻塞 I/O 固有缺陷 |
| **v0.7** | **纯阻塞（回归 v0.2）** | **正常** | 仅保留 snd_pcm_drop 退出修复 |

**核心教训**：
1. 阻塞 I/O 由内核中断驱动，时序精确；非阻塞+wait/累积器由用户态调度驱动，帧级抖动 → 底噪（统计健康≠音质健康）
2. 修 bug 遵循最小改动：v0.2 音质好 → 只需修退出，不该重写 I/O 架构
3. 设备冻结根因链：`eval "$CMD" &` 孤儿化 bridge → PCM 未释放 → 解绑挂起；v0.7 修复 eval + drop 后，完整解绑+ADB 恢复安全（218ms）

## 五、已解决与待办

### 已解决
- xrun 风暴：双线程+ring（v0.2）
- 退出卡死：snd_pcm_drop 解阻塞（v0.7，89ms）
- 设备冻结：eval bug + PCM 释放顺序（v0.7）
- 完整启停：解绑 uac + ADB 恢复（v0.7，218ms 验证）
- 音质：纯阻塞回归，与 v0.2 一致

### 待办
1. **长测**：连续播放 1 小时+，验证稳定性
2. **时钟漂移**：codec 47.7k vs 48k，ring 满每 ~1.8s drop 一次（2.7ms 偶发跳过，用户可接受）。如需消除：在阻塞 I/O 基础上做 ALSA rate-shift 或 PLL 微调（勿重蹈非阻塞覆辙）
3. 更新 HANDOFF.md（v0.7 状态）

## 六、文件清单

```
lvgl_project/uac_bridge.c     桥接器 v0.7（纯阻塞双线程 + snd_pcm_drop 退出）
lvgl_project/probe_block.c    阻塞 read 验证程序
lvgl_project/uac/             部署脚本（audio_start.sh v2 / audio_stop.sh v3 / uacFunc.sh 幂等 + orig 备份）
lvgl_project/UAC_REPORT.md    本报告
```

注：v0.7 已集成进 `/opt/aku/web/audio_start.sh`（uac_bridge 优先，alsaloop 回退）。

---

## 七、补充（2026-08-02）：v0.8 让 Windows 原生滑块真正控制设备音量

### 背景与机制

UAC Feature Unit 暴露的音量控件（`PCM Capture Volume`，0..100 → −100..0 dB）在 v6.1 内核里
**只是状态镜像**：`u_audio.c` 的 `prm->volume/mute` 只被控制回调读写，数据通路
（`u_audio_iso_complete`）从不使用；同时实测 Windows 也不缩放发往 USB 的 PCM（滑块只写
SET_CUR，增益委托给设备侧）。因此 v0.7 及以前，Windows 滑块**听感无效**。

v0.8 在 `uac_bridge` 数据通路（capture 线程入 ring 前）读取 card1 `PCM Capture Volume`
与 `PCM Capture Switch`，按 `gain = 10^((v-100)/20)` 应用采样增益/静音，每 ~100ms 轮询一次。
链路：Windows 滑块 → SET_CUR(FU) → ALSA 镜像 → bridge 增益 → codec → 扬声器。

### 实测（bridge 输出采样 dump，即送扬声器的数据）

| Windows 滑块 | bridge 输出 RMS | bridge 日志增益 |
|---|---|---|
| 100% | 6398（满幅） | 1.0000 |
| 50% | 1811（0.283×，精确对应 −11dB） | 0.2818 |
| 0% | 0 | 0.0000 |
| 静音 | 0 | 0.0000 |

UAC1/UAC2 均生效（同一 `u_audio` 控件层，按名字解析）。设备 UI 音量键仍只控制
`Power Amplifier`（本地 0..63），与 Windows 滑块互不干扰。

### 新发现的坑

1. **musb 热解绑/重绑会弄坏 USB OUT 端**：多次 unbind/rebind UDC 后 Windows 播放端点
   失效（`AUDCLNT_E_DEVICE_INVALIDATED`，采集/控件正常、唯独播放打不开），重启设备恢复。
2. **uac2 实验 gadget 采集方向需 48kHz**：原 64kHz 下 bridge（严格 48kHz、无重采样）无法工作，
   已改为 48kHz；重建脚本见 `uac/uac2_setup.sh`（实验性质，未纳入开机自启）。

### 文件清单更新

```
lvgl_project/uac_bridge.c     桥接器 v0.8（v0.7 + 主机音量/静音增益，可选 UAC_BRIDGE_DUMP 调试转储）
lvgl_project/uac/uac2_setup.sh  实验：48kHz UAC2 gadget 一键重建（未转产）
lvgl_project/Makefile          新增 uac_bridge / bridge-install 目标
```
