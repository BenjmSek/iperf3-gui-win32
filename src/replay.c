/*
 * replay.c — offline log replay
 *
 * Loads a saved iperf3 log (plain text or -J JSON), turns it into a flat
 * array of per-interval samples, and feeds those samples into a ChartData
 * at an arbitrary cut-off index (cumulative curve).
 *
 * Text logs are parsed line-by-line with ParseIperf3Line (same parser as
 * live mode). JSON logs are a single enormous line, so they get their own
 * sequential scanner over the "intervals" array, reusing the low-level
 * JsonGetDouble/JsonGetInt helpers (exposed by iperf3.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "replay.h"
#include "iperf3.h"
#include "lang.h"

/* ===== sample array ===== */

static bool Replay_Append(ReplayData* rd, double time)
{
    if (!rd) return false;
    if (rd->count >= rd->capacity) {
        int newCap = rd->capacity > 0 ? rd->capacity * 2 : 256;
        ReplaySample* ns = (ReplaySample*)realloc(rd->samples,
                                                  newCap * sizeof(ReplaySample));
        if (!ns) return false;
        rd->samples = ns;
        rd->capacity = newCap;
    }
    ReplaySample* s = &rd->samples[rd->count++];
    memset(s, 0, sizeof(*s));
    s->time = time;
    return true;
}

void Replay_Free(ReplayData* rd)
{
    if (!rd) return;
    free(rd->samples);
    rd->samples = NULL;
    rd->count = 0;
    rd->capacity = 0;
    rd->isJson = false;
    rd->isClient = true;
    rd->isUdp = false;
    rd->axis = RA_NONE;
    rd->duration = 0;
    rd->streamOverflow = false;
}

/* Fill a sample from one interval's values */
static void FillSample(ReplaySample* s, double bps, double jitter,
                       double lossRate, double datagrams)
{
    s->sumBps = bps;
    s->jitter = jitter;
    s->lossRate = lossRate;
    s->datagrams = datagrams;
}

/* Derive axis mode / UDP flag / duration from the collected samples */
static void FinalizeMeta(ReplayData* rd)
{
    bool hasJitterLoss = false, hasDatagrams = false;
    for (int i = 0; i < rd->count; i++) {
        if (rd->samples[i].jitter > 0 || rd->samples[i].lossRate > 0)
            hasJitterLoss = true;
        if (rd->samples[i].datagrams > 0)
            hasDatagrams = true;
    }
    if (hasJitterLoss)      rd->axis = RA_JITTER_LOSS;
    else if (hasDatagrams)  rd->axis = RA_DATAGRAMS;
    else                    rd->axis = RA_NONE;
    rd->isUdp = (rd->axis != RA_NONE);
    rd->duration = (rd->count > 0) ? rd->samples[rd->count - 1].time : 0;
}

/* ===== text log ===== */

