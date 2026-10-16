/*
 * chart.cpp — GDI+ bandwidth line chart with double buffering
 * Compiled as C++ (g++) for GDI+ class support
 */
#include <windows.h>

/* Define PROPID if missing (MinGW GDI+ header issue) */
#ifndef PROPID
typedef ULONG PROPID;
#endif

#include <gdiplus.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "chart.h"
#include "lang.h"

/* GDI+ startup token */
static ULONG_PTR g_gdiplusToken = 0;

/* Per-stream palette (ARGB). Chosen to stay clear of the UDP right-axis
   colors (orange FF8C00 / red FF4444 / teal 0088AA). */
static const DWORD g_streamPalette[CHART_MAX_STREAMS] = {
    0xFF7C3AED,  /* 流1 紫 */
    0xFFDB2777,  /* 流2 玫红 */
    0xFF0D9488,  /* 流3 墨绿 */
    0xFF9333EA,  /* 流4 亮紫 */
    0xFF2563EB,  /* 流5 宝石蓝 */
    0xFF4D7C0F,  /* 流6 橄榄 */
    0xFFB45309,  /* 流7 赭棕 */
    0xFF4B5563   /* 流8 灰 */
};

/* Initialize GDI+ (call once at startup) */
void Chart_GdiplusInit(void)
{
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, NULL);
}

