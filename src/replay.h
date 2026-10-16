#ifndef REPLAY_H
#define REPLAY_H

#include <windows.h>
#include <stdbool.h>
#include "chart.h"

/* Offline replay of a saved iperf3 log.
   The log format (text vs -J JSON) is auto-detected on load; the parsed
   result is a flat array of per-interval samples that can be fed into a
   ChartData at any cut-off index (cumulative curve). */

#define REPLAY_MAX_STREAMS 8

typedef struct {
    double time;                          /* interval end (seconds) */
    double sumBps;                        /* SUM series bandwidth (bits/s) */
    double streamBps[REPLAY_MAX_STREAMS]; /* per-stream bandwidth */
    int    streamCount;                   /* 0 = sum only */
    double jitter;                        /* ms, 0 = not applicable */
    double lossRate;                      /* %, 0 = not applicable */
    double datagrams;                     /* count, 0 = not applicable */
} ReplaySample;

typedef struct {
    ReplaySample* samples;
    int    count, capacity;
    bool   isJson;      /* -J JSON log (instead of plain text) */
    bool   isClient;    /* true = client log (blue), false = server log (green) */
    bool   isUdp;       /* UDP test → right axis carries jitter/loss/datagrams */
    bool   streamOverflow; /* >8 distinct streams seen (only sum drawn + note) */
    RightAxisType axis; /* RA_NONE / RA_DATAGRAMS / RA_JITTER_LOSS */
    char   unit[16];    /* Y-axis unit label, e.g. "Mb/s" */
    double yScale;      /* bps → display unit divisor */
    double duration;    /* last sample time (seconds) */
    char   filePath[MAX_PATH];
} ReplayData;

/* Load a log file and populate rd. Auto-detects text vs JSON.
   Returns false (with a message in errBuf, if supplied) on failure. */
bool Replay_LoadFile(const char* path, ReplayData* rd, char* errBuf, int errBufLen);

/* Free all samples */
void Replay_Free(ReplayData* rd);

/* Rebuild cd from samples [0..idx] (cumulative curve) and apply the
   axis/unit/scale metadata. idx is clamped to [0, count-1]. */
void Replay_Seek(ReplayData* rd, ChartData* cd, int idx);

#endif /* REPLAY_H */