static bool ParseTextLog(const char* buf, ReplayData* rd,
                         char* errBuf, int errBufLen)
{
    const char* p = buf;
    char line[4096];
    int seenIds[REPLAY_MAX_STREAMS];   /* distinct streamId → slot map */
    int seenCount = 0;
    rd->streamOverflow = false;

    while (*p) {
        size_t n = 0;
        while (p[n] && p[n] != '\n' && n < sizeof(line) - 1) n++;
        memcpy(line, p, n);
        line[n] = 0;
        p += n;
        if (*p == '\n') p++;

        CleanOutput(line);

        /* Role detection: log header written by Log_Init, then content */
        if (strstr(line, "iperf3 GUI log: client")) { rd->isClient = true; continue; }
        if (strstr(line, "iperf3 GUI log: server")) { rd->isClient = false; continue; }
        if (strstr(line, "Connecting to host")) { rd->isClient = true; continue; }
        if (strstr(line, "Server listening")) { rd->isClient = false; continue; }

        Iperf3Data d;
        if (!ParseIperf3Line(line, false, &d)) continue;
        if (d.isSummary) continue;

        double t = d.intervalEnd;
        bool sameTime = (rd->count > 0) &&
                        (fabs(rd->samples[rd->count - 1].time - t) < 1e-9);

        int dgrams = d.totalPackets;
        if (dgrams == 0 && d.retransmits > 0) dgrams = d.retransmits;

        /* Resolve per-stream slot for this line (SUM = -1 → no stream slot).
           Distinct streamIds map to stable slots so colors stay consistent. */
        int slot = -1;
        if (d.streamId >= 0) {
            for (int k = 0; k < seenCount; k++) {
                if (seenIds[k] == d.streamId) { slot = k; break; }
            }
            if (slot < 0 && seenCount < REPLAY_MAX_STREAMS) {
                slot = seenCount;
                seenIds[seenCount++] = d.streamId;
            } else if (slot < 0) {
                rd->streamOverflow = true; /* >8 streams: only sum is drawn */
            }
        }
        /* Single distinct stream → treat as plain sum (no per-stream legend). */
        int eff = (seenCount <= 1) ? 0
                  : (seenCount > REPLAY_MAX_STREAMS ? REPLAY_MAX_STREAMS : seenCount);

        if (sameTime) {
            /* Lines repeating the current time: -P n emits per-stream lines
               then the authoritative [SUM] line. */
            ReplaySample* s = &rd->samples[rd->count - 1];
            if (d.streamId == -1) {
                FillSample(s, d.bps, d.jitter, d.lossRate, (double)dgrams);
            } else if (slot >= 0) {
                s->streamBps[slot] = d.bps;
            }
            s->streamCount = eff;
            continue;
        }
        /* Final summary lines repeat the last interval's end time — skip.
           Any line older than the playhead (bidir 2nd block restarting at 0)
           is also skipped. */
        if (rd->count > 0 && rd->samples[rd->count - 1].time > t + 1e-9)
            continue;

        if (!Replay_Append(rd, t)) {
            snprintf(errBuf, errBufLen, TA(STR_ERR_OOM));
            return false;
        }
        ReplaySample* s = &rd->samples[rd->count - 1];
        /* First line of a new interval: seed sum from it (correct for
           single-stream; for multi-stream a later [SUM] line overwrites). */
        FillSample(s, d.bps, d.jitter, d.lossRate, (double)dgrams);
        if (slot >= 0) s->streamBps[slot] = d.bps;
        s->streamCount = eff;
    }
    return true;
}

/* ===== JSON log ===== */

/* p points at '{'; returns pointer to its matching '}' (or NULL).
   Handles string literals with escapes. */
static const char* JsonSkipObject(const char* p)
{
    int depth = 0;
    bool inStr = false;
    for (; *p; p++) {
        if (inStr) {
            if (*p == '\\') { if (p[1]) p++; continue; }
            if (*p == '"') inStr = false;
            continue;
        }
        if (*p == '"') { inStr = true; continue; }
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) return p; }
    }
    return NULL;
}

/* Copy [a, b) into a NUL-terminated malloc'd buffer (caller frees) */
static char* DupRange(const char* a, const char* b)
{
    size_t n = (size_t)(b - a);
    char* s = (char*)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, a, n);
    s[n] = 0;
    return s;
}