/* Shutdown GDI+ (call once at exit) */
void Chart_GdiplusShutdown(void)
{
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

/* ===== ChartData implementation ===== */

void Chart_Init(ChartData* cd, DWORD lineColor, DWORD areaColor)
{
    if (!cd) return;
    memset(cd, 0, sizeof(*cd));
    cd->capacity = 256;
    cd->times = (double*)malloc(cd->capacity * sizeof(double));
    cd->values = (double*)malloc(cd->capacity * sizeof(double));
    cd->values2 = (double*)malloc(cd->capacity * sizeof(double));
    cd->jitter = (double*)malloc(cd->capacity * sizeof(double));
    cd->lossRate = (double*)malloc(cd->capacity * sizeof(double));
    cd->datagrams = (double*)malloc(cd->capacity * sizeof(double));
    cd->lineColor = lineColor;
    cd->areaColor = areaColor;
    /* Default: Mbits/sec (auto mode) */
    strcpy(cd->yUnit, "Mb/s");
    cd->yScale = 1e6;
    cd->rightAxis = RA_NONE;
    /* UDP secondary axis colors: jitter=orange, loss%=red, datagrams=teal */
    cd->jitterColor = 0xFFFF8C00;    /* ARGB: orange */
    cd->lossColor = 0xFFFF4444;      /* ARGB: red */
    cd->datagramColor = 0xFF0088AA;  /* ARGB: teal */

    /* Multi-stream series: default = sum only, all streams visible */
    cd->seriesCount = 0;
    cd->showSum = true;
    cd->showStreams = false;
    /* Bidir dual-sum defaults: inactive; RX line uses a contrasting amber so
       it reads clearly against the TX (client blue / server green) line. */
    cd->hasTwoSum = false;
    cd->showSum2 = true;
    cd->lineColor2 = 0xFFF59E0B;
    cd->legendHitCount = 0;
    for (int i = 0; i < CHART_MAX_STREAMS; i++) {
        cd->series[i] = (double*)malloc(cd->capacity * sizeof(double));
        cd->seriesColor[i] = g_streamPalette[i];
        cd->seriesVisible[i] = true;
    }
}

void Chart_SetStreamCount(ChartData* cd, int streamCount)
{
    if (!cd) return;
    if (streamCount < 0) streamCount = 0;
    if (streamCount > CHART_MAX_STREAMS) streamCount = CHART_MAX_STREAMS;
    cd->seriesCount = streamCount;
    for (int i = 0; i < CHART_MAX_STREAMS; i++) cd->seriesVisible[i] = true;
}

/* Grow every parallel array to twice the current capacity.
   Returns false if any realloc failed (arrays are left inconsistent but
   still freeable — every pointer is either old or new). */
static bool Chart_Grow(ChartData* cd)
{
    int newCap = cd->capacity * 2;
    double* nt = (double*)realloc(cd->times, newCap * sizeof(double));
    double* nv = (double*)realloc(cd->values, newCap * sizeof(double));
    double* nv2 = (double*)realloc(cd->values2, newCap * sizeof(double));
    double* nj = (double*)realloc(cd->jitter, newCap * sizeof(double));
    double* nl = (double*)realloc(cd->lossRate, newCap * sizeof(double));
    double* nd = (double*)realloc(cd->datagrams, newCap * sizeof(double));
    if (!nt || !nv || !nv2 || !nj || !nl || !nd) return false;
    cd->times = nt;
    cd->values = nv;
    cd->values2 = nv2;
    cd->jitter = nj;
    cd->lossRate = nl;
    cd->datagrams = nd;
    for (int i = 0; i < CHART_MAX_STREAMS; i++) {
        double* ns = (double*)realloc(cd->series[i], newCap * sizeof(double));
        if (!ns) return false;
        cd->series[i] = ns;
    }
    cd->capacity = newCap;
    return true;
}

void Chart_AddPoint(ChartData* cd, double time, double value)
{
    if (!cd) return;
    if (cd->count >= cd->capacity && !Chart_Grow(cd)) return;
    cd->times[cd->count] = time;
    cd->values[cd->count] = value;
    cd->jitter[cd->count] = 0;
    cd->lossRate[cd->count] = 0;
    cd->datagrams[cd->count] = 0;
    for (int i = 0; i < CHART_MAX_STREAMS; i++) cd->series[i][cd->count] = 0;
    cd->count++;
}

/* Attach a per-stream value to the most recent point */
void Chart_AddStreamPoint(ChartData* cd, int streamIdx, double bps)
{
    if (!cd || cd->count == 0) return;
    if (streamIdx < 0 || streamIdx >= CHART_MAX_STREAMS) return;
    cd->series[streamIdx][cd->count - 1] = bps;
}

/* Add UDP secondary data for the latest point */
void Chart_AddUdpData(ChartData* cd, double jitter, double lossRate, double datagrams)
{
    if (!cd || cd->count == 0) return;
    int idx = cd->count - 1;
    cd->jitter[idx] = jitter;
    cd->lossRate[idx] = lossRate;
    cd->datagrams[idx] = datagrams;
}

void Chart_Clear(ChartData* cd)
{
    if (!cd) return;
    cd->count = 0;
    /* A fresh run may be unidir or bidir; reset the dual-sum flag so the
       chart falls back to the single-sum layout until data says otherwise. */
    cd->hasTwoSum = false;
    cd->showSum2 = true;
}

void Chart_Free(ChartData* cd)
{
    if (!cd) return;
    free(cd->times);
    free(cd->values);
    free(cd->values2);
    free(cd->jitter);
    free(cd->lossRate);
    free(cd->datagrams);
    cd->times = NULL;
    cd->values = NULL;
    cd->jitter = NULL;
    cd->lossRate = NULL;
    cd->datagrams = NULL;
    for (int i = 0; i < CHART_MAX_STREAMS; i++) {
        free(cd->series[i]);
        cd->series[i] = NULL;
    }
    cd->seriesCount = 0;
    cd->count = 0;
    cd->capacity = 0;
}

/* ===== Legend (top "带宽 <unit>" row) =====
   Draws a color-key row: 汇总 + per-stream swatches (clickable) + UDP
   right-axis keys. Stores hit rectangles in cd->legendHit* (chart-window
   client coords) so ChartWndProc's WM_LBUTTONDOWN can toggle visibility.
   startX lets the caller keep the row clear of the "带宽 <unit>" label. */
/* 「隐藏流汇总」是否生效（设置页新增选项，2026-09-24）。
   三个条件必须同时满足：勾了「隐藏流汇总」、勾了「显示各流」、确实有多流数据。
   bidir 下 seriesCount 被置 0（汇总线是唯一曲线），故自动不生效。 */
static bool Chart_SumHidden(const ChartData* cd)
{
    return cd->hideStreamSum && cd->showStreams && cd->seriesCount > 0;
}

static void DrawLegend(ChartData* cd, Gdiplus::Graphics& graphics,
                       Gdiplus::Font& font, int topY, int startX)
{
    cd->legendHitCount = 0;
    Gdiplus::Color textCol(cd->isDark ? 0xFFF0F4FF : 0xFF000000);
    Gdiplus::SolidBrush textBrush(textCol);
    Gdiplus::Color dimCol(cd->isDark ? 0xFF5A5A6A : 0xFFBBBBBB);

    int x = startX, y = topY, sq = 10, gap = 5;
    #define PUSH_HIT(idx, w) do { \
        if (cd->legendHitCount < 16) { \
            cd->legendHitX[cd->legendHitCount] = x; \
            cd->legendHitY[cd->legendHitCount] = y - 2; \
            cd->legendHitW[cd->legendHitCount] = sq + gap + (w); \
            cd->legendHitH[cd->legendHitCount] = 16; \
            cd->legendHitIdx[cd->legendHitCount] = (idx); \
            cd->legendHitCount++; \
        } \
    } while (0)

    /* 2026-09-24：图例文字宽度一律按当前语言实测，替代原先写死的 44/34/30/54/60/64。
       那些常数是按中文「汇总」「流1」「抖动(ms)」估的，英文（Stream/Summary/Jitter）
       明显更宽 ⇒ 相邻项互相压住。实测后中文宽度几乎不变，英文自动让开，
       点击热区（PUSH_HIT）也跟着真实宽度走。 */
    auto TextW = [&](const wchar_t* s) -> int {
        Gdiplus::RectF m;
        graphics.MeasureString(s, -1, &font, Gdiplus::PointF(0, 0), &m);
        return (int)ceil(m.Width) + 2;   /* +2 抵消 ClearType 测量的半像素误差 */
    };

    /* 汇总 (sum line) — bidir shows TX + RX as two separate keys */
    if (Chart_SumHidden(cd)) {
        /* 「隐藏流汇总」生效：整条图例项都不画（不是画空心方框），
           否则点了没反应会让人以为坏了。x 不推进，各流色块顺势左移。 */
    } else if (cd->hasTwoSum) {
        Gdiplus::RectF r1((float)x, (float)y, sq, sq);
        if (cd->showSum) {
            Gdiplus::SolidBrush b(cd->lineColor); graphics.FillRectangle(&b, r1);
        } else {
            Gdiplus::Pen p(dimCol, 1); graphics.DrawRectangle(&p, r1);
        }
        graphics.DrawString(T(STR_LEGEND_SUM_TX), -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        { int tw = TextW(T(STR_LEGEND_SUM_TX)); PUSH_HIT(-1, tw); x += sq + gap + tw + gap + 10; }

        Gdiplus::RectF r2((float)x, (float)y, sq, sq);
        if (cd->showSum2) {
            Gdiplus::SolidBrush b(cd->lineColor2); graphics.FillRectangle(&b, r2);
        } else {
            Gdiplus::Pen p(dimCol, 1); graphics.DrawRectangle(&p, r2);
        }
        graphics.DrawString(T(STR_LEGEND_SUM_RX), -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        { int tw = TextW(T(STR_LEGEND_SUM_RX)); PUSH_HIT(-2, tw); x += sq + gap + tw + gap + 10; }
    } else {
        Gdiplus::RectF r((float)x, (float)y, sq, sq);
        if (cd->showSum) {
            Gdiplus::SolidBrush b(cd->lineColor); graphics.FillRectangle(&b, r);
        } else {
            Gdiplus::Pen p(dimCol, 1); graphics.DrawRectangle(&p, r);
        }
        graphics.DrawString(T(STR_LBL_SUMMARY), -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        { int tw = TextW(T(STR_LBL_SUMMARY)); PUSH_HIT(-1, tw); x += sq + gap + tw + gap + 10; }
    }

    /* Per-stream swatches (only listed when stream data is present) */
    for (int i = 0; i < cd->seriesCount && i < CHART_MAX_STREAMS; i++) {
        Gdiplus::RectF r((float)x, (float)y, sq, sq);
        if (cd->seriesVisible[i]) {
            Gdiplus::SolidBrush b(cd->seriesColor[i]); graphics.FillRectangle(&b, r);
        } else {
            Gdiplus::Pen p(dimCol, 1); graphics.DrawRectangle(&p, r);
        }
        wchar_t nm[16]; swprintf(nm, 16, T(STR_FMT_STREAM), i + 1);
        graphics.DrawString(nm, -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        { int tw = TextW(nm); PUSH_HIT(i, tw); x += sq + gap + tw + gap + 10; }
    }

    /* UDP right-axis keys (color squares, non-toggle) */
    if (cd->rightAxis == RA_JITTER_LOSS) {
        Gdiplus::SolidBrush jb(cd->jitterColor); graphics.FillRectangle(&jb, Gdiplus::RectF((float)x, (float)y, sq, sq));
        graphics.DrawString(T(STR_AXIS_JITTER), -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        x += sq + gap + TextW(T(STR_AXIS_JITTER)) + gap + 10;
        Gdiplus::SolidBrush lb(cd->lossColor); graphics.FillRectangle(&lb, Gdiplus::RectF((float)x, (float)y, sq, sq));
        graphics.DrawString(T(STR_AXIS_LOSS), -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        x += sq + gap + TextW(T(STR_AXIS_LOSS)) + gap + 10;
    } else if (cd->rightAxis == RA_DATAGRAMS) {
        Gdiplus::SolidBrush db(cd->datagramColor); graphics.FillRectangle(&db, Gdiplus::RectF((float)x, (float)y, sq, sq));
        graphics.DrawString(T(STR_AXIS_DATAGRAM), -1, &font, Gdiplus::PointF((float)(x + sq + gap), (float)y - 2), &textBrush);
        x += sq + gap + TextW(T(STR_AXIS_DATAGRAM)) + gap + 10;
    }
    #undef PUSH_HIT
}

/* ===== Render ===== */
void Chart_Render(ChartData* cd, HDC hdc, RECT rc)
{
    if (!cd || !hdc) return;

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 10 || height <= 10) return;

    /* Create compatible DC for double buffering */
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    {
        Gdiplus::Graphics graphics(memDC);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        /* Background — 必须与 main.c 的 COLOR_BG_DARK(28,32,41) 保持一致 */
        Gdiplus::Color bgCol(cd->isDark ? 0xFF1C2029 : 0xFFFFFFFF);
        graphics.Clear(bgCol);

        /* Colors */
        Gdiplus::Color textCol(cd->isDark ? 0xFFF0F4FF : 0xFF000000);
        Gdiplus::Color splitCol(cd->isDark ? 0xFF3A3A3A : 0xFFE0E0E0);
        Gdiplus::SolidBrush textBrush(textCol);
        Gdiplus::Font font(L"Segoe UI", 9);

        /* Y-axis top label = "带宽 <unit>" — drawn in its own row ABOVE the plot
           area (left-aligned at x=2), so it never needs to reserve left padding */
        wchar_t yLabelW[32] = {0};
        wchar_t yUnitW[16] = {0};
        MultiByteToWideChar(CP_UTF8, 0, cd->yUnit, -1, yUnitW, 16);
        swprintf(yLabelW, 32, T(STR_FMT_BANDWIDTH), yUnitW);

        int yDivisions = 5;

        /* Calculate left Y range (bandwidth, raw bps → display unit) */
        double ySc = cd->yScale;
        if (ySc <= 0) ySc = 1e6;
        /* 「隐藏流汇总」生效时汇总值不再参与量程：否则量程仍按汇总（≈各流之和）
           取最大值，各流细线会全挤在底部 1/N 处。 */
        const bool sumHidden = Chart_SumHidden(cd);
        double maxY = 1.0;
        for (int i = 0; i < cd->count; i++) {
            if (!sumHidden) {
                double dv = cd->values[i] / ySc;
                if (dv > maxY) maxY = dv;
                if (cd->hasTwoSum) {
                    double dv2 = cd->values2[i] / ySc;
                    if (dv2 > maxY) maxY = dv2;
                }
            }
            /* 各可见流始终参与量程：单流时它们与汇总同值，多流隐藏汇总后由它们决定上限 */
            if (cd->showStreams) {
                for (int si = 0; si < cd->seriesCount && si < CHART_MAX_STREAMS; si++) {
                    if (!cd->seriesVisible[si] || !cd->series[si]) continue;
                    double sv = cd->series[si][i] / ySc;
                    if (sv > maxY) maxY = sv;
                }
            }
        }
        if (maxY < 1) maxY = 1;
        double yScaleNice = 1.0;
        while (maxY * yScaleNice > 100) yScaleNice /= 10;
        while (maxY * yScaleNice < 1) yScaleNice *= 10;
        double niceMax = ceil(maxY / yScaleNice) * yScaleNice;
        if (niceMax <= 0) niceMax = 1.0;

        /* Helper lambda: nice rounding for right axis values */
        auto NiceMax = [](double rawMax) -> double {
            if (rawMax < 0.001) rawMax = 0.001;
            double n = 1.0;
            while (rawMax * n > 100) n /= 10;
            while (rawMax * n < 1) n *= 10;
            double result = ceil(rawMax * n) / n;
            return (result <= 0) ? 1.0 : result;
        };

        /* Calculate right Y ranges based on mode */
        double maxJitter = 0.01, maxLoss = 0.01, maxDgram = 1.0;
        if (cd->rightAxis == RA_JITTER_LOSS) {
            for (int i = 0; i < cd->count; i++) {
                if (cd->jitter[i] > maxJitter) maxJitter = cd->jitter[i];
                if (cd->lossRate[i] > maxLoss) maxLoss = cd->lossRate[i];
            }
        } else if (cd->rightAxis == RA_DATAGRAMS) {
            for (int i = 0; i < cd->count; i++) {
                if (cd->datagrams[i] > maxDgram) maxDgram = cd->datagrams[i];
            }
        }
        double jitterNiceMax = NiceMax(maxJitter);
        double lossNiceMax = NiceMax(maxLoss);
        double dgramNiceMax = NiceMax(maxDgram);

        /* Left padding is ZERO: the plot area starts at the canvas left edge so the
           curve reaches the very left. The Y tick numbers are drawn INSIDE the plot
           area (over the fill) at the end, with a semi-transparent backing chip. */
        int padLeft = 0;

        /* Padding: right for Y2 labels (dual-axis modes) */
        bool hasRight = (cd->rightAxis != RA_NONE);
        /* Measure right-axis top label to compute padRight dynamically */
        int padRight;
        if (cd->rightAxis == RA_DATAGRAMS) {
            /* Client UDP: datagram numbers are drawn INSIDE the plot area
               (right-aligned 6px from the canvas right edge, last, with a chip),
               so no right padding — the curve reaches the right edge. */
            padRight = 0;
        } else if (!hasRight) {
            padRight = 20;
        } else {
            /* Measure the widest right-axis label text */
            const wchar_t* rLbl = nullptr;
            if (cd->rightAxis == RA_DATAGRAMS) {
                rLbl = T(STR_AXIS_DATAGRAM);
            } else {
                /* RA_JITTER_LOSS: label is on the RIGHT side of axis (丢包率(%)),
                   plus we need room for left-side label too but that overlaps
                   the chart area, not the right padding. Measure 丢包率(%). */
                rLbl = T(STR_AXIS_LOSS);
            }
            Gdiplus::RectF rMeas;
            graphics.MeasureString(rLbl, -1, &font, Gdiplus::PointF(0,0), &rMeas);
            /* axisX+6 is the start x; label width + 6 margin + 6 start = rMeas.Width + 12 */
            int needed = (int)ceil(rMeas.Width) + 12;
            padRight = (needed > 55) ? needed : 55;
        }
        int padTop = 30, padBottom = 30;
        int chartW = width - padLeft - padRight;
        int chartH = height - padTop - padBottom;
        if (chartW <= 20 || chartH <= 20) {
            BitBlt(hdc, rc.left, rc.top, width, height, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            return;
        }

        /* Draw grid lines only — the left Y numbers are drawn at the end so the
           area fill / curves cannot cover them (they live INSIDE the plot area) */
        Gdiplus::Pen gridPen(splitCol, 1);
        for (int i = 0; i <= yDivisions; i++) {
            int y = padTop + chartH - (int)(chartH * (double)i / yDivisions);
            graphics.DrawLine(&gridPen, padLeft, y, padLeft + chartW, y);
        }

        /* Right Y-axis labels — mode dependent.
           RA_DATAGRAMS numbers are drawn at the end (inside the plot area). */
        int axisX = padLeft + chartW;
        if (cd->rightAxis == RA_JITTER_LOSS) {
            /* Dual scale: jitter (ms) LEFT of axis line, loss (%) RIGHT of axis line */
            for (int i = 0; i <= yDivisions; i++) {
                int y = padTop + chartH - (int)(chartH * (double)i / yDivisions);
                /* Jitter labels (left of axis, right-aligned) */
                double jv = jitterNiceMax * i / yDivisions;
                wchar_t jlabel[32];
                if (jv >= 100) swprintf(jlabel, 32, L"%.0f", jv);
                else if (jv >= 10) swprintf(jlabel, 32, L"%.1f", jv);
                else swprintf(jlabel, 32, L"%.2f", jv);
                Gdiplus::RectF jrect;
                graphics.MeasureString(jlabel, -1, &font, Gdiplus::PointF(0,0), &jrect);
                Gdiplus::PointF jpt((float)(axisX - jrect.Width - 4), (float)y - 7);
                Gdiplus::SolidBrush jitterBrush(Gdiplus::Color(cd->jitterColor));
                graphics.DrawString(jlabel, -1, &font, jpt, &jitterBrush);
                /* Loss labels (right of axis, hug the axis, 4px gap) */
                double lv = lossNiceMax * i / yDivisions;
                wchar_t llabel[32];
                if (lv >= 100) swprintf(llabel, 32, L"%.0f", lv);
                else if (lv >= 10) swprintf(llabel, 32, L"%.1f", lv);
                else swprintf(llabel, 32, L"%.2f", lv);
                Gdiplus::PointF lpt((float)(axisX + 4), (float)y - 7);
                Gdiplus::SolidBrush lossBrush(Gdiplus::Color(cd->lossColor));
                graphics.DrawString(llabel, -1, &font, lpt, &lossBrush);
            }
        }

        /* Draw line + area */
        if (cd->count > 0) {
            Gdiplus::Color areaCol(cd->areaColor);

            /* Area fill path — bidir draws two separate lines, so no single
               area fill (it would overlap and confuse the two directions). */
            if (cd->showSum && !sumHidden && cd->count >= 2 && !cd->hasTwoSum) {
                Gdiplus::GraphicsPath path;
                double xStep = (double)chartW / (cd->count > 1 ? cd->count - 1 : 1);
                int prevX = padLeft, prevY = padTop + chartH;
                for (int i = 0; i < cd->count; i++) {
                    int x = padLeft + (int)(xStep * i);
                    double dv = cd->values[i] / ySc;
                    int y = padTop + chartH - (int)(chartH * dv / niceMax);
                    if (y < padTop) y = padTop;
                    if (y > padTop + chartH) y = padTop + chartH;
                    if (i == 0) {
                        path.StartFigure();
                        path.AddLine(x, padTop + chartH, x, y);
                    } else {
                        path.AddLine(prevX, prevY, x, y);
                    }
                    prevX = x;
                    prevY = y;
                    if (i == cd->count - 1) {
                        path.AddLine(x, y, x, padTop + chartH);
                        path.CloseFigure();
                    }
                }
                Gdiplus::SolidBrush areaBrush(areaCol);
                graphics.FillPath(&areaBrush, &path);
            }

            /* Draw Bandwidth summary line(s).
               Bidir (hasTwoSum): two 2px lines — TX(values[]) + RX(values2[]).
               Unidir: single values[] line. */
            auto DrawSumLine = [&](const double* arr, DWORD col, bool vis) {
                if (!vis) return;
                Gdiplus::Color lc(col);
                Gdiplus::Pen pen(lc, 2.0f);
                for (int i = 1; i < cd->count; i++) {
                    double xStep = (double)chartW / (cd->count > 1 ? cd->count - 1 : 1);
                    int x1 = padLeft + (int)(xStep * (i - 1));
                    double dv1 = arr[i - 1] / ySc;
                    int y1 = padTop + chartH - (int)(chartH * dv1 / niceMax);
                    int x2 = padLeft + (int)(xStep * i);
                    double dv2 = arr[i] / ySc;
                    int y2 = padTop + chartH - (int)(chartH * dv2 / niceMax);
                    if (y1 < padTop) y1 = padTop;
                    if (y2 < padTop) y2 = padTop;
                    graphics.DrawLine(&pen, x1, y1, x2, y2);
                }
            };
            if (sumHidden) {
                /* 「隐藏流汇总」生效：汇总线（面积已在上面跳过）全部不画，
                   只留各流细线。seriesCount>0 由 Chart_SumHidden 保证。 */
            } else if (cd->hasTwoSum) {
                DrawSumLine(cd->values,  cd->lineColor,  cd->showSum);
                DrawSumLine(cd->values2, cd->lineColor2, cd->showSum2);
            } else if (cd->showSum) {
                DrawSumLine(cd->values, cd->lineColor, true);
            }

            /* Per-stream thin lines (multi-stream replay / live -P n) */
            if (cd->showStreams) {
                for (int si = 0; si < cd->seriesCount && si < CHART_MAX_STREAMS; si++) {
                    if (!cd->seriesVisible[si]) continue;
                    Gdiplus::Color sc(cd->seriesColor[si]);
                    Gdiplus::Pen sp(sc, 1.2f);
                    for (int i = 1; i < cd->count; i++) {
                        double xStep = (double)chartW / (cd->count > 1 ? cd->count - 1 : 1);
                        int x1 = padLeft + (int)(xStep * (i - 1));
                        double dv1 = cd->series[si][i - 1] / ySc;
                        int y1 = padTop + chartH - (int)(chartH * dv1 / niceMax);
                        int x2 = padLeft + (int)(xStep * i);
                        double dv2 = cd->series[si][i] / ySc;
                        int y2 = padTop + chartH - (int)(chartH * dv2 / niceMax);
                        if (y1 < padTop) y1 = padTop;
                        if (y2 < padTop) y2 = padTop;
                        if (y1 > padTop + chartH) y1 = padTop + chartH;
                        if (y2 > padTop + chartH) y2 = padTop + chartH;
                        graphics.DrawLine(&sp, x1, y1, x2, y2);
                    }
                }
            }

            /* Right-axis secondary lines */
            if (cd->rightAxis == RA_JITTER_LOSS) {
                /* Jitter line (dashed, jitterColor) — mapped to jitterNiceMax */
                Gdiplus::Color jitterCol(cd->jitterColor);
                Gdiplus::Pen jitterPen(jitterCol, 1.5f);
                Gdiplus::REAL dashVals[] = {5.0f, 3.0f};
                jitterPen.SetDashPattern(dashVals, 2);
                for (int i = 1; i < cd->count; i++) {
                    double xStep = (double)chartW / (cd->count > 1 ? cd->count - 1 : 1);
                    int x1 = padLeft + (int)(xStep * (i - 1));
                    int x2 = padLeft + (int)(xStep * i);
                    double jv1 = cd->jitter[i - 1] / jitterNiceMax;
                    double jv2 = cd->jitter[i] / jitterNiceMax;
                    int y1 = padTop + chartH - (int)(chartH * jv1);
                    int y2 = padTop + chartH - (int)(chartH * jv2);
                    if (y1 < padTop) y1 = padTop;
                    if (y2 < padTop) y2 = padTop;
                    graphics.DrawLine(&jitterPen, x1, y1, x2, y2);
                }
                /* Loss% line (dotted, lossColor) — mapped to lossNiceMax */
                Gdiplus::Color lossCol(cd->lossColor);
                Gdiplus::Pen lossPen(lossCol, 1.5f);
                Gdiplus::REAL dotVals[] = {2.0f, 2.0f};
                lossPen.SetDashPattern(dotVals, 2);
                for (int i = 1; i < cd->count; i++) {
                    double xStep = (double)chartW / (cd->count > 1 ? cd->count - 1 : 1);
                    int x1 = padLeft + (int)(xStep * (i - 1));
                    int x2 = padLeft + (int)(xStep * i);
                    double lv1 = cd->lossRate[i - 1] / lossNiceMax;
                    double lv2 = cd->lossRate[i] / lossNiceMax;
                    int y1 = padTop + chartH - (int)(chartH * lv1);
                    int y2 = padTop + chartH - (int)(chartH * lv2);
                    if (y1 < padTop) y1 = padTop;
                    if (y2 < padTop) y2 = padTop;
                    graphics.DrawLine(&lossPen, x1, y1, x2, y2);
                }
            } else if (cd->rightAxis == RA_DATAGRAMS) {
                /* Datagram line (solid thin, datagramColor) — mapped to dgramNiceMax */
                Gdiplus::Color dgramCol(cd->datagramColor);
                Gdiplus::Pen dgramPen(dgramCol, 1.5f);
                for (int i = 1; i < cd->count; i++) {
                    double xStep = (double)chartW / (cd->count > 1 ? cd->count - 1 : 1);
                    int x1 = padLeft + (int)(xStep * (i - 1));
                    int x2 = padLeft + (int)(xStep * i);
                    double dv1 = cd->datagrams[i - 1] / dgramNiceMax;
                    double dv2 = cd->datagrams[i] / dgramNiceMax;
                    int y1 = padTop + chartH - (int)(chartH * dv1);
                    int y2 = padTop + chartH - (int)(chartH * dv2);
                    if (y1 < padTop) y1 = padTop;
                    if (y2 < padTop) y2 = padTop;
                    graphics.DrawLine(&dgramPen, x1, y1, x2, y2);
                }
            }

            /* X labels */
            wchar_t label[32];
            swprintf(label, 32, L"%.1f", cd->times[0]);
            Gdiplus::PointF pt1((float)padLeft, (float)padTop + chartH + 8);
            graphics.DrawString(label, -1, &font, pt1, &textBrush);
            if (cd->count > 1) {
                swprintf(label, 32, L"%.1f", cd->times[cd->count - 1]);
                Gdiplus::PointF pt2((float)padLeft + chartW - 30, (float)padTop + chartH + 8);
                graphics.DrawString(label, -1, &font, pt2, &textBrush);
            }
        }

        /* Left Y tick numbers — drawn last, INSIDE the plot area (over the fill),
           left-aligned 4px from the plot's left edge. A semi-transparent chip in
           the background color keeps them readable over grid lines and the area
           fill. Same treatment for every mode (client/server, TCP/UDP). */
        {
            Gdiplus::Color chipCol(170, bgCol.GetR(), bgCol.GetG(), bgCol.GetB());
            Gdiplus::SolidBrush chipBrush(chipCol);
            float lx = (float)padLeft + 4;
            for (int i = 0; i <= yDivisions; i++) {
                int y = padTop + chartH - (int)(chartH * (double)i / yDivisions);
                double val = niceMax * i / yDivisions;
                wchar_t label[32];
                if (val >= 100000) swprintf(label, 32, L"%.0f", val);
                else if (val >= 100) swprintf(label, 32, L"%.1f", val);
                else swprintf(label, 32, L"%.2f", val);
                Gdiplus::RectF m;
                graphics.MeasureString(label, -1, &font, Gdiplus::PointF(0,0), &m);
                graphics.FillRectangle(&chipBrush,
                    Gdiplus::RectF(lx - 2, (float)y - 8, m.Width + 4, 16));
                graphics.DrawString(label, -1, &font,
                    Gdiplus::PointF(lx, (float)y - 7), &textBrush);
            }

            /* Client UDP: datagram numbers — same treatment on the right, still
               right-aligned 6px from the canvas right edge, now inside the plot */
            if (cd->rightAxis == RA_DATAGRAMS) {
                float dRight = (float)(width - 6);
                for (int i = 0; i <= yDivisions; i++) {
                    int y = padTop + chartH - (int)(chartH * (double)i / yDivisions);
                    double val = dgramNiceMax * i / yDivisions;
                    wchar_t dlabel[32];
                    swprintf(dlabel, 32, L"%.0f", val);
                    Gdiplus::RectF m;
                    graphics.MeasureString(dlabel, -1, &font, Gdiplus::PointF(0,0), &m);
                    graphics.FillRectangle(&chipBrush,
                        Gdiplus::RectF(dRight - m.Width - 2, (float)y - 8, m.Width + 4, 16));
                    graphics.DrawString(dlabel, -1, &font,
                        Gdiplus::PointF(dRight - m.Width, (float)y - 7), &textBrush);
                }
            }
        }

        /* Top axis labels: draw last with opaque background */
        Gdiplus::SolidBrush bgLabelBrush(cd->isDark ? Gdiplus::Color(0xFF1C2029)
                                                    : Gdiplus::Color(0xFFFFFFFF));
        /* Left Y-axis top label: "带宽 <unit>" */
        {
            /* y offset raised by 4px so the opaque chip no longer clips the
               top-most Y tick number (which now sits inside the plot area) */
            Gdiplus::PointF yLabelPt(2, (float)padTop - 26);
            Gdiplus::RectF yMeas;
            graphics.MeasureString(yLabelW, -1, &font, Gdiplus::PointF(0,0), &yMeas);
            graphics.FillRectangle(&bgLabelBrush,
                Gdiplus::RectF(0, (float)padTop - 30, yMeas.Width + 6, 20));
            graphics.DrawString(yLabelW, -1, &font, yLabelPt, &textBrush);
            /* Legend row: starts to the right of the "带宽 <unit>" label */
            DrawLegend(cd, graphics, font, padTop - 26, 2 + (int)ceil(yMeas.Width) + 12);
        }
        /* Right Y-axis top label: mode dependent */
        if (cd->rightAxis == RA_DATAGRAMS) {
            /* Plot spans the full width now — right-align the title to the canvas
               right edge instead of hanging it at axisX + 6 (off-canvas) */
            const wchar_t* lbl = T(STR_AXIS_DATAGRAM);
            Gdiplus::RectF rMeas;
            graphics.MeasureString(lbl, -1, &font, Gdiplus::PointF(0,0), &rMeas);
            float rx = (float)width - rMeas.Width - 4;
            graphics.FillRectangle(&bgLabelBrush,
                Gdiplus::RectF(rx - 3, (float)padTop - 30, rMeas.Width + 6, 20));
            Gdiplus::PointF rLabelPt(rx, (float)padTop - 26);
            Gdiplus::SolidBrush dgramBrush(Gdiplus::Color(cd->datagramColor));
            graphics.DrawString(lbl, -1, &font, rLabelPt, &dgramBrush);
        } else if (cd->rightAxis == RA_JITTER_LOSS) {
            /* Left side of axis: "抖动(ms)" in jitter color */
            const wchar_t* jlbl = T(STR_AXIS_JITTER);
            Gdiplus::RectF jMeas;
            graphics.MeasureString(jlbl, -1, &font, Gdiplus::PointF(0,0), &jMeas);
            float jx = (float)(axisX - jMeas.Width - 4);
            graphics.FillRectangle(&bgLabelBrush,
                Gdiplus::RectF(jx - 3, (float)padTop - 30, jMeas.Width + 6, 20));
            Gdiplus::SolidBrush jitterBrush(Gdiplus::Color(cd->jitterColor));
            graphics.DrawString(jlbl, -1, &font, Gdiplus::PointF(jx, (float)padTop - 26), &jitterBrush);
            /* Right side of axis: "丢包率(%)" in loss color */
            const wchar_t* llbl = T(STR_AXIS_LOSS);
            Gdiplus::PointF lpt((float)(axisX + 4), (float)padTop - 26);
            Gdiplus::RectF lMeas;
            graphics.MeasureString(llbl, -1, &font, Gdiplus::PointF(0,0), &lMeas);
            graphics.FillRectangle(&bgLabelBrush,
                Gdiplus::RectF((float)(axisX + 4), (float)padTop - 30, lMeas.Width + 6, 20));
            Gdiplus::SolidBrush lossBrush(Gdiplus::Color(cd->lossColor));
            graphics.DrawString(llbl, -1, &font, lpt, &lossBrush);
        }
        /* X-axis unit label */
        Gdiplus::PointF xLabelPt((float)padLeft + chartW / 2 - 10, (float)padTop + chartH + 8);
        graphics.DrawString(L"s", -1, &font, xLabelPt, &textBrush);

        /* No "waiting for data" placeholder — an empty chart just shows
           the empty grid, per user request. */
    }

    /* Copy to screen */
    BitBlt(hdc, rc.left, rc.top, width, height, memDC, 0, 0, SRCCOPY);

    /* Cleanup */
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}
