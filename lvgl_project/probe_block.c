#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>

/* Blocking-read probe for the UAC gadget capture PCM (hw:1,0).
 * Goal: verify that snd_pcm_readi() truly BLOCKS until USB audio arrives
 * (vs. returning immediately = busy loop, like alsaloop's poll loop).
 * Also measures per-read latency and total CPU time. */

static long long now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

int main(int argc, char **argv)
{
    const char *dev   = "hw:1,0";
    int   rate        = 48000;
    int   ch          = 2;
    int   frames      = 128;          /* period: 128 frames = 2.67ms @48k */
    int   iters       = 100;
    int   r;

    if (argc > 1) frames = atoi(argv[1]);
    if (argc > 2) iters  = atoi(argv[2]);

    snd_pcm_t *pcm = NULL;
    r = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_CAPTURE, 0);
    if (r < 0) { fprintf(stderr, "open %s failed: %s\n", dev, snd_strerror(r)); return 1; }

    snd_pcm_hw_params_t *hp;
    snd_pcm_hw_params_alloca(&hp);
    snd_pcm_hw_params_any(pcm, hp);
    snd_pcm_hw_params_set_access(pcm, hp, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hp, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, hp, ch);
    r = snd_pcm_hw_params_set_rate(pcm, hp, rate, 0);
    if (r < 0) { fprintf(stderr, "set_rate failed: %s\n", snd_strerror(r)); return 1; }
    snd_pcm_hw_params_set_period_size(pcm, hp, frames, 0);
    snd_pcm_hw_params_set_buffer_size(pcm, hp, frames * 4);
    r = snd_pcm_hw_params(pcm, hp);
    if (r < 0) { fprintf(stderr, "hw_params failed: %s\n", snd_strerror(r)); return 1; }

    snd_pcm_prepare(pcm);

    short *buf = malloc((size_t)frames * ch * 2);
    long long t_start = now_us();
    long long t_last = t_start;
    long long max_wait = 0, min_wait = 999999999, sum_wait = 0;
    int immediate = 0;

    for (int i = 0; i < iters; i++) {
        long long t0 = now_us();
        int n = snd_pcm_readi(pcm, buf, frames);
        long long t1 = now_us();
        long long wait = t1 - t0;

        if (n < 0) {
            printf("read %d: ERROR %s\n", i, snd_strerror(n));
            snd_pcm_recover(pcm, n, 1);
            continue;
        }
        if (wait < 200) immediate++;          /* <0.2ms = returned without waiting */
        if (wait > max_wait) max_wait = wait;
        if (wait < min_wait) min_wait = wait;
        sum_wait += wait;
        if (i < 8 || i == iters - 1)
            printf("read %3d: %d frames, wait %5lld us (gap %5lld us)\n",
                   i, n, wait, t0 - t_last);
        t_last = t1;
    }

    long long total = now_us() - t_start;
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    long long cpu_us = ru.ru_utime.tv_sec * 1000000LL + ru.ru_utime.tv_usec
                     + ru.ru_stime.tv_sec * 1000000LL + ru.ru_stime.tv_usec;

    printf("\n=== summary ===\n");
    printf("period %d frames, %d reads\n", frames, iters);
    printf("wall   %lld us (%.1f s)\n", total, total / 1e6);
    printf("cpu    %lld us (%.1f%% of wall)\n", cpu_us, 100.0 * cpu_us / total);
    printf("wait   avg %lld us, min %lld, max %lld\n",
           sum_wait / (iters ? iters : 1), min_wait, max_wait);
    printf("immediate(<0.2ms) reads: %d/%d\n", immediate, iters);
    printf("=> %s\n", immediate > iters / 2
           ? "read NEVER blocks (busy loop)" : "read BLOCKS properly");

    free(buf);
    snd_pcm_close(pcm);
    return 0;
}