static bool ParseJsonLog(const char* buf, ReplayData* rd,
                         char* errBuf, int errBufLen)
{
    rd->isJson = true;
    rd->streamOverflow = false;
    int seenCount = 0;   /* distinct streams across the whole log */

    /* --get-server-output embeds the server log (escaped JSON) in
       "server_output_json"; never scan past it. */
    const char* stop = strstr(buf, "\"server_output_json\"");

    const char* pi = strstr(buf, "\"intervals\"");
    if (pi && stop && pi > stop) pi = NULL;
    if (!pi) {
        snprintf(errBuf, errBufLen, TA(STR_ERR_JSON_NO_INTERVALS));
        return false;
    }
    const char* p = strchr(pi, '[');
    if (!p || (stop && p > stop)) {
        snprintf(errBuf, errBufLen, TA(STR_ERR_JSON_EMPTY_INTERVALS));
        return false;
    }
    p++;

    while (*p && *p != ']') {
        if (*p == '{') {
            const char* objEnd = JsonSkipObject(p);
            if (!objEnd) break;

            char* obj = DupRange(p, objEnd + 1);
            if (obj) {
                double endTime = 0, bps = 0, jitter = 0, loss = 0, dgrams = 0;
                bool have = false;

                /* Preferred: interval-level "sum" object (UDP and -P n).
                   Stream objects never carry a "sum" key, so the first hit
                   is the interval-level one. */
                const char* sumKey = strstr(obj, "\"sum\"");
                if (sumKey) {
                    const char* sb = strchr(sumKey, '{');
                    const char* se = sb ? JsonSkipObject(sb) : NULL;
                    if (sb && se) {
                        char* sub = DupRange(sb, se + 1);
                        if (sub) {
                            JsonGetDouble(sub, "end", &endTime);
                            JsonGetDouble(sub, "bits_per_second", &bps);
                            JsonGetDouble(sub, "jitter_ms", &jitter);
                            JsonGetDouble(sub, "lost_percent", &loss);
                            JsonGetDouble(sub, "packets", &dgrams);
                            have = true;
                            free(sub);
                        }
                    }
                } else if (strstr(obj, "\"bits_per_second\"")) {
                    /* Single-stream TCP: the interval object holds the data
                       directly (no "sum" wrapper). */
                    JsonGetDouble(obj, "end", &endTime);
                    JsonGetDouble(obj, "bits_per_second", &bps);
                    have = true;
                }

                /* Per-stream bandwidth from "streams": [ {...}, ... ].
                   Positional: streams[0] → slot 0, etc. (only when -P > 1) */
                double tmpStreamBps[REPLAY_MAX_STREAMS];
                int nStreams = 0;
                for (int z = 0; z < REPLAY_MAX_STREAMS; z++) tmpStreamBps[z] = 0;
                const char* streamsKey = strstr(obj, "\"streams\"");
                if (streamsKey) {
                    const char* ab = strchr(streamsKey, '[');
                    if (ab) {
                        const char* arrEnd = strchr(ab, ']');
                        if (arrEnd) {
                            const char* sp2 = ab + 1;
                            while (sp2 < arrEnd) {
                                if (*sp2 == '{') {
                                    const char* so = JsonSkipObject(sp2);
                                    if (!so) break;
                                    char* sObj = DupRange(sp2, so + 1);
                                    if (sObj) {
                                        double sbps = 0;
                                        if (JsonGetDouble(sObj, "bits_per_second", &sbps)) {
                                            int slot = -1;
                                            if (nStreams < REPLAY_MAX_STREAMS) {
                                                slot = nStreams++;
                                            } else {
                                                rd->streamOverflow = true;
                                                nStreams++;
                                            }
                                            if (slot >= 0) tmpStreamBps[slot] = sbps;
                                        }
                                        free(sObj);
                                    }
                                    sp2 = so + 1;
                                } else sp2++;
                            }
                        }
                    }
                }
                if (nStreams > seenCount) seenCount = nStreams;

                if (have && endTime >= 0 &&
                    (rd->count == 0 ||
                     endTime > rd->samples[rd->count - 1].time + 1e-9)) {
                    if (!Replay_Append(rd, endTime)) {
                        free(obj);
                        snprintf(errBuf, errBufLen, TA(STR_ERR_OOM));
                        return false;
                    }
                    ReplaySample* s = &rd->samples[rd->count - 1];
                    FillSample(s, bps, jitter, loss, dgrams);
                    for (int k = 0; k < nStreams && k < REPLAY_MAX_STREAMS; k++)
                        s->streamBps[k] = tmpStreamBps[k];
                    /* Single distinct stream → plain sum (no per-stream legend) */
                    int eff = (seenCount <= 1) ? 0
                              : (seenCount > REPLAY_MAX_STREAMS ? REPLAY_MAX_STREAMS : seenCount);
                    s->streamCount = eff;
                }
                free(obj);
            }
            p = objEnd + 1;
        } else {
            p++;
        }
    }

    if (rd->count == 0) {
        snprintf(errBuf, errBufLen, TA(STR_ERR_JSON_NO_DATA));
        return false;
    }
    return true;
}

/* ===== public API ===== */

