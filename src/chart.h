#ifndef CHART_H
#define CHART_H

#include <windows.h>
#include <stdbool.h>

/* Simple line chart with GDI+ — drawn on a child window.
   Uses double buffering to avoid flicker. */

/* Right-axis mode determines what the right Y-axis displays */
typedef enum {
    RA_NONE = 0,        /* single Y axis — TCP, or UDP with no extra data */
    RA_DATAGRAMS,       /* right Y = datagram count (client UDP sender) */
    RA_JITTER_LOSS      /* right Y = dual ms (left of axis) / % (right of axis) (server UDP) */
} RightAxisType;

/* Max number of per-stream series drawn alongside the sum curve.
   The SUM series is stored in times[]/values[] (see below), so only the
   8 per-stream value arrays need to be kept here. */
#define CHART_MAX_STREAMS 8

typedef struct {
    double* times;     /* x-axis values (seconds) */
    double* values;    /* y-axis values (bps, raw) — SUM (TX) channel */
    double* values2;   /* y-axis values (bps, raw) — 2nd SUM (RX) channel, bidir only */
    int count;
    int capacity;
    DWORD  lineColor;  /* ARGB color for the line */
    DWORD  areaColor;  /* ARGB color for area fill (with alpha) */
    bool   isDark;     /* dark mode flag */
    char   yUnit[16];  /* Y-axis unit label, e.g. "Mb/s", "KB/s" */
    double yScale;     /* bps multiplier for display: value_displayed = bps / yScale */

    /* Right Y-axis mode */
    RightAxisType rightAxis;
    /* UDP data arrays (parallel to times[]) — filled by Chart_AddUdpData */
    double* jitter;        /* jitter (ms) — server UDP receiver */
    double* lossRate;      /* loss rate (%) — server UDP receiver */
    double* datagrams;     /* total datagrams per interval — client UDP sender */
    DWORD  jitterColor;    /* ARGB color for jitter line */
    DWORD  lossColor;      /* ARGB color for loss% line */
    DWORD  datagramColor;  /* ARGB color for datagram line */

    /* ===== Multi-stream series =====
       times[]/values[] hold the SUM series; series[i] holds stream (i+1)'s
       bps, parallel to times[] (so point k of series i is at times[k]).
       Rendered as thin lines when showStreams is on; when hideStreamSum is
       also on, the sum line/area/legend entry are suppressed and the Y range
       is taken from the visible series instead of values[].
       (Legend labels come from STR_FMT_STREAM at draw time — there is no
        cached per-series name field.) */
    double* series[CHART_MAX_STREAMS];
    DWORD  seriesColor[CHART_MAX_STREAMS];
    int    seriesCount;                         /* 0 = sum only */
    bool   showSum;                             /* sum curve (TX / values[]): 2px + area fill */
    bool   showStreams;                         /* per-stream thin lines */
    /* 隐藏汇总曲线（设置页「隐藏流汇总」）。仅当 showStreams && seriesCount>0
       时生效：不画汇总线/面积、图例不列汇总项，且 Y 量程改按各可见流取最大值。
       bidir 下 seriesCount 被置 0 ⇒ 自动不生效。 */
    bool   hideStreamSum;
    bool   seriesVisible[CHART_MAX_STREAMS];    /* legend click override */
    /* Bidirectional-mode dual sum lines (iperf3 --bidir). When true, the
       chart draws TWO summary lines instead of one, and per-stream lines are
       suppressed. values[] = TX direction, values2[] = RX direction. */
    bool   hasTwoSum;                           /* bidir active flag */
    bool   showSum2;                            /* RX / values2 line visible */
    DWORD  lineColor2;                          /* RX / values2 line color */

    /* Legend hit-test regions (chart-window client coords), recomputed every
       paint by Chart_Render so WM_LBUTTONDOWN (in ChartWndProc) can toggle
       stream visibility. legendHitIdx: -1 = 汇总(sum) line, 0..n-1 = per-stream. */
    int legendHitCount;
    int legendHitX[16], legendHitY[16], legendHitW[16], legendHitH[16];
    int legendHitIdx[16];
} ChartData;

/* Initialize chart data (call once per chart) */
#ifdef __cplusplus
extern "C" {
#endif

void Chart_Init(ChartData* cd, DWORD lineColor, DWORD areaColor);

/* Add a data point */
void Chart_AddPoint(ChartData* cd, double time, double value);

/* Add UDP secondary data for the latest point.
   Parameters: jitter (ms), lossRate (%), datagrams (count).
   Values not applicable to the current rightAxis mode are ignored. */
void Chart_AddUdpData(ChartData* cd, double jitter, double lossRate, double datagrams);

/* Set how many per-stream series exist (0..CHART_MAX_STREAMS).
   Resets their visibility to "all visible". Call before adding stream points. */
void Chart_SetStreamCount(ChartData* cd, int streamCount);

/* Attach a per-stream value to the point most recently added by Chart_AddPoint.
   streamIdx is 0-based (流1 = 0). Out-of-range indices are ignored. */
void Chart_AddStreamPoint(ChartData* cd, int streamIdx, double bps);

/* Clear all data points */
void Chart_Clear(ChartData* cd);

/* Render chart to HDC using GDI+.
   rc is the drawing rectangle. */
void Chart_Render(ChartData* cd, HDC hdc, RECT rc);

/* Free chart data resources */
void Chart_Free(ChartData* cd);

/* Initialize GDI+ (call once at startup) */
void Chart_GdiplusInit(void);

/* Shutdown GDI+ (call once at exit) */
void Chart_GdiplusShutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* CHART_H */
