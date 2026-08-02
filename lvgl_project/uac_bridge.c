#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <math.h>

/* ======================================================================
 * uac_bridge v0.8 — dual-thread BLOCKING I/O + host (UAC) volume stage
 *
 *   capture  hw:1,0 (f_uac1 gadget)  ---->  ring  ---->  playback  hw:0,0 (sun4i codec)
 *
 * v0.7 strips all the complexity added from v0.3 onwards (non-blocking,
 * frame accumulator, output FIFO, resampling, adaptive clock sync) because
 * every one of those layers introduced audible artifacts that v0.2 (pure
 * blocking I/O) did not have.
 *
 * v0.8: the UAC Feature Unit volume/mute exposed by f_uac1/f_uac2 (u_audio.c)
 * is only a control-state mirror in the v6.1 kernel: the gadget never applies
 * gain in the data path, and Windows does not scale the PCM it sends over USB
 * for UAC endpoints. The bridge therefore applies the host's volume itself by
 * reading the "PCM Capture Volume" / "PCM Capture Switch" controls of card 1
 * (0..100 maps to -100..0 dB) and scaling the samples before the ring.
 * Local device volume (Power Amplifier) stays an independent hardware gain.
 *
 * The one fix carried over from v0.3+ : on SIGTERM we call snd_pcm_drop()
 * to force blocked readi/writei to return (previously pthread_join hung
 * forever when the USB host was idle). v0.2.1 had the same idea but the
 * eval "$CMD" & bug (orphaned bridge process, kill hit a shell, not the
 * real binary) masked the fix — with direct $CMD & it should work.
 *
 * Trade-off: no clock sync — the sun4i codec runs ~47.7kHz while the host
 * sends 48kHz. Ring fills gradually → periodic drops (≈ every 1.8s with
 * 16-period ring). Acceptable for now; clock sync can be added as a v2
 * feature without touching the I/O pathway.
 * ====================================================================== */

#define CAP_DEV   "hw:1,0"
#define PLAY_DEV  "hw:0,0"
#define RATE      48000
#define CH        2

static volatile sig_atomic_t quit = 0;
static void on_term(int s) { (void)s; quit = 1; }

static long g_reads = 0, g_writes = 0, g_xruns = 0;
static long g_drops = 0, g_dups = 0;
static const char *g_dump_path = NULL;   /* UAC_BRIDGE_DUMP=/path: debug dump of post-gain samples */

/* ---------------- UAC host volume (control mirror -> gain) ---------------- */
static snd_ctl_t *g_ctl = NULL;
static int g_vol_numid = -1;
static int g_sw_numid  = -1;
static _Atomic float g_gain = 1.0f;
static int g_poll_cnt = 0;

static int ctl_resolve(snd_ctl_t *ctl, const char *name, int *numid)
{
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_id_set_index(id, 0);
    snd_ctl_elem_info_alloca(&info);
    for (int trial = 0; trial <= 1; trial++) {
        snd_ctl_elem_id_set_numid(id, trial ? -1 : 0);
        snd_ctl_elem_info_set_id(info, id);
        if (snd_ctl_elem_info(ctl, info) == 0) {
            *numid = (int)snd_ctl_elem_info_get_numid(info);
            return 0;
        }
    }
    return -1;
}

static void vol_poll(void)
{
    snd_ctl_elem_value_t *val;
    long v = 100;
    int muted = 0;

    if (!g_ctl || g_vol_numid < 0)
        return;

    snd_ctl_elem_value_alloca(&val);
    snd_ctl_elem_value_set_numid(val, (unsigned)g_vol_numid);
    if (snd_ctl_elem_read(g_ctl, val) == 0)
        v = snd_ctl_elem_value_get_integer(val, 0);
    if (v < 0) v = 0; else if (v > 100) v = 100;

    if (g_sw_numid >= 0) {
        snd_ctl_elem_value_set_numid(val, (unsigned)g_sw_numid);
        if (snd_ctl_elem_read(g_ctl, val) == 0)
            muted = snd_ctl_elem_value_get_integer(val, 0) ? 0 : 1;
    }

    /* 0..100 -> -100..0 dB (1 dB per step), mute -> silence */
    float gain = muted ? 0.0f : powf(10.0f, (float)(v - 100) / 20.0f);
    float prev = atomic_load(&g_gain);
    if (fabsf(gain - prev) > 0.001f) {
        fprintf(stderr, "uac_bridge: host vol=%ld%s gain=%.4f\n",
                v, muted ? " MUTE" : "", gain);
    }
    atomic_store(&g_gain, gain);
}