/* The GUI writes its own header line (Log_Init) ahead of the iperf3 output:
   "=== iperf3 GUI log: client -> <target> === <timestamp> ===".
   Real logs therefore never start with '{' — skip the header before format
   detection, and use it for role detection (JSON logs have no other role
   cue). Returns a pointer to the first character after the header block. */
static const char* SkipGuiHeader(const char* q, ReplayData* rd)
{
    for (;;) {
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (q[0] != '=' || q[1] != '=' || q[2] != '=') break;
        const char* eol = strchr(q, '\n');
        const char* end = eol ? eol : q + strlen(q);
        size_t len = (size_t)(end - q);
        if (len > 255) len = 255;
        char hdr[256];
        memcpy(hdr, q, len);
        hdr[len] = 0;
        if (rd) {
            if (strstr(hdr, "iperf3 GUI log: client")) rd->isClient = true;
            else if (strstr(hdr, "iperf3 GUI log: server")) rd->isClient = false;
        }
        q = end;
    }
    return q;
}

bool Replay_LoadFile(const char* path, ReplayData* rd, char* errBuf, int errBufLen)
{
    if (errBuf && errBufLen > 0) errBuf[0] = 0;
    if (!rd || !path || !path[0]) {
        snprintf(errBuf, errBufLen, TA(STR_ERR_PATH_EMPTY));
        return false;
    }
    Replay_Free(rd);
    memset(rd, 0, sizeof(*rd));
    strncpy(rd->filePath, path, sizeof(rd->filePath) - 1);
    strcpy(rd->unit, "Mb/s");   /* auto display mode, same as live charts */
    rd->yScale = 1e6;
    rd->isClient = true;        /* default: client log (blue) when undetectable */

    /* Open via the wide path so non-ASCII directories work */
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
    FILE* f = _wfopen(wpath, L"rb");
    if (!f) {
        snprintf(errBuf, errBufLen, TA(STR_ERR_OPEN_FILE));
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        snprintf(errBuf, errBufLen, TA(STR_ERR_FILE_EMPTY));
        return false;
    }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        snprintf(errBuf, errBufLen, TA(STR_ERR_OOM));
        return false;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;

    /* Format detection: skip a UTF-8 BOM, skip the GUI's own log header
       line(s), then look at the first significant character. */
    const char* q = buf;
    if ((unsigned char)q[0] == 0xEF && (unsigned char)q[1] == 0xBB &&
        (unsigned char)q[2] == 0xBF) q += 3;
    q = SkipGuiHeader(q, rd);

    bool ok = (*q == '{')
        ? ParseJsonLog(buf, rd, errBuf, errBufLen)
        : ParseTextLog(buf, rd, errBuf, errBufLen);
    free(buf);

    if (!ok || rd->count == 0) {
        if (ok) snprintf(errBuf, errBufLen, TA(STR_ERR_NO_INTERVAL_DATA));
        Replay_Free(rd);
        return false;
    }
    FinalizeMeta(rd);
    return true;
}

void Replay_Seek(ReplayData* rd, ChartData* cd, int idx)
{
    if (!rd || !cd) return;

    Chart_Clear(cd);
    if (rd->count <= 0) return;
    if (idx < 0) idx = 0;
    if (idx >= rd->count) idx = rd->count - 1;

    cd->rightAxis = rd->axis;
    strncpy(cd->yUnit, rd->unit, sizeof(cd->yUnit) - 1);
    cd->yUnit[sizeof(cd->yUnit) - 1] = '\0';
    cd->yScale = rd->yScale;

    int streamCount = rd->samples[idx].streamCount;
    if (streamCount > REPLAY_MAX_STREAMS) streamCount = REPLAY_MAX_STREAMS;
    Chart_SetStreamCount(cd, streamCount);

    for (int i = 0; i <= idx; i++) {
        const ReplaySample* s = &rd->samples[i];
        Chart_AddPoint(cd, s->time, s->sumBps);
        Chart_AddUdpData(cd, s->jitter, s->lossRate, s->datagrams);
        for (int k = 0; k < streamCount; k++)
            Chart_AddStreamPoint(cd, k, s->streamBps[k]);
    }
}
