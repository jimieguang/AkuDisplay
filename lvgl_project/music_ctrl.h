#ifndef MUSIC_CTRL_H
#define MUSIC_CTRL_H

/* ======================================================================
 * music_ctrl.h / music_ctrl.c
 * Background client for the "FlipPanel Bridge" PC-side music agent
 * (https://github.com/jimieguang/FlipPanel).
 *
 * The bridge, running on a Windows PC on the same LAN, does two things:
 *   1. UDP-broadcasts a discovery JSON every ~1s on port 50570, e.g.
 *      {"protocolVersion":2,"deviceName":"PC-NAME",
 *       "hostAddress":"192.168.1.50","hostPort":50571,
 *       "endpoint":"ws://192.168.1.50:50571/ws", ...}
 *   2. Serves a WebSocket at ws://<hostAddress>:50571/ws that
 *      - pushes "status" messages (musicTitle/musicArtist/musicPlaybackState)
 *      - accepts command messages of the form
 *        {"messageType":"command","actionId":"music.playPause","value":null}
 *
 * This module runs a single background pthread that auto-discovers the
 * bridge, keeps the WebSocket open, mirrors the now-playing status into a
 * mutex-guarded snapshot, and sends transport commands queued by the UI.
 * All public calls are safe to invoke from the LVGL main thread.
 * ====================================================================== */

typedef struct {
    int  connected;      /* 1 = WebSocket currently open to a bridge      */
    char device[64];     /* bridge deviceName (usually ASCII hostname)    */
    char title[160];     /* now-playing track title (UTF-8, may be CJK)   */
    char artist[160];    /* now-playing artist (UTF-8)                    */
    char state[24];      /* playback state: "Playing"/"Paused"/"Stopped"  */
} mc_status_t;

/* Start the background networking thread. Idempotent (safe to call once
 * from main() at startup). */
void music_ctrl_init(void);

/* Queue a transport command by its FlipPanel action id, e.g.
 * "music.playPause", "music.next", "music.previous". Non-blocking; the
 * command is sent by the worker thread on its next loop iteration. */
void music_ctrl_action(const char *action_id);

/* Copy the latest status snapshot into *out. Cheap and thread-safe. */
void music_ctrl_get(mc_status_t *out);

/* Returns a monotonically increasing generation counter that bumps each time
 * the worker receives a new status frame from the bridge. The UI can poll
 * this every main-loop iteration (5ms) and refresh only when it changes,
 * giving near-instant play/pause feedback without waiting for the 1s tick. */
unsigned music_ctrl_generation(void);

/* Signal the worker thread to stop and wait briefly for it to exit.
 * Call once during graceful shutdown (e.g. on SIGTERM). After this call
 * the module is inert; no further network activity occurs. */
void music_ctrl_shutdown(void);

#endif /* MUSIC_CTRL_H */