/* ---------------- ring buffer (SPSC, 16 periods) ---------------- */
static short *g_ring;
static int    g_period;
static int    g_ring_n = 16;
static _Atomic int g_head = 0;
static _Atomic int g_tail = 0;

/* ---------------- ALSA setup ---------------- */
static int setup_pcm(snd_pcm_t *p, int period, int bufsz)
{
    snd_pcm_hw_params_t *hp;
    snd_pcm_hw_params_alloca(&hp);
    int r = snd_pcm_hw_params_any(p, hp);
    if (r < 0) { fprintf(stderr, "hw_params_any: %s\n", snd_strerror(r)); return -1; }
    snd_pcm_hw_params_set_access(p, hp, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(p, hp, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(p, hp, CH);
    if ((r = snd_pcm_hw_params_set_rate(p, hp, RATE, 0)) < 0) {
        fprintf(stderr, "set_rate: %s\n", snd_strerror(r)); return -1;
    }
    snd_pcm_hw_params_set_period_size(p, hp, period, 0);
    snd_pcm_hw_params_set_buffer_size(p, hp, bufsz);
    if ((r = snd_pcm_hw_params(p, hp)) < 0) {
        fprintf(stderr, "hw_params: %s\n", snd_strerror(r)); return -1;
    }
    /* BLOCKING mode for clean, glitch-free audio */
    return 0;
}

/* ---------------- capture thread ---------------- */
static void *capture_thread(void *arg)
{
    snd_pcm_t *cap = (snd_pcm_t *)arg;
    int period = g_period;
    short *tmp = malloc((size_t)period * CH * 2);

    while (!quit) {
        int n = snd_pcm_readi(cap, tmp, period);
        if (n < 0) {
            if (n == -EPIPE || n == -ESTRPIPE) g_xruns++;
            snd_pcm_recover(cap, n, 1);
            continue;
        }

        /* Poll the UAC control mirror ~10x/s and apply host volume. */
        if (++g_poll_cnt >= 37) {   /* 37 periods ~ 100ms @ 2.7ms/period */
            g_poll_cnt = 0;
            vol_poll();
        }
        float gain = atomic_load(&g_gain);
        if (gain < 0.9995f || gain > 1.0005f) {
            int ns = n * CH;
            for (int i = 0; i < ns; i++) {
                float s = (float)tmp[i] * gain;
                if (s > 32767.0f) s = 32767.0f;
                else if (s < -32768.0f) s = -32768.0f;
                tmp[i] = (short)s;
            }
        }
        if (g_dump_path) {
            FILE *f = fopen(g_dump_path, "ab");
            if (f) {
                fwrite(tmp, 2, (size_t)n * CH, f);
                fclose(f);
            }
        }

        int h = atomic_load(&g_head);
        int t = atomic_load(&g_tail);
        if (h - t >= g_ring_n) {              /* ring full: drop oldest */
            atomic_fetch_add(&g_tail, 1);
            g_drops++;
        }
        memcpy(g_ring + (size_t)(h % g_ring_n) * period * CH, tmp,
               (size_t)period * CH * 2);
        atomic_store(&g_head, h + 1);
        g_reads++;
    }
    free(tmp);
    return NULL;
}

/* ---------------- playback thread ---------------- */
static void *playback_thread(void *arg)
{
    snd_pcm_t *play = (snd_pcm_t *)arg;
    int period = g_period;
    short *sil = calloc(1, (size_t)period * CH * 2);

    while (!quit) {
        int h = atomic_load(&g_head);
        int t = atomic_load(&g_tail);
        short *src;

        if (t < h) {
            src = g_ring + (size_t)(t % g_ring_n) * period * CH;
        } else {
            src = sil;
        }

        int n = snd_pcm_writei(play, src, period);
        if (n < 0) {
            if (n == -EPIPE || n == -ESTRPIPE) g_xruns++;
            snd_pcm_recover(play, n, 1);
            continue;
        }
        if (t < h) atomic_store(&g_tail, t + 1);
        else       g_dups++;
        g_writes++;
    }
    free(sil);
    return NULL;
}

/* ---------------- main ---------------- */
int main(int argc, char **argv)
{
    int period = (argc > 1) ? atoi(argv[1]) : 128;
    int rt = (argc > 2) ? atoi(argv[2]) : 0;

    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    g_dump_path = getenv("UAC_BRIDGE_DUMP");
    if (g_dump_path)
        fprintf(stderr, "uac_bridge: dumping post-gain samples to %s\n", g_dump_path);

    snd_pcm_t *cap = NULL, *play = NULL;
    int r;
    if ((r = snd_pcm_open(&cap, CAP_DEV, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "open %s: %s\n", CAP_DEV, snd_strerror(r)); return 1;
    }
    if ((r = snd_pcm_open(&play, PLAY_DEV, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "open %s: %s\n", PLAY_DEV, snd_strerror(r)); return 1;
    }
    if (setup_pcm(cap, period, period * 8) < 0) return 1;
    if (setup_pcm(play, period, period * 8) < 0) return 1;
    snd_pcm_prepare(cap);
    snd_pcm_prepare(play);

    /* Resolve the UAC Feature Unit mirror controls on card 1. */
    if (snd_ctl_open(&g_ctl, "hw:1", 0) == 0) {
        if (ctl_resolve(g_ctl, "PCM Capture Volume", &g_vol_numid) == 0) {
            ctl_resolve(g_ctl, "PCM Capture Switch", &g_sw_numid);
            vol_poll();
            fprintf(stderr, "uac_bridge: UAC vol ctl numid=%d%s\n",
                    g_vol_numid, g_sw_numid >= 0 ? " + mute" : " (no mute)");
        } else {
            snd_ctl_close(g_ctl);
            g_ctl = NULL;
            fprintf(stderr, "uac_bridge: WARNING no 'PCM Capture Volume' on hw:1, unity gain\n");
        }
    } else {
        fprintf(stderr, "uac_bridge: WARNING cannot open ctl hw:1, unity gain\n");
    }

    g_period = period;
    g_ring = malloc((size_t)g_ring_n * period * CH * 2);
    memset(g_ring, 0, (size_t)g_ring_n * period * CH * 2);

    struct sched_param sp;
    if (rt) {
        memset(&sp, 0, sizeof sp);
        sp.sched_priority = 50;
    }

    pthread_t ta, tb;
    pthread_create(&ta, NULL, capture_thread, cap);
    pthread_create(&tb, NULL, playback_thread, play);

    if (rt) {
        pthread_setschedparam(ta, SCHED_FIFO, &sp);
        pthread_setschedparam(tb, SCHED_FIFO, &sp);
    }

    fprintf(stderr, "uac_bridge v0.8 running: period %d (%.1fms), ring %d periods\n",
            period, 1000.0 * period / RATE, g_ring_n);

    while (!quit) usleep(200000);

    /* Unblock the worker threads: snd_pcm_drop forces any blocked I/O to
     * return immediately. Without this, SIGTERM sets quit=1 but a thread
     * stuck in readi/writei never sees it -> join hangs forever. */
    snd_pcm_drop(cap);
    snd_pcm_drop(play);

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    fprintf(stderr, "uac_bridge stopped: reads=%ld writes=%ld xruns=%ld drops=%ld dups=%ld\n",
            g_reads, g_writes, g_xruns, g_drops, g_dups);

    free(g_ring);
    snd_pcm_close(cap);
    snd_pcm_close(play);
    return 0;
}
