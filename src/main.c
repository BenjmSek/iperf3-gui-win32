/*
 * main.c — iperf3 GUI main window (Win32 API + GDI+)
 *
 * Standalone exe for iperf3 network testing.
 * Layout: top tab bar + status bar + client/server/settings panels.
 * Client & Server panels: left (settings + command + summary) | right (chart + output).
 */
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "resource.h"
#include "iperf3.h"
#include "chart.h"
#include "replay.h"
#include "ui.h"
#include "lang.h"
#include "license_text.h"
#include "help_text.h"

/* ===== Global state ===== */
AppState g_state = {0};

/* ===== UI theme selection (S3): 档位（与 isDark 生效值区分）===== */
enum { THEME_SEL_AUTO = 0, THEME_SEL_DARK = 1, THEME_SEL_LIGHT = 2, THEME_SEL_COUNT };

/* ===== 回放显示单位档位数与默认档（auto）=====
   放在文件顶部：创建控件(约 1963)、DEF_ 宏(约 2862)、Save/Load(约 3160/3235)
   都早于档位表 kReplayUnits[](约 4880)，宏必须先于它们可见。 */
#define REPLAY_UNIT_COUNT   9
#define REPLAY_UNIT_AUTO    8

/* Font handles */
static HFONT s_hFontNormal = NULL;
static HFONT s_hFontBold = NULL;
static HFONT s_hFontMono = NULL;
static HFONT s_hFontGroup = NULL;   /* 组标题：22px 加粗 */
static HFONT s_hFontGroupNormal = NULL; /* 状态栏标签：22px 常规（与组名同大，不加粗） */

/* RichEdit module */
static HMODULE s_hRichEdit = NULL;
static bool s_hRichEdit50Loaded = false;

/* Output toolbar labels (need repositioning on resize) */
static HWND s_hClientOutLbl = NULL, s_hServerOutLbl = NULL;

/* Labels that need gray-out sync with their associated controls */
static HWND s_hClientOmitLabel = NULL;
static HWND s_hClientBwLabel = NULL;
static HWND s_hClientParallelLabel = NULL;

/* Settings panel: log labels that need gray-out sync with log enable checkbox */
static HWND s_hLogDirLabel = NULL;
static HWND s_hLogNamingLabel = NULL;
static HWND s_hLogMaxSizeLabel = NULL;
static HWND s_hLogMaxSizeUnitLabel = NULL;  /* "KB" label */

/* AutoTest state (declared early so MainWndProc can reference them) */
static bool s_autotestMode = false;
static int  s_autotestCaseCount = 0;
static int  s_autotestCaseIdx = 0;
static char s_autotestResultsDir[MAX_PATH] = {0};
static char s_autotestServerIP[64] = "127.0.0.1";

/* Window class name */
#define MAIN_CLASS_NAME L"iperf3GUIMain"
#define CHART_CLASS_NAME L"iperf3ChartChild"
#define PANEL_CLASS_NAME L"iperf3GUIPanel"

/* Custom colors
   深色一组的取值 = Excel 深色主题逐个像素实测结果（`#1C2029` 顶栏底衬 /
   `#292929` 面板与字段面 / `#B1B2B4` 次文字），不再是原来的蓝紫调 (30,30,46)/(40,40,56)。
   面板与页面底色亮度几乎不变 → 主文字对比度 13:1 仍远超 WCAG AA。
   浅色一组一字未动。 */
#define COLOR_BG_DARK       RGB(28, 32, 41)      /* #1C2029 */
#define COLOR_BG_CARD_DARK  RGB(41, 41, 41)      /* #292929 */
#define COLOR_BG_LIGHT      RGB(250, 250, 252)
#define COLOR_BG_CARD_LIGHT RGB(255, 255, 255)
#define COLOR_TEXT_DARK     RGB(240, 244, 255)
#define COLOR_TEXT_LIGHT    RGB(0, 0, 0)
#define COLOR_GRAY_DARK     RGB(177, 178, 180)   /* #B1B2B4 Excel 次文字 */
#define COLOR_GRAY_LIGHT    RGB(80, 80, 80)
#define COLOR_CLIENT_LINE   0xFF0EA5E9  /* ARGB: blue */
#define COLOR_CLIENT_AREA   0x280EA5E9
#define COLOR_SERVER_LINE   0xFF10B981  /* ARGB: green */
#define COLOR_SERVER_AREA   0x2810B981
/* Status dot colors: running = red, idle = green */
#define COLOR_RUN_DARK      RGB(255, 107, 107)
#define COLOR_RUN_LIGHT     RGB(220, 38, 38)
#define COLOR_IDLE_DARK     RGB(52, 211, 153)
#define COLOR_IDLE_LIGHT    RGB(22, 163, 74)
/* Dark-mode control chrome (values taken from the Excel dark-theme reference):
   字段/下拉边框统一 #727272；按钮平时不加常驻边框，hover 才出面与边框。 */
#define COLOR_FIELD_EDGE_DARK RGB(114, 114, 114)  /* #727272 Excel 输入框边框 */
#define COLOR_BTN_HOVER_DARK  RGB(58, 58, 58)     /* #3A3A3A hover 面 */
#define COLOR_BTN_PRESS_DARK  RGB(74, 74, 74)     /* #4A4A4A 按下/选中面 */
/* 经典滚动条也用系统色（COLOR_3DFACE = #F0F0F0），深色下是一条亮带 */
#define COLOR_SBAR_TRACK_DARK RGB(35, 35, 35)     /* #232323 滑轨 */
#define COLOR_SBAR_THUMB_DARK RGB(90, 90, 90)     /* #5A5A5A 滑块 */
#define COLOR_SBAR_ARROW_DARK RGB(160, 160, 160)  /* #A0A0A0 上下箭头 */

/* ===== Forward declarations ===== */
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ChartWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PanelWndProc(HWND, UINT, WPARAM, LPARAM);
static void CreateAllControls(HWND hwnd);
static void LayoutControls(HWND hwnd);
static void ShowActivePanel(void);
static void OnStartClient(void);
static void OnStartServer(void);
static void OnStopClient(void);
static void OnStopServer(void);
static void OnProcessLine(const char* line, int mode);
static void OnIperf3Line(const char* line, bool isSummary, int mode);
static void CheckClientExited(DWORD exitCode);
static void CheckServerExited(DWORD exitCode);
static DWORD WINAPI PipeReaderThread(LPVOID param);
static COLORREF GetBgColor(void);
static COLORREF GetCardColor(void);
static COLORREF GetTextColorValue(void);
static COLORREF GetGrayTextColor(void);

/* AutoTest forward declarations */
static void AutoTest_StartNext(void);
static void AutoTest_DumpAndAdvance(int mode);
static void AutoTest_DumpWindowText(HWND hwnd, const char* filePath);

/* ===== Forward declarations (helpers defined further below) ===== */
static void GetSettingsPath(char* out, int outLen); /* forward decl */
static void Lang_FillCombo(void);                /* forward decl (S3) */
static void Theme_FillCombo(void);               /* forward decl (S3 theme) */
static int  FitW(HWND hwnd, int designW, int pad);        /* (S3b) */
static void WidenInPlace(HWND hwnd, int designW, int pad); /* (S3b) */
static int  LeftColumnWidth(void);                        /* (S3b) */
static void Settings_UnifyButtonWidth(void);              /* 设置页 6 个按钮等宽 */
static void Log_Init(int mode, const char* ip);
static void Log_WriteLine(const char* line);
static void Log_Truncate(void);
static void Log_Close(void);
static void UI_BrowseLogDir(void);
static void UI_UpdateLogControls(void);

/* Replay forward declarations */
static void OnReplayBrowse(void);
static void OnReplayLoad(void);
static void Replay_ApplyIndex(int idx, bool repaint);
static void Replay_UpdateLabels(void);
static void Replay_UpdateSliderRange(void);
static void Replay_SetPlaying(bool playing);
static void Replay_Tick(void);
static void Replay_ReadSpeed(void);
static void Replay_ReadUnit(void);
static void ApplyShowStreams(bool val);
static void ApplyHideStreamSum(bool val);

/* ===== Helper: get text from edit control ===== */
int UI_GetEditText(HWND hwnd, char* buf, int bufLen)
{
    if (!hwnd || !buf || bufLen <= 0) return 0;
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) { buf[0] = 0; return 0; }
    if (len >= bufLen) len = bufLen - 1;
    wchar_t* wbuf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!wbuf) { buf[0] = 0; return 0; }
    GetWindowTextW(hwnd, wbuf, len + 1);
    wbuf[len] = 0;
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, bufLen, NULL, NULL);
    free(wbuf);
    return (int)strlen(buf);
}

/* ===== Helper: set edit control text from UTF-8 ===== */
static void SetEditUtf8(HWND hwnd, const char* text)
{
    if (!hwnd) return;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wlen <= 0) return;
    wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (!wbuf) return;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wbuf, wlen);
    SetWindowTextW(hwnd, wbuf);
    free(wbuf);
}

/* ===== Helper: append text to RichEdit ===== */
/* Append one line to an output box.
   Uses EM_SETSEL to move caret to end + EM_REPLACESEL to insert text.
   This preserves existing formatting/colour and is O(1) per append instead
   of the O(n) full-rebuild via SetWindowTextW. */
static void AppendRichText(HWND hEdit, const char* text, bool isError)
{
    if (!hEdit || !text || !*text) return;

    /* Convert to wide */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wlen <= 0) return;
    wchar_t* wbuf = (wchar_t*)malloc((size_t)(wlen + 2) * sizeof(wchar_t));
    if (!wbuf) return;
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wbuf, wlen);
    wbuf[wlen - 1] = 0;
    wcscat(wbuf, L"\r\n");

    /* Move caret to the very end */
    int n = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)n, (LPARAM)n);

    /* Set formatting for the text about to be inserted */
    CHARFORMAT2W cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = isError ? RGB(248, 113, 113) : GetTextColorValue();
    cf.yHeight = 16 * 72 / 96 * 20;  /* 16px -> 12pt -> twips (same as the summary box) */
    wcscpy(cf.szFaceName, L"Consolas");
    SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    /* ES_READONLY blocks EM_REPLACESEL, so toggle off around the insert */
    SendMessageW(hEdit, EM_SETREADONLY, FALSE, 0);
    SendMessageW(hEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)wbuf);
    SendMessageW(hEdit, EM_SETREADONLY, TRUE, 0);
    free(wbuf);
}

/* ===== Color helpers ===== */
static COLORREF GetBgColor(void)
{
    return g_state.isDark ? COLOR_BG_DARK : COLOR_BG_LIGHT;
}
static COLORREF GetCardColor(void)
{
    return g_state.isDark ? COLOR_BG_CARD_DARK : COLOR_BG_CARD_LIGHT;
}
static COLORREF GetTextColorValue(void)
{
    return g_state.isDark ? COLOR_TEXT_DARK : COLOR_TEXT_LIGHT;
}
static COLORREF GetGrayTextColor(void)
{
    return g_state.isDark ? COLOR_GRAY_DARK : COLOR_GRAY_LIGHT;
}
/* Status dot: running = red, idle = green */
static COLORREF GetRunColor(void)
{
    return g_state.isDark ? COLOR_RUN_DARK : COLOR_RUN_LIGHT;
}
static COLORREF GetIdleColor(void)
{
    return g_state.isDark ? COLOR_IDLE_DARK : COLOR_IDLE_LIGHT;
}

/* ===== Cached theme brushes =====
   These are shared by WM_CTLCOLORSTATIC / BTN / EDIT. They MUST be rebuilt when
   the dark/light theme flips, otherwise the old colour lingers after a switch.
   The previous code either never invalidated them (card brush) or rebuilt on
   every paint while dark (bg/btn brushes) — both wrong. A single theme stamp
   fixes all three. */
static HBRUSH s_bgBrush   = NULL;   /* WM_CTLCOLORSTATIC background */
static HBRUSH s_btnBrush  = NULL;   /* WM_CTLCOLORBTN background */
static HBRUSH s_cardBrush = NULL;   /* WM_CTLCOLOREDIT background */
static int    s_brushDark = -1;     /* theme when brushes were built (-1 = uninit) */

static void EnsureThemeBrushes(void)
{
    int d = g_state.isDark ? 1 : 0;
    if (s_brushDark == d) return;   /* unchanged -> keep cached brushes */
    if (s_bgBrush)   DeleteObject(s_bgBrush);
    if (s_btnBrush)  DeleteObject(s_btnBrush);
    if (s_cardBrush) DeleteObject(s_cardBrush);
    s_bgBrush   = CreateSolidBrush(GetBgColor());
    s_btnBrush  = CreateSolidBrush(GetBgColor());
    s_cardBrush = CreateSolidBrush(GetCardColor());
    s_brushDark = d;
}

/* RichEdit caches EM_SETBKGNDCOLOR and does NOT follow WM_CTLCOLOR, so re-apply
   its card background whenever the theme flips.
   注意：客户端/服务端的「命令」框与「输出」框一样都是 RichEdit（不是普通 EDIT），
   文字颜色同样不受 WM_SETFONT / WM_CTLCOLOR* 控制 —— 必须用 EM_SETCHARFORMAT。 */
static void ApplyRichEditTextColor(HWND hw)
{
    CHARFORMAT2W cf;
    if (!hw) return;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;                  /* 只改颜色：字体/字号仍由 WM_SETFONT 决定 */
    cf.crTextColor = GetTextColorValue();
    SendMessageW(hw, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SendMessageW(hw, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
}

static void ApplyThemeToEdits(void)
{
    COLORREF c = GetCardColor();
    if (g_state.hClientOutput) SendMessageW(g_state.hClientOutput, EM_SETBKGNDCOLOR, 0, (LPARAM)c);
    if (g_state.hServerOutput) SendMessageW(g_state.hServerOutput, EM_SETBKGNDCOLOR, 0, (LPARAM)c);
    if (g_state.hClientCmd)    SendMessageW(g_state.hClientCmd,    EM_SETBKGNDCOLOR, 0, (LPARAM)c);
    if (g_state.hServerCmd)    SendMessageW(g_state.hServerCmd,    EM_SETBKGNDCOLOR, 0, (LPARAM)c);
    /* 文字色跟着主题重设，否则切到深色后命令框仍是系统黑字。 */
    ApplyRichEditTextColor(g_state.hClientCmd);
    ApplyRichEditTextColor(g_state.hServerCmd);
}

static BOOL CALLBACK InvalidateChildProc(HWND hw, LPARAM lp)
{
    (void)lp;
    /* 输入类控件的 2px 边框在「非客户区」，只 InvalidateRect 刷不到它
       （字段边框子类靠 WM_NCPAINT 重涂）→ 必须带 RDW_FRAME。 */
    RedrawWindow(hw, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    return TRUE;
}

/* ===== Check dark mode ===== */
static bool IsSystemDarkMode(void)
{
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (BYTE*)&value, &size);
        RegCloseKey(hKey);
    }
    return (value == 0); /* 0 = dark mode */
}

/* ===== 深色标题栏（非客户区，DWM 绘制） =====
   标题栏不属于客户区，我们的 WM_ERASEBKGND / WM_CTLCOLOR* 一律管不到它，
   所以深色模式下窗口顶上会留一条系统浅色标题栏（很扎眼）。
   Win10 1809+ 用 DWMWA_USE_IMMERSIVE_DARK_MODE(20) 让 DWM 自己画深色标题栏；
   1809 之前该属性是 19。两个都试，都不支持时静默忽略（不影响其余功能）。
   用 GetProcAddress 动态取，避免给 build.bat 增加 -ldwmapi 依赖。 */
static void ApplyTitleBarTheme(HWND hwnd)
{
    typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    static HMODULE s_hDwm = NULL;
    static PFN_DwmSetWindowAttribute s_fnSetAttr = NULL;
    static bool s_probed = false;
    BOOL dark;

    if (!hwnd) return;
    if (!s_probed) {
        s_probed = true;
        s_hDwm = LoadLibraryW(L"dwmapi.dll");
        if (s_hDwm)
            s_fnSetAttr = (PFN_DwmSetWindowAttribute)GetProcAddress(s_hDwm, "DwmSetWindowAttribute");
    }
    if (!s_fnSetAttr) return;

    dark = g_state.isDark ? TRUE : FALSE;
    if (FAILED(s_fnSetAttr(hwnd, 20, &dark, sizeof(dark))))
        s_fnSetAttr(hwnd, 19, &dark, sizeof(dark));
}

/* ===== UI theme: 档位 -> 生效暗色标志 =====
   档位（themeSel）与生效值（isDark）必须分开：AUTO 档依赖系统，手动档覆盖系统。 */
static bool Theme_ResolveSel(int sel)
{
    if (sel == THEME_SEL_DARK)  return true;
    if (sel == THEME_SEL_LIGHT) return false;
    return IsSystemDarkMode();          /* THEME_SEL_AUTO */
}

/* 应用当前主题：解析 isDark、失效画刷缓存、重涂 RichEdit 背景、
   全窗口（含子控件与 GDI+ 图表）强制重绘。 */
static void Theme_Apply(void)
{
    int sel = g_state.themeSel;
    if (sel < 0 || sel >= THEME_SEL_COUNT) sel = THEME_SEL_AUTO;
    g_state.isDark = Theme_ResolveSel(sel);
    EnsureThemeBrushes();               /* 主题戳变化时重建三把画刷 */
    ApplyThemeToEdits();                /* RichEdit 背景不跟随 WM_CTLCOLOR，需手动重发 */
    if (g_state.hMain) {
        ApplyTitleBarTheme(g_state.hMain);   /* DWM 标题栏不随客户区配色，须显式同步 */
        InvalidateRect(g_state.hMain, NULL, TRUE);
        EnumChildWindows(g_state.hMain, InvalidateChildProc, 0);
    }
    /* GDI+ 图表自绘文字/图例，不走 WM_CTLCOLOR，需单独触发重绘 */
    if (g_state.hClientChartWnd) InvalidateRect(g_state.hClientChartWnd, NULL, TRUE);
    if (g_state.hServerChartWnd) InvalidateRect(g_state.hServerChartWnd, NULL, TRUE);
    if (g_state.hReplayChartWnd) InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
}

/* ============================================================
   深色模式原生控件 chrome（对齐 Excel 深色主题）
   本程序无 comctl32 v6 清单 → 经典控件，chrome 由系统按系统色画、
   完全不吃应用配色。下面三块把「系统画错的地方」在深色下重涂。
   浅色分支一律不介入（保持 F 定稿外观逐像素不变）。
   ============================================================ */

/* ---- 1) 按钮 hover ----
   经典按钮没有 ODS_HOTLIGHT。这里刻意**不缓存** hover 状态：
   「是否高亮」在 WM_DRAWITEM 里按当前真实光标位置（WindowFromPoint）判定，
   无状态 → 不会因为 WM_MOUSELEAVE 时序、面板切换或窗口隐藏留下残留高亮。
   本子类只负责在光标进/出时触发一次重绘。 */
static LRESULT CALLBACK ButtonHoverSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                            UINT_PTR uid, DWORD_PTR ref)
{
    (void)uid; (void)ref;
    if (msg == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tme;
        memset(&tme, 0, sizeof(tme));
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        InvalidateRect(hwnd, NULL, TRUE);
    } else if (msg == WM_MOUSELEAVE) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ---- 2) 输入类控件的边框 ----
   经典 WS_EX_CLIENTEDGE 与 combo 自带凹陷边都是 2px（实测外圈 COLOR_3DSHADOW
   #A0A0A0 + 内圈 #696969），深色下就是两条浅灰线——这是深色界面「发灰不干净」
   的主因。深色下接管 WM_NCPAINT：外圈刷成页面底色、内圈刷成 #727272，
   视觉上等价 Excel 的 1px #727272 边框。 */
/* 在给定 DC 的 (0,0,w,h) 区域画 2px 边框：外圈刷页面底色、内圈刷 #727272
   → 视觉上等价 Excel 的 1px #727272 边框。EDIT/RichEdit 用「窗口 DC + 非客户区」，
   combo 用「客户区 DC」（它的凹陷边画在客户区里，见 ComboSubclassProc）。 */
static void PaintFrameRing(HDC dc, int w, int h)
{
    RECT r;
    HBRUSH bg, ed;
    if (w < 4 || h < 4) return;
    bg = CreateSolidBrush(GetBgColor());
    ed = CreateSolidBrush(COLOR_FIELD_EDGE_DARK);
    SetRect(&r, 0, 0, w, 1);             FillRect(dc, &r, bg);
    SetRect(&r, 0, h - 1, w, h);         FillRect(dc, &r, bg);
    SetRect(&r, 0, 0, 1, h);             FillRect(dc, &r, bg);
    SetRect(&r, w - 1, 0, w, h);         FillRect(dc, &r, bg);
    SetRect(&r, 1, 1, w - 1, 2);         FillRect(dc, &r, ed);
    SetRect(&r, 1, h - 2, w - 1, h - 1); FillRect(dc, &r, ed);
    SetRect(&r, 1, 1, 2, h - 1);         FillRect(dc, &r, ed);
    SetRect(&r, w - 2, 1, w - 1, h - 1); FillRect(dc, &r, ed);
    DeleteObject(bg);
    DeleteObject(ed);
}

static void PaintFieldFrame(HWND hwnd)
{
    RECT rc;
    HDC dc;
    GetWindowRect(hwnd, &rc);
    dc = GetWindowDC(hwnd);
    PaintFrameRing(dc, rc.right - rc.left, rc.bottom - rc.top);
    ReleaseDC(hwnd, dc);
}

/* ---- 2b) 经典滚动条 ----
   带 WS_VSCROLL 的只读输出框（如客户端汇总框）走经典滚动条，用的是系统色
   COLOR_3DFACE(#F0F0F0) → 深色下右侧一条 17px 亮带。滚动条同样在「非客户区」，
   在 WM_NCPAINT 之后重涂成「深色滑轨 + 滑块 + 上下细勾」。 */
#define SCROLLBAR_W 17

static void PaintVScrollBar(HWND hwnd)
{
    RECT wr, r;
    SCROLLINFO si;
    int w, h, x0, x1, y0, y1, trackH;
    HDC dc;
    HBRUSH br;

    if (!(GetWindowLongW(hwnd, GWL_STYLE) & WS_VSCROLL)) return;
    GetWindowRect(hwnd, &wr);
    w = wr.right - wr.left;
    h = wr.bottom - wr.top;
    if (w < SCROLLBAR_W + 6 || h < 24) return;

    x0 = w - 2 - SCROLLBAR_W;      /* 落在外圈边框之内 */
    x1 = w - 2;
    y0 = 2;
    y1 = h - 2;
    trackH = y1 - y0;

    dc = GetWindowDC(hwnd);
    br = CreateSolidBrush(COLOR_SBAR_TRACK_DARK);
    SetRect(&r, x0, y0, x1, y1);
    FillRect(dc, &r, br);
    DeleteObject(br);

    /* 滑块（按 GetScrollInfo 的 pos/page/total 换算） */
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    if (GetScrollInfo(hwnd, SB_VERT, &si)) {
        int total = si.nMax - si.nMin + 1;
        int page  = (int)si.nPage;
        if (page <= 0 || page > total) page = total;
        if (total > page) {
            int thumbH = trackH * page / total;
            int maxPos = total - page;
            int pos    = si.nPos - si.nMin;
            int thumbY;
            if (thumbH < 24) thumbH = 24;
            if (thumbH > trackH) thumbH = trackH;
            if (pos < 0) pos = 0;
            if (pos > maxPos) pos = maxPos;
            thumbY = y0 + (trackH - thumbH) * pos / maxPos;
            br = CreateSolidBrush(COLOR_SBAR_THUMB_DARK);
            SetRect(&r, x0 + 1, thumbY, x1 - 1, thumbY + thumbH);
            FillRect(dc, &r, br);
            DeleteObject(br);
        }
    }

    /* 上下箭头细勾 */
    {
        int cx = (x0 + x1) / 2;
        POINT up[3], dn[3];
        HPEN pen = CreatePen(PS_SOLID, 1, COLOR_SBAR_ARROW_DARK);
        HGDIOBJ op = SelectObject(dc, pen);
        up[0].x = cx - 3; up[0].y = y0 + 9;
        up[1].x = cx;     up[1].y = y0 + 6;
        up[2].x = cx + 3; up[2].y = y0 + 9;
        dn[0].x = cx - 3; dn[0].y = y1 - 9;
        dn[1].x = cx;     dn[1].y = y1 - 6;
        dn[2].x = cx + 3; dn[2].y = y1 - 9;
        Polyline(dc, up, 3);
        Polyline(dc, dn, 3);
        SelectObject(dc, op);
        DeleteObject(pen);
    }
    ReleaseDC(hwnd, dc);
}

static LRESULT CALLBACK FieldChromeSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                            UINT_PTR uid, DWORD_PTR ref)
{
    (void)uid; (void)ref;
    if (msg == WM_NCPAINT) {
        /* 先让默认处理画完并校验区域，再覆盖 —— 与 combo 箭头同一套路，
           避免自己接管区域校验带来重绘抖动。 */
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        if (g_state.isDark) {
            PaintFieldFrame(hwnd);
            PaintVScrollBar(hwnd);
        }
        return r;
    }
    /* 带 WS_VSCROLL 的只读框（汇总框）在内容变化时由控件自己重画滚动条 ——
       它走的是 WM_PAINT，不走 WM_NCPAINT，所以每次 WM_PAINT 后要补涂一次，
       否则一有输出滚动条就退回系统亮色。WM_NC* 系列覆盖拖动滑块的场景。
       */
    if (msg == WM_PAINT ||
        msg == WM_VSCROLL || msg == WM_MOUSEWHEEL ||
        msg == WM_NCMOUSEMOVE || msg == WM_NCLBUTTONDOWN || msg == WM_NCLBUTTONUP ||
        msg == WM_KEYUP || msg == WM_SIZE) {
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        if (g_state.isDark) {
            PaintFieldFrame(hwnd);
            if (GetWindowLongW(hwnd, GWL_STYLE) & WS_VSCROLL) PaintVScrollBar(hwnd);
        }
        return r;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static BOOL CALLBACK AttachFieldChromeProc(HWND hw, LPARAM lp)
{
    wchar_t cls[64];
    (void)lp;
    /* 右上角只读消息框（hStatusRight）故意不挂边框子类：它是展示用、无边框，
       深色下若走 PaintFieldFrame 会在客户区外圈画出 #727272 框线，与「去框线」需求冲突。 */
    if (hw == g_state.hStatusRight) return TRUE;
    if (GetClassNameW(hw, cls, 64)) {
        /* 只挂 EDIT / RichEdit：它们的边在「非客户区」，靠 WM_NCPAINT 重涂。
           combo 没有 WS_EX_CLIENTEDGE、凹陷边画在客户区里，由 ComboSubclassProc
           在 WM_PAINT 里处理，不在这里挂（挂了也不会被触发）。 */
        if (lstrcmpiW(cls, L"EDIT") == 0 ||
            lstrcmpiW(cls, L"RichEdit50W") == 0 ||
            lstrcmpiW(cls, L"RichEdit20W") == 0) {
            SetWindowSubclass(hw, FieldChromeSubclass, 2, 0);
        }
    }
    return TRUE;
}

/* 建完控件后统一挂一次：新增输入控件自动覆盖，不用逐个记得加。 */
static void AttachFieldChrome(void)
{
    if (g_state.hMain) EnumChildWindows(g_state.hMain, AttachFieldChromeProc, 0);
}

/* ---- 2b) 回放进度条（TRACKBAR_CLASS）----
   无 v6 manifest ⇒ 走经典绘制，滑轨是写死的深色 3D 凹槽，不响应 WM_CTLCOLOR*、
   SetWindowTheme 也是空操作，所以浅色主题下会留一条黑带。
   这里子类化后只接 WM_PAINT 整条自绘，输入（拖动 / TBM_*）仍由控件自己处理，我们只换皮。

   ⚠ 硬约束：深色分支绝不出现白底 —— 深色沿用滚动条同源色 #232323 / #5A5A5A，
     与「浅色主题下变灰白」是两套独立分支，改浅色时不要误动深色分支的颜色。 */
#define TRK_THUMB_W        14
#define TRK_THUMB_H        22
#define TRK_CHAN_H         8
/* 浅色 */
#define COLOR_TRK_CHAN_LIGHT       RGB(225, 225, 230)  /* #E1E1E6 滑轨   */
#define COLOR_TRK_CHAN_EDGE_LIGHT  RGB(196, 196, 204)  /* #C4C4CC 滑轨边 */
#define COLOR_TRK_THUMB_LIGHT      RGB(255, 255, 255)  /* 白滑块         */
#define COLOR_TRK_THUMB_EDGE_LIGHT RGB(154, 154, 165)  /* #9A9AA5 滑块边 */
#define COLOR_TRK_CHAN_DIS_LIGHT   RGB(237, 237, 240)  /* #EDEDF0 禁用   */
#define COLOR_TRK_THUMB_DIS_LIGHT  RGB(242, 242, 244)  /* #F2F2F4 禁用   */
#define COLOR_TRK_EDGE_DIS_LIGHT   RGB(200, 200, 206)  /* #C8C8CE 禁用边 */
/* 深色（与滚动条同源，禁用态用更深灰，绝不用白） */
#define COLOR_TRK_CHAN_EDGE_DARK   RGB(69, 69, 69)     /* #454545 */
#define COLOR_TRK_THUMB_EDGE_DARK  RGB(122, 122, 122)  /* #7A7A7A */
#define COLOR_TRK_CHAN_DIS_DARK    RGB(46, 46, 46)     /* #2E2E2E */
#define COLOR_TRK_THUMB_DIS_DARK   RGB(58, 58, 58)     /* #3A3A3A */
#define COLOR_TRK_EDGE_DIS_DARK    RGB(74, 74, 74)     /* #4A4A4A */

static void PaintReplaySlider(HWND hwnd)
{
    RECT cr;
    HDC dc;
    bool dark, enabled;
    int minv, maxv, pos, span, chanL, chanR, chanW, cy, cx;
    COLORREF colBg, colChan, colChanEdge, colThumb, colThumbEdge;
    HBRUSH brBg, brChan, brThumb;
    HPEN penChan, penThumb;
    HGDIOBJ oldBr, oldPen;

    if (!hwnd) return;
    dc = GetDC(hwnd);
    if (!dc) return;
    GetClientRect(hwnd, &cr);
    if (cr.right <= 0 || cr.bottom <= 0) { ReleaseDC(hwnd, dc); return; }

    dark    = g_state.isDark;
    enabled = IsWindowEnabled(hwnd) != FALSE;

    colBg = GetBgColor();   /* 与回放面板同色：浅色 #FAFAFC / 深色 #1C2029 */
    if (dark) {
        colChan      = enabled ? COLOR_SBAR_TRACK_DARK  : COLOR_TRK_CHAN_DIS_DARK;
        colChanEdge  = enabled ? COLOR_TRK_CHAN_EDGE_DARK : COLOR_TRK_EDGE_DIS_DARK;
        colThumb     = enabled ? COLOR_SBAR_THUMB_DARK  : COLOR_TRK_THUMB_DIS_DARK;
        colThumbEdge = enabled ? COLOR_TRK_THUMB_EDGE_DARK : COLOR_TRK_EDGE_DIS_DARK;
    } else {
        colChan      = enabled ? COLOR_TRK_CHAN_LIGHT  : COLOR_TRK_CHAN_DIS_LIGHT;
        colChanEdge  = enabled ? COLOR_TRK_CHAN_EDGE_LIGHT : COLOR_TRK_EDGE_DIS_LIGHT;
        colThumb     = enabled ? COLOR_TRK_THUMB_LIGHT : COLOR_TRK_THUMB_DIS_LIGHT;
        colThumbEdge = enabled ? COLOR_TRK_THUMB_EDGE_LIGHT : COLOR_TRK_EDGE_DIS_LIGHT;
    }

    /* 1) 背景：整块刷面板色，盖掉经典控件的亮底残留 */
    brBg = CreateSolidBrush(colBg);
    FillRect(dc, &cr, brBg);
    DeleteObject(brBg);

    cy    = cr.bottom / 2;
    chanL = TRK_THUMB_W / 2;
    chanR = cr.right - TRK_THUMB_W / 2;
    if (chanR <= chanL) chanR = chanL + 1;
    chanW = chanR - chanL;

    /* 2) 滑轨：左右各留半个滑块宽，滑块行程与经典 trackbar 一致 */
    brChan  = CreateSolidBrush(colChan);
    penChan = CreatePen(PS_SOLID, 1, colChanEdge);
    oldBr   = SelectObject(dc, brChan);
    oldPen  = SelectObject(dc, penChan);
    RoundRect(dc, chanL, cy - TRK_CHAN_H / 2, chanR, cy + TRK_CHAN_H / 2, 4, 4);
    SelectObject(dc, oldBr);
    SelectObject(dc, oldPen);
    DeleteObject(penChan);
    DeleteObject(brChan);

    /* 3) 滑块：位置按 range/pos 换算，不依赖控件自己的绘制 */
    minv = (int)SendMessageW(hwnd, TBM_GETRANGEMIN, 0, 0);
    maxv = (int)SendMessageW(hwnd, TBM_GETRANGEMAX, 0, 0);
    pos  = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);
    span = maxv - minv;
    if (span <= 0 || pos <= minv)      cx = chanL;
    else if (pos >= maxv)              cx = chanR;
    else cx = chanL + (int)((double)(pos - minv) * chanW / span);

    brThumb  = CreateSolidBrush(colThumb);
    penThumb = CreatePen(PS_SOLID, 1, colThumbEdge);
    oldBr    = SelectObject(dc, brThumb);
    oldPen   = SelectObject(dc, penThumb);
    RoundRect(dc, cx - TRK_THUMB_W / 2, cy - TRK_THUMB_H / 2,
                  cx - TRK_THUMB_W / 2 + TRK_THUMB_W,
                  cy - TRK_THUMB_H / 2 + TRK_THUMB_H, 3, 3);
    SelectObject(dc, oldBr);
    SelectObject(dc, oldPen);
    DeleteObject(penThumb);
    DeleteObject(brThumb);

    ReleaseDC(hwnd, dc);
}

static LRESULT CALLBACK SliderThemeSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                            UINT_PTR uid, DWORD_PTR ref)
{
    (void)uid; (void)ref;
    if (msg == WM_PAINT) {
        /* 先让控件走完默认流程（保证裁剪与区域校验），再整条覆盖自绘。
           浅色深色都自绘：深色沿用原有色，绝不会落成白底。 */
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        PaintReplaySlider(hwnd);
        return r;
    }
    if (msg == WM_ERASEBKGND) {
        /* 自绘整块背景，禁止默认擦除，避免闪一下系统亮底 */
        HDC hdc = (HDC)wp;
        RECT rc;
        HBRUSH br;
        GetClientRect(hwnd, &rc);
        br = CreateSolidBrush(GetBgColor());
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        return 1;
    }
    /* 拖动结束 / 键盘 / 尺寸 / 启用态变化后补刷一次 */
    if (msg == WM_SIZE || msg == WM_LBUTTONUP || msg == WM_KEYUP ||
        msg == WM_ENABLE || msg == WM_SETFOCUS || msg == WM_KILLFOCUS ||
        msg == WM_HSCROLL) {
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        InvalidateRect(hwnd, NULL, FALSE);
        return r;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ---- 3) 复选框方块 ----
   经典 BS_AUTOCHECKBOX 的方块固定用 COLOR_WINDOW（白），不吃 WM_CTLCOLOR*，
   深色下必然是一块白。方块几何为实测值：13x13，位于 (0, (h-13)/2)。
   默认绘制之后把方块重涂成「#727272 细边 + 深色面 + 白勾」。 */
#define CHK_BOX_SIZE 13

static LRESULT CALLBACK CheckboxSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                         UINT_PTR uid, DWORD_PTR ref)
{
    (void)uid; (void)ref;
    if (msg == WM_PAINT) {
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        if (g_state.isDark) {
            RECT cr, box;
            GetClientRect(hwnd, &cr);
            if (cr.bottom >= CHK_BOX_SIZE) {
                bool checked = (SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
                bool enabled = IsWindowEnabled(hwnd) != FALSE;
                HDC dc = GetDC(hwnd);
                HBRUSH edge = CreateSolidBrush(COLOR_FIELD_EDGE_DARK);
                HBRUSH face = CreateSolidBrush(GetCardColor());

                box.left = 0;
                box.top = (cr.bottom - CHK_BOX_SIZE) / 2;
                box.right = CHK_BOX_SIZE;
                box.bottom = box.top + CHK_BOX_SIZE;
                FillRect(dc, &box, edge);
                InflateRect(&box, -1, -1);
                FillRect(dc, &box, face);

                if (checked) {
                    HPEN pen = CreatePen(PS_SOLID, 2,
                                         enabled ? GetTextColorValue() : GetGrayTextColor());
                    HGDIOBJ op = SelectObject(dc, pen);
                    POINT p[3];
                    p[0].x = box.left + 2; p[0].y = box.top + 5;
                    p[1].x = box.left + 4; p[1].y = box.top + 7;
                    p[2].x = box.left + 9; p[2].y = box.top + 2;
                    Polyline(dc, p, 3);
                    SelectObject(dc, op);
                    DeleteObject(pen);
                }
                DeleteObject(edge);
                DeleteObject(face);
                ReleaseDC(hwnd, dc);
            }
        }
        return r;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ===== Create a static label ===== */
static HWND CreateLabel(HWND parent, int strId, int id,
                        int x, int y, int w, int h)
{
    HWND hw = CreateWindowExW(0, L"STATIC", T(strId),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)s_hFontNormal, TRUE);
    Lang_Bind(hw, strId);
    return hw;
}

/* Group title label: like CreateLabel but uses the larger group font (s_hFontGroup) */
static HWND CreateGroupLabel(HWND parent, int strId, int x, int y, int w, int h)
{
    HWND hw = CreateLabel(parent, strId, 0, x, y, w, h);
    SendMessageW(hw, WM_SETFONT, (WPARAM)s_hFontGroup, TRUE);
    return hw;
}

/* ===== Create an edit control ===== */
static HWND CreateEdit(HWND parent, const wchar_t* text, int id,
                       int x, int y, int w, int h, DWORD style)
{
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | style,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)s_hFontNormal, TRUE);
    return hw;
}

/* ===== Create a button =====
   用 BS_OWNERDRAW：本程序没有 comctl32 v6 清单（无视觉样式），经典按钮的底色
   由系统按 COLOR_3DFACE 画，WM_CTLCOLORBTN 返回的画刷会被忽略 —— 深色下必然白底。
   自绘后浅色用 DrawFrameControl 复刻原经典外观（零视觉变化），深色画深色底。 */
static HWND CreateButton(HWND parent, int strId, int id,
                         int x, int y, int w, int h, bool primary)
{
    HWND hw = CreateWindowExW(0, L"BUTTON", T(strId),
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)(primary ? s_hFontBold : s_hFontNormal), TRUE);
    Lang_Bind(hw, strId);
    SetWindowSubclass(hw, ButtonHoverSubclass, 3, 0);   /* hover 才出面/边框 */
    return hw;
}

/* ===== 自绘按钮（BS_OWNERDRAW）=====
   浅色：用 DrawFrameControl 复刻原经典按钮外观（零视觉变化）。
   深色：自绘深色底 + 浅色文字；平时只画面色，hover / 按下才出面 + 1px 边框。
   主窗口与 License 对话框共用同一份（两边的 WM_DRAWITEM 都转到这里）。
   返回 true 表示已处理。 */
static bool DrawOwnerButton(const DRAWITEMSTRUCT* dis)
{
    RECT rc, tr;
    HDC hdc;
    bool pressed, disabled, focused;
    wchar_t text[256];
    HFONT font, oldFont;

    if (!dis || dis->CtlType != ODT_BUTTON) return false;

    rc = dis->rcItem;
    hdc = dis->hDC;
    pressed  = (dis->itemState & ODS_SELECTED) != 0;
    disabled = (dis->itemState & ODS_DISABLED) != 0;
    focused  = (dis->itemState & ODS_FOCUS) != 0;

    GetWindowTextW(dis->hwndItem, text, 256);
    font = (HFONT)SendMessageW(dis->hwndItem, WM_GETFONT, 0, 0);

    if (g_state.isDark) {
        /* Excel 式：平时只有面色、不画常驻边框；hover / 按下才出面与 1px 边框。
           面色 #292929 与页面底色 #1C2029 有可见差异 → 按钮边界仍可辨识。
           hover 按真实光标位置判定（无状态，见 ButtonHoverSubclass 注释）。 */
        POINT cur;
        bool hot;
        HBRUSH face;
        GetCursorPos(&cur);
        /* 光标落在本按钮上才算 hover（禁用按钮被 WindowFromPoint 跳过 →
           正好不会高亮，符合预期）。 */
        hot = (WindowFromPoint(cur) == dis->hwndItem);
        face = CreateSolidBrush(pressed ? COLOR_BTN_PRESS_DARK
                                        : (hot ? COLOR_BTN_HOVER_DARK : GetCardColor()));
        FillRect(hdc, &rc, face);
        DeleteObject(face);
        if (hot || pressed) {
            HPEN pen = CreatePen(PS_SOLID, 1, COLOR_FIELD_EDGE_DARK);
            HGDIOBJ op = SelectObject(hdc, pen);
            HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(pen);
        }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, disabled ? GetGrayTextColor() : GetTextColorValue());
    } else {
        UINT flags = DFCS_BUTTONPUSH;
        if (pressed)  flags |= DFCS_PUSHED;
        if (disabled) flags |= DFCS_INACTIVE;
        DrawFrameControl(hdc, &rc, DFC_BUTTON, flags);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, disabled ? GetSysColor(COLOR_GRAYTEXT)
                                   : GetSysColor(COLOR_BTNTEXT));
    }

    oldFont = (HFONT)SelectObject(hdc, font);
    tr = rc;
    if (pressed) OffsetRect(&tr, 1, 1);
    DrawTextW(hdc, text, -1, &tr,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
    if (focused) {
        RECT fr = rc;
        InflateRect(&fr, -3, -3);
        DrawFocusRect(hdc, &fr);
    }
    return true;
}

/* ===== Create a checkbox ===== */
static HWND CreateCheckbox(HWND parent, int strId, int id,
                           int x, int y, int w, int h)
{
    HWND hw = CreateWindowExW(0, L"BUTTON", T(strId),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)s_hFontNormal, TRUE);
    Lang_Bind(hw, strId);
    SetWindowSubclass(hw, CheckboxSubclass, 4, 0);      /* 深色下重涂方块 */
    return hw;
}

/* ===== ComboBox subclass =====
   深色下，主题引擎给下拉箭头按钮画的是「浅底黑三角」，且不受 WM_CTLCOLOR* 控制。
   这里在默认绘制之后把箭头按钮那一格重涂成「同底色 + 细白勾（chevron）」——
   与 Excel 深色下拉一致：箭头与字段同底、没有独立按钮块。 */
static LRESULT CALLBACK ComboSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR uid, DWORD_PTR ref)
{
    (void)uid; (void)ref;
    if (msg == WM_PAINT) {
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        if (g_state.isDark) {
            COMBOBOXINFO ci;
            RECT cr;
            HDC dc = GetDC(hwnd);
            /* 本程序 combo 没带 WS_EX_CLIENTEDGE，经典凹陷边（A0A0A0+696969 两像素）
               是画在「客户区」里的 —— WM_NCPAINT 管不到它，只能在这里重涂。 */
            GetClientRect(hwnd, &cr);
            PaintFrameRing(dc, cr.right, cr.bottom);
            memset(&ci, 0, sizeof(ci));
            ci.cbSize = sizeof(ci);
            /* 注意：ci.rcButton 已经是 combo 的「客户区」坐标，不要再 ScreenToClient
               （先前多转一次 → 重涂画到控件外，看起来毫无变化）。 */
            if (GetComboBoxInfo(hwnd, &ci) && ci.rcButton.right > ci.rcButton.left) {
                RECT br = ci.rcButton;
                /* 底色 */
                HBRUSH b = CreateSolidBrush(GetCardColor());
                FillRect(dc, &br, b);
                DeleteObject(b);
                /* 细白勾（chevron）：2px 描边，非实心三角 */
                {
                    int cx = (br.left + br.right) / 2;
                    int cy = (br.top + br.bottom) / 2;
                    POINT pts[3];
                    HPEN pen = CreatePen(PS_SOLID, 2, GetTextColorValue());
                    HGDIOBJ op = SelectObject(dc, pen);
                    pts[0].x = cx - 4; pts[0].y = cy - 2;
                    pts[1].x = cx;     pts[1].y = cy + 2;
                    pts[2].x = cx + 4; pts[2].y = cy - 2;
                    Polyline(dc, pts, 3);
                    SelectObject(dc, op);
                    DeleteObject(pen);
                }
            }
            ReleaseDC(hwnd, dc);
        }
        return r;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

/* ===== Create a combo box ===== */
static HWND CreateCombo(HWND parent, int id, int x, int y, int w, int h)
{
    HWND hw = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)s_hFontNormal, TRUE);
    SetWindowSubclass(hw, ComboSubclassProc, 1, 0);
    return hw;
}

/* ===== Create a RichEdit control ===== */
static HWND CreateRichEdit(HWND parent, int id, int x, int y, int w, int h)
{
    LPCWSTR cls = s_hRichEdit50Loaded ? L"RichEdit50W" : L"RichEdit20W";
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, cls, L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
        WS_VSCROLL | ES_AUTOVSCROLL,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    if (!hw) {
        hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
            WS_VSCROLL | ES_AUTOVSCROLL,
            x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    }
    /* Same font as the summary box (Consolas 16px) */
    SendMessageW(hw, WM_SETFONT, (WPARAM)s_hFontMono, TRUE);
    /* Same colours as the summary box: card background + themed text */
    SendMessageW(hw, EM_SETBKGNDCOLOR, 0, (LPARAM)GetCardColor());
    return hw;
}

/* ============================================================
   License 对话框（模态）— 方案：README/ui-settings-plan.md §6
   · 自建模态：禁用主窗口 + 本地消息循环（不用 DialogBoxIndirectParam）
   · 正文自包含：编译进 exe，见 src/license_text.c
   · 深色/浅色继承主窗配色；标题栏走 ApplyTitleBarTheme
   ============================================================ */
#define LICENSE_CLASS_NAME L"iperf3guiLicenseWnd"

static HWND s_hLicDlg  = NULL;
static HWND s_hLicText = NULL;

/* 组装整篇正文：版权行 + 摘要 + MIT 原文 + 附注（§6.2 段序）。
   摘要与附注跟随当前界面语言（简/繁/英）；MIT 条款原文恒英文，三语共用一套。 */
static void Lic_BuildBody(wchar_t* buf, int cch)
{
    const wchar_t* sum;
    const wchar_t* note;

    switch (Lang_GetAct()) {
    case LANG_ZH_TW: sum = kLicSummaryTw; note = kLicNoteTw; break;
    case LANG_EN:    sum = kLicSummaryEn; note = kLicNoteEn; break;
    default:         sum = kLicSummaryZh; note = kLicNoteZh; break;
    }
    (void)cch;
    buf[0] = L'\0';
    lstrcatW(buf, kLicCopyright);
    lstrcatW(buf, L"\r\n\r\n");
    lstrcatW(buf, sum);
    lstrcatW(buf, L"\r\n\r\n");
    lstrcatW(buf, kLicMitFull);
    lstrcatW(buf, L"\r\n\r\n");
    lstrcatW(buf, note);
}

/* 重灌正文。RichEdit 的字色不受 WM_SETTEXT 保留 → 每次重灌都要重设。 */
static void Lic_Fill(void)
{
    static wchar_t body[8192];
    if (!s_hLicText) return;
    Lic_BuildBody(body, 8192);
    SetWindowTextW(s_hLicText, body);
    ApplyRichEditTextColor(s_hLicText);
}

/* 对话框内按钮：自绘 + hover 子类；不登记语言表（避免对话框销毁后 Lang_Refresh 误刷） */
static HWND Lic_MakeButton(HWND parent, const wchar_t* text, int id,
                           int x, int y, int w, int h, bool bold)
{
    HWND hw = CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)(bold ? s_hFontBold : s_hFontNormal), TRUE);
    SetWindowSubclass(hw, ButtonHoverSubclass, 3, 0);
    return hw;
}

/* 复制整篇正文到剪贴板（§6.3） */
static void Lic_CopyToClipboard(void)
{
    static wchar_t body[8192];
    HGLOBAL hMem;
    size_t len;
    Lic_BuildBody(body, 8192);
    len = (lstrlenW(body) + 1) * sizeof(wchar_t);
    if (!OpenClipboard(s_hLicDlg)) return;
    EmptyClipboard();
    hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        void* p = GlobalLock(hMem);
        if (p) memcpy(p, body, len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

static LRESULT CALLBACK LicenseWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        /* 顶部不再放 中/英 切换按钮：摘要与附注直接跟随界面语言（简/繁/英），
           MIT 条款原文恒英文共用一套。故正文上移、高度吃掉原按钮那一行。 */
        s_hLicText = CreateRichEdit(hwnd, IDC_LICENSE_TEXT, 12, 12, 528, 384);
        Lic_MakeButton(hwnd, T(STR_BTN_COPY),  BTN_LICENSE_COPY,  352, 404, 90, 28, false);
        Lic_MakeButton(hwnd, T(STR_BTN_CLOSE), BTN_LICENSE_CLOSE, 450, 404, 90, 28, true);
        Lic_Fill();
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == BTN_LICENSE_COPY) { Lic_CopyToClipboard(); return 0; }
        if (id == BTN_LICENSE_CLOSE || id == IDCANCEL) {
            DestroyWindow(hwnd);    /* Esc 由 IsDialogMessage 转成 IDCANCEL */
            return 0;
        }
        return 0;
    }

    case WM_DRAWITEM:
        if (DrawOwnerButton((const DRAWITEMSTRUCT*)lp)) return TRUE;
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, GetTextColorValue());
        SetBkColor(dc, GetBgColor());
        EnsureThemeBrushes();
        return (LRESULT)s_bgBrush;
    }

    case WM_ERASEBKGND:
        return 1;   /* 统一在 WM_PAINT 里填 */

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        HBRUSH b;
        GetClientRect(hwnd, &rc);
        b = CreateSolidBrush(GetBgColor());
        FillRect(dc, &rc, b);
        DeleteObject(b);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hLicDlg  = NULL;
        s_hLicText = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* 打开模态对话框（设置页 License 按钮的入口） */
void UI_ShowLicenseDialog(void)
{
    static bool s_clsReg = false;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_state.hMain, GWLP_HINSTANCE);
    const int dw = 560, dh = 480;
    RECT mr;
    MSG msg;
    int x, y;

    if (s_hLicDlg) { SetForegroundWindow(s_hLicDlg); return; }

    if (!s_clsReg) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = LicenseWndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hbrBackground = NULL;
        wc.lpszClassName = LICENSE_CLASS_NAME;
        if (!RegisterClassExW(&wc)) return;
        s_clsReg = true;
    }

    GetWindowRect(g_state.hMain, &mr);
    x = mr.left + ((mr.right - mr.left) - dw) / 2;
    y = mr.top  + ((mr.bottom - mr.top) - dh) / 2;

    s_hLicDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, LICENSE_CLASS_NAME,
        T(STR_LICENSE_TITLE), WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, dw, dh, g_state.hMain, NULL, hInst, NULL);
    if (!s_hLicDlg) return;
    ApplyTitleBarTheme(s_hLicDlg);               /* 深色下对话框标题栏同样变深 */
    ShowWindow(s_hLicDlg, SW_SHOW);
    UpdateWindow(s_hLicDlg);
    SetFocus(s_hLicDlg);

    while (IsWindow(s_hLicDlg) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(s_hLicDlg, &msg)) {
            /* 模态：拦截发往主窗的输入消息，不 Disable 主窗以免标题栏闪灰 */
            if (msg.hwnd != s_hLicDlg && !IsChild(s_hLicDlg, msg.hwnd) &&
                (msg.message == WM_COMMAND || msg.message == WM_NOTIFY ||
                 (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) ||
                 (msg.message >= WM_KEYFIRST  && msg.message <= WM_KEYLAST)))
                continue;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

/* ============================================================
   说明对话框（模态）— iperf3 下载来源 + 自动检测查找顺序
   · 与 License 对话框同款：自建模态（禁用主窗 + 本地消息循环）
   · 正文自包含、三语各一份，见 src/help_text.c，按界面语言取一套
   · 不做语言切换按钮：模态期间主窗被禁用，界面语言不可能变
   ============================================================ */
#define HELP_CLASS_NAME L"iperf3guiHelpWnd"

static HWND s_hHelpDlg  = NULL;
static HWND s_hHelpText = NULL;

/* 取当前界面语言对应的正文（繁/英/简） */
static const wchar_t* Help_Body(void)
{
    switch (Lang_GetAct()) {
    case LANG_ZH_TW: return kHelpIperf3Tw;
    case LANG_EN:    return kHelpIperf3En;
    default:         return kHelpIperf3Zh;
    }
}

/* 重灌正文。RichEdit 的字色不随 WM_SETTEXT 保留 → 每次都要重设。 */
static void Help_Fill(void)
{
    if (!s_hHelpText) return;
    SetWindowTextW(s_hHelpText, Help_Body());
    ApplyRichEditTextColor(s_hHelpText);
}

static LRESULT CALLBACK HelpWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        s_hHelpText = CreateRichEdit(hwnd, IDC_HELP_TEXT, 12, 12, 536, 296);
        /* 只有一个「关闭」按钮，水平居中：(560-90)/2 = 235 */
        Lic_MakeButton(hwnd, T(STR_BTN_CLOSE), BTN_HELP_CLOSE, 235, 318, 90, 28, true);
        Help_Fill();
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == BTN_HELP_CLOSE || id == IDCANCEL) {
            DestroyWindow(hwnd);    /* Esc 由 IsDialogMessage 转成 IDCANCEL */
            return 0;
        }
        return 0;
    }

    case WM_DRAWITEM:
        if (DrawOwnerButton((const DRAWITEMSTRUCT*)lp)) return TRUE;
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_ERASEBKGND:
        return 1;   /* 统一在 WM_PAINT 里填 */

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        HBRUSH b;
        GetClientRect(hwnd, &rc);
        b = CreateSolidBrush(GetBgColor());
        FillRect(dc, &rc, b);
        DeleteObject(b);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hHelpDlg  = NULL;
        s_hHelpText = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* 打开模态对话框（设置页「说明」按钮的入口） */
void UI_ShowHelpDialog(void)
{
    static bool s_clsReg = false;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_state.hMain, GWLP_HINSTANCE);
    const int dw = 560, dh = 390;
    RECT mr;
    MSG msg;
    int x, y;

    if (s_hHelpDlg) { SetForegroundWindow(s_hHelpDlg); return; }

    if (!s_clsReg) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = HelpWndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        wc.hbrBackground = NULL;
        wc.lpszClassName = HELP_CLASS_NAME;
        if (!RegisterClassExW(&wc)) return;
        s_clsReg = true;
    }

    GetWindowRect(g_state.hMain, &mr);
    x = mr.left + ((mr.right - mr.left) - dw) / 2;
    y = mr.top  + ((mr.bottom - mr.top) - dh) / 2;

    s_hHelpDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, HELP_CLASS_NAME,
        T(STR_BTN_HELP_IPERF3), WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, dw, dh, g_state.hMain, NULL, hInst, NULL);
    if (!s_hHelpDlg) return;
    ApplyTitleBarTheme(s_hHelpDlg);              /* 深色下对话框标题栏同样变深 */
    ShowWindow(s_hHelpDlg, SW_SHOW);
    UpdateWindow(s_hHelpDlg);
    SetFocus(s_hHelpDlg);

    while (IsWindow(s_hHelpDlg) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(s_hHelpDlg, &msg)) {
            /* 模态：拦截发往主窗的输入消息，不 Disable 主窗以免标题栏闪灰 */
            if (msg.hwnd != s_hHelpDlg && !IsChild(s_hHelpDlg, msg.hwnd) &&
                (msg.message == WM_COMMAND || msg.message == WM_NOTIFY ||
                 (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) ||
                 (msg.message >= WM_KEYFIRST  && msg.message <= WM_KEYLAST)))
                continue;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

/* ===== Append output to RichEdit ===== */
void UI_AppendOutput(const char* text, int mode)
{
    HWND hEdit = (mode == MODE_CLIENT) ? g_state.hClientOutput : g_state.hServerOutput;
    bool isError = (text[0] == '[' && (strstr(text, "stderr") || strstr(text, "error")));

    AppendRichText(hEdit, text, isError);

    /* Auto-scroll if enabled */
    bool* autoScroll = (mode == MODE_CLIENT) ? &g_state.clientAutoScroll : &g_state.serverAutoScroll;
    if (*autoScroll) {
        /* Put the caret at the very end, then scroll it into view */
        int n = GetWindowTextLengthW(hEdit);
        SendMessageW(hEdit, EM_SETSEL, (WPARAM)n, (LPARAM)n);
        SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
        SendMessageW(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
    }
}

void UI_ClearOutput(int mode)
{
    /* Clearing via WM_SETTEXT drops all formatting, so re-apply the default
       style (card bg + themed Consolas text) to the whole control. */
    CHARFORMAT2W cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = GetTextColorValue();
    cf.yHeight = 16 * 72 / 96 * 20;  /* 16px -> 12pt -> twips (same as the summary box) */
    wcscpy(cf.szFaceName, L"Consolas");

    if (mode == MODE_CLIENT) {
        SetWindowTextW(g_state.hClientOutput, L"");
        SendMessageW(g_state.hClientOutput, EM_SETBKGNDCOLOR, 0, (LPARAM)GetCardColor());
        SendMessageW(g_state.hClientOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    } else {
        SetWindowTextW(g_state.hServerOutput, L"");
        SendMessageW(g_state.hServerOutput, EM_SETBKGNDCOLOR, 0, (LPARAM)GetCardColor());
        SendMessageW(g_state.hServerOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }
}

void UI_CopyOutput(int mode)
{
    HWND hEdit = (mode == MODE_CLIENT) ? g_state.hClientOutput : g_state.hServerOutput;
    if (!hEdit) return;

    /* Use plain WM_GETTEXT — works on both RichEdit and the plain-EDIT
       fallback. */
    int len = GetWindowTextLengthW(hEdit);
    if (len <= 0) return;

    wchar_t* wbuf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!wbuf) return;
    GetWindowTextW(hEdit, wbuf, len + 1);

    /* Copy to clipboard */
    BOOL copied = FALSE;
    if (OpenClipboard(g_state.hMain)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wcslen(wbuf) + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
            if (pMem) {
                wcscpy(pMem, wbuf);
                GlobalUnlock(hMem);
                if (SetClipboardData(CF_UNICODETEXT, hMem)) {
                    copied = TRUE;
                }
            }
        }
        CloseClipboard();
    }
    free(wbuf);

    /* Feedback goes to the output toolbar (right of 自动滚动) instead of the
       status bar, so it never covers the status text. */
    HWND hHint = (mode == MODE_CLIENT) ? g_state.hClientCopyHint
                                       : g_state.hServerCopyHint;
    if (!hHint) return;
    SetWindowTextW(hHint, copied ? T(STR_COPIED_TO_CLIPBOARD)
                                 : T(STR_COPY_FAILED));
}

/* ===== Chart window proc ===== */
static HWND s_hClientChartHwnd = NULL;
static HWND s_hServerChartHwnd = NULL;
static HWND s_hReplayChartHwnd = NULL;
static HWND s_hLangCombo = NULL;        /* settings: UI language (S3) */
static HWND s_hThemeCombo = NULL;       /* settings: UI theme (S3 theme) */
static HWND s_hClientTransferLbl = NULL; /* client col3 "传输量" label (S3) */
static HWND s_hReplaySpeedLbl = NULL;    /* replay "倍速" label (S3) */
static HWND s_hReplayUnitLbl = NULL;     /* replay "显示单位" label (S3) */

LRESULT CALLBACK ChartWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        /* Determine which chart */
        ChartData* cd = NULL;
        if (hwnd == s_hClientChartHwnd) cd = &g_state.clientChart;
        else if (hwnd == s_hServerChartHwnd) cd = &g_state.serverChart;
        else if (hwnd == s_hReplayChartHwnd) cd = &g_state.replayChart;

        if (cd) {
            cd->isDark = g_state.isDark;
            Chart_Render(cd, hdc, rc);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        /* Legend hit-test: toggle 汇总 line or a per-stream series.
           Applies to whichever chart window received the click. */
        ChartData* cd = NULL;
        if (hwnd == s_hClientChartHwnd) cd = &g_state.clientChart;
        else if (hwnd == s_hServerChartHwnd) cd = &g_state.serverChart;
        else if (hwnd == s_hReplayChartHwnd) cd = &g_state.replayChart;
        if (cd) {
            int x = (int)(short)LOWORD(lParam);
            int y = (int)(short)HIWORD(lParam);
            for (int i = 0; i < cd->legendHitCount; i++) {
                if (x >= cd->legendHitX[i] && x <= cd->legendHitX[i] + cd->legendHitW[i] &&
                    y >= cd->legendHitY[i] && y <= cd->legendHitY[i] + cd->legendHitH[i]) {
                    int idx = cd->legendHitIdx[i];
                    if (idx == -1) {
                        cd->showSum = !cd->showSum;
                    } else if (idx == -2) {
                        cd->showSum2 = !cd->showSum2;
                    } else if (idx >= 0 && idx < CHART_MAX_STREAMS) {
                        cd->seriesVisible[idx] = !cd->seriesVisible[idx];
                        if (cd->seriesVisible[idx]) cd->showStreams = true;
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
            }
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; /* prevent flicker — we draw in WM_PAINT */
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

/* ===== Panel window procedure ===== */
/* Custom panel class that forwards child control notifications to the
   main window. STATIC controls swallow these notifications. */
LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
    case WM_NOTIFY:
    case WM_DRAWITEM:
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX: {
        /* Forward to parent (main window) which has the real handlers */
        HWND hParent = GetParent(hwnd);
        if (hParent) {
            return SendMessageW(hParent, msg, wParam, lParam);
        }
        break;
    }
    case WM_ERASEBKGND: {
        /* Paint our own background to match theme */
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(GetBgColor());
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
        return 1;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ===== Create all controls ===== */
static void CreateAllControls(HWND hwnd)
{
    /* ===== Tab bar buttons (22px: regular by default, bold when active) ===== */
    int tabW = 90, tabH = 30, tabY = 4;
    g_state.hTabClient = CreateButton(hwnd, STR_TAB_CLIENT, BTN_TAB_CLIENT, 4, tabY, tabW, tabH, true);
    g_state.hTabServer = CreateButton(hwnd, STR_TAB_SERVER, BTN_TAB_SERVER, 4 + tabW, tabY, tabW, tabH, false);
    g_state.hTabReplay = CreateButton(hwnd, STR_TAB_REPLAY, BTN_TAB_REPLAY, 4 + tabW*2, tabY, tabW, tabH, false);
    g_state.hTabSettings = CreateButton(hwnd, STR_TAB_SETTINGS, BTN_TAB_SETTINGS, 4 + tabW*3, tabY, tabW, tabH, false);
    /* Global config buttons (right side of tab bar) — positioned in LayoutControls */
    int gcW = 90, gcH = 30;
    g_state.hSaveConfig = CreateButton(hwnd, STR_BTN_SAVE_CONFIG, BTN_SAVE_CONFIG, 0, tabY, gcW, gcH, false);
    g_state.hLoadConfig = CreateButton(hwnd, STR_BTN_LOAD_CONFIG, BTN_LOAD_CONFIG, 0, tabY, gcW, gcH, false);
    g_state.hRestoreDefault = CreateButton(hwnd, STR_BTN_RESTORE_DEFAULT, BTN_RESTORE_DEFAULT, 0, tabY, gcW, gcH, false);
    /* Tab bar buttons use 22px (group font): regular default, bold when selected.
       CreateButton sets primary->bold, others->normal(20px); override all to 22px here. */
    SendMessageW(g_state.hTabClient,       WM_SETFONT, (WPARAM)s_hFontGroup, TRUE);       /* active at startup */
    SendMessageW(g_state.hTabServer,       WM_SETFONT, (WPARAM)s_hFontGroupNormal, TRUE);
    SendMessageW(g_state.hTabReplay,       WM_SETFONT, (WPARAM)s_hFontGroupNormal, TRUE);
    SendMessageW(g_state.hTabSettings,     WM_SETFONT, (WPARAM)s_hFontGroupNormal, TRUE);
    SendMessageW(g_state.hSaveConfig,      WM_SETFONT, (WPARAM)s_hFontGroupNormal, TRUE);
    SendMessageW(g_state.hLoadConfig,      WM_SETFONT, (WPARAM)s_hFontGroupNormal, TRUE);
    SendMessageW(g_state.hRestoreDefault,  WM_SETFONT, (WPARAM)s_hFontGroupNormal, TRUE);

    /* ===== Header status bar (below tab bar) =====
       Left  : 客户端:●运行   服务端:●空闲   (dot colored by state: red=run, green=idle)
       Right : 程序路径: <path>
       Left is split into 6 fixed STATIC segments so only the dots get colored.
       The two groups are separated by a 3-character gap (3 x 14px = 42px at
       17px Segoe UI) so the client / server states read as distinct blocks. */
    g_state.hStatusClientTag = CreateLabel(hwnd, STR_STATUS_CLIENT, LBL_STATUS_CLIENT_TAG,
                                            8, 38, 64, 24);
    g_state.hStatusClientDot = CreateLabel(hwnd, STR_DOT, LBL_STATUS_CLIENT_DOT,
                                            68, 38, 14, 24);
    g_state.hStatusClientVal = CreateLabel(hwnd, STR_STATUS_IDLE, LBL_STATUS_CLIENT_VAL,
                                            82, 38, 30, 24);
    g_state.hStatusServerTag = CreateLabel(hwnd, STR_STATUS_SERVER, LBL_STATUS_SERVER_TAG,
                                            152, 38, 64, 24);
    g_state.hStatusServerDot = CreateLabel(hwnd, STR_DOT, LBL_STATUS_SERVER_DOT,
                                            212, 38, 14, 24);
    g_state.hStatusServerVal = CreateLabel(hwnd, STR_STATUS_IDLE, LBL_STATUS_SERVER_VAL,
                                            226, 38, 30, 24);
    /* 右侧消息输出框：多行、左对齐、只读 EDIT，与状态行(y=38)同一行。
       第 1 行显示 版本 / 未检测到 + 程序类错误（合并，无间隔）；仅当日志路径出错时向下多撑第 2 行(高44)。
       故意不挂 FieldChromeSubclass（见 AttachFieldChromeProc），深色下不画边框；背景由 WM_CTLCOLOREDIT 处理。
       ES_MULTILINE+ES_AUTOHSCROLL 使第 2 行不折行(可横向滚)，高度恒为 22 或 44。 */
    g_state.hStatusRight = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL,
        268, 38, 500, 22, hwnd, NULL, NULL, NULL);
    SendMessageW(g_state.hStatusRight, WM_SETFONT, (WPARAM)s_hFontNormal, TRUE);

    /* ===== Client Panel ===== */
    g_state.hClientPanel = CreateWindowExW(0, PANEL_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, NULL, NULL, NULL);

    /* Left side: settings + command */
    int lx = 12, ly = 0, lw = 280;
    int yy = ly;
    int gap = 8;
    int thirdW = (lw - gap * 2) / 3;

    /* Row 1: Server IP (2/3) + Port (1/3) */
    CreateLabel(g_state.hClientPanel, STR_LBL_SERVER_IP, 0, lx, yy, thirdW * 2 + gap, 20);
    CreateLabel(g_state.hClientPanel, STR_LBL_PORT, 0, lx + thirdW * 2 + gap * 2, yy, thirdW, 20);
    yy += 22;
    g_state.hClientIP = CreateEdit(g_state.hClientPanel, L"127.0.0.1", EDT_CLIENT_IP,
                                    lx, yy, thirdW * 2 + gap, 22, 0);
    g_state.hClientPort = CreateEdit(g_state.hClientPanel, L"5201", EDT_CLIENT_PORT,
                                      lx + thirdW * 2 + gap * 2, yy, thirdW, 22, ES_NUMBER);
    yy += 24;

    /* Row 2: Protocol + Format + NumBytes */
    CreateLabel(g_state.hClientPanel, STR_LBL_PROTO, 0, lx, yy, thirdW, 20);
    CreateLabel(g_state.hClientPanel, STR_LBL_UNIT, 0, lx + thirdW + gap, yy, thirdW, 20);
    s_hClientTransferLbl = CreateLabel(g_state.hClientPanel, STR_LBL_TRANSFER, 0,
                                       lx + (thirdW + gap) * 2, yy, thirdW, 20);
    yy += 22;
    g_state.hClientProto = CreateCombo(g_state.hClientPanel, CBO_CLIENT_PROTO,
                                        lx, yy, thirdW, 100);
    SendMessageW(g_state.hClientProto, CB_ADDSTRING, 0, (LPARAM)L"TCP");
    SendMessageW(g_state.hClientProto, CB_ADDSTRING, 0, (LPARAM)L"UDP");
    SendMessageW(g_state.hClientProto, CB_SETCURSEL, 0, 0);

    g_state.hClientFormat = CreateCombo(g_state.hClientPanel, CBO_CLIENT_FORMAT,
                                         lx + thirdW + gap, yy, thirdW, 120);
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"Kb/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"Mb/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"Gb/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"Tb/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"KB/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"MB/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"GB/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"TB/s");
    SendMessageW(g_state.hClientFormat, CB_ADDSTRING, 0, (LPARAM)L"auto");
    SendMessageW(g_state.hClientFormat, CB_SETCURSEL, 8, 0); /* auto */

    g_state.hClientNum = CreateEdit(g_state.hClientPanel, L"", EDT_CLIENT_NUM,
                                     lx + (thirdW + gap) * 2, yy, thirdW, 22, 0);
    yy += 24;

    /* Row 3: Duration + Interval + Omit */
    CreateLabel(g_state.hClientPanel, STR_LBL_DURATION, 0, lx, yy, thirdW, 20);
    CreateLabel(g_state.hClientPanel, STR_LBL_INTERVAL, 0, lx + thirdW + gap, yy, thirdW, 20);
    s_hClientOmitLabel = CreateLabel(g_state.hClientPanel, STR_LBL_OMIT, 0, lx + (thirdW + gap) * 2, yy, thirdW, 20);
    yy += 22;
    g_state.hClientDuration = CreateEdit(g_state.hClientPanel, L"10", EDT_CLIENT_DURATION,
                                          lx, yy, thirdW, 22, ES_NUMBER);
    g_state.hClientInterval = CreateEdit(g_state.hClientPanel, L"1", EDT_CLIENT_INTERVAL,
                                          lx + thirdW + gap, yy, thirdW, 22, ES_NUMBER);
    g_state.hClientOmit = CreateEdit(g_state.hClientPanel, L"0", EDT_CLIENT_OMIT,
                                      lx + (thirdW + gap) * 2, yy, thirdW, 22, ES_NUMBER);
    yy += 24;

    /* Row 4: Bandwidth (1/3) + Parallel streams / 多流 (1/3) */
    s_hClientBwLabel = CreateLabel(g_state.hClientPanel, STR_LBL_BANDWIDTH, 0, lx, yy, thirdW, 20);
    s_hClientParallelLabel = CreateLabel(g_state.hClientPanel, STR_LBL_PARALLEL, 0,
                                         lx + thirdW + gap, yy, thirdW, 20);
    yy += 22;
    g_state.hClientBw = CreateEdit(g_state.hClientPanel, L"", EDT_CLIENT_BW,
                                    lx, yy, thirdW, 22, 0);
    g_state.hClientParallel = CreateCombo(g_state.hClientPanel, CBO_CLIENT_PARALLEL,
                                          lx + thirdW + gap, yy, thirdW, 120);
    for (int p = 1; p <= 8; p++) {
        wchar_t buf[4];
        swprintf(buf, 4, L"%d", p);
        SendMessageW(g_state.hClientParallel, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendMessageW(g_state.hClientParallel, CB_SETCURSEL, 0, 0); /* 1 */
    yy += 24;

    /* Row 5: Manual extra args (full width) */
    CreateLabel(g_state.hClientPanel, STR_LBL_EXTRA, 0, lx, yy, lw, 20);
    yy += 22;
    g_state.hClientExtra = CreateEdit(g_state.hClientPanel, L"", EDT_CLIENT_EXTRA,
                                       lx, yy, lw, 22, 0);
    yy += 24;

    /* Row 6: Checkboxes — Reverse, Bidir, JSON, GetServerOutput
       Short labels get 56px each, "服务端报告" gets 96px, gap=4 */
    {
        int cbGap = 4;
        int cbShort = 56;
        int cbLong = 96;
        g_state.hClientReverse = CreateCheckbox(g_state.hClientPanel, STR_CHK_REVERSE,
                                                  CHK_CLIENT_REVERSE, lx, yy, cbShort, 20);
        g_state.hClientBidir = CreateCheckbox(g_state.hClientPanel, STR_CHK_BIDIR,
                                                CHK_CLIENT_BIDIR, lx + cbShort + cbGap, yy, cbShort, 20);
        g_state.hClientJson = CreateCheckbox(g_state.hClientPanel, STR_JSON,
                                               CHK_CLIENT_JSON, lx + (cbShort + cbGap) * 2, yy, cbShort, 20);
        g_state.hClientGetServer = CreateCheckbox(g_state.hClientPanel, STR_CHK_GETSERVER,
                                                    CHK_CLIENT_GETSERVER, lx + (cbShort + cbGap) * 3, yy, cbLong, 20);
    }
    yy += 26;

    /* Row 7: Start / Stop buttons */
    g_state.hClientStart = CreateButton(g_state.hClientPanel, STR_BTN_START,
                                         BTN_CLIENT_START, lx, yy, 90, 28, false);
    g_state.hClientStop = CreateButton(g_state.hClientPanel, STR_BTN_STOP,
                                        BTN_CLIENT_STOP, lx + 98, yy, 90, 28, false);
    yy += 36;

    /* Command display (RichEdit for Ctrl+A support) */
    CreateLabel(g_state.hClientPanel, STR_LBL_CMD, 0, lx, yy, lw, 20);
    yy += 22;
    {
        LPCWSTR cmdCls = s_hRichEdit50Loaded ? L"RichEdit50W" : L"RichEdit20W";
        g_state.hClientCmd = CreateWindowExW(WS_EX_CLIENTEDGE, cmdCls, L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            lx, yy, lw, 64, g_state.hClientPanel, (HMENU)(INT_PTR)EDT_CLIENT_CMD, NULL, NULL);
        if (!g_state.hClientCmd) {
            g_state.hClientCmd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                lx, yy, lw, 64, g_state.hClientPanel, (HMENU)(INT_PTR)EDT_CLIENT_CMD, NULL, NULL);
        }
    }
    SendMessageW(g_state.hClientCmd, WM_SETFONT, (WPARAM)s_hFontMono, TRUE);
    SendMessageW(g_state.hClientCmd, EM_SETBKGNDCOLOR, 0, (LPARAM)GetCardColor());
    ApplyRichEditTextColor(g_state.hClientCmd);   /* RichEdit：字色须单独设，否则黑字 */
    yy += 66;

    /* Summary box (scrollable multi-line edit) */
    CreateLabel(g_state.hClientPanel, STR_LBL_SUMMARY, 0, lx, yy, lw, 20);
    yy += 22;
    g_state.hClientSummaryBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        lx, yy, lw, 200, g_state.hClientPanel, (HMENU)(INT_PTR)LBL_CLIENT_SUMMARY, NULL, NULL);
    SendMessageW(g_state.hClientSummaryBox, WM_SETFONT, (WPARAM)s_hFontMono, TRUE);
    yy += 204;

    /* Right side: chart + output */
    int rx = lx + lw + 12;

    /* Chart */
    /* Register chart class */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = ChartWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = CHART_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    g_state.hClientChartWnd = CreateWindowExW(0, CHART_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE,
        rx, 32, 400, 150, g_state.hClientPanel, (HMENU)(INT_PTR)WND_CLIENT_CHART, NULL, NULL);
    s_hClientChartHwnd = g_state.hClientChartWnd;

    /* Output toolbar */
    int oty = 190 + 12;
    s_hClientOutLbl = CreateLabel(g_state.hClientPanel, STR_LBL_RAW_OUTPUT, 0, rx, oty, 70, 20);
    g_state.hClientClear = CreateButton(g_state.hClientPanel, STR_BTN_CLEAR,
                                         BTN_CLIENT_CLEAR, rx + 74, oty - 2, 90, 22, false);
    g_state.hClientCopy = CreateButton(g_state.hClientPanel, STR_BTN_COPY_ALL,
                                        BTN_CLIENT_COPY, rx + 170, oty - 2, 90, 22, false);
    g_state.hClientAutoScroll = CreateCheckbox(g_state.hClientPanel, STR_CHK_AUTOSCROLL,
                                                 CHK_CLIENT_AUTOSCROLL, rx + 266, oty - 2, 80, 22);
    SendMessageW(g_state.hClientAutoScroll, BM_SETCHECK, BST_CHECKED, 0);
    /* Copy feedback label — sits right of 自动滚动 (was shown in status bar before) */
    g_state.hClientCopyHint = CreateLabel(g_state.hClientPanel, STR_EMPTY, LBL_CLIENT_COPY_HINT,
                                           rx + 352, oty, 200, 20);
    oty += 24;

    /* RichEdit output */
    g_state.hClientOutput = CreateRichEdit(g_state.hClientPanel, EDT_CLIENT_OUTPUT,
                                            rx, oty, 400, 200);

    /* ===== Server Panel ===== */
    g_state.hServerPanel = CreateWindowExW(0, PANEL_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);

    yy = 0;

    /* Row 1: Port (1/3) + Format (1/3) + AF Version (1/3) */
    CreateLabel(g_state.hServerPanel, STR_LBL_LISTEN_PORT, 0, lx, yy, thirdW, 20);
    CreateLabel(g_state.hServerPanel, STR_LBL_UNIT, 0, lx + thirdW + gap, yy, thirdW, 20);
    CreateLabel(g_state.hServerPanel, STR_LBL_AF, 0, lx + (thirdW + gap) * 2, yy, thirdW, 20);
    yy += 22;
    g_state.hServerPort = CreateEdit(g_state.hServerPanel, L"5201", EDT_SERVER_PORT,
                                      lx, yy, thirdW, 22, ES_NUMBER);

    g_state.hServerFormat = CreateCombo(g_state.hServerPanel, CBO_SERVER_FORMAT,
                                         lx + thirdW + gap, yy, thirdW, 120);
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"Kb/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"Mb/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"Gb/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"Tb/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"KB/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"MB/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"GB/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"TB/s");
    SendMessageW(g_state.hServerFormat, CB_ADDSTRING, 0, (LPARAM)L"auto");
    SendMessageW(g_state.hServerFormat, CB_SETCURSEL, 8, 0); /* auto */

    g_state.hServerAf = CreateCombo(g_state.hServerPanel, CBO_SERVER_AF,
                                     lx + (thirdW + gap) * 2, yy, thirdW, 100);
    SendMessageW(g_state.hServerAf, CB_ADDSTRING, 0, (LPARAM)L"IPv4");
    SendMessageW(g_state.hServerAf, CB_ADDSTRING, 0, (LPARAM)L"IPv6");
    SendMessageW(g_state.hServerAf, CB_ADDSTRING, 0, (LPARAM)L"both");
    SendMessageW(g_state.hServerAf, CB_SETCURSEL, 2, 0); /* both */
    yy += 24;

    /* Row 2: Manual extra args (full width) */
    CreateLabel(g_state.hServerPanel, STR_LBL_EXTRA, 0, lx, yy, lw, 20);
    yy += 22;
    g_state.hServerExtra = CreateEdit(g_state.hServerPanel, L"", EDT_SERVER_EXTRA,
                                       lx, yy, lw, 22, 0);
    yy += 24;

    /* Row 3: Checkboxes — OneOff, Verbose, JSON */
    {
        int cw = (lw - gap * 2) / 3;
        g_state.hServerOneOff = CreateCheckbox(g_state.hServerPanel, STR_CHK_ONEOFF,
                                                CHK_SERVER_ONEOFF, lx, yy, cw, 20);
        g_state.hServerVerbose = CreateCheckbox(g_state.hServerPanel, STR_CHK_VERBOSE,
                                                 CHK_SERVER_VERBOSE, lx + cw + gap, yy, cw, 20);
        g_state.hServerJson = CreateCheckbox(g_state.hServerPanel, STR_JSON,
                                               CHK_SERVER_JSON, lx + (cw + gap) * 2, yy, cw, 20);
    }
    yy += 26;

    /* Row 4: Start / Stop buttons */
    g_state.hServerStart = CreateButton(g_state.hServerPanel, STR_BTN_START_SERVER,
                                         BTN_SERVER_START, lx, yy, 90, 28, false);
    g_state.hServerStop = CreateButton(g_state.hServerPanel, STR_BTN_STOP_SERVER,
                                        BTN_SERVER_STOP, lx + 98, yy, 90, 28, false);
    yy += 36;

    /* Command display box (RichEdit for Ctrl+A support) */
    CreateLabel(g_state.hServerPanel, STR_LBL_CMD, 0, lx, yy, lw, 20);
    yy += 22;
    {
        LPCWSTR cmdCls = s_hRichEdit50Loaded ? L"RichEdit50W" : L"RichEdit20W";
        g_state.hServerCmd = CreateWindowExW(WS_EX_CLIENTEDGE, cmdCls, L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            lx, yy, lw, 64, g_state.hServerPanel, (HMENU)(INT_PTR)EDT_SERVER_CMD, NULL, NULL);
        if (!g_state.hServerCmd) {
            g_state.hServerCmd = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                lx, yy, lw, 64, g_state.hServerPanel, (HMENU)(INT_PTR)EDT_SERVER_CMD, NULL, NULL);
        }
    }
    SendMessageW(g_state.hServerCmd, WM_SETFONT, (WPARAM)s_hFontMono, TRUE);
    SendMessageW(g_state.hServerCmd, EM_SETBKGNDCOLOR, 0, (LPARAM)GetCardColor());
    ApplyRichEditTextColor(g_state.hServerCmd);   /* RichEdit：字色须单独设，否则黑字 */
    yy += 66;

    /* Summary box (scrollable multi-line edit) */
    CreateLabel(g_state.hServerPanel, STR_LBL_SUMMARY, 0, lx, yy, lw, 20);
    yy += 22;
    g_state.hServerSummaryBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        lx, yy, lw, 200, g_state.hServerPanel, (HMENU)(INT_PTR)LBL_SERVER_SUMMARY, NULL, NULL);
    SendMessageW(g_state.hServerSummaryBox, WM_SETFONT, (WPARAM)s_hFontMono, TRUE);
    yy += 204;

    /* Server right side */
    g_state.hServerChartWnd = CreateWindowExW(0, CHART_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE, rx, 32, 400, 150, g_state.hServerPanel, (HMENU)(INT_PTR)WND_SERVER_CHART, NULL, NULL);
    s_hServerChartHwnd = g_state.hServerChartWnd;

    int soty = 190 + 12;
    s_hServerOutLbl = CreateLabel(g_state.hServerPanel, STR_LBL_RAW_OUTPUT, 0, rx, soty, 70, 20);
    g_state.hServerClear = CreateButton(g_state.hServerPanel, STR_BTN_CLEAR,
                                         BTN_SERVER_CLEAR, rx + 74, soty - 2, 90, 22, false);
    g_state.hServerCopy = CreateButton(g_state.hServerPanel, STR_BTN_COPY_ALL,
                                        BTN_SERVER_COPY, rx + 170, soty - 2, 90, 22, false);
    g_state.hServerAutoScroll = CreateCheckbox(g_state.hServerPanel, STR_CHK_AUTOSCROLL,
                                                 CHK_SERVER_AUTOSCROLL, rx + 266, soty - 2, 80, 22);
    SendMessageW(g_state.hServerAutoScroll, BM_SETCHECK, BST_CHECKED, 0);
    /* Copy feedback label — sits right of 自动滚动 (was shown in status bar before) */
    g_state.hServerCopyHint = CreateLabel(g_state.hServerPanel, STR_EMPTY, LBL_SERVER_COPY_HINT,
                                           rx + 352, soty, 200, 20);
    soty += 24;

    g_state.hServerOutput = CreateRichEdit(g_state.hServerPanel, EDT_SERVER_OUTPUT,
                                            rx, soty, 400, 200);

    /* ===== Settings Panel =====
       排版（2026-09-23 修订）：删掉 G1 末尾"检测到的路径"只读框；
       G2~G5 整体上移 28px 关掉空档；组标题用 22px 加粗（CreateGroupLabel）。
       组间间距 2× = 16px；组内行间距 1× = 8px；组名 → 第一行内容 间距 1× = 8px 的一半（4px）。
       G1 路径输入框/按钮从 sy+38 下移到 sy+44，使 G1 与 G2~G5 的组名到内容间距统一为 4px。 */
    g_state.hSettingsPanel = CreateWindowExW(0, PANEL_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);
    int sy = 0;

    /* ===== G1: iperf3 路径 ===== */
    CreateGroupLabel(g_state.hSettingsPanel, STR_LBL_IPERF3_PATH, 12, sy + 16, 200, 24);
    g_state.hSettingsPath = CreateEdit(g_state.hSettingsPanel, L"", EDT_SETTINGS_PATH,
                                       12, sy + 44, 400, 22, 0);
    g_state.hSettingsBrowse = CreateButton(g_state.hSettingsPanel, STR_BTN_BROWSE,
                                            BTN_SETTINGS_BROWSE, 420, sy + 44, 124, 22, false);
    g_state.hSettingsDetect = CreateButton(g_state.hSettingsPanel, STR_BTN_AUTODETECT,
                                            BTN_SETTINGS_DETECT, 550, sy + 44, 124, 22, false);
    g_state.hHelpIperf3 = CreateButton(g_state.hSettingsPanel, STR_BTN_HELP_IPERF3,
                                       BTN_HELP_IPERF3, 680, sy + 44, 124, 22, false);

    /* ===== G2: 日志 ===== */
    CreateGroupLabel(g_state.hSettingsPanel, STR_LBL_LOG_SECTION, 12, sy + 82, 200, 24);
    g_state.hLogEnable = CreateCheckbox(g_state.hSettingsPanel, STR_CHK_LOG_ENABLE,
                                         CHK_LOG_ENABLE, 12, sy + 110, 200, 22);
    s_hLogDirLabel = CreateLabel(g_state.hSettingsPanel, STR_LBL_LOG_DIR, LBL_LOG_DIR, 12, sy + 140, 200, 20);
    g_state.hLogDir = CreateEdit(g_state.hSettingsPanel, L"./logs", EDT_LOG_DIR,
                                  12, sy + 162, 400, 22, 0);
    g_state.hLogBrowse = CreateButton(g_state.hSettingsPanel, STR_BTN_BROWSE_DIR,
                                       BTN_LOG_BROWSE, 420, sy + 162, 124, 22, false);
    s_hLogNamingLabel = CreateLabel(g_state.hSettingsPanel, STR_NAMING_RULE, LBL_LOG_NAMING, 12, sy + 192, 400, 20);
    g_state.hLogNaming = CreateEdit(g_state.hSettingsPanel, L"%mode-%ip-%Y%M%D-%h%m%s.log",
                                     EDT_LOG_NAMING, 12, sy + 214, 400, 22, 0);
    g_state.hLogNamingHelp = CreateButton(g_state.hSettingsPanel, STR_NAMING_RULE,
                                          BTN_LOG_NAMING_HELP, 420, sy + 214, 124, 22, false);
    s_hLogMaxSizeLabel = CreateLabel(g_state.hSettingsPanel, STR_LBL_LOG_MAXSIZE, LBL_LOG_MAXSIZE, 12, sy + 244, 200, 20);
    g_state.hLogMaxSize = CreateEdit(g_state.hSettingsPanel, L"1024", EDT_LOG_MAXSIZE,
                                      12, sy + 266, 80, 22, ES_NUMBER);
    s_hLogMaxSizeUnitLabel = CreateLabel(g_state.hSettingsPanel, STR_KB, 0, 96, sy + 270, 30, 20);

    /* ===== G3: 图表 ===== */
    CreateGroupLabel(g_state.hSettingsPanel, STR_LBL_CHART_SECTION, 12, sy + 310, 200, 24);
    g_state.hShowStreams = CreateCheckbox(g_state.hSettingsPanel,
                                          STR_CHK_SHOW_STREAMS,
                                          CHK_SHOW_STREAMS, 12, sy + 338, 384, 22);
    /* 「隐藏流汇总」：仅在勾了「显示各流」时可点（EnableWindow 由 ApplyShowStreams 管），
       未勾选时置灰但保留已存值，重新勾回来自动恢复。 */
    g_state.hHideStreamSum = CreateCheckbox(g_state.hSettingsPanel,
                                            STR_CHK_HIDE_STREAM_SUM,
                                            CHK_HIDE_STREAM_SUM, 12, sy + 366, 384, 22);

    /* ===== G4: 外观 ===== */
    /* 2026-09-24：G4/G5 整体 +28px，给 G3 新增的「隐藏流汇总」让位 */
    CreateGroupLabel(g_state.hSettingsPanel, STR_LBL_APPEARANCE_SECTION, 12, sy + 404, 200, 24);
    CreateLabel(g_state.hSettingsPanel, STR_LBL_LANG, 0, 12, sy + 432, 200, 20);
    s_hLangCombo = CreateCombo(g_state.hSettingsPanel, CBO_LANG, 12, sy + 454, 200, 120);
    Lang_FillCombo();
    CreateLabel(g_state.hSettingsPanel, STR_LBL_THEME, 0, 12, sy + 484, 200, 20);
    s_hThemeCombo = CreateCombo(g_state.hSettingsPanel, CBO_THEME, 12, sy + 506, 200, 120);
    Theme_FillCombo();

    /* ===== G5: 其它 ===== */
    CreateGroupLabel(g_state.hSettingsPanel, STR_LBL_OTHER_SECTION, 12, sy + 544, 200, 24);
    g_state.hLicense = CreateButton(g_state.hSettingsPanel, STR_BTN_LICENSE,
                                    BTN_LICENSE, 12, sy + 572, 124, 28, false);

    /* ===== Replay Panel =====
       All coordinates here are placeholders — LayoutControls repositions
       everything (the panel is sized to the full client area). */
    g_state.hReplayPanel = CreateWindowExW(0, PANEL_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, NULL, NULL, NULL);

    /* Row 1: log file path + 浏览 (choosing a file loads it immediately) */
    g_state.hReplayPath = CreateEdit(g_state.hReplayPanel, L"", EDT_REPLAY_PATH,
                                     12, 0, 400, 22, 0);
    g_state.hReplayBrowse = CreateButton(g_state.hReplayPanel, STR_BTN_BROWSE_ELLIPSIS,
                                         BTN_REPLAY_BROWSE, 420, 0, 90, 22, false);

    /* Row 2: source / format / sample count / duration */
    g_state.hReplayInfo = CreateLabel(g_state.hReplayPanel, STR_REPLAY_NO_FILE,
                                      LBL_REPLAY_INFO, 12, 28, 500, 20);

    /* Row 3: position slider + time readout */
    g_state.hReplaySlider = CreateWindowExW(0, TRACKBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | TBS_NOTICKS,
        12, 52, 400, 24, g_state.hReplayPanel,
        (HMENU)(INT_PTR)TRK_REPLAY_POS, NULL, NULL);
    SendMessageW(g_state.hReplaySlider, TBM_SETRANGE, TRUE, MAKELONG(0, 0));
    SendMessageW(g_state.hReplaySlider, TBM_SETPOS, TRUE, 0);
    /* 经典 trackbar 不吃主题色，挂自绘子类让它跟随浅/深色切换（深色不落白底） */
    SetWindowSubclass(g_state.hReplaySlider, SliderThemeSubclass, 4, 0);
    g_state.hReplayTime = CreateLabel(g_state.hReplayPanel, STR_REPLAY_TIME_INIT,
                                      LBL_REPLAY_TIME, 420, 56, 200, 20);

    /* Row 4: 播放 / 停止 / 倍速 / 当前值 */
    g_state.hReplayPlay = CreateButton(g_state.hReplayPanel, STR_BTN_PLAY,
                                       BTN_REPLAY_PLAY, 12, 84, 80, 26, false);
    g_state.hReplayStop = CreateButton(g_state.hReplayPanel, STR_BTN_STOP_PLAY,
                                       BTN_REPLAY_STOP, 100, 84, 80, 26, false);
    s_hReplaySpeedLbl = CreateLabel(g_state.hReplayPanel, STR_LBL_SPEED, 0, 188, 88, 36, 20);
    g_state.hReplaySpeed = CreateCombo(g_state.hReplayPanel, CBO_REPLAY_SPEED,
                                       228, 86, 70, 100);
    SendMessageW(g_state.hReplaySpeed, CB_ADDSTRING, 0, (LPARAM)L"1x");
    SendMessageW(g_state.hReplaySpeed, CB_ADDSTRING, 0, (LPARAM)L"1.5x");
    SendMessageW(g_state.hReplaySpeed, CB_ADDSTRING, 0, (LPARAM)L"2x");
    SendMessageW(g_state.hReplaySpeed, CB_ADDSTRING, 0, (LPARAM)L"3x");
    SendMessageW(g_state.hReplaySpeed, CB_ADDSTRING, 0, (LPARAM)L"4x");
    SendMessageW(g_state.hReplaySpeed, CB_SETCURSEL, 0, 0);
    /* 显示单位：与客户端/服务端页同款 9 档（末项 auto = Mb/s），放在倍速之后 */
    s_hReplayUnitLbl = CreateLabel(g_state.hReplayPanel, STR_LBL_UNIT, 0,
                                   310, 88, 58, 20);
    g_state.hReplayUnit = CreateCombo(g_state.hReplayPanel, CBO_REPLAY_UNIT,
                                      372, 86, 70, 100);
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"Kb/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"Mb/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"Gb/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"Tb/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"KB/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"MB/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"GB/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"TB/s");
    SendMessageW(g_state.hReplayUnit, CB_ADDSTRING, 0, (LPARAM)L"auto");
    SendMessageW(g_state.hReplayUnit, CB_SETCURSEL, REPLAY_UNIT_AUTO, 0);
    g_state.hReplayValues = CreateLabel(g_state.hReplayPanel, STR_EMPTY,
                                        LBL_REPLAY_VALUES, 452, 88, 400, 20);

    /* Chart (reuses CHART_CLASS_NAME — registered above for the client chart) */
    g_state.hReplayChartWnd = CreateWindowExW(0, CHART_CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE, 12, 118, 400, 150,
        g_state.hReplayPanel, (HMENU)(INT_PTR)WND_REPLAY_CHART, NULL, NULL);
    s_hReplayChartHwnd = g_state.hReplayChartWnd;

    /* Nothing loaded yet → playback controls start disabled */
    EnableWindow(g_state.hReplaySlider, FALSE);
    EnableWindow(g_state.hReplayPlay, FALSE);
    EnableWindow(g_state.hReplayStop, FALSE);
    EnableWindow(g_state.hReplayUnit, FALSE);
}

/* ===== Layout controls ===== */
static void LayoutControls(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right;
    int height = rc.bottom;

    /* 状态栏：第 1 行永远是 客户端/服务端状态(y=38)。右侧消息框与状态同行(y=38)：
       无日志错误时单行(高22)只显示版本/程序类错误；仅当日志路径出错时向下多撑一行(高44)放日志错误。 */
    int hasLogErr = g_state.msgErrorLog[0] ? 1 : 0;
    int msgH = hasLogErr ? 44 : 22;
    MoveWindow(g_state.hStatusRight, 268, 38, width - 280, msgH, TRUE);

    /* Position global config buttons at right side of tab bar.
       Widths follow the UI language — "Restore Defaults" is nearly twice
       as wide as 恢复默认. Chinese keeps the exact original geometry. */
    {
        int gcH = 30, gcGap = 6, i, gx = width - 12;
        HWND gcb[3];
        gcb[0] = g_state.hSaveConfig;
        gcb[1] = g_state.hLoadConfig;
        gcb[2] = g_state.hRestoreDefault;   /* rightmost first */
        for (i = 2; i >= 0; i--) {
            int w = FitW(gcb[i], 90, 24);
            gx -= w;
            MoveWindow(gcb[i], gx, 4, w, gcH, TRUE);
            gx -= gcGap;
        }
    }

    /* Panel area: 在状态栏下方。消息框 y=38 起、高 msgH(22/44)，底部留 4px 间隙。 */
    int panelTop = 38 + msgH + 4;
    int panelHeight = height - panelTop;
    int panelWidth = width;

    /* Move panels */
    MoveWindow(g_state.hClientPanel, 0, panelTop, panelWidth, panelHeight, TRUE);
    MoveWindow(g_state.hServerPanel, 0, panelTop, panelWidth, panelHeight, TRUE);
    MoveWindow(g_state.hSettingsPanel, 0, panelTop, panelWidth, panelHeight, TRUE);
    MoveWindow(g_state.hReplayPanel, 0, panelTop, panelWidth, panelHeight, TRUE);

    /* Layout right-side chart + output in client/server panels.
       Chart gets 3/10, output gets 7/10 of available height.
       lw follows the UI language: the English checkbox row is wider than
       the Chinese one and would otherwise run into the right-hand column. */
    int lx = 12, lw = LeftColumnWidth();
    int rx = lx + lw + 12;
    int rightW = panelWidth - rx - 12;
    if (rightW < 200) rightW = 200;

    /* Right column vertical budget: from y=18 (panel titles were removed, so the
       first row is a parameter label at y=0..16) to panel bottom */
    int topY = 18;
    int availH = panelHeight - topY - 12;
    if (availH < 200) availH = 200;

    int toolbarH = 24;                    /* toolbar row height */
    int gap = 8;                          /* gap between chart and toolbar */
    int chartH = (availH - toolbarH - gap - 8) * 3 / 10;   /* chart compressed by 2/5, output gets 7/10 */
    if (chartH < 80) chartH = 80;
    int toolbarY = topY + chartH + gap;
    int outputY = toolbarY + toolbarH + 4;
    int outputH = panelHeight - outputY - 12;
    if (outputH < 100) outputH = 100;

    /* Client right side */
    MoveWindow(g_state.hClientChartWnd, rx, topY, rightW, chartH, TRUE);
    {
        int tx = rx, bw;
        bw = FitW(s_hClientOutLbl, 70, 2);
        MoveWindow(s_hClientOutLbl, tx, toolbarY + 2, bw, 20, TRUE);
        tx += bw + 4;
        bw = FitW(g_state.hClientClear, 90, 24);
        MoveWindow(g_state.hClientClear, tx, toolbarY, bw, 22, TRUE);
        tx += bw + 6;
        bw = FitW(g_state.hClientCopy, 90, 24);
        MoveWindow(g_state.hClientCopy, tx, toolbarY, bw, 22, TRUE);
        tx += bw + 6;
        bw = FitW(g_state.hClientAutoScroll, 80, 24);
        MoveWindow(g_state.hClientAutoScroll, tx, toolbarY, bw, 22, TRUE);
        tx += bw + 6;
        MoveWindow(g_state.hClientCopyHint, tx, toolbarY + 2, 200, 20, TRUE);
    }
    MoveWindow(g_state.hClientOutput, rx, outputY, rightW, outputH, TRUE);

    /* Server right side */
    MoveWindow(g_state.hServerChartWnd, rx, topY, rightW, chartH, TRUE);
    {
        int tx = rx, bw;
        bw = FitW(s_hServerOutLbl, 70, 2);
        MoveWindow(s_hServerOutLbl, tx, toolbarY + 2, bw, 20, TRUE);
        tx += bw + 4;
        bw = FitW(g_state.hServerClear, 90, 24);
        MoveWindow(g_state.hServerClear, tx, toolbarY, bw, 22, TRUE);
        tx += bw + 6;
        bw = FitW(g_state.hServerCopy, 90, 24);
        MoveWindow(g_state.hServerCopy, tx, toolbarY, bw, 22, TRUE);
        tx += bw + 6;
        bw = FitW(g_state.hServerAutoScroll, 80, 24);
        MoveWindow(g_state.hServerAutoScroll, tx, toolbarY, bw, 22, TRUE);
        tx += bw + 6;
        MoveWindow(g_state.hServerCopyHint, tx, toolbarY + 2, 200, 20, TRUE);
    }
    MoveWindow(g_state.hServerOutput, rx, outputY, rightW, outputH, TRUE);

    /* Dynamically resize left-side summary boxes to fill remaining panel height.
       The summary box starts at a fixed Y (below command box) and should extend
       to near the panel bottom. We use GetWindowRect to find the current Y. */
    {
        /* Client summary box: find its current position, then resize height */
        RECT rcs;
        GetWindowRect(g_state.hClientSummaryBox, &rcs);
        MapWindowPoints(NULL, g_state.hClientPanel, (LPPOINT)&rcs, 2);
        int sumTop = rcs.top;
        int sumH = panelHeight - sumTop - 12;
        if (sumH < 80) sumH = 80;
        MoveWindow(g_state.hClientSummaryBox, lx, sumTop, lw, sumH, TRUE);

        /* Server summary box */
        RECT rss;
        GetWindowRect(g_state.hServerSummaryBox, &rss);
        MapWindowPoints(NULL, g_state.hServerPanel, (LPPOINT)&rss, 2);
        sumTop = rss.top;
        sumH = panelHeight - sumTop - 12;
        if (sumH < 80) sumH = 80;
        MoveWindow(g_state.hServerSummaryBox, lx, sumTop, lw, sumH, TRUE);
    }

    /* Replay panel: full-width single column.
       Row1 path+browse → Row2 info → Row3 slider+time →
       Row4 play/stop/speed/values → chart fills the rest. */
    {
        int repW = panelWidth - 24;                 /* 12px margins each side */
        int btnW = 90;
        int editW = repW - (btnW + 8);
        if (editW < 120) editW = 120;

        MoveWindow(g_state.hReplayPath,   12, 0,  editW, 22, TRUE);
        MoveWindow(g_state.hReplayBrowse, 12 + editW + 8, 0, btnW, 22, TRUE);
        MoveWindow(g_state.hReplayInfo,   12, 28, repW, 20, TRUE);

        int timeW = 140;
        int sliderW = repW - timeW - 8;
        if (sliderW < 120) sliderW = 120;
        MoveWindow(g_state.hReplaySlider, 12, 52, sliderW, 24, TRUE);
        MoveWindow(g_state.hReplayTime, 12 + sliderW + 8, 56, timeW, 20, TRUE);

        {
            int bx = 12, bw;
            /* pad 给足 40：文本里的 ▶ / ■ 由字体链接回退渲染，
               实测宽比 GetTextExtentPoint32 报的多约 10px。
               中文下 FitW 直接返回 80，版式不变。 */
            bw = FitW(g_state.hReplayPlay, 80, 40);
            MoveWindow(g_state.hReplayPlay, bx, 84, bw, 26, TRUE);
            bx += bw + 8;
            bw = FitW(g_state.hReplayStop, 80, 40);
            MoveWindow(g_state.hReplayStop, bx, 84, bw, 26, TRUE);
            bx += bw + 8;
            bw = FitW(s_hReplaySpeedLbl, 36, 2);
            MoveWindow(s_hReplaySpeedLbl, bx, 88, bw, 20, TRUE);
            bx += bw + 4;
            MoveWindow(g_state.hReplaySpeed, bx, 86, 70, 24, TRUE);
            bx += 70 + 12;
            /* 显示单位（紧跟倍速）：设计宽 58 = 实测「显示单位」56px + 2 余量 */
            bw = FitW(s_hReplayUnitLbl, 58, 2);
            MoveWindow(s_hReplayUnitLbl, bx, 88, bw, 20, TRUE);
            bx += bw + 4;
            MoveWindow(g_state.hReplayUnit, bx, 86, 70, 24, TRUE);
            bx += 70 + 12;
            {
                int valW = panelWidth - bx - 12;
                if (valW < 100) valW = 100;
                MoveWindow(g_state.hReplayValues, bx, 88, valW, 20, TRUE);
            }
        }

        int chartTop = 118;
        int chartH2 = panelHeight - chartTop - 12;
        if (chartH2 < 80) chartH2 = 80;
        MoveWindow(g_state.hReplayChartWnd, 12, chartTop, repW, chartH2, TRUE);
    }

    /* ===== 语言自适应（S3）=====
       以下控件坐标按中文定长，英文下会截字：只加宽、不改设计坐标，
       中文（Lang_GetAct()==LANG_ZH_CN）一律保持原尺寸。 */

    /* 客户端左列复选框行：反向 / 双向 / JSON / 服务端报告 */
    {
        int bx = 12, i;
        RECT cb;
        HWND box[4];
        int des[4];
        box[0] = g_state.hClientReverse;   des[0] = 56;
        box[1] = g_state.hClientBidir;     des[1] = 56;
        box[2] = g_state.hClientJson;      des[2] = 56;
        box[3] = g_state.hClientGetServer; des[3] = 96;
        GetWindowRect(box[0], &cb);
        MapWindowPoints(NULL, g_state.hClientPanel, (LPPOINT)&cb, 2);
        for (i = 0; i < 4; i++) {
            int bw = FitW(box[i], des[i], 24);
            MoveWindow(box[i], bx, cb.top, bw, 20, TRUE);
            bx += bw + 4;
        }
    }

    /* 服务端左列复选框行：单次 / 详细 / JSON */
    {
        int bx = 12, i;
        RECT cb;
        HWND box[3];
        box[0] = g_state.hServerOneOff;
        box[1] = g_state.hServerVerbose;
        box[2] = g_state.hServerJson;
        GetWindowRect(box[0], &cb);
        MapWindowPoints(NULL, g_state.hServerPanel, (LPPOINT)&cb, 2);
        for (i = 0; i < 3; i++) {
            int bw = FitW(box[i], 88, 24);
            MoveWindow(box[i], bx, cb.top, bw, 20, TRUE);
            bx += bw + 8;
        }
    }

    /* 服务端 启动/停止 按钮（并排）：英文 "Start Server" 比「启动服务端」宽 */
    {
        RECT rb;
        int w1, w2;
        GetWindowRect(g_state.hServerStart, &rb);
        MapWindowPoints(NULL, g_state.hServerPanel, (LPPOINT)&rb, 2);
        w1 = FitW(g_state.hServerStart, 90, 28);
        w2 = FitW(g_state.hServerStop, 90, 28);
        MoveWindow(g_state.hServerStart, rb.left, rb.top, w1,
                   rb.bottom - rb.top, TRUE);
        MoveWindow(g_state.hServerStop, rb.left + w1 + 8, rb.top, w2,
                   rb.bottom - rb.top, TRUE);
    }

    /* 客户端第三列标签「传输量(Byte)」 */
    if (s_hClientTransferLbl) {
        RECT r3;
        int ww;
        GetWindowRect(s_hClientTransferLbl, &r3);
        MapWindowPoints(NULL, g_state.hClientPanel, (LPPOINT)&r3, 2);
        ww = FitW(s_hClientTransferLbl, r3.right - r3.left, 2);
        MoveWindow(s_hClientTransferLbl, r3.left, r3.top, ww, r3.bottom - r3.top, TRUE);
    }

    /* 设置页：6 个按钮统一等宽（原先各自 FitW ⇒ 宽度参差 90/100/116），
       复选框维持只加宽。语言切换后文本长度会变，故每次布局都重算。 */
    WidenInPlace(g_state.hShowStreams, 384, 26);
    WidenInPlace(g_state.hHideStreamSum, 384, 26);
    Settings_UnifyButtonWidth();

    /* Redraw charts */
    InvalidateRect(g_state.hClientChartWnd, NULL, TRUE);
    InvalidateRect(g_state.hServerChartWnd, NULL, TRUE);
    InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
}

/* ===== Show active panel ===== */
static void ShowActivePanel(void)
{
    ShowWindow(g_state.hClientPanel, (g_state.activeTab == TAB_CLIENT) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_state.hServerPanel, (g_state.activeTab == TAB_SERVER) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_state.hSettingsPanel, (g_state.activeTab == TAB_SETTINGS) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_state.hReplayPanel, (g_state.activeTab == TAB_REPLAY) ? SW_SHOW : SW_HIDE);

    /* Redraw chart if its panel is visible */
    if (g_state.activeTab == TAB_CLIENT) {
        InvalidateRect(g_state.hClientChartWnd, NULL, TRUE);
    } else if (g_state.activeTab == TAB_SERVER) {
        InvalidateRect(g_state.hServerChartWnd, NULL, TRUE);
    } else if (g_state.activeTab == TAB_REPLAY) {
        InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
    }
}

/* 前向声明：日志目录解析（定义在文件下方） */
static void ResolveLogDir(const char* relDir, char* out, int outLen);

/* ===== Update status bar ===== */
/* 刷新右侧消息输出框（状态栏第 2 行，固定单行、左对齐）：iperf3 版本 + 程序/日志路径错误
   用「  ；  」拼成同一行。状态栏总共两行：第 1 行 = 客户端/服务端状态，第 2 行 = 本框。
   版本不进入原始输出框。 */
static void UI_RefreshMsgBox(void)
{
    wchar_t buf[1024];
    int n = 0;
    buf[0] = 0;

    /* 第 1 行：版本 / 未检测到 + 程序类错误（未检测到 iperf3、版本获取失败、程序路径无效），
       全部合并到同一行、彼此之间无间隔（用户要求程序类错误放成一行）。 */
    if (g_state.iperf3Ver[0]) {
        wchar_t wv[32];
        MultiByteToWideChar(CP_UTF8, 0, g_state.iperf3Ver, -1, wv, 32);
        n = swprintf(buf, sizeof(buf)/sizeof(buf[0]), L"iperf3 v%ls", wv);
    } else {
        n = swprintf(buf, sizeof(buf)/sizeof(buf[0]), L"未检测到 iperf3");
    }
    if (g_state.msgErrorIperf3[0]) {
        wchar_t we[256];
        MultiByteToWideChar(CP_UTF8, 0, g_state.msgErrorIperf3, -1, we, 256);
        n += swprintf(buf + n, sizeof(buf)/sizeof(buf[0]) - n, L"%ls", we);  /* 无间隔拼接 */
    }

    /* 第 2 行：仅日志路径错误才显示；无日志错误则第二行隐藏。 */
    if (g_state.msgErrorLog[0]) {
        wchar_t we[256];
        MultiByteToWideChar(CP_UTF8, 0, g_state.msgErrorLog, -1, we, 256);
        n += swprintf(buf + n, sizeof(buf)/sizeof(buf[0]) - n, L"\r\n%ls", we);
    }

    SetWindowTextW(g_state.hStatusRight, buf);

    /* 高度仅随日志错误出现与否变化(22 / 44)：只在真变了才重排，避免每次刷新都 MoveWindow。 */
    {
        static int s_lastMsgH = 0;
        int h = g_state.msgErrorLog[0] ? 44 : 22;
        if (h != s_lastMsgH) {
            s_lastMsgH = h;
            LayoutControls(g_state.hMain);
        }
    }
}

/* 刷新缓存的 iperf3 版本号：从当前有效路径探测，结果存入 g_state.iperf3Ver。
   路径无效 / 未找到时清空（消息框显示「未检测到 iperf3」）。
   该函数会按需调用 FindIperf3 / CheckIperf3Version（涉及进程调用），故只在路径变更时调用，
   不可在每次 UI_RefreshMsgBox 中调用，以免频繁派生进程。 */
static void UI_UpdateIperf3Ver(void)
{
    g_state.iperf3Ver[0] = 0;
    const char* path = g_state.iperf3Path[0] ? g_state.iperf3Path : NULL;
    char* found = NULL;
    if (!path) {
        found = FindIperf3();
        path = found;
    }
    if (path) {
        char ver[32] = {0};
        CheckIperf3Version(path, ver, sizeof(ver));  /* 副作用：能运行即填充 ver（即使版本过低） */
        if (ver[0])
            strncpy(g_state.iperf3Ver, ver, sizeof(g_state.iperf3Ver) - 1);
    }
    if (found) free(found);
}

/* 立即校验 iperf3 路径（编辑框失去焦点时调用）：无效写入 msgErrorIperf3，
   有效则清除。这样在设置页填错路径能立刻在右上角看到报错，不必等到点开始。 */
static void ValidateIperf3Path(void)
{
    UI_UpdateIperf3Ver();   /* 先刷新版本缓存，供消息框展示 */
    char path[MAX_PATH];
    UI_GetEditText(g_state.hSettingsPath, path, MAX_PATH);
    if (!path[0]) {
        g_state.msgErrorIperf3[0] = 0;   /* 空：运行期由 FindIperf3 兜底 */
    } else {
        char ver[32] = {0};
        if (!CheckIperf3Version(path, ver, sizeof(ver))) {
            if (ver[0])
                snprintf(g_state.msgErrorIperf3, sizeof(g_state.msgErrorIperf3),
                         TA(STR_ERR_VERSION_LOW), ver);
            else
                strncpy(g_state.msgErrorIperf3, TA(STR_ERR_NO_VERSION),
                        sizeof(g_state.msgErrorIperf3) - 1);
            g_state.msgErrorIperf3[sizeof(g_state.msgErrorIperf3) - 1] = 0;
        } else {
            g_state.msgErrorIperf3[0] = 0;   /* 路径有效，清除错误 */
        }
    }
    UI_RefreshMsgBox();
}

/* 立即校验日志目录（编辑框失去焦点时调用）：无效写入 msgErrorLog，有效则清除。 */
static void ValidateLogDir(void)
{
    if (!g_state.logEnabled) { g_state.msgErrorLog[0] = 0; UI_RefreshMsgBox(); return; }
    char dir[MAX_PATH];
    UI_GetEditText(g_state.hLogDir, dir, MAX_PATH);
    if (!dir[0]) { g_state.msgErrorLog[0] = 0; UI_RefreshMsgBox(); return; }
    char absDir[MAX_PATH];
    ResolveLogDir(dir, absDir, sizeof(absDir));
    if (!CreateDirectoryA(absDir, NULL)) {
        DWORD e = GetLastError();
        if (e != ERROR_ALREADY_EXISTS) {
            char emsg[256];
            snprintf(emsg, sizeof(emsg), TA(STR_ERR_LOG_DIR), absDir);
            strncpy(g_state.msgErrorLog, emsg, sizeof(g_state.msgErrorLog) - 1);
            g_state.msgErrorLog[sizeof(g_state.msgErrorLog) - 1] = 0;
            UI_RefreshMsgBox();
            return;
        }
    }
    g_state.msgErrorLog[0] = 0;
    UI_RefreshMsgBox();
}

static void UpdateStatusBar(void)
{
    /* Left: 客户端:●运行    (3-char gap)    服务端:●空闲
       Dot labels always hold "●"; only the state words change. Dot color is
       applied in WM_CTLCOLORSTATIC (GetRunColor / GetIdleColor). */
    if (g_state.hStatusClientVal)
        SetWindowTextW(g_state.hStatusClientVal,
                       g_state.clientRunning ? T(STR_STATUS_RUNNING) : T(STR_STATUS_IDLE));
    if (g_state.hStatusServerVal)
        SetWindowTextW(g_state.hStatusServerVal,
                       g_state.serverRunning ? T(STR_STATUS_RUNNING) : T(STR_STATUS_IDLE));

    /* The dot text never changes (always "●"), so SetWindowTextW above does not
       touch it and it would never repaint. Force a repaint so WM_CTLCOLORSTATIC
       runs again and picks up the new state color. */
    if (g_state.hStatusClientDot)
        InvalidateRect(g_state.hStatusClientDot, NULL, TRUE);
    if (g_state.hStatusServerDot)
        InvalidateRect(g_state.hStatusServerDot, NULL, TRUE);

    /* Right: 消息输出框（程序路径 + 版本 + 最新错误） */
    UI_RefreshMsgBox();
}

/* ===== UI_UpdateStatus ===== */
void UI_UpdateStatus(void) { UpdateStatusBar(); }

/* ===== UI_UpdateChart ===== */
void UI_UpdateChart(int mode)
{
    if (mode == MODE_CLIENT) {
        InvalidateRect(g_state.hClientChartWnd, NULL, FALSE);
    } else {
        InvalidateRect(g_state.hServerChartWnd, NULL, FALSE);
    }
}

/* Helper: render summary blocks into text buffer */
static void RenderSummaryBlocks(wchar_t* text, int textLen,
                                int blockCount,
                                const Iperf3Data* senders, const Iperf3Data* receivers,
                                const bool* hasSnd, const bool* hasRcv)
{
    for (int blk = 0; blk < blockCount; blk++) {
        wchar_t buf[512];
        wchar_t dir[16];
        wchar_t roleW[16];
        /* Pre-convert narrow strings to wide to avoid %hs issues in MinGW swprintf */
        wchar_t wFmtSpeed[32], wTransfer[64];

        if (blk > 0) wcscat(text, L"\r\n");

        if (blockCount > 1) {
            roleW[0] = L'\0';
            if (hasSnd[blk] && senders[blk].role[0])
                MultiByteToWideChar(CP_UTF8, 0, senders[blk].role, -1, roleW, 16);
            else if (hasRcv[blk] && receivers[blk].role[0])
                MultiByteToWideChar(CP_UTF8, 0, receivers[blk].role, -1, roleW, 16);
            if (roleW[0]) {
                swprintf(buf, 64, T(STR_FMT_SUMMARY_HDR), roleW, blk + 1);
            } else {
                swprintf(buf, 64, T(STR_FMT_SUMMARY_HDR2), blk + 1);
            }
            wcscat(text, buf);
        }

        if (hasSnd[blk]) {
            dir[0] = L'\0';
            if (senders[blk].direction[0]) MultiByteToWideChar(CP_UTF8, 0, senders[blk].direction, -1, dir, 16);
            wFmtSpeed[0] = L'\0';
            if (senders[blk].formatSpeed[0]) MultiByteToWideChar(CP_UTF8, 0, senders[blk].formatSpeed, -1, wFmtSpeed, 32);
            wTransfer[0] = L'\0';
            if (senders[blk].transfer[0]) MultiByteToWideChar(CP_UTF8, 0, senders[blk].transfer, -1, wTransfer, 64);
            swprintf(buf, 512,
                T(STR_FMT_BLOCK_SENDER),
                dir, wFmtSpeed, wTransfer, senders[blk].retransmits,
                senders[blk].jitter, senders[blk].lostPackets, senders[blk].totalPackets, senders[blk].lossRate);
            wcscat(text, buf);
        }
        if (hasRcv[blk]) {
            dir[0] = L'\0';
            if (receivers[blk].direction[0]) MultiByteToWideChar(CP_UTF8, 0, receivers[blk].direction, -1, dir, 16);
            wFmtSpeed[0] = L'\0';
            if (receivers[blk].formatSpeed[0]) MultiByteToWideChar(CP_UTF8, 0, receivers[blk].formatSpeed, -1, wFmtSpeed, 32);
            wTransfer[0] = L'\0';
            if (receivers[blk].transfer[0]) MultiByteToWideChar(CP_UTF8, 0, receivers[blk].transfer, -1, wTransfer, 64);
            swprintf(buf, 512,
                T(STR_FMT_BLOCK_RECEIVER),
                dir, wFmtSpeed, wTransfer, receivers[blk].retransmits,
                receivers[blk].jitter, receivers[blk].lostPackets, receivers[blk].totalPackets, receivers[blk].lossRate);
            wcscat(text, buf);
        }
    }
}

/* ===== UI_UpdateSummary ===== */
void UI_UpdateSummary(int mode)
{
    HWND hBox = (mode == MODE_CLIENT) ? g_state.hClientSummaryBox : g_state.hServerSummaryBox;
    if (!hBox) return;

    wchar_t text[4096];
    text[0] = 0;

    if (mode == MODE_CLIENT) {
        if (g_state.clientSummaryBlockCount > 0) {
            wcscat(text, T(STR_OUT_HDR_CLIENT_SUM));
            RenderSummaryBlocks(text, 4096,
                g_state.clientSummaryBlockCount,
                g_state.clientSumSender, g_state.clientSumReceiver,
                g_state.clientHasSender, g_state.clientHasReceiver);
        }
        /* Append server output section if --get-server-output was used */
        if (g_state.clientHasServerOutput && g_state.clientServerSummaryBlockCount > 0) {
            if (text[0]) wcscat(text, L"\r\n");
            wcscat(text, T(STR_OUT_HDR_SERVER_REPORT));
            RenderSummaryBlocks(text, 4096,
                g_state.clientServerSummaryBlockCount,
                g_state.clientServerSumSender, g_state.clientServerSumReceiver,
                g_state.clientServerHasSender, g_state.clientServerHasReceiver);
        }
    } else {
        RenderSummaryBlocks(text, 4096,
            g_state.serverSummaryBlockCount,
            g_state.serverSumSender, g_state.serverSumReceiver,
            g_state.serverHasSender, g_state.serverHasReceiver);
    }
    SetWindowTextW(hBox, text);
}

/* ===== Enable/disable controls based on running state ===== */
void UI_EnableControls(void)
{
    BOOL clientRunning = g_state.clientRunning;
    BOOL serverRunning = g_state.serverRunning;

    EnableWindow(g_state.hClientStart, !clientRunning);
    EnableWindow(g_state.hClientStop, clientRunning);
    EnableWindow(g_state.hServerStart, !serverRunning);
    EnableWindow(g_state.hServerStop, serverRunning);
}

/* ===== Enable/disable controls based on protocol (TCP/UDP) ===== */
/* -O (omit/防抖) is TCP-only (skips slow start). Gray out when UDP is selected.
   -b (bandwidth/带宽) is UDP-only (sets target send rate). Gray out when TCP is selected. */
void UpdateProtocolDependentControls(void)
{
    int protoIdx = (int)SendMessageW(g_state.hClientProto, CB_GETCURSEL, 0, 0);
    bool isUdp = (protoIdx == 1);
    /* -O: TCP only → disable when UDP */
    EnableWindow(g_state.hClientOmit, !isUdp);
    EnableWindow(s_hClientOmitLabel, !isUdp);
    /* -b: UDP only → disable when TCP */
    EnableWindow(g_state.hClientBw, isUdp);
    EnableWindow(s_hClientBwLabel, isUdp);
    /* Update chart right-axis mode immediately so the empty chart
       reflects the selected protocol before a test starts */
    g_state.clientChart.rightAxis = isUdp ? RA_DATAGRAMS : RA_NONE;
    UI_UpdateChart(MODE_CLIENT);
}

/* ===== Tab switching ===== */
void UI_SwitchTab(int tab)
{
    g_state.activeTab = tab;
    ShowActivePanel();

    /* Active tab button gets 22px bold, others 22px regular */
    HWND tabs[4] = { g_state.hTabClient, g_state.hTabServer,
                     g_state.hTabReplay, g_state.hTabSettings };
    for (int i = 0; i < 4; i++) {
        if (!tabs[i]) continue;
        SendMessageW(tabs[i], WM_SETFONT,
                     (WPARAM)((i == tab) ? s_hFontGroup : s_hFontGroupNormal), TRUE);
        InvalidateRect(tabs[i], NULL, TRUE);
        UpdateWindow(tabs[i]);
    }
}

/* ===== Browse for iperf3.exe ===== */
void UI_BrowsePath(void)
{
    wchar_t wpath[MAX_PATH] = {0};
    /* Pre-fill with current path if any */
    if (g_state.iperf3Path[0]) {
        MultiByteToWideChar(CP_UTF8, 0, g_state.iperf3Path, -1, wpath, MAX_PATH);
    }

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_state.hMain;
    ofn.lpstrFile = wpath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = T(STR_FILTER_EXE);
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = T(STR_DLG_TITLE_SELECT_EXE);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) {
        /* Convert wide to UTF-8 and store */
        char path[MAX_PATH] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH, NULL, NULL);
        strncpy(g_state.iperf3Path, path, MAX_PATH - 1);
        g_state.iperf3Path[MAX_PATH - 1] = 0;
        SetEditUtf8(g_state.hSettingsPath, g_state.iperf3Path);
        g_state.msgErrorIperf3[0] = 0;   /* 程序已找到，清除上一次的错误提示 */
        UI_UpdateIperf3Ver();
        UpdateStatusBar();
    }
}

/* ===== Auto detect path ===== */
void UI_AutoDetectPath(void)
{
    char* found = FindIperf3();
    if (found) {
        strncpy(g_state.iperf3Path, found, MAX_PATH - 1);
        g_state.iperf3Path[MAX_PATH-1] = 0;
        SetEditUtf8(g_state.hSettingsPath, g_state.iperf3Path);
        free(found);
        g_state.msgErrorIperf3[0] = 0;   /* 程序已找到，清除上一次的错误提示 */
    } else {
        /* 自动检测失败：状态栏右侧框已显示"未检测到 iperf3" + 程序类错误 */
    }
    UI_UpdateIperf3Ver();
    UpdateStatusBar();
}

/* ===== Log functions ===== */

/* Resolve a possibly-relative log dir to an absolute path.
   Relative paths are resolved against the exe's directory. */
static void ResolveLogDir(const char* relDir, char* out, int outLen)
{
    out[0] = 0;
    if (!relDir || !relDir[0]) return;

    /* If absolute path (drive letter or UNC), use as-is */
    if (relDir[1] == ':' || (relDir[0] == '\\' && relDir[1] == '\\')) {
        strncpy(out, relDir, outLen - 1);
        out[outLen - 1] = 0;
        return;
    }

    /* Relative: prepend exe directory */
    wchar_t wexe[MAX_PATH];
    GetModuleFileNameW(NULL, wexe, MAX_PATH);
    wchar_t* slash = wcsrchr(wexe, L'\\');
    if (slash) *slash = 0;
    char exeDir[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, wexe, -1, exeDir, MAX_PATH, NULL, NULL);
    snprintf(out, outLen, "%s\\%s", exeDir, relDir);
}

/* Sanitize IP for filename: replace ':' with '-' (IPv6) */
static void SanitizeIPForFilename(const char* ip, char* out, int outLen)
{
    int i;
    for (i = 0; ip[i] && i < outLen - 1; i++) {
        out[i] = (ip[i] == ':') ? '-' : ip[i];
    }
    out[i] = 0;
}

/* Build log file path from naming template */
static void BuildLogFilePath(const char* tmpl, const char* ip, const char* mode,
                              const char* logDir, char* out, int outLen)
{
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);

    char ipSafe[128];
    SanitizeIPForFilename(ip, ipSafe, sizeof(ipSafe));

    char result[256];
    result[0] = 0;
    int ri = 0;

    for (int i = 0; tmpl[i] && ri < (int)sizeof(result) - 1; i++) {
        if (tmpl[i] == '%') {
            char buf[16];
            const char* insert = NULL;
            i++; /* skip % */
            if (strncmp(&tmpl[i], "ip", 2) == 0) {
                insert = ipSafe; i += 1;
            } else if (strncmp(&tmpl[i], "mode", 4) == 0) {
                insert = mode; i += 3;
            } else if (tmpl[i] == 'y') {
                snprintf(buf, sizeof(buf), "%02d", (lt->tm_year + 1900) % 100);
                insert = buf;
            } else if (tmpl[i] == 'Y') {
                snprintf(buf, sizeof(buf), "%04d", lt->tm_year + 1900);
                insert = buf;
            } else if (tmpl[i] == 'M') {
                snprintf(buf, sizeof(buf), "%02d", lt->tm_mon + 1);
                insert = buf;
            } else if (tmpl[i] == 'D') {
                snprintf(buf, sizeof(buf), "%02d", lt->tm_mday);
                insert = buf;
            } else if (tmpl[i] == 'h') {
                snprintf(buf, sizeof(buf), "%02d", lt->tm_hour);
                insert = buf;
            } else if (tmpl[i] == 'm') {
                snprintf(buf, sizeof(buf), "%02d", lt->tm_min);
                insert = buf;
            } else if (tmpl[i] == 's') {
                snprintf(buf, sizeof(buf), "%02d", lt->tm_sec);
                insert = buf;
            } else {
                /* Unknown placeholder: output % as-is then the char */
                result[ri++] = '%';
                if (tmpl[i]) result[ri++] = tmpl[i];
                continue;
            }
            if (insert) {
                int len = (int)strlen(insert);
                for (int j = 0; j < len && ri < (int)sizeof(result) - 1; j++) {
                    result[ri++] = insert[j];
                }
            }
        } else {
            result[ri++] = tmpl[i];
        }
    }
    result[ri] = 0;

    /* Combine dir + filename */
    char absDir[MAX_PATH];
    ResolveLogDir(logDir, absDir, sizeof(absDir));
    CreateDirectoryA(absDir, NULL); /* create if not exists */
    snprintf(out, outLen, "%s\\%s", absDir, result);
}

/* Initialize log for a test run */
static void Log_Init(int mode, const char* ip)
{
    if (!g_state.logEnabled) return;
    if (g_state.logFile) { fclose(g_state.logFile); g_state.logFile = NULL; }

    char naming[128];
    UI_GetEditText(g_state.hLogNaming, naming, sizeof(naming));
    if (!naming[0]) strcpy(naming, "%mode-%ip-%Y%M%D-%h%m%s.log");

    char dir[MAX_PATH];
    UI_GetEditText(g_state.hLogDir, dir, sizeof(dir));
    if (!dir[0]) strcpy(dir, "./logs");

    const char* modeStr = (mode == MODE_CLIENT) ? "client" : "server";
    if (!ip || !ip[0]) ip = "unknown";

    BuildLogFilePath(naming, ip, modeStr, dir,
                     g_state.logFilePath, sizeof(g_state.logFilePath));

    g_state.logFile = fopen(g_state.logFilePath, "a");
    if (!g_state.logFile) {
        char emsg[256];
        snprintf(emsg, sizeof(emsg), TA(STR_ERR_LOG_DIR), g_state.logFilePath);
        strncpy(g_state.msgErrorLog, emsg, sizeof(g_state.msgErrorLog) - 1);
        g_state.msgErrorLog[sizeof(g_state.msgErrorLog) - 1] = 0;
        UI_RefreshMsgBox();
        return;
    }
    g_state.msgErrorLog[0] = 0;   /* 日志打开成功，清除上一次的日志路径错误 */

    /* Write header */
    char header[256];
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);
    snprintf(header, sizeof(header),
             "=== iperf3 GUI log: %s -> %s === %04d-%02d-%02d %02d:%02d:%02d ===\n",
             modeStr, ip,
             lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
             lt->tm_hour, lt->tm_min, lt->tm_sec);
    fputs(header, g_state.logFile);
    fflush(g_state.logFile);
}

/* Write a line to log */
static void Log_WriteLine(const char* line)
{
    if (!g_state.logFile || !line) return;
    fputs(line, g_state.logFile);
    fputc('\n', g_state.logFile);
    fflush(g_state.logFile);

    /* Check if truncation needed */
    long pos = ftell(g_state.logFile);
    int maxBytes = g_state.logMaxSize * 1024;
    if (maxBytes > 0 && pos > maxBytes) {
        Log_Truncate();
    }
}

/* Truncate: keep last 50% of file */
static void Log_Truncate(void)
{
    if (!g_state.logFile) return;
    fflush(g_state.logFile);

    long fileSize = ftell(g_state.logFile);
    if (fileSize <= 0) return;

    long keepSize = fileSize / 2;
    long startPos = fileSize - keepSize;

    fclose(g_state.logFile);
    g_state.logFile = NULL;

    /* Read second half into memory */
    FILE* f = fopen(g_state.logFilePath, "rb");
    if (!f) return;
    fseek(f, startPos, SEEK_SET);
    char* buf = (char*)malloc(keepSize + 1);
    if (!buf) { fclose(f); return; }
    size_t read = fread(buf, 1, keepSize, f);
    fclose(f);

    /* Skip partial first line */
    size_t start = 0;
    while (start < read && buf[start] != '\n') start++;
    if (start < read) start++; /* skip the \n */

    /* Write back */
    f = fopen(g_state.logFilePath, "wb");
    if (f) {
        fwrite(buf + start, 1, read - start, f);
        fclose(f);
    }
    free(buf);

    /* Reopen for append */
    g_state.logFile = fopen(g_state.logFilePath, "a");
}

/* Close log */
static void Log_Close(void)
{
    if (g_state.logFile) {
        fputs("=== Test finished ===\n", g_state.logFile);
        fclose(g_state.logFile);
        g_state.logFile = NULL;
    }
}

/* Browse for log directory */
static void UI_BrowseLogDir(void)
{
    BROWSEINFOW bi = {0};
    bi.hwndOwner = g_state.hMain;
    bi.lpszTitle = T(STR_DLG_TITLE_SELECT_DIR);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t wpath[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, wpath)) {
            char path[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH, NULL, NULL);
            SetEditUtf8(g_state.hLogDir, path);
        }
        CoTaskMemFree(pidl);
    }
}

/* Enable/disable log controls based on checkbox */
static void UI_UpdateLogControls(void)
{
    BOOL enabled = SendMessageW(g_state.hLogEnable, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_state.logEnabled = enabled;
    EnableWindow(g_state.hLogDir, enabled);
    EnableWindow(g_state.hLogBrowse, enabled);
    EnableWindow(g_state.hLogNaming, enabled);
    EnableWindow(g_state.hLogMaxSize, enabled);
    EnableWindow(g_state.hLogNamingHelp, enabled);
    /* Gray out labels too */
    EnableWindow(s_hLogDirLabel, enabled);
    EnableWindow(s_hLogNamingLabel, enabled);
    EnableWindow(s_hLogMaxSizeLabel, enabled);
    EnableWindow(s_hLogMaxSizeUnitLabel, enabled);
}

/* ===== Default values ===== */
#define DEF_CLIENT_IP       "127.0.0.1"
#define DEF_CLIENT_PORT     "5201"
#define DEF_CLIENT_PROTO    "tcp"
#define DEF_CLIENT_DURATION "10"
#define DEF_CLIENT_INTERVAL "1"
#define DEF_CLIENT_FORMAT   "auto"
#define DEF_CLIENT_NUM      ""
#define DEF_CLIENT_OMIT     "0"
#define DEF_CLIENT_BW       ""
#define DEF_CLIENT_PARALLEL 1
#define DEF_CLIENT_REVERSE  false
#define DEF_CLIENT_BIDIR    false
#define DEF_CLIENT_JSON     false
#define DEF_CLIENT_GETSERVER false

#define DEF_SERVER_PORT     "5201"
#define DEF_SERVER_FORMAT   "auto"
#define DEF_SERVER_AF       "both"
#define DEF_SERVER_ONEOFF   false
#define DEF_SERVER_VERBOSE  false
#define DEF_SERVER_JSON     false

#define DEF_REPLAY_UNIT     REPLAY_UNIT_AUTO   /* auto = Mb/s */

#define DEF_SETTINGS_PATH   ".\\tools\\iperf3.exe"
#define DEF_LOG_ENABLED     false
#define DEF_LOG_DIR         "./logs"
#define DEF_LOG_NAMING      "%mode-%ip-%Y%M%D-%h%m%s.log"
#define DEF_LOG_MAXSIZE     1024
#define DEF_SHOW_STREAMS    false
#define DEF_HIDE_STREAM_SUM false

/* ===== Saved settings (settings.ini next to iperf3gui.exe) ===== */
static void GetSettingsPath(char* out, int outLen)
{
    out[0] = 0;
    if (!GetModuleFileNameA(NULL, out, outLen)) return;
    char* p = strrchr(out, '\\');
    if (!p) return;
    *p = 0;
    strncat(out, "\\settings.ini", outLen - strlen(out) - 1);
}

/* ===== Restore all controls to hardcoded defaults (does NOT write INI) ===== */
void RestoreAllDefaults(void)
{
    /* Client */
    SetEditUtf8(g_state.hClientIP, DEF_CLIENT_IP);
    SetEditUtf8(g_state.hClientPort, DEF_CLIENT_PORT);
    SendMessageW(g_state.hClientProto, CB_SETCURSEL, 0, 0);
    SetEditUtf8(g_state.hClientDuration, DEF_CLIENT_DURATION);
    SetEditUtf8(g_state.hClientInterval, DEF_CLIENT_INTERVAL);
    SendMessageW(g_state.hClientFormat, CB_SETCURSEL, 8, 0); /* auto */
    SetEditUtf8(g_state.hClientNum, DEF_CLIENT_NUM);
    SetEditUtf8(g_state.hClientOmit, DEF_CLIENT_OMIT);
    SetEditUtf8(g_state.hClientBw, DEF_CLIENT_BW);
    SendMessageW(g_state.hClientParallel, CB_SETCURSEL, DEF_CLIENT_PARALLEL - 1, 0);
    SetEditUtf8(g_state.hClientExtra, "");
    SendMessageW(g_state.hClientReverse, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(g_state.hClientBidir, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(g_state.hClientJson, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(g_state.hClientGetServer, BM_SETCHECK, BST_UNCHECKED, 0);
    g_state.clientReverse = false;
    g_state.clientBidir = false;
    g_state.clientJson = false;
    g_state.clientGetServer = false;
    UpdateProtocolDependentControls();

    /* Server */
    SetEditUtf8(g_state.hServerPort, DEF_SERVER_PORT);
    SendMessageW(g_state.hServerFormat, CB_SETCURSEL, 8, 0); /* auto */
    SendMessageW(g_state.hServerAf, CB_SETCURSEL, 2, 0); /* both */
    SetEditUtf8(g_state.hServerExtra, "");
    SendMessageW(g_state.hServerOneOff, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(g_state.hServerVerbose, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(g_state.hServerJson, BM_SETCHECK, BST_UNCHECKED, 0);
    g_state.serverOneOff = false;
    g_state.serverVerbose = false;
    g_state.serverJson = false;

    /* Replay display unit (index into kReplayUnits) */
    SendMessageW(g_state.hReplayUnit, CB_SETCURSEL, DEF_REPLAY_UNIT, 0);
    Replay_ReadUnit();
    if (g_state.replayLoaded) Replay_ApplyIndex(g_state.replayIdx, true);

    /* Settings */
    SetEditUtf8(g_state.hSettingsPath, DEF_SETTINGS_PATH);
    strncpy(g_state.iperf3Path, DEF_SETTINGS_PATH, MAX_PATH - 1);
    g_state.iperf3Path[MAX_PATH - 1] = 0;
    UI_UpdateIperf3Ver();
    UpdateStatusBar();

    /* Log */
    SendMessageW(g_state.hLogEnable, BM_SETCHECK, BST_UNCHECKED, 0);
    SetEditUtf8(g_state.hLogDir, DEF_LOG_DIR);
    SetEditUtf8(g_state.hLogNaming, DEF_LOG_NAMING);
    char ms[16];
    snprintf(ms, sizeof(ms), "%d", DEF_LOG_MAXSIZE);
    SetEditUtf8(g_state.hLogMaxSize, ms);
    g_state.logEnabled = false;
    g_state.logMaxSize = DEF_LOG_MAXSIZE;
    UI_UpdateLogControls();

    /* Chart — restore default (off) */
    SendMessageW(g_state.hShowStreams, BM_SETCHECK, BST_UNCHECKED, 0);
    ApplyShowStreams(false);
    SendMessageW(g_state.hHideStreamSum, BM_SETCHECK, BST_UNCHECKED, 0);
    ApplyHideStreamSum(false);
}

/* Apply the "显示各流" default to all three charts. This is the INITIAL
   state for new tests / new replay loads (Decision #8: settings checkbox =
   initial state). Per-stream legend clicks override it at runtime without
   touching the persisted default. */
static void ApplyShowStreams(bool val)
{
    g_state.showStreams = val;
    /* 「隐藏流汇总」仅在勾了「显示各流」时可点。取消勾选只置灰、不清空已存值，
       重新勾回来自动恢复。 */
    if (g_state.hHideStreamSum) EnableWindow(g_state.hHideStreamSum, val);
    ChartData* cds[3] = { &g_state.clientChart, &g_state.serverChart, &g_state.replayChart };
    for (int i = 0; i < 3; i++) {
        cds[i]->showStreams = val;
        cds[i]->hideStreamSum = g_state.hideStreamSum;
        for (int k = 0; k < CHART_MAX_STREAMS; k++) cds[i]->seriesVisible[k] = true;
    }
    InvalidateRect(g_state.hClientChartWnd, NULL, TRUE);
    InvalidateRect(g_state.hServerChartWnd, NULL, TRUE);
    InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
}

/* Apply the "隐藏流汇总" default to all three charts (2026-09-24).
   实际是否隐藏由 chart.cpp 的 Chart_SumHidden() 判定：还要求
   showStreams == true 且 seriesCount > 0（bidir 下 seriesCount=0 ⇒ 不生效）。 */
static void ApplyHideStreamSum(bool val)
{
    g_state.hideStreamSum = val;
    ChartData* cds[3] = { &g_state.clientChart, &g_state.serverChart, &g_state.replayChart };
    for (int i = 0; i < 3; i++) cds[i]->hideStreamSum = val;
    InvalidateRect(g_state.hClientChartWnd, NULL, TRUE);
    InvalidateRect(g_state.hServerChartWnd, NULL, TRUE);
    InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
}

/* ===== Save all controls to INI ===== */
void SaveAllConfig(void)
{
    char ini[MAX_PATH];
    GetSettingsPath(ini, sizeof(ini));
    if (!ini[0]) return;

    /* Client */
    char ip[64], port[16], duration[16], interval[16], num[64], omit[16], bw[32], extra[256];
    UI_GetEditText(g_state.hClientIP, ip, sizeof(ip));
    UI_GetEditText(g_state.hClientPort, port, sizeof(port));
    int protoIdx = (int)SendMessageW(g_state.hClientProto, CB_GETCURSEL, 0, 0);
    UI_GetEditText(g_state.hClientDuration, duration, sizeof(duration));
    UI_GetEditText(g_state.hClientInterval, interval, sizeof(interval));
    int fmtIdx = (int)SendMessageW(g_state.hClientFormat, CB_GETCURSEL, 0, 0);
    UI_GetEditText(g_state.hClientNum, num, sizeof(num));
    UI_GetEditText(g_state.hClientOmit, omit, sizeof(omit));
    UI_GetEditText(g_state.hClientBw, bw, sizeof(bw));
    UI_GetEditText(g_state.hClientExtra, extra, sizeof(extra));
    bool reverse = SendMessageW(g_state.hClientReverse, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool bidir = SendMessageW(g_state.hClientBidir, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool json = SendMessageW(g_state.hClientJson, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool getServer = SendMessageW(g_state.hClientGetServer, BM_GETCHECK, 0, 0) == BST_CHECKED;
    int parallelIdx = (int)SendMessageW(g_state.hClientParallel, CB_GETCURSEL, 0, 0);
    if (parallelIdx < 0) parallelIdx = 0;

    static const char* fmtCodes[] = {"k","m","g","t","K","M","G","T",""};
    const char* fmtCode = (fmtIdx >= 0 && fmtIdx < 9) ? fmtCodes[fmtIdx] : "";

    WritePrivateProfileStringA("client", "ip", ip, ini);
    WritePrivateProfileStringA("client", "port", port, ini);
    WritePrivateProfileStringA("client", "proto", protoIdx == 1 ? "udp" : "tcp", ini);
    WritePrivateProfileStringA("client", "duration", duration, ini);
    WritePrivateProfileStringA("client", "interval", interval, ini);
    WritePrivateProfileStringA("client", "format", fmtCode, ini);
    WritePrivateProfileStringA("client", "num", num, ini);
    WritePrivateProfileStringA("client", "omit", omit, ini);
    WritePrivateProfileStringA("client", "bw", bw, ini);
    WritePrivateProfileStringA("client", "extra", extra, ini);
    WritePrivateProfileStringA("client", "reverse", reverse ? "1" : "0", ini);
    WritePrivateProfileStringA("client", "bidir", bidir ? "1" : "0", ini);
    WritePrivateProfileStringA("client", "json", json ? "1" : "0", ini);
    WritePrivateProfileStringA("client", "getserveroutput", getServer ? "1" : "0", ini);
    {
        char parStr[8];
        snprintf(parStr, sizeof(parStr), "%d", parallelIdx + 1);
        WritePrivateProfileStringA("client", "parallel", parStr, ini);
    }

    /* Server */
    char sport[16], sExtra[256];
    UI_GetEditText(g_state.hServerPort, sport, sizeof(sport));
    int sFmtIdx = (int)SendMessageW(g_state.hServerFormat, CB_GETCURSEL, 0, 0);
    int sAfIdx = (int)SendMessageW(g_state.hServerAf, CB_GETCURSEL, 0, 0);
    UI_GetEditText(g_state.hServerExtra, sExtra, sizeof(sExtra));
    bool oneOff = SendMessageW(g_state.hServerOneOff, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool verbose = SendMessageW(g_state.hServerVerbose, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool sJson = SendMessageW(g_state.hServerJson, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const char* sFmtCode = (sFmtIdx >= 0 && sFmtIdx < 9) ? fmtCodes[sFmtIdx] : "";
    static const char* afCodes[] = {"-4", "-6", "both"};
    const char* sAfCode = (sAfIdx >= 0 && sAfIdx < 3) ? afCodes[sAfIdx] : "both";
    WritePrivateProfileStringA("server", "port", sport, ini);
    WritePrivateProfileStringA("server", "format", sFmtCode, ini);
    WritePrivateProfileStringA("server", "af", sAfCode, ini);
    WritePrivateProfileStringA("server", "extra", sExtra, ini);
    WritePrivateProfileStringA("server", "oneoff", oneOff ? "1" : "0", ini);
    WritePrivateProfileStringA("server", "verbose", verbose ? "1" : "0", ini);
    WritePrivateProfileStringA("server", "json", sJson ? "1" : "0", ini);

    /* Settings — iperf3 path */
    char path[MAX_PATH] = {0};
    UI_GetEditText(g_state.hSettingsPath, path, MAX_PATH);
    WritePrivateProfileStringA("settings", "iperf3Path", path, ini);
    if (path[0]) {
        strncpy(g_state.iperf3Path, path, MAX_PATH - 1);
        g_state.iperf3Path[MAX_PATH - 1] = 0;
    }
    UI_UpdateIperf3Ver();

    /* Log */
    bool logEnabled = SendMessageW(g_state.hLogEnable, BM_GETCHECK, 0, 0) == BST_CHECKED;
    char logDir[MAX_PATH], logNaming[128], logMaxsize[16];
    UI_GetEditText(g_state.hLogDir, logDir, sizeof(logDir));
    UI_GetEditText(g_state.hLogNaming, logNaming, sizeof(logNaming));
    UI_GetEditText(g_state.hLogMaxSize, logMaxsize, sizeof(logMaxsize));
    WritePrivateProfileStringA("log", "Enabled", logEnabled ? "1" : "0", ini);
    WritePrivateProfileStringA("log", "Dir", logDir, ini);
    WritePrivateProfileStringA("log", "Naming", logNaming, ini);
    WritePrivateProfileStringA("log", "MaxSizeKB", logMaxsize, ini);

    /* Chart */
    WritePrivateProfileStringA("Chart", "ShowStreams", g_state.showStreams ? "1" : "0", ini);
    WritePrivateProfileStringA("Chart", "HideStreamSum", g_state.hideStreamSum ? "1" : "0", ini);

    /* Replay display unit */
    {
        char uStr[8];
        int uIdx = (int)SendMessageW(g_state.hReplayUnit, CB_GETCURSEL, 0, 0);
        if (uIdx < 0 || uIdx >= REPLAY_UNIT_COUNT) uIdx = DEF_REPLAY_UNIT;
        snprintf(uStr, sizeof(uStr), "%d", uIdx);
        WritePrivateProfileStringA("replay", "unit", uStr, ini);
    }

    UpdateStatusBar();
}

/* ===== Load all controls from INI ===== */
void LoadAllConfig(void)
{
    char ini[MAX_PATH];
    GetSettingsPath(ini, sizeof(ini));
    if (!ini[0]) return;

    /* Client */
    char ip[64], port[16], proto[8], duration[16], interval[16], fmt[8], num[64], omit[16], bw[32], extra[256];
    GetPrivateProfileStringA("client", "ip", DEF_CLIENT_IP, ip, sizeof(ip), ini);
    GetPrivateProfileStringA("client", "port", DEF_CLIENT_PORT, port, sizeof(port), ini);
    GetPrivateProfileStringA("client", "proto", DEF_CLIENT_PROTO, proto, sizeof(proto), ini);
    GetPrivateProfileStringA("client", "duration", DEF_CLIENT_DURATION, duration, sizeof(duration), ini);
    GetPrivateProfileStringA("client", "interval", DEF_CLIENT_INTERVAL, interval, sizeof(interval), ini);
    GetPrivateProfileStringA("client", "format", DEF_CLIENT_FORMAT, fmt, sizeof(fmt), ini);
    GetPrivateProfileStringA("client", "num", DEF_CLIENT_NUM, num, sizeof(num), ini);
    GetPrivateProfileStringA("client", "omit", DEF_CLIENT_OMIT, omit, sizeof(omit), ini);
    GetPrivateProfileStringA("client", "bw", DEF_CLIENT_BW, bw, sizeof(bw), ini);
    GetPrivateProfileStringA("client", "extra", "", extra, sizeof(extra), ini);
    bool reverse = GetPrivateProfileIntA("client", "reverse", DEF_CLIENT_REVERSE ? 1 : 0, ini) != 0;
    bool bidir = GetPrivateProfileIntA("client", "bidir", DEF_CLIENT_BIDIR ? 1 : 0, ini) != 0;
    bool json = GetPrivateProfileIntA("client", "json", DEF_CLIENT_JSON ? 1 : 0, ini) != 0;
    bool getServer = GetPrivateProfileIntA("client", "getserveroutput", DEF_CLIENT_GETSERVER ? 1 : 0, ini) != 0;

    SetEditUtf8(g_state.hClientIP, ip);
    SetEditUtf8(g_state.hClientPort, port);
    SendMessageW(g_state.hClientProto, CB_SETCURSEL, (stricmp(proto, "udp") == 0) ? 1 : 0, 0);
    SetEditUtf8(g_state.hClientDuration, duration);
    SetEditUtf8(g_state.hClientInterval, interval);
    SetEditUtf8(g_state.hClientNum, num);
    SetEditUtf8(g_state.hClientOmit, omit);
    SetEditUtf8(g_state.hClientBw, bw);
    SetEditUtf8(g_state.hClientExtra, extra);
    /* Map format code to combo index */
    {
        static const char* codes[] = {"k","m","g","t","K","M","G","T"};
        int fmtIdx = 8; /* auto */
        for (int i = 0; i < 8; i++) {
            if (stricmp(fmt, codes[i]) == 0) { fmtIdx = i; break; }
        }
        SendMessageW(g_state.hClientFormat, CB_SETCURSEL, fmtIdx, 0);
    }
    {
        int parallel = GetPrivateProfileIntA("client", "parallel", DEF_CLIENT_PARALLEL, ini);
        if (parallel < 1) parallel = 1;
        if (parallel > 8) parallel = 8;
        SendMessageW(g_state.hClientParallel, CB_SETCURSEL, parallel - 1, 0);
    }
    SendMessageW(g_state.hClientReverse, BM_SETCHECK, reverse ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_state.hClientBidir, BM_SETCHECK, bidir ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_state.hClientJson, BM_SETCHECK, json ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_state.hClientGetServer, BM_SETCHECK, getServer ? BST_CHECKED : BST_UNCHECKED, 0);
    g_state.clientReverse = reverse;
    g_state.clientBidir = bidir;
    g_state.clientJson = json;
    g_state.clientGetServer = getServer;
    UpdateProtocolDependentControls();

    /* Server */
    char sport[16], sFmt[8], sAf[8], sExtra[256];
    GetPrivateProfileStringA("server", "port", DEF_SERVER_PORT, sport, sizeof(sport), ini);
    GetPrivateProfileStringA("server", "format", DEF_SERVER_FORMAT, sFmt, sizeof(sFmt), ini);
    GetPrivateProfileStringA("server", "af", DEF_SERVER_AF, sAf, sizeof(sAf), ini);
    GetPrivateProfileStringA("server", "extra", "", sExtra, sizeof(sExtra), ini);
    bool oneOff = GetPrivateProfileIntA("server", "oneoff", DEF_SERVER_ONEOFF ? 1 : 0, ini) != 0;
    bool verbose = GetPrivateProfileIntA("server", "verbose", DEF_SERVER_VERBOSE ? 1 : 0, ini) != 0;
    bool sJson = GetPrivateProfileIntA("server", "json", DEF_SERVER_JSON ? 1 : 0, ini) != 0;
    SetEditUtf8(g_state.hServerPort, sport);
    SetEditUtf8(g_state.hServerExtra, sExtra);
    {
        static const char* codes[] = {"k","m","g","t","K","M","G","T"};
        int sFmtIdx = 8; /* auto */
        for (int i = 0; i < 8; i++) {
            if (stricmp(sFmt, codes[i]) == 0) { sFmtIdx = i; break; }
        }
        SendMessageW(g_state.hServerFormat, CB_SETCURSEL, sFmtIdx, 0);
    }
    /* Map AF code to combo index */
    {
        int sAfIdx = 2; /* both */
        if (stricmp(sAf, "-4") == 0) sAfIdx = 0;
        else if (stricmp(sAf, "-6") == 0) sAfIdx = 1;
        SendMessageW(g_state.hServerAf, CB_SETCURSEL, sAfIdx, 0);
    }
    SendMessageW(g_state.hServerOneOff, BM_SETCHECK, oneOff ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_state.hServerVerbose, BM_SETCHECK, verbose ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_state.hServerJson, BM_SETCHECK, sJson ? BST_CHECKED : BST_UNCHECKED, 0);
    g_state.serverOneOff = oneOff;
    g_state.serverVerbose = verbose;
    g_state.serverJson = sJson;

    /* Settings — iperf3 path */
    char path[MAX_PATH] = {0};
    GetPrivateProfileStringA("settings", "iperf3Path", DEF_SETTINGS_PATH, path, MAX_PATH, ini);
    SetEditUtf8(g_state.hSettingsPath, path);
    strncpy(g_state.iperf3Path, path, MAX_PATH - 1);
    g_state.iperf3Path[MAX_PATH - 1] = 0;
    UpdateStatusBar();

    /* Log */
    bool logEn = GetPrivateProfileIntA("log", "Enabled", DEF_LOG_ENABLED ? 1 : 0, ini) != 0;
    char logDir[MAX_PATH], logNaming[128];
    int logMax = GetPrivateProfileIntA("log", "MaxSizeKB", DEF_LOG_MAXSIZE, ini);
    GetPrivateProfileStringA("log", "Dir", DEF_LOG_DIR, logDir, sizeof(logDir), ini);
    GetPrivateProfileStringA("log", "Naming", DEF_LOG_NAMING, logNaming, sizeof(logNaming), ini);

    SendMessageW(g_state.hLogEnable, BM_SETCHECK, logEn ? BST_CHECKED : BST_UNCHECKED, 0);
    SetEditUtf8(g_state.hLogDir, logDir);
    SetEditUtf8(g_state.hLogNaming, logNaming);
    char ms[16];
    snprintf(ms, sizeof(ms), "%d", logMax);
    SetEditUtf8(g_state.hLogMaxSize, ms);
    g_state.logEnabled = logEn;
    g_state.logMaxSize = logMax;
    UI_UpdateLogControls();

    /* Chart — "显示各流" default (persisted; applies to the three charts) */
    bool showStreams = GetPrivateProfileIntA("Chart", "ShowStreams", DEF_SHOW_STREAMS ? 1 : 0, ini) != 0;
    /* Chart — "隐藏流汇总" (persisted; 仅在上述开关打开时生效) */
    bool hideStreamSum = GetPrivateProfileIntA("Chart", "HideStreamSum", DEF_HIDE_STREAM_SUM ? 1 : 0, ini) != 0;
    SendMessageW(g_state.hHideStreamSum, BM_SETCHECK, hideStreamSum ? BST_CHECKED : BST_UNCHECKED, 0);
    ApplyHideStreamSum(hideStreamSum);
    SendMessageW(g_state.hShowStreams, BM_SETCHECK, showStreams ? BST_CHECKED : BST_UNCHECKED, 0);
    ApplyShowStreams(showStreams);

    /* Replay display unit (index into kReplayUnits; 8 = auto) */
    {
        int uIdx = GetPrivateProfileIntA("replay", "unit", DEF_REPLAY_UNIT, ini);
        if (uIdx < 0 || uIdx >= REPLAY_UNIT_COUNT) uIdx = DEF_REPLAY_UNIT;
        SendMessageW(g_state.hReplayUnit, CB_SETCURSEL, uIdx, 0);
        Replay_ReadUnit();
    }

    /* 加载完配置立即校验路径，填错能在右上角消息框立刻看到报错 */
    ValidateIperf3Path();
    ValidateLogDir();
}

/* ===== Write defaults to INI if it doesn't exist (first run) ===== */
void InitDefaultConfig(void)
{
    char ini[MAX_PATH];
    GetSettingsPath(ini, sizeof(ini));
    if (!ini[0]) return;
    if (GetFileAttributesA(ini) != INVALID_FILE_ATTRIBUTES) return; /* already exists */

    WritePrivateProfileStringA("client", "ip", DEF_CLIENT_IP, ini);
    WritePrivateProfileStringA("client", "port", DEF_CLIENT_PORT, ini);
    WritePrivateProfileStringA("client", "proto", DEF_CLIENT_PROTO, ini);
    WritePrivateProfileStringA("client", "duration", DEF_CLIENT_DURATION, ini);
    WritePrivateProfileStringA("client", "interval", DEF_CLIENT_INTERVAL, ini);
    WritePrivateProfileStringA("client", "format", DEF_CLIENT_FORMAT, ini);
    WritePrivateProfileStringA("client", "num", DEF_CLIENT_NUM, ini);
    WritePrivateProfileStringA("client", "omit", DEF_CLIENT_OMIT, ini);
    WritePrivateProfileStringA("client", "bw", DEF_CLIENT_BW, ini);
    WritePrivateProfileStringA("client", "extra", "", ini);
    {
        char parStr[8];
        snprintf(parStr, sizeof(parStr), "%d", DEF_CLIENT_PARALLEL);
        WritePrivateProfileStringA("client", "parallel", parStr, ini);
    }
    WritePrivateProfileStringA("client", "reverse", DEF_CLIENT_REVERSE ? "1" : "0", ini);
    WritePrivateProfileStringA("client", "bidir", DEF_CLIENT_BIDIR ? "1" : "0", ini);
    WritePrivateProfileStringA("client", "json", DEF_CLIENT_JSON ? "1" : "0", ini);
    WritePrivateProfileStringA("client", "getserveroutput", DEF_CLIENT_GETSERVER ? "1" : "0", ini);

    WritePrivateProfileStringA("server", "port", DEF_SERVER_PORT, ini);
    WritePrivateProfileStringA("server", "format", DEF_SERVER_FORMAT, ini);
    WritePrivateProfileStringA("server", "af", DEF_SERVER_AF, ini);
    WritePrivateProfileStringA("server", "extra", "", ini);
    WritePrivateProfileStringA("server", "oneoff", DEF_SERVER_ONEOFF ? "1" : "0", ini);
    WritePrivateProfileStringA("server", "verbose", DEF_SERVER_VERBOSE ? "1" : "0", ini);
    WritePrivateProfileStringA("server", "json", DEF_SERVER_JSON ? "1" : "0", ini);

    WritePrivateProfileStringA("settings", "iperf3Path", DEF_SETTINGS_PATH, ini);

    WritePrivateProfileStringA("log", "Enabled", DEF_LOG_ENABLED ? "1" : "0", ini);
    WritePrivateProfileStringA("log", "Dir", DEF_LOG_DIR, ini);
    WritePrivateProfileStringA("log", "Naming", DEF_LOG_NAMING, ini);
    char ms[16];
    snprintf(ms, sizeof(ms), "%d", DEF_LOG_MAXSIZE);
    WritePrivateProfileStringA("log", "MaxSizeKB", ms, ini);
    WritePrivateProfileStringA("Chart", "ShowStreams", DEF_SHOW_STREAMS ? "1" : "0", ini);
    WritePrivateProfileStringA("Chart", "HideStreamSum", DEF_HIDE_STREAM_SUM ? "1" : "0", ini);
    {
        char uStr[8];
        snprintf(uStr, sizeof(uStr), "%d", DEF_REPLAY_UNIT);
        WritePrivateProfileStringA("replay", "unit", uStr, ini);
    }
}

/* ===== UI language：持久化在 settings.ini [UI] Language =====
   存 ASCII 键（auto / zh-CN / zh-TW / en）；内容不认识一律回落 auto。 */
static const char* kLangSelKeys[LANG_SEL_COUNT] = { "auto", "zh-CN", "zh-TW", "en" };

/* ===== UI theme：持久化在 settings.ini [UI] Theme =====
   存 ASCII 键（auto / dark / light）；内容不认识一律回落 auto。 */
static const char* kThemeSelKeys[THEME_SEL_COUNT] = { "auto", "dark", "light" };

/* ---- 文本度量（S3 宽度自适应）---- */

/* 控件文本的像素宽。用控件自己的字体量，跟它实际绘制时用的字体一致。 */
static int TextWidthOf(HWND hwnd)
{
    wchar_t buf[512];
    HDC dc;
    HFONT old;
    SIZE sz;
    int n, i, extra = 0;
    if (!hwnd) return 0;
    n = GetWindowTextW(hwnd, buf, 512);
    if (n <= 0) return 0;
    /* 「▶」「■」这类几何/箭头符号由字体链接回退渲染，GDI 测量取到的字形
       与实际绘制用的不是同一个，测量值明显偏窄（实测 39px vs 渲染约 60px）。
       按符号个数补一个固定量，否则按钮会算成「放得下」而右侧截字。 */
    for (i = 0; i < n; i++) {
        unsigned c = (unsigned)buf[i];
        if ((c >= 0x25A0 && c <= 0x25FF) || (c >= 0x2600 && c <= 0x27BF))
            extra += 20;
    }
    dc = GetDC(hwnd);
    old = (HFONT)SelectObject(dc, (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0));
    GetTextExtentPoint32W(dc, buf, n, &sz);
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);
    return sz.cx + extra;
}

/* 控件「够用宽度」：不低于设计宽，且足以装下当前语言的文本。
   简体中文版式已按 F 定稿，故中文一律返回设计宽 —— 保证零视觉变化；
   繁体 / English 才按实测文本宽加宽。 */
static int FitW(HWND hwnd, int designW, int pad)
{
    int tw;
    if (Lang_GetAct() == LANG_ZH_CN) return designW;
    tw = TextWidthOf(hwnd) + pad;
    return (tw > designW) ? tw : designW;
}

/* 只改宽、不改坐标（用于坐标需要保持原样的设置页） */
static void WidenInPlace(HWND hwnd, int designW, int pad)
{
    RECT r;
    int w;
    if (!hwnd) return;
    GetWindowRect(hwnd, &r);
    MapWindowPoints(NULL, GetParent(hwnd), (LPPOINT)&r, 2);
    w = FitW(hwnd, designW, pad);
    if (w != r.right - r.left)
        MoveWindow(hwnd, r.left, r.top, w, r.bottom - r.top, TRUE);
}

/* ===== 设置页 6 个按钮等宽（用户 2026-09-23：「配置页面所有按钮的长度改成一致」） =====
   宽度 = max(基准 124, 当前语言下最长按钮文本 + 24)。
   最长文本是「浏览文件夹 / Browse Folder」（5 个汉字 / 13 字符），故中文 124、
   英文更宽 —— 但 6 个按钮**取同一个值**，同页永远等宽。
   第一行（浏览 / 自动检测 / 添加说明）按新宽度重排 x；日志两行按钮与 License
   只改宽度、左边缘不动。幂等：同一语言下重复调用结果不变。 */
#define SETTINGS_BTN_W_BASE 124
#define SETTINGS_BTN_GAP    6

static void Settings_UnifyButtonWidth(void)
{
    HWND btns[6];
    int i, w = SETTINGS_BTN_W_BASE, bx = 420;   /* 第一行起点 = 路径框右侧 */

    btns[0] = g_state.hSettingsBrowse;
    btns[1] = g_state.hSettingsDetect;
    btns[2] = g_state.hHelpIperf3;
    btns[3] = g_state.hLogBrowse;
    btns[4] = g_state.hLogNamingHelp;
    btns[5] = g_state.hLicense;

    /* 先按当前语言求「最宽者」，6 个按钮共用这个宽度 */
    for (i = 0; i < 6; i++) {
        int tw;
        if (!btns[i]) continue;
        tw = TextWidthOf(btns[i]) + 24;
        if (tw > w) w = tw;
    }

    for (i = 0; i < 6; i++) {
        RECT r;
        if (!btns[i]) continue;
        GetWindowRect(btns[i], &r);
        MapWindowPoints(NULL, g_state.hSettingsPanel, (LPPOINT)&r, 2);
        if (i < 3) {                        /* 第一行：依次重排 */
            MoveWindow(btns[i], bx, r.top, w, r.bottom - r.top, TRUE);
            bx += w + SETTINGS_BTN_GAP;
        } else if (i == 5) {                /* License：左缘固定 12 */
            MoveWindow(btns[i], 12, r.top, w, r.bottom - r.top, TRUE);
        } else {                            /* 日志两行：只改宽 */
            MoveWindow(btns[i], r.left, r.top, w, r.bottom - r.top, TRUE);
        }
    }
}

/* 左列（参数栏）宽度。英文的复选框行与三列标签比中文长得多，
   280 会与右列「Raw Output」撞上，故英文下放到 320。 */
static int LeftColumnWidth(void)
{
    return (Lang_GetAct() == LANG_ZH_CN) ? 280 : 320;
}


/* (重)填语言下拉。只有「跟随系统」这一项随界面语言翻，
   其余三项恒用该语言的本地原生写法 —— 用户在任何界面语言下都能认出自己的语言。 */
/* 语言下拉第 1~3 项的文本：**不走 T()**，直接写死。
   T() 是按当前界面语言取列 —— 等于把「让用户认出自己的语言」这件事押在语言表三列
   都写对上。曾经就在繁体界面下把「简体中文」显示成「簡體中文」（该行「繁体列」被
   误当成待翻译文案）。这里与语言表数据解耦，加语种也不会再串味。 */
static const wchar_t kLangNameZhCn[] = L"简体中文";
static const wchar_t kLangNameZhTw[] = L"繁體中文";
static const wchar_t kLangNameEn[]   = L"English";

/* 填充「界面语言」下拉。
   关键约束：绝不在已填充（count==4）的隐藏/未实现面板上调用 CB_RESETCONTENT ——
   那会把下拉列表的积分高度锁成 ~0（展开只能看到选中项一行）。此 bug 只在语言下拉
   上出现，因为其它下拉（服务端/回放）首次填充时从不调 CB_RESETCONTENT。
   故：首次填充（count<4）直接追加四项；之后切换语言时只就地更新第 0 项
   「跟随系统」（四个选项里唯一随界面语言变化的项），不再整体重置。 */
static void Lang_FillCombo(void)
{
    int sel;
    if (!s_hLangCombo) return;
    sel = Lang_GetSel();
    if (sel < 0 || sel >= LANG_SEL_COUNT) sel = LANG_SEL_AUTO;
    int cnt = (int)SendMessageW(s_hLangCombo, CB_GETCOUNT, 0, 0);
    if (cnt == LANG_SEL_COUNT) {
        /* 已填充：仅就地替换第 0 项文本，避免 CB_RESETCONTENT 锁死下拉高度 */
        SendMessageW(s_hLangCombo, CB_DELETESTRING, 0, 0);
        SendMessageW(s_hLangCombo, CB_INSERTSTRING, 0, (LPARAM)T(STR_LANG_OPT_AUTO));
    } else {
        /* 首次填充（含异常兜底）：直接追加四项，绝不用 CB_RESETCONTENT */
        SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)T(STR_LANG_OPT_AUTO));
        SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)kLangNameZhCn);
        SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)kLangNameZhTw);
        SendMessageW(s_hLangCombo, CB_ADDSTRING, 0, (LPARAM)kLangNameEn);
    }
    SendMessageW(s_hLangCombo, CB_SETCURSEL, sel, 0);
}

/* 读档位并应用。必须在 CreateAllControls 之前调用：
   控件一旦用错语言创建，就只能靠 Lang_Refresh() 事后补救。 */
static void LangCfg_Load(void)
{
    char ini[MAX_PATH], buf[32];
    int i, sel = LANG_SEL_AUTO;
    GetSettingsPath(ini, sizeof(ini));
    if (ini[0]) {
        GetPrivateProfileStringA("UI", "Language", "auto", buf, sizeof(buf), ini);
        for (i = 0; i < LANG_SEL_COUNT; i++) {
            if (stricmp(buf, kLangSelKeys[i]) == 0) { sel = i; break; }
        }
    }
    Lang_SetSel(sel);
}

static void LangCfg_Save(void)
{
    char ini[MAX_PATH];
    int sel = Lang_GetSel();
    GetSettingsPath(ini, sizeof(ini));
    if (!ini[0]) return;
    if (sel < 0 || sel >= LANG_SEL_COUNT) sel = LANG_SEL_AUTO;
    WritePrivateProfileStringA("UI", "Language", kLangSelKeys[sel], ini);
}

/* ===== UI theme：填充下拉 + 配置读写 ===== */
/* 主题下拉**三项都随界面语言翻**（lang.c 里 深色/深色/Dark、浅色/淺色/Light 三列
   故意不同）。这与语言下拉第 1~3 项正好相反 —— 那三项恒用各语言原生写法、不翻。 */
static void Theme_FillCombo(void)
{
    static const int themeStrIds[THEME_SEL_COUNT] = {
        STR_THEME_OPT_AUTO, STR_THEME_OPT_DARK, STR_THEME_OPT_LIGHT
    };
    int sel, i;
    if (!s_hThemeCombo) return;
    sel = g_state.themeSel;
    if (sel < 0 || sel >= THEME_SEL_COUNT) sel = THEME_SEL_AUTO;
    int cnt = (int)SendMessageW(s_hThemeCombo, CB_GETCOUNT, 0, 0);
    if (cnt == THEME_SEL_COUNT) {
        /* 已填充：逐项就地替换（三项都要跟着语言翻；原先只换第 0 项 ⇒ 切到英文后
           主题里仍留着「深色/浅色」）。
           用 DELETESTRING + INSERTSTRING 而非 CB_RESETCONTENT —— 后者会把下拉列表
           的积分高度锁成 ~0、展开只剩一行（语言下拉踩过的坑）。
           删掉 idx 后原 idx+1.. 前移，再插回同一个 idx ⇒ 顺序与项数都不变。 */
        for (i = 0; i < THEME_SEL_COUNT; i++) {
            SendMessageW(s_hThemeCombo, CB_DELETESTRING, i, 0);
            SendMessageW(s_hThemeCombo, CB_INSERTSTRING, i, (LPARAM)T(themeStrIds[i]));
        }
    } else {
        SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)T(STR_THEME_OPT_AUTO));
        SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)T(STR_THEME_OPT_DARK));
        SendMessageW(s_hThemeCombo, CB_ADDSTRING, 0, (LPARAM)T(STR_THEME_OPT_LIGHT));
    }
    SendMessageW(s_hThemeCombo, CB_SETCURSEL, sel, 0);
}

static void ThemeCfg_Load(void)
{
    char ini[MAX_PATH], buf[32];
    int i, sel = THEME_SEL_AUTO;
    GetSettingsPath(ini, sizeof(ini));
    if (ini[0]) {
        GetPrivateProfileStringA("UI", "Theme", "auto", buf, sizeof(buf), ini);
        for (i = 0; i < THEME_SEL_COUNT; i++) {
            if (stricmp(buf, kThemeSelKeys[i]) == 0) { sel = i; break; }
        }
    }
    g_state.themeSel = sel;
    g_state.isDark = Theme_ResolveSel(sel);
}

static void ThemeCfg_Save(void)
{
    char ini[MAX_PATH];
    int sel = g_state.themeSel;
    GetSettingsPath(ini, sizeof(ini));
    if (!ini[0]) return;
    if (sel < 0 || sel >= THEME_SEL_COUNT) sel = THEME_SEL_AUTO;
    WritePrivateProfileStringA("UI", "Theme", kThemeSelKeys[sel], ini);
}

/* 切语言后补刷 Lang_Refresh() 够不到的地方：
   下拉框内容、GDI+ 自绘的图表、状态栏、以及已有数据的汇总框。 */
static void Lang_AfterSwitch(void)
{
    Lang_FillCombo();
    Theme_FillCombo();   /* 主题下拉三项都随界面语言翻（深色/浅色也翻） */
    if (g_state.hClientChartWnd) InvalidateRect(g_state.hClientChartWnd, NULL, TRUE);
    if (g_state.hServerChartWnd) InvalidateRect(g_state.hServerChartWnd, NULL, TRUE);
    if (g_state.hReplayChartWnd) InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
    UpdateStatusBar();
    UI_UpdateSummary(MODE_CLIENT);
    UI_UpdateSummary(MODE_SERVER);
    /* 控件宽度随语言变（英文更长），重排一次 */
    LayoutControls(g_state.hMain);
    LangCfg_Save();
}

/* ===== Start client ===== */

static void OnStartClient(void)
{
    if (g_state.clientRunning) return;

    /* Read params from UI */
    char ip[64], port[16], proto[8], duration[16], interval[16],
         num[64], omit[16], bw[32], extra[256];
    UI_GetEditText(g_state.hClientIP, ip, sizeof(ip));
    UI_GetEditText(g_state.hClientPort, port, sizeof(port));
    int protoIdx = (int)SendMessageW(g_state.hClientProto, CB_GETCURSEL, 0, 0);
    strcpy(proto, protoIdx == 1 ? "udp" : "tcp");
    UI_GetEditText(g_state.hClientDuration, duration, sizeof(duration));
    UI_GetEditText(g_state.hClientInterval, interval, sizeof(interval));
    UI_GetEditText(g_state.hClientNum, num, sizeof(num));
    UI_GetEditText(g_state.hClientOmit, omit, sizeof(omit));
    UI_GetEditText(g_state.hClientBw, bw, sizeof(bw));
    UI_GetEditText(g_state.hClientExtra, extra, sizeof(extra));
    bool reverse = SendMessageW(g_state.hClientReverse, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool bidir = SendMessageW(g_state.hClientBidir, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool json = SendMessageW(g_state.hClientJson, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool getServerOutput = SendMessageW(g_state.hClientGetServer, BM_GETCHECK, 0, 0) == BST_CHECKED;

    /* Format code from combo */
    int fmtIdx = (int)SendMessageW(g_state.hClientFormat, CB_GETCURSEL, 0, 0);
    static const char* fmtCodes[] = {"k","m","g","t","K","M","G","T",""};
    const char* format = (fmtIdx >= 0 && fmtIdx < 9) ? fmtCodes[fmtIdx] : "";

    if (!duration[0]) strcpy(duration, "10");
    if (!interval[0]) strcpy(interval, "1");
    if (!port[0]) strcpy(port, "5201");
    if (!omit[0]) strcpy(omit, "0");

    if (bw[0] && !isdigit((unsigned char)bw[0])) {
        UI_AppendOutput(TA(STR_WARN_BW_INVALID), MODE_CLIENT);
        bw[0] = 0;
    }

    if (!ip[0]) {
        UI_AppendOutput(TA(STR_ERR_NO_SERVER_IP), MODE_CLIENT);
        return;
    }

    if (bidir) {
        UI_AppendOutput(TA(STR_INFO_BIDIR_37), MODE_CLIENT);
    }

    if (bw[0] && stricmp(proto, "udp") != 0) {
        UI_AppendOutput(TA(STR_WARN_BW_UDP_ONLY), MODE_CLIENT);
    }

    char iperf3Path[MAX_PATH] = {0};
    if (g_state.iperf3Path[0]) {
        strcpy(iperf3Path, g_state.iperf3Path);
    } else {
        char* found = FindIperf3();
        if (!found) {
            strncpy(g_state.msgErrorIperf3, TA(STR_ERR_NO_IPERF3_SETTINGS), sizeof(g_state.msgErrorIperf3) - 1);
            g_state.msgErrorIperf3[sizeof(g_state.msgErrorIperf3) - 1] = 0;
            UI_RefreshMsgBox();
            return;
        }
        strcpy(iperf3Path, found);
        free(found);
    }

    /* Version check: --forceflush requires iperf3 3.7+ */
    char verStr[32] = {0};
    if (!CheckIperf3Version(iperf3Path, verStr, sizeof(verStr))) {
        if (verStr[0]) {
            char msg[256];
            snprintf(msg, sizeof(msg), TA(STR_ERR_VERSION_LOW), verStr);
            strncpy(g_state.msgErrorIperf3, msg, sizeof(g_state.msgErrorIperf3) - 1);
        } else {
            strncpy(g_state.msgErrorIperf3, TA(STR_ERR_NO_VERSION), sizeof(g_state.msgErrorIperf3) - 1);
        }
        g_state.msgErrorIperf3[sizeof(g_state.msgErrorIperf3) - 1] = 0;
        UI_RefreshMsgBox();
        return;
    }

    /* 程序已找到且版本合格：清除上一次的错误提示 */
    g_state.msgErrorIperf3[0] = 0;

    /* Clear output, chart and summary */
    UI_ClearOutput(MODE_CLIENT);
    Chart_Clear(&g_state.clientChart);
    UI_RefreshMsgBox();
    g_state.liveSeenCount[0] = 0;
    /* Set right-axis mode for chart: UDP → DATAGRAMS, TCP → NONE */
    g_state.clientIsUdp = (protoIdx == 1);
    g_state.clientChart.rightAxis = g_state.clientIsUdp ? RA_DATAGRAMS : RA_NONE;
    /* Reset JSON mode state */
    g_state.isJsonMode[0] = false;
    memset(&g_state.jsonState[0], 0, sizeof(JsonParseState));
    g_state.jsonPendingBps[0] = 0;
    g_state.jsonPendingTime[0] = 0;
    /* Set chart Y-axis unit/scale based on format selection */
    {
        static const char* yUnits[] = {"Kb/s","Mb/s","Gb/s","Tb/s",
                                       "KB/s","MB/s","GB/s","TB/s",
                                       "Mb/s"};
        static const double yScales[] = {1e3, 1e6, 1e9, 1e12,
                                         8e3, 8e6, 8e9, 8e12,
                                         1e6};
        int idx = (fmtIdx >= 0 && fmtIdx < 9) ? fmtIdx : 8;
        strcpy(g_state.clientChart.yUnit, yUnits[idx]);
        g_state.clientChart.yScale = yScales[idx];
    }
    SetWindowTextW(g_state.hClientSummaryBox, L"");
    g_state.clientSummaryBlockCount = 0;
    g_state.clientSummaryBlockIdx = 0;
    memset(g_state.clientHasSender, 0, sizeof(g_state.clientHasSender));
    memset(g_state.clientHasReceiver, 0, sizeof(g_state.clientHasReceiver));
    memset(g_state.clientSumSender, 0, sizeof(g_state.clientSumSender));
    memset(g_state.clientSumReceiver, 0, sizeof(g_state.clientSumReceiver));
    g_state.clientPastSeparator = false;
    /* Clear --get-server-output state */
    g_state.clientHasServerOutput = getServerOutput;
    g_state.clientInServerOutput = false;
    g_state.clientServerPastSeparator = false;
    g_state.clientServerSummaryBlockCount = 0;
    g_state.clientServerSummaryBlockIdx = 0;
    memset(g_state.clientServerHasSender, 0, sizeof(g_state.clientServerHasSender));
    memset(g_state.clientServerHasReceiver, 0, sizeof(g_state.clientServerHasReceiver));
    memset(g_state.clientServerSumSender, 0, sizeof(g_state.clientServerSumSender));
    memset(g_state.clientServerSumReceiver, 0, sizeof(g_state.clientServerSumReceiver));

    strncpy(g_state.currentServerIP, ip, sizeof(g_state.currentServerIP) - 1);

    char cmdStr[512] = {0};
    HANDLE hPipeRead = NULL;

    int parallelIdx = (int)SendMessageW(g_state.hClientParallel, CB_GETCURSEL, 0, 0);
    int parallel = (parallelIdx >= 0 ? parallelIdx : 0) + 1;

    bool ok = StartIperf3Client(iperf3Path, ip, port, proto,
                                atoi(duration), atoi(interval),
                                parallel,
                                reverse, bidir, bw, extra,
                                format,
                                num,
                                atoi(omit),
                                json, getServerOutput,
                                cmdStr, sizeof(cmdStr),
                                &hPipeRead);
    if (!ok) {
        char err[160];
        snprintf(err, sizeof(err), TA(STR_ERR_SPAWN_CLIENT),
                 GetLastError());
        UI_AppendOutput(err, MODE_CLIENT);
        return;
    }

    /* Show command in the command box */
    SetEditUtf8(g_state.hClientCmd, cmdStr);

    g_state.clientPipeRead = hPipeRead;
    g_state.clientRunning = true;
    UI_EnableControls();
    UpdateStatusBar();

    /* Initialize log file */
    Log_Init(MODE_CLIENT, ip);

    /* Start pipe reader thread */
    g_state.clientPipeThread = CreateThread(NULL, 0, PipeReaderThread,
                                            (void*)MODE_CLIENT, 0, NULL);
}

/* ===== Start server ===== */
static void OnStartServer(void)
{
    if (g_state.serverRunning) return;

    char port[16], sExtra[256];
    UI_GetEditText(g_state.hServerPort, port, sizeof(port));
    if (!port[0]) strcpy(port, "5201");
    strncpy(g_state.serverPort, port, sizeof(g_state.serverPort) - 1);
    g_state.serverPort[sizeof(g_state.serverPort) - 1] = 0;
    UI_GetEditText(g_state.hServerExtra, sExtra, sizeof(sExtra));
    bool oneOff = SendMessageW(g_state.hServerOneOff, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool verbose = SendMessageW(g_state.hServerVerbose, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool sJson = SendMessageW(g_state.hServerJson, BM_GETCHECK, 0, 0) == BST_CHECKED;

    /* Format code from combo */
    int sFmtIdx = (int)SendMessageW(g_state.hServerFormat, CB_GETCURSEL, 0, 0);
    static const char* fmtCodesS[] = {"k","m","g","t","K","M","G","T",""};
    const char* sFormat = (sFmtIdx >= 0 && sFmtIdx < 9) ? fmtCodesS[sFmtIdx] : "";

    /* AF version from combo */
    int sAfIdx = (int)SendMessageW(g_state.hServerAf, CB_GETCURSEL, 0, 0);
    static const char* afCodesS[] = {"-4", "-6", ""};
    const char* sAfVersion = (sAfIdx >= 0 && sAfIdx < 3) ? afCodesS[sAfIdx] : "";

    char iperf3Path[MAX_PATH] = {0};
    if (g_state.iperf3Path[0]) {
        strcpy(iperf3Path, g_state.iperf3Path);
    } else {
        char* found = FindIperf3();
        if (!found) {
            strncpy(g_state.msgErrorIperf3, TA(STR_ERR_NO_IPERF3), sizeof(g_state.msgErrorIperf3) - 1);
            g_state.msgErrorIperf3[sizeof(g_state.msgErrorIperf3) - 1] = 0;
            UI_RefreshMsgBox();
            return;
        }
        strcpy(iperf3Path, found);
        free(found);
    }

    /* Version check: --forceflush requires iperf3 3.7+ */
    char verStr[32] = {0};
    if (!CheckIperf3Version(iperf3Path, verStr, sizeof(verStr))) {
        if (verStr[0]) {
            char msg[256];
            snprintf(msg, sizeof(msg), TA(STR_ERR_VERSION_LOW), verStr);
            strncpy(g_state.msgErrorIperf3, msg, sizeof(g_state.msgErrorIperf3) - 1);
        } else {
            strncpy(g_state.msgErrorIperf3, TA(STR_ERR_NO_VERSION), sizeof(g_state.msgErrorIperf3) - 1);
        }
        g_state.msgErrorIperf3[sizeof(g_state.msgErrorIperf3) - 1] = 0;
        UI_RefreshMsgBox();
        return;
    }

    /* Kill any process on the port */
    KillPortListeners(port);
    char killMsg[128];
    snprintf(killMsg, sizeof(killMsg), TA(STR_INFO_CLEANUP_PORT), port);
    UI_AppendOutput(killMsg, MODE_SERVER);

    /* 程序已找到且版本合格：清除上一次的错误提示 */
    g_state.msgErrorIperf3[0] = 0;

    /* Clear output, chart and summary */
    UI_ClearOutput(MODE_SERVER);
    Chart_Clear(&g_state.serverChart);
    UI_RefreshMsgBox();
    g_state.liveSeenCount[1] = 0;
    /* Server protocol unknown until client connects; default to TCP (NONE) */
    g_state.serverIsUdp = false;
    g_state.serverChart.rightAxis = RA_NONE;
    /* Reset JSON mode state */
    g_state.isJsonMode[1] = false;
    memset(&g_state.jsonState[1], 0, sizeof(JsonParseState));
    g_state.jsonPendingBps[1] = 0;
    g_state.jsonPendingTime[1] = 0;
    /* Set chart Y-axis unit/scale based on format selection */
    {
        static const char* yUnits[] = {"Kb/s","Mb/s","Gb/s","Tb/s",
                                       "KB/s","MB/s","GB/s","TB/s",
                                       "Mb/s"};
        static const double yScales[] = {1e3, 1e6, 1e9, 1e12,
                                         8e3, 8e6, 8e9, 8e12,
                                         1e6};
        int idx = (sFmtIdx >= 0 && sFmtIdx < 9) ? sFmtIdx : 8;
        strcpy(g_state.serverChart.yUnit, yUnits[idx]);
        g_state.serverChart.yScale = yScales[idx];
    }
    SetWindowTextW(g_state.hServerSummaryBox, L"");
    g_state.serverSummaryBlockCount = 0;
    g_state.serverSummaryBlockIdx = 0;
    memset(g_state.serverHasSender, 0, sizeof(g_state.serverHasSender));
    memset(g_state.serverHasReceiver, 0, sizeof(g_state.serverHasReceiver));
    memset(g_state.serverSumSender, 0, sizeof(g_state.serverSumSender));
    memset(g_state.serverSumReceiver, 0, sizeof(g_state.serverSumReceiver));
    g_state.serverPastSeparator = false;

    char cmdStr[512] = {0};
    HANDLE hPipeRead = NULL;

    bool ok = StartIperf3Server(iperf3Path, port, oneOff, verbose,
                                sFormat, sExtra, sJson, sAfVersion,
                                cmdStr, sizeof(cmdStr),
                                &hPipeRead);
    if (!ok) {
        char err[160];
        snprintf(err, sizeof(err), TA(STR_ERR_SPAWN_SERVER),
                 GetLastError());
        UI_AppendOutput(err, MODE_SERVER);
        return;
    }

    /* Show command in the command box */
    SetEditUtf8(g_state.hServerCmd, cmdStr);

    g_state.serverPipeRead = hPipeRead;
    g_state.serverRunning = true;
    UI_EnableControls();
    UpdateStatusBar();

    /* Initialize log file */
    Log_Init(MODE_SERVER, port);

    /* Start pipe reader thread */
    g_state.serverPipeThread = CreateThread(NULL, 0, PipeReaderThread,
                                            (void*)MODE_SERVER, 0, NULL);
}

/* ===== Pipe reader thread ===== */
/* Runs in a dedicated thread. Reads from the anonymous pipe (iperf3's
   stdout+stderr merged), splits into lines, and PostMessages each line to
   the main thread for processing. When ReadFile returns 0 (pipe write end
   closed = iperf3 exited), sends WM_PIPE_DONE and exits. */
static DWORD WINAPI PipeReaderThread(LPVOID param)
{
    int mode = (int)(intptr_t)param;
    HANDLE hPipe = (mode == MODE_CLIENT) ? g_state.clientPipeRead
                                         : g_state.serverPipeRead;
    HWND hMain = g_state.hMain;

    char buf[8192];
    char lineAccum[4096]; /* accumulator for partial lines */
    int lineLen = 0;

    while (hPipe) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, NULL);
        if (!ok || bytesRead == 0) {
            /* Pipe closed (iperf3 exited or was killed) */
            break;
        }
        buf[bytesRead] = 0;

        /* Split buffer into lines */
        for (DWORD i = 0; i < bytesRead; i++) {
            char c = buf[i];
            if (c == '\n') {
                /* End of line — send accumulated line */
                lineAccum[lineLen] = 0;
                /* Strip trailing \r */
                if (lineLen > 0 && lineAccum[lineLen-1] == '\r') {
                    lineAccum[lineLen-1] = 0;
                }
                if (lineLen > 0 || lineAccum[0]) {
                    /* malloc a copy for the main thread */
                    char* pLine = (char*)malloc(strlen(lineAccum) + 1);
                    if (pLine) {
                        strcpy(pLine, lineAccum);
                        PostMessageW(hMain, WM_PIPE_LINE, (WPARAM)mode, (LPARAM)pLine);
                    }
                }
                lineLen = 0;
            } else if (c != '\r' || (i + 1 < bytesRead && buf[i+1] != '\n')) {
                /* Accumulate character (skip \r if followed by \n) */
                if (lineLen < (int)sizeof(lineAccum) - 1) {
                    lineAccum[lineLen++] = c;
                }
            }
        }
    }

    /* Send any remaining partial line */
    if (lineLen > 0) {
        lineAccum[lineLen] = 0;
        char* pLine = (char*)malloc(strlen(lineAccum) + 1);
        if (pLine) {
            strcpy(pLine, lineAccum);
            PostMessageW(hMain, WM_PIPE_LINE, (WPARAM)mode, (LPARAM)pLine);
        }
    }

    /* Notify main thread that the pipe is done (iperf3 exited) */
    PostMessageW(hMain, WM_PIPE_DONE, (WPARAM)mode, 0);

    return 0;
}

/* ===== Stop client ===== */
static void OnStopClient(void)
{
    /* Kill iperf3 process — pipe write end will close, reader thread will
       finish reading remaining buffered data then exit naturally. */
    StopIperf3Client();

    /* Wait for pipe reader thread to finish (it drains remaining output) */
    if (g_state.clientPipeThread) {
        WaitForSingleObject(g_state.clientPipeThread, 3000);
        CloseHandle(g_state.clientPipeThread);
        g_state.clientPipeThread = NULL;
    }
    if (g_state.clientPipeRead) {
        CloseHandle(g_state.clientPipeRead);
        g_state.clientPipeRead = NULL;
    }

    g_state.clientRunning = false;
    UI_EnableControls();
    UpdateStatusBar();
    UI_AppendOutput(TA(STR_STOPPED_BY_USER), MODE_CLIENT);
    Log_WriteLine(TA(STR_STOPPED_BY_USER));
    if (g_state.clientSummaryBlockCount == 0) {
        UI_AppendOutput(TA(STR_INFO_TEST_ABORTED),
                        MODE_CLIENT);
    }
    Log_Close();
}

/* ===== Stop server ===== */
static void OnStopServer(void)
{
    StopIperf3Server();

    if (g_state.serverPipeThread) {
        WaitForSingleObject(g_state.serverPipeThread, 3000);
        CloseHandle(g_state.serverPipeThread);
        g_state.serverPipeThread = NULL;
    }
    if (g_state.serverPipeRead) {
        CloseHandle(g_state.serverPipeRead);
        g_state.serverPipeRead = NULL;
    }

    g_state.serverRunning = false;
    UI_EnableControls();
    UpdateStatusBar();
    UI_AppendOutput(TA(STR_STOPPED_BY_USER), MODE_SERVER);
    Log_WriteLine(TA(STR_STOPPED_BY_USER));
    Log_Close();
}

/* ===== Process a line from iperf3 (called from WM_PIPE_LINE handler) ===== */
static void OnProcessLine(const char* rawLine, int mode)
{
    char line[1024];
    strncpy(line, rawLine, sizeof(line) - 1);
    line[sizeof(line)-1] = 0;
    CleanOutput(line);

    if (!line[0]) return;

    /* JSON mode detection: first meaningful line starts with '{' */
    int mi = (mode == MODE_CLIENT) ? 0 : 1;
    if (!g_state.isJsonMode[mi]) {
        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '{') {
            g_state.isJsonMode[mi] = true;
            memset(&g_state.jsonState[mi], 0, sizeof(JsonParseState));
            g_state.jsonPendingBps[mi] = 0;
            g_state.jsonPendingTime[mi] = 0;
        }
    }

    /* Route to JSON parser if in JSON mode */
    if (g_state.isJsonMode[mi]) {
        /* Output raw line */
        UI_AppendOutput(line, mode);
        Log_WriteLine(line);

        Iperf3Data sumData;
        double jTime = 0, jBps = 0, jJitter = 0, jLoss = 0, jDgrams = 0;
        bool isInterval = false;

        bool gotSummary = ParseIperf3JsonLine(line, &g_state.jsonState[mi],
                                               &sumData, &jTime, &jBps, &isInterval,
                                               &jJitter, &jLoss, &jDgrams);

        /* Handle interval data for chart */
        if (jBps > 0) {
            g_state.jsonPendingBps[mi] = jBps;
        }
        if (jTime >= 0 && jTime > 0) {
            g_state.jsonPendingTime[mi] = jTime;
        }
        /* When we have both bps and time, add to chart */
        if (g_state.jsonPendingBps[mi] > 0 && g_state.jsonPendingTime[mi] > 0) {
            /* Check if we're inside sum_sent/sum_received (skip intervals then) */
            if (g_state.jsonState[mi].state != JS_IN_SUM_SENT &&
                g_state.jsonState[mi].state != JS_IN_SUM_RECEIVED) {
                if (mi == 0) {
                    Chart_AddPoint(&g_state.clientChart, g_state.jsonPendingTime[mi], g_state.jsonPendingBps[mi]);
                    /* UDP: add datagrams for latest point from JSON packets field */
                    if (g_state.clientIsUdp) {
                        Chart_AddUdpData(&g_state.clientChart, jJitter, jLoss, jDgrams);
                    }
                    UI_UpdateChart(MODE_CLIENT);
                } else {
                    /* Server: detect UDP from JSON interval data */
                    if (!g_state.serverIsUdp && (jJitter > 0 || jLoss > 0)) {
                        g_state.serverIsUdp = true;
                        g_state.serverChart.rightAxis = RA_JITTER_LOSS;
                    }
                    Chart_AddPoint(&g_state.serverChart, g_state.jsonPendingTime[mi], g_state.jsonPendingBps[mi]);
                    if (g_state.serverIsUdp && (jJitter > 0 || jLoss > 0)) {
                        Chart_AddUdpData(&g_state.serverChart, jJitter, jLoss, jDgrams);
                    }
                    UI_UpdateChart(MODE_SERVER);
                }
            }
            g_state.jsonPendingBps[mi] = 0;
            g_state.jsonPendingTime[mi] = 0;
        }

        /* Handle summary data */
        if (gotSummary) {
            bool inServerOutput = g_state.jsonState[mi].inServerOutput;
            int blk = 0; /* JSON mode: block 0 for now */

            if (inServerOutput) {
                if (_stricmp(sumData.direction, "sender") == 0) {
                    g_state.clientServerSumSender[blk] = sumData;
                    g_state.clientServerHasSender[blk] = true;
                } else if (_stricmp(sumData.direction, "receiver") == 0) {
                    g_state.clientServerSumReceiver[blk] = sumData;
                    g_state.clientServerHasReceiver[blk] = true;
                }
                if (g_state.clientServerSummaryBlockCount < 1)
                    g_state.clientServerSummaryBlockCount = 1;
                UI_UpdateSummary(MODE_CLIENT);
            } else if (mi == 0) {
                if (_stricmp(sumData.direction, "sender") == 0) {
                    g_state.clientSumSender[blk] = sumData;
                    g_state.clientHasSender[blk] = true;
                } else if (_stricmp(sumData.direction, "receiver") == 0) {
                    g_state.clientSumReceiver[blk] = sumData;
                    g_state.clientHasReceiver[blk] = true;
                }
                if (g_state.clientSummaryBlockCount < 1)
                    g_state.clientSummaryBlockCount = 1;
                UI_UpdateSummary(MODE_CLIENT);
            } else {
                if (_stricmp(sumData.direction, "sender") == 0) {
                    g_state.serverSumSender[blk] = sumData;
                    g_state.serverHasSender[blk] = true;
                } else if (_stricmp(sumData.direction, "receiver") == 0) {
                    g_state.serverSumReceiver[blk] = sumData;
                    g_state.serverHasReceiver[blk] = true;
                }
                if (g_state.serverSummaryBlockCount < 1)
                    g_state.serverSummaryBlockCount = 1;
                UI_UpdateSummary(MODE_SERVER);
            }
        }
        return;
    }

    /* Text mode processing (original path) */
    /* Output raw line */
    UI_AppendOutput(line, mode);

    /* Write to log file if enabled */
    Log_WriteLine(line);

    /* Server: detect new connection — only "Accepted connection from" signals
       a brand-new client session. In multi-stream mode, iperf3 prints per-stream
       "connected to" lines for each flow; those must NOT trigger Chart_Clear
       (otherwise they wipe interval data that may have started arriving). */
    if (mode == MODE_SERVER) {
        if (strstr(line, "Accepted connection from")) {
            g_state.serverPastSeparator = false;
            Chart_Clear(&g_state.serverChart);
            /* Reset UDP flag — will be set when first interval with jitter arrives */
            g_state.serverIsUdp = false;
            g_state.serverChart.rightAxis = RA_NONE;
            g_state.serverSummaryBlockCount = 0;
            g_state.serverSummaryBlockIdx = 0;
            memset(g_state.serverHasSender, 0, sizeof(g_state.serverHasSender));
            memset(g_state.serverHasReceiver, 0, sizeof(g_state.serverHasReceiver));
            memset(g_state.serverSumSender, 0, sizeof(g_state.serverSumSender));
            memset(g_state.serverSumReceiver, 0, sizeof(g_state.serverSumReceiver));
            /* Reset JSON state for new connection */
            g_state.isJsonMode[1] = false;
            memset(&g_state.jsonState[1], 0, sizeof(JsonParseState));
            g_state.jsonPendingBps[1] = 0;
            g_state.jsonPendingTime[1] = 0;
            /* Reset multi-stream tracking for new client session */
            g_state.liveSeenCount[1] = 0;
            UI_UpdateChart(MODE_SERVER);
            /* Don't return — let the line be displayed in output */
        }
    }

    /* Client: detect "Server output:" section (--get-server-output) */
    if (mode == MODE_CLIENT && g_state.clientHasServerOutput) {
        if (strstr(line, "Server output:")) {
            g_state.clientInServerOutput = true;
            g_state.clientServerPastSeparator = false;
            g_state.clientServerSummaryBlockCount = 0;
            g_state.clientServerSummaryBlockIdx = 0;
            return;
        }
        /* "iperf Done." marks end of server output section */
        if (g_state.clientInServerOutput && strstr(line, "iperf Done.")) {
            g_state.clientInServerOutput = false;
            return;
        }
    }

    /* Check for separator line */
    const char* trimmed = line;
    while (*trimmed == ' ') trimmed++;
    if (trimmed[0] == '-') {
        bool isSep = true;
        for (const char* p = trimmed; *p; p++) {
            if (*p != '-' && *p != ' ') { isSep = false; break; }
        }
        if (isSep) {
            if (mode == MODE_CLIENT) {
                if (g_state.clientInServerOutput) {
                    /* Server output section separator */
                    g_state.clientServerPastSeparator = true;
                    g_state.clientServerSummaryBlockIdx = 0;
                    if (g_state.clientServerSummaryBlockCount < 1)
                        g_state.clientServerSummaryBlockCount = 1;
                } else {
                    if (g_state.clientPastSeparator) {
                        g_state.clientSummaryBlockIdx = 1;
                        if (g_state.clientSummaryBlockCount < 2)
                            g_state.clientSummaryBlockCount = 2;
                    } else {
                        g_state.clientPastSeparator = true;
                        if (g_state.clientSummaryBlockCount < 1)
                            g_state.clientSummaryBlockCount = 1;
                        g_state.clientSummaryBlockIdx = 0;
                    }
                }
            } else {
                if (g_state.serverPastSeparator) {
                    g_state.serverSummaryBlockIdx = 1;
                    if (g_state.serverSummaryBlockCount < 2)
                        g_state.serverSummaryBlockCount = 2;
                } else {
                    g_state.serverPastSeparator = true;
                    if (g_state.serverSummaryBlockCount < 1)
                        g_state.serverSummaryBlockCount = 1;
                    g_state.serverSummaryBlockIdx = 0;
                }
            }
            return;
        }
    }

    /* Parse data line */
    if (mode == MODE_CLIENT && g_state.clientInServerOutput) {
        /* In server output section — parse summaries but don't add intervals to chart */
        bool isSummary = g_state.clientServerPastSeparator;
        OnIperf3Line(line, isSummary, mode);
    } else {
        bool isSummary = (mode == MODE_CLIENT) ? g_state.clientPastSeparator : g_state.serverPastSeparator;
        OnIperf3Line(line, isSummary, mode);
    }
}

/* ===== Parse and emit iperf3 data ===== */
static void OnIperf3Line(const char* line, bool isSummary, int mode)
{
    Iperf3Data data;
    if (!ParseIperf3Line(line, isSummary, &data)) return;

    /* iperf3 summary rows carry direction (sender/receiver); interval rows do not.
       Use this as the authoritative signal instead of separator tracking, because
       iperf3 prints separator lines between every interval in multi-stream mode. */
    isSummary = (data.direction[0] != 0);

    /* If in client server-output section, route to server-output storage */
    bool inServerOutput = (mode == MODE_CLIENT && g_state.clientInServerOutput);

    if (isSummary) {
        int blk;
        if (data.role[0] && inServerOutput) {
            /* Bidir Server output: each role gets its own block to avoid overwrite */
            if (strstr(data.role, "TX-C"))
                blk = 0;
            else if (strstr(data.role, "RX-S"))
                blk = 1;
            else if (strstr(data.role, "TX-S"))
                blk = 2;
            else if (strstr(data.role, "RX-C"))
                blk = 3;
            else
                blk = 0;
        } else if (data.role[0]) {
            /* Bidir client local: TX-C / RX-S → block 0, TX-S / RX-C → block 1 */
            if (strstr(data.role, "TX-C") || strstr(data.role, "RX-S"))
                blk = 0;
            else if (strstr(data.role, "TX-S") || strstr(data.role, "RX-C"))
                blk = 1;
            else
                blk = 0;
        } else {
            /* Non-bidir summary always uses a single block (block 0). */
            blk = 0;
        }
        if (blk < 0) blk = 0;
        if (inServerOutput) {
            if (blk > 3) blk = 3;
        } else {
            if (blk > 1) blk = 1;
        }

        if (inServerOutput) {
            int* pBlockCount = &g_state.clientServerSummaryBlockCount;
            if (blk + 1 > *pBlockCount) *pBlockCount = blk + 1;
            if (_stricmp(data.direction, "sender") == 0) {
                g_state.clientServerSumSender[blk] = data;
                g_state.clientServerHasSender[blk] = true;
            } else if (_stricmp(data.direction, "receiver") == 0) {
                g_state.clientServerSumReceiver[blk] = data;
                g_state.clientServerHasReceiver[blk] = true;
            }
            UI_UpdateSummary(MODE_CLIENT);
        } else if (mode == MODE_CLIENT) {
            int* pBlockCount = &g_state.clientSummaryBlockCount;
            if (blk + 1 > *pBlockCount) *pBlockCount = blk + 1;
            if (_stricmp(data.direction, "sender") == 0) {
                g_state.clientSumSender[blk] = data;
                g_state.clientHasSender[blk] = true;
            } else if (_stricmp(data.direction, "receiver") == 0) {
                g_state.clientSumReceiver[blk] = data;
                g_state.clientHasReceiver[blk] = true;
            }
            UI_UpdateSummary(MODE_CLIENT);
        } else {
            int* pBlockCount = &g_state.serverSummaryBlockCount;
            if (blk + 1 > *pBlockCount) *pBlockCount = blk + 1;
            if (_stricmp(data.direction, "sender") == 0) {
                g_state.serverSumSender[blk] = data;
                g_state.serverHasSender[blk] = true;
            } else if (_stricmp(data.direction, "receiver") == 0) {
                g_state.serverSumReceiver[blk] = data;
                g_state.serverHasReceiver[blk] = true;
            }
            UI_UpdateSummary(MODE_SERVER);
        }
    } else {
        /* Interval data — add to chart (skip if in server output section).
           Multi-stream (-P n): dedupe by interval end time so each interval
           contributes ONE point (the [SUM] aggregate), while per-stream bps
           attach to that same point. This fixes the sawtooth bug where every
           per-stream line used to add its own point at the same X. */
        if (!inServerOutput) {
            int mi = (mode == MODE_CLIENT) ? 0 : 1;
            ChartData* cd = (mode == MODE_CLIENT) ? &g_state.clientChart : &g_state.serverChart;
            double t = data.intervalEnd;

            /* Server: detect UDP from interval data (jitter / packets / loss) */
            if (mode == MODE_SERVER &&
                !g_state.serverIsUdp &&
                (data.jitter > 0 || data.totalPackets > 0 || data.lostPackets > 0)) {
                g_state.serverIsUdp = true;
                g_state.serverChart.rightAxis = RA_JITTER_LOSS;
            }
            bool isUdp = (mode == MODE_CLIENT) ? g_state.clientIsUdp : g_state.serverIsUdp;

            int dgrams = data.totalPackets;
            if (dgrams == 0 && data.retransmits > 0) dgrams = data.retransmits;

            /* Bidirectional mode (iperf3 --bidir): every interval row carries a
               [Role] bracket. Role is relative to the printing side:
                    client prints [TX-C] / [RX-C]
                    server prints [TX-S] / [RX-S]
               Draw the two directions as two separate summary lines; per-stream
               curves are suppressed. Each line simply overwrites the slot for
               its role — iperf3 prints the per-stream rows of an interval FIRST
               and its [SUM] row LAST, so the aggregate naturally wins. This also
               covers the single-pair case (no -P), where iperf3 emits NO [SUM]
               row at all and the per-stream row is the only (and correct) value. */
            if (data.role[0] != 0) {
                cd->hasTwoSum = true;
                if (cd->seriesCount != 0) Chart_SetStreamCount(cd, 0); /* bidir: no per-stream lines */
                bool isRX = (data.role[0] == 'R' && data.role[1] == 'X');
                if (mode == MODE_SERVER &&
                    !g_state.serverIsUdp &&
                    (data.jitter > 0 || data.totalPackets > 0 || data.lostPackets > 0)) {
                    g_state.serverIsUdp = true;
                    g_state.serverChart.rightAxis = RA_JITTER_LOSS;
                }
                bool isUdp = (mode == MODE_CLIENT) ? g_state.clientIsUdp : g_state.serverIsUdp;

                /* 流式记账：本分支直接 return，不走下面的 slot 分配，
                   所以在这里自己登记 streamId，才能判断「是否多流」。
                   liveSeenCount[] 在每次开测时被清零（3557 / 3707 / 4036）。 */
                if (data.streamId >= 0) {
                    bool seen = false;
                    for (int k = 0; k < g_state.liveSeenCount[mi]; k++) {
                        if (g_state.liveSeenIds[mi][k] == data.streamId) { seen = true; break; }
                    }
                    if (!seen && g_state.liveSeenCount[mi] < CHART_MAX_STREAMS)
                        g_state.liveSeenIds[mi][g_state.liveSeenCount[mi]++] = data.streamId;
                }
                bool multiStream = (g_state.liveSeenCount[mi] > 1);
                bool isSumRow    = (data.streamId == -1);   /* [SUM] 聚合行 */

                bool newPoint = (cd->count == 0) || (t > cd->times[cd->count - 1] + 1e-9);
                if (newPoint) {
                    /* 多流时新点先沿用上一区间的汇总值（等 [SUM] 覆盖），
                       避免被第一条流行压成 1/N —— 与非 bidir 分支同一套路。 */
                    double seedV  = (multiStream && cd->count > 0) ? cd->values[cd->count - 1]  : 0.0;
                    double seedV2 = (multiStream && cd->count > 0) ? cd->values2[cd->count - 1] : 0.0;
                    Chart_AddPoint(cd, t, seedV);
                    cd->values2[cd->count - 1] = seedV2;
                    if (isUdp) Chart_AddUdpData(cd, data.jitter, data.lossRate, (double)dgrams);
                }
                int idx = cd->count - 1;
                /* 多流：只有 [SUM] 行才写汇总值（流行跳过）；
                   单对流：iperf3 不打 [SUM]，必须按 role 无条件写入，否则曲线全空。 */
                if (!multiStream || isSumRow) {
                    if (isRX) cd->values2[idx] = data.bps;
                    else      cd->values[idx]  = data.bps;
                }
                UI_UpdateChart(mode);
                return;
            }

            /* Assign a stable per-stream slot for this line (by streamId) */
            int slot = -1;
            if (data.streamId >= 0) {
                for (int k = 0; k < g_state.liveSeenCount[mi]; k++) {
                    if (g_state.liveSeenIds[mi][k] == data.streamId) { slot = k; break; }
                }
                if (slot < 0 && g_state.liveSeenCount[mi] < CHART_MAX_STREAMS) {
                    slot = g_state.liveSeenCount[mi];
                    g_state.liveSeenIds[mi][g_state.liveSeenCount[mi]++] = data.streamId;
                }
            }

            bool newPoint = (cd->count == 0) || (t > cd->times[cd->count - 1] + 1e-9);
            if (newPoint) {
                /* Seed the point from this line (correct for single-stream;
                   for multi-stream a following [SUM] line overwrites the value).

                   多流(-P n)时行序是 [4][7]...[19][SUM]：新区间被第一条流"种子"成
                   单条流的速率（约总量 1/N），在 [SUM] 到达前每来一行都会重绘一次，
                   于是汇总曲线头部会向下扎一截（测试结束/回放看不到，因为那时
                   [SUM] 已覆盖）。这里改用它出点前"上一个点的汇总值"作为种子 ——
                   那比 1/N 更接近真值，等 [SUM] 到达照旧被覆盖成真实汇总。
                   单流（iperf3 不打 [SUM]）必须仍用本行值，否则值永远停在上一秒。 */
                double seed = data.bps;
                if (cd->count > 0 && g_state.liveSeenCount[mi] > 1)
                    seed = cd->values[cd->count - 1];
                Chart_AddPoint(cd, t, seed);
                if (isUdp) Chart_AddUdpData(cd, data.jitter, data.lossRate, (double)dgrams);
                if (slot >= 0) Chart_AddStreamPoint(cd, slot, data.bps);
            } else {
                /* Same interval: SUM overwrites the aggregated value; per-stream
                   lines attach to the existing point. */
                if (data.streamId == -1) {
                    cd->values[cd->count - 1] = data.bps;
                } else if (slot >= 0) {
                    Chart_AddStreamPoint(cd, slot, data.bps);
                }
            }

            /* Keep the chart's stream count in sync (preserves legend toggles) */
            int eff = (g_state.liveSeenCount[mi] <= 1) ? 0
                      : (g_state.liveSeenCount[mi] > CHART_MAX_STREAMS ? CHART_MAX_STREAMS
                                                                        : g_state.liveSeenCount[mi]);
            if (cd->seriesCount != eff) Chart_SetStreamCount(cd, eff);

            UI_UpdateChart(mode);
        }
    }
}

/* ===== Check if client process exited (called from WM_PIPE_DONE) ===== */
static void CheckClientExited(DWORD exitCode)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "[exit code %lu]", exitCode);
    UI_AppendOutput(msg, MODE_CLIENT);
    Log_WriteLine(msg);
    Log_Close();

    /* Clean up pipe and thread */
    if (g_state.clientPipeThread) {
        WaitForSingleObject(g_state.clientPipeThread, 2000);
        CloseHandle(g_state.clientPipeThread);
        g_state.clientPipeThread = NULL;
    }
    if (g_state.clientPipeRead) {
        CloseHandle(g_state.clientPipeRead);
        g_state.clientPipeRead = NULL;
    }

    /* CRITICAL: release the stale process handle, otherwise the next
       StartIperf3Client sees "already running" and fails. */
    ResetClientProcess();
    g_state.clientRunning = false;
    UI_EnableControls();
    UpdateStatusBar();
}

static void CheckServerExited(DWORD exitCode)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "[server exit code %lu]", exitCode);
    UI_AppendOutput(msg, MODE_SERVER);
    Log_WriteLine(msg);
    Log_Close();

    if (g_state.serverPipeThread) {
        WaitForSingleObject(g_state.serverPipeThread, 2000);
        CloseHandle(g_state.serverPipeThread);
        g_state.serverPipeThread = NULL;
    }
    if (g_state.serverPipeRead) {
        CloseHandle(g_state.serverPipeRead);
        g_state.serverPipeRead = NULL;
    }

    ResetServerProcess();
    g_state.serverRunning = false;
    UI_EnableControls();
    UpdateStatusBar();
}

/* ===== Main window proc ===== */
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        /* Load RichEdit */
        s_hRichEdit = LoadLibraryW(L"Msftedit.dll");
        if (s_hRichEdit) {
            s_hRichEdit50Loaded = true;
        } else {
            s_hRichEdit = LoadLibraryW(L"Riched20.dll");
        }

        /* Create fonts */
        s_hFontNormal = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_hFontBold = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_hFontMono = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        s_hFontGroup = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        s_hFontGroupNormal = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        /* Initialize GDI+ */
        Chart_GdiplusInit();

        /* Initialize charts */
        Chart_Init(&g_state.clientChart, COLOR_CLIENT_LINE, COLOR_CLIENT_AREA);
        g_state.clientChart.lineColor2 = 0xFFF59E0B;   /* bidir RX = amber */
        Chart_Init(&g_state.serverChart, COLOR_SERVER_LINE, COLOR_SERVER_AREA);
        g_state.serverChart.lineColor2 = 0xFFF59E0B;
        /* Replay chart starts with the client color; the line color is reset
           to client/server when a log file is loaded. */
        Chart_Init(&g_state.replayChart, COLOR_CLIENT_LINE, COLOR_CLIENT_AREA);
        g_state.replayChart.lineColor2 = 0xFFF59E0B;

        /* Dark mode is resolved by ThemeCfg_Load() below (reads [UI] Theme).
           Must run BEFORE CreateAllControls so controls born with the right
           colours (RichEdit bakes its background at creation). */
        g_state.activeTab = TAB_CLIENT;
        g_state.hMain = hwnd;
        g_state.clientAutoScroll = true;
        g_state.serverAutoScroll = true;
        g_state.replaySpeed = 1.0;

        /* INI defaults first (first run), then the UI language: the language
           must be known BEFORE any control is created, otherwise every label
           is born in the wrong language and only fixed after the fact. */
        InitDefaultConfig();
        LangCfg_Load();
        ThemeCfg_Load();   /* 读 [UI] Theme 并解析 isDark，须在 CreateAllControls 之前 */

        CreateAllControls(hwnd);
        AttachFieldChrome();    /* 输入类控件挂非客户区边框子类（深色下重涂 #727272） */
        LayoutControls(hwnd);
        ShowActivePanel();
        LoadAllConfig();
        /* 启动路径不经过 Theme_Apply（它只由切档/系统变更触发），
           深色档启动时标题栏需在这里同步一次。 */
        ApplyTitleBarTheme(hwnd);
        return 0;

    case WM_THEMECHANGED: {
        /* 系统主题变了：只有「跟随系统」档才跟随；手动的深色/浅色选择保持不变。 */
        if (g_state.themeSel == THEME_SEL_AUTO) Theme_Apply();
        return 0;
    }

    case WM_SETTINGCHANGE:
        /* Windows 切换浅色/深色时发 WM_SETTINGCHANGE 且 lParam="ImmersiveColorSet"，
           这是「跟随系统」实时生效的可靠信号。其余设置变更交回默认处理。 */
        if (g_state.themeSel == THEME_SEL_AUTO && lParam &&
            lstrcmpW((const wchar_t*)lParam, L"ImmersiveColorSet") == 0) {
            Theme_Apply();
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        switch (id) {
        /* Tab buttons */
        case BTN_TAB_CLIENT:   UI_SwitchTab(TAB_CLIENT); break;
        case BTN_TAB_SERVER:   UI_SwitchTab(TAB_SERVER); break;
        case BTN_TAB_REPLAY:   UI_SwitchTab(TAB_REPLAY); break;
        case BTN_TAB_SETTINGS: UI_SwitchTab(TAB_SETTINGS); break;

        /* Global config buttons */
        case BTN_SAVE_CONFIG:     SaveAllConfig(); break;
        case BTN_LOAD_CONFIG:     LoadAllConfig(); break;
        case BTN_RESTORE_DEFAULT: RestoreAllDefaults(); break;

        /* Client buttons */
        case BTN_CLIENT_START: OnStartClient(); break;
        case BTN_CLIENT_STOP:  OnStopClient(); break;
        case BTN_CLIENT_CLEAR: UI_ClearOutput(MODE_CLIENT); break;
        case BTN_CLIENT_COPY:  UI_CopyOutput(MODE_CLIENT); break;

        /* Server buttons */
        case BTN_SERVER_START: OnStartServer(); break;
        case BTN_SERVER_STOP:  OnStopServer(); break;
        case BTN_SERVER_CLEAR: UI_ClearOutput(MODE_SERVER); break;
        case BTN_SERVER_COPY:  UI_CopyOutput(MODE_SERVER); break;

        /* Settings */
        case BTN_SETTINGS_BROWSE: UI_BrowsePath(); break;
        case BTN_SETTINGS_DETECT: UI_AutoDetectPath(); break;
        case BTN_LICENSE:         UI_ShowLicenseDialog(); break;   /* 授权声明（方案 §6） */
        case BTN_HELP_IPERF3:     UI_ShowHelpDialog();    break;   /* 说明（iperf3 下载 + 自动检测） */

        /* iperf3 路径框：输入即生效（与日志目录框一致）；失去焦点时立即校验 */
        case EDT_SETTINGS_PATH:
            if (code == EN_CHANGE) {
                char path[MAX_PATH];
                UI_GetEditText(g_state.hSettingsPath, path, MAX_PATH);
                strncpy(g_state.iperf3Path, path, MAX_PATH - 1);
                g_state.iperf3Path[MAX_PATH - 1] = 0;
                UpdateStatusBar();
            } else if (code == EN_KILLFOCUS) {
                ValidateIperf3Path();
            }
            break;

        /* 日志目录框：失去焦点时立即校验目录是否可写 */
        case EDT_LOG_DIR:
            if (code == EN_KILLFOCUS) {
                ValidateLogDir();
            }
            break;

        /* Replay — 浏览即载入，无独立载入按钮 */
        case BTN_REPLAY_BROWSE: OnReplayBrowse(); break;
        case BTN_REPLAY_PLAY:
            if (g_state.replayLoaded && !g_state.replayPlaying) {
                /* (re)start from the beginning when already at the end */
                if (g_state.replayIdx >= g_state.replay.count - 1)
                    Replay_ApplyIndex(0, true);
                g_state.replayVt = g_state.replay.samples[g_state.replayIdx].time;
                Replay_SetPlaying(true);
            }
            break;
        case BTN_REPLAY_STOP: Replay_SetPlaying(false); break;
        case CBO_REPLAY_SPEED:
            if (code == CBN_SELCHANGE) Replay_ReadSpeed();
            break;
        case CBO_REPLAY_UNIT:
            if (code == CBN_SELCHANGE) {
                Replay_ReadUnit();
                /* Replay_Seek 每次定位都重读 unit/yScale，直接重放当前帧即可重算量程 */
                Replay_ApplyIndex(g_state.replayIdx, true);
            }
            break;

        /* UI language */
        case CBO_LANG:
            if (code == CBN_SELCHANGE) {
                int ls = (int)SendMessageW(s_hLangCombo, CB_GETCURSEL, 0, 0);
                Lang_SetSel(ls);
                Lang_AfterSwitch();
            }
            break;

        /* UI theme */
        case CBO_THEME:
            if (code == CBN_SELCHANGE) {
                g_state.themeSel = (int)SendMessageW(s_hThemeCombo, CB_GETCURSEL, 0, 0);
                Theme_Apply();
                ThemeCfg_Save();
            }
            break;

        /* Log controls */
        case BTN_LOG_BROWSE: UI_BrowseLogDir(); break;
        case BTN_LOG_NAMING_HELP: {
            MessageBoxW(g_state.hMain,
                T(STR_HELP_NAMING_BODY),
                T(STR_HELP_NAMING_TITLE),
                MB_OK | MB_ICONINFORMATION);
            break;
        }
        case CHK_LOG_ENABLE:
            if (code == BN_CLICKED) {
                UI_UpdateLogControls();
                ValidateLogDir();
            }
            break;

        /* Checkbox changes */
        case CBO_CLIENT_PROTO:
            if (code == CBN_SELCHANGE) {
                UpdateProtocolDependentControls();
            }
            break;

        case CHK_CLIENT_AUTOSCROLL:
            if (code == BN_CLICKED) {
                g_state.clientAutoScroll = SendMessageW(g_state.hClientAutoScroll, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;

        /* -R and --bidir are mutually exclusive in iperf3 */
        case CHK_CLIENT_REVERSE:
            if (code == BN_CLICKED) {
                if (SendMessageW(g_state.hClientReverse, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    SendMessageW(g_state.hClientBidir, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                g_state.clientReverse = SendMessageW(g_state.hClientReverse, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;
        case CHK_CLIENT_BIDIR:
            if (code == BN_CLICKED) {
                if (SendMessageW(g_state.hClientBidir, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    SendMessageW(g_state.hClientReverse, BM_SETCHECK, BST_UNCHECKED, 0);
                }
                g_state.clientBidir = SendMessageW(g_state.hClientBidir, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;
        case CHK_CLIENT_JSON:
            if (code == BN_CLICKED) {
                g_state.clientJson = SendMessageW(g_state.hClientJson, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;
        case CHK_CLIENT_GETSERVER:
            if (code == BN_CLICKED) {
                g_state.clientGetServer = SendMessageW(g_state.hClientGetServer, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;

        case CHK_SERVER_ONEOFF:
            if (code == BN_CLICKED) {
                g_state.serverOneOff = SendMessageW(g_state.hServerOneOff, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;
        case CHK_SERVER_VERBOSE:
            if (code == BN_CLICKED) {
                g_state.serverVerbose = SendMessageW(g_state.hServerVerbose, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;
        case CHK_SERVER_JSON:
            if (code == BN_CLICKED) {
                g_state.serverJson = SendMessageW(g_state.hServerJson, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;
        case CHK_SERVER_AUTOSCROLL:
            if (code == BN_CLICKED) {
                g_state.serverAutoScroll = SendMessageW(g_state.hServerAutoScroll, BM_GETCHECK, 0, 0) == BST_CHECKED;
            }
            break;

        /* Chart — "显示各流" default toggle (persisted) */
        case CHK_SHOW_STREAMS:
            if (code == BN_CLICKED) {
                bool val = SendMessageW(g_state.hShowStreams, BM_GETCHECK, 0, 0) == BST_CHECKED;
                ApplyShowStreams(val);
                char ini[MAX_PATH];
                GetSettingsPath(ini, sizeof(ini));
                if (ini[0]) WritePrivateProfileStringA("Chart", "ShowStreams",
                                                       val ? "1" : "0", ini);
            }
            break;

        /* Chart — "隐藏流汇总" toggle (persisted; 仅在「显示各流」勾选时生效) */
        case CHK_HIDE_STREAM_SUM:
            if (code == BN_CLICKED) {
                bool val = SendMessageW(g_state.hHideStreamSum, BM_GETCHECK, 0, 0) == BST_CHECKED;
                ApplyHideStreamSum(val);
                char ini[MAX_PATH];
                GetSettingsPath(ini, sizeof(ini));
                if (ini[0]) WritePrivateProfileStringA("Chart", "HideStreamSum",
                                                       val ? "1" : "0", ini);
            }
            break;
        }
        return 0;
    }

    case WM_PIPE_LINE: {
        /* A line from the pipe reader thread.
           wParam = mode, lParam = malloc'd line (must free). */
        int mode = (int)wParam;
        char* pLine = (char*)lParam;
        if (pLine) {
            OnProcessLine(pLine, mode);
            free(pLine);
        }
        return 0;
    }

    case WM_PIPE_DONE: {
        /* Pipe reader thread finished (iperf3 exited).
           wParam = mode, lParam = 0. */
        int mode = (int)wParam;
        DWORD exitCode = 0;
        HANDLE hProc = (mode == MODE_CLIENT) ? GetClientProcess() : GetServerProcess();
        if (hProc) {
            GetExitCodeProcess(hProc, &exitCode);
            if (exitCode == STILL_ACTIVE) exitCode = 0;
        }
        if (mode == MODE_CLIENT) {
            CheckClientExited(exitCode);
        } else {
            CheckServerExited(exitCode);
        }
        /* In autotest mode, dump results and advance to next case */
        if (s_autotestMode) {
            AutoTest_DumpAndAdvance(mode);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        /* Owner-drawn push buttons（自绘逻辑抽在 DrawOwnerButton，与 License 对话框共用） */
        if (DrawOwnerButton((const DRAWITEMSTRUCT*)lParam)) return TRUE;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_CTLCOLORSTATIC: {
        /* Color static labels */
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;
        /* Status dots: running = red, idle = green (words keep default color) */
        if (hCtl == g_state.hStatusClientDot) {
            SetTextColor(hdc, g_state.clientRunning ? GetRunColor() : GetIdleColor());
        } else if (hCtl == g_state.hStatusServerDot) {
            SetTextColor(hdc, g_state.serverRunning ? GetRunColor() : GetIdleColor());
        } else if (!IsWindowEnabled(hCtl)) {
            /* If the control is disabled, use gray text */
            SetTextColor(hdc, GetGrayTextColor());
        } else {
            SetTextColor(hdc, GetTextColorValue());
        }
        SetBkColor(hdc, GetBgColor());
        EnsureThemeBrushes();
        return (LRESULT)s_bgBrush;
    }

    case WM_CTLCOLORBTN: {
        /* Color buttons/checkboxes — disabled ones get gray text */
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;
        if (!IsWindowEnabled(hCtl)) {
            SetTextColor(hdc, GetGrayTextColor());
        } else {
            SetTextColor(hdc, GetTextColorValue());
        }
        SetBkColor(hdc, GetBgColor());
        EnsureThemeBrushes();
        return (LRESULT)s_btnBrush;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        /* All edit controls (output boxes, summary boxes, path fields) share
           the card background + themed text colour. */
        SetTextColor(hdc, GetTextColorValue());
        SetBkColor(hdc, GetCardColor());
        EnsureThemeBrushes();
        return (LRESULT)s_cardBrush;
    }

    case WM_CTLCOLORLISTBOX: {
        /* ComboBox 的下拉列表是独立窗口，默认白底 —— 跟随主题。 */
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, GetTextColorValue());
        SetBkColor(hdc, GetCardColor());
        EnsureThemeBrushes();
        return (LRESULT)s_cardBrush;
    }

    case WM_ERASEBKGND: {
        /* Main window background: hbrBackground is NULL, so without this
           handler the client area is never erased — unpainted regions
           (visible when maximized) show up as black stripes. */
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH brush = CreateSolidBrush(GetBgColor());
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
        return 1;
    }

    case WM_TIMER: {
        if (wParam == 999 && s_autotestMode) {
            /* AutoTest: server timeout — stop server and let WM_PIPE_DONE fire */
            KillTimer(hwnd, 999);
            OnStopServer();
        }
        else if (wParam == REPLAY_TIMER_ID) {
            Replay_Tick();
        }
        return 0;
    }

    case WM_HSCROLL: {
        /* Replay position slider (trackbar) */
        if ((HWND)lParam == g_state.hReplaySlider && g_state.replayLoaded) {
            if (g_state.replayPlaying) Replay_SetPlaying(false); /* drag pauses playback */
            int pos = (int)SendMessageW(g_state.hReplaySlider, TBM_GETPOS, 0, 0);
            Replay_ApplyIndex(pos, true);
        }
        return 0;
    }

    case WM_AUTOTEST_NEXT: {
        /* Small delay to let cleanup settle, then start next case */
        AutoTest_StartNext();
        return 0;
    }

    case WM_AUTOTEST_DONE: {
        /* All test cases complete — exit */
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
        /* Stop any running processes and pipe reader threads */
        StopIperf3Client();
        StopIperf3Server();
        if (g_state.clientPipeThread) {
            WaitForSingleObject(g_state.clientPipeThread, 2000);
            CloseHandle(g_state.clientPipeThread);
        }
        if (g_state.serverPipeThread) {
            WaitForSingleObject(g_state.serverPipeThread, 2000);
            CloseHandle(g_state.serverPipeThread);
        }
        if (g_state.clientPipeRead) CloseHandle(g_state.clientPipeRead);
        if (g_state.serverPipeRead) CloseHandle(g_state.serverPipeRead);
        Log_Close();

        /* Free charts */
        Chart_Free(&g_state.clientChart);
        Chart_Free(&g_state.serverChart);
        Chart_Free(&g_state.replayChart);
        Replay_Free(&g_state.replay);

        /* Free GDI+ */
        Chart_GdiplusShutdown();

        /* Free fonts */
        if (s_hFontNormal) DeleteObject(s_hFontNormal);
        if (s_hFontBold) DeleteObject(s_hFontBold);
        if (s_hFontMono) DeleteObject(s_hFontMono);
        if (s_hFontGroup) DeleteObject(s_hFontGroup);
        if (s_hFontGroupNormal) DeleteObject(s_hFontGroupNormal);

        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

/* ===== Replay ===== */

/* 回放显示单位档位：与客户端/服务端页「显示单位」下拉逐字一致（9 项，索引 8 = auto）。
   三处下拉共用同一份语义，但各页独立持有控件，故表在这里再声明一次
   （客户端页同款表见本文件 StartClient 内的映射）。 */
static const char*  kReplayUnits[]  = {"Kb/s","Mb/s","Gb/s","Tb/s",
                                       "KB/s","MB/s","GB/s","TB/s",
                                       "Mb/s"};
static const double kReplayScales[] = {1e3, 1e6, 1e9, 1e12,
                                       8e3, 8e6, 8e9, 8e12,
                                       1e6};

static void Replay_ReadSpeed(void)
{
    static const double speeds[5] = { 1.0, 1.5, 2.0, 3.0, 4.0 };
    int sel = (int)SendMessageW(g_state.hReplaySpeed, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel > 4) sel = 0;
    g_state.replaySpeed = speeds[sel];
}

/* 把当前档位写进 ReplayData。Replay_Seek 每次定位都会把 unit/yScale 拷进图表，
   所以下一次 Replay_ApplyIndex 就会按新单位重画 Y 轴与整条曲线。 */
static void Replay_ReadUnit(void)
{
    int sel = (int)SendMessageW(g_state.hReplayUnit, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= REPLAY_UNIT_COUNT) sel = REPLAY_UNIT_AUTO;
    strncpy(g_state.replay.unit, kReplayUnits[sel], sizeof(g_state.replay.unit) - 1);
    g_state.replay.unit[sizeof(g_state.replay.unit) - 1] = '\0';
    g_state.replay.yScale = kReplayScales[sel];
}

/* 回放「当前:」速率按选定档位换算（D1-A）。
   刻意不走共用 FormatSpeed —— 那是硬编码自动三档，且汇总框依赖它
   （用户 2026-10-16 决定汇总框不跟随 -f），改它会波及全局。 */
static void Replay_FormatRate(double bps, char* out, int outLen)
{
    double sc = g_state.replay.yScale;
    if (!out || outLen <= 0) return;
    if (bps < 0) bps = 0;
    if (sc <= 0) sc = 1e6;
    snprintf(out, outLen, "%.2f %s", bps / sc, g_state.replay.unit);
}

static void Replay_UpdateSliderRange(void)
{
    if (!g_state.hReplaySlider) return;
    int max = g_state.replayLoaded ? g_state.replay.count - 1 : 0;
    SendMessageW(g_state.hReplaySlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, max));
    SendMessageW(g_state.hReplaySlider, TBM_SETPOS, TRUE, (LPARAM)g_state.replayIdx);
    EnableWindow(g_state.hReplaySlider, g_state.replayLoaded ? TRUE : FALSE);
    EnableWindow(g_state.hReplayUnit, g_state.replayLoaded ? TRUE : FALSE);
}

static void Replay_UpdateLabels(void)
{
    wchar_t w[512];
    if (!g_state.replayLoaded) {
        SetWindowTextW(g_state.hReplayTime, T(STR_REPLAY_TIME_INIT));
        SetWindowTextW(g_state.hReplayValues, L"");
        return;
    }
    const ReplaySample* s = &g_state.replay.samples[g_state.replayIdx];
    swprintf(w, 512, T(STR_FMT_REPLAY_TIME), s->time, g_state.replay.duration);
    SetWindowTextW(g_state.hReplayTime, w);

    /* Bottom value row: 带宽 (+ UDP extras when the axis carries them) */
    char spd[64];
    Replay_FormatRate(s->sumBps, spd, sizeof(spd));
    wchar_t wspd[64];
    MultiByteToWideChar(CP_UTF8, 0, spd, -1, wspd, 64);
    if (g_state.replay.axis == RA_JITTER_LOSS)
        swprintf(w, 512, T(STR_FMT_CUR_UDP),
                 wspd, s->jitter, s->lossRate);
    else if (g_state.replay.axis == RA_DATAGRAMS)
        swprintf(w, 512, T(STR_FMT_CUR_DGRAM), wspd, s->datagrams);
    else
        swprintf(w, 512, T(STR_FMT_CUR_PLAIN), wspd);
    SetWindowTextW(g_state.hReplayValues, w);
}

static void Replay_ApplyIndex(int idx, bool repaint)
{
    if (!g_state.replayLoaded) return;
    if (idx < 0) idx = 0;
    if (idx > g_state.replay.count - 1) idx = g_state.replay.count - 1;
    g_state.replayIdx = idx;
    Replay_Seek(&g_state.replay, &g_state.replayChart, idx);
    SendMessageW(g_state.hReplaySlider, TBM_SETPOS, TRUE, (LPARAM)idx);
    Replay_UpdateLabels();
    if (repaint) InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
}

static void Replay_SetPlaying(bool playing)
{
    g_state.replayPlaying = playing;
    if (playing) SetTimer(g_state.hMain, REPLAY_TIMER_ID, 100, NULL);
    else KillTimer(g_state.hMain, REPLAY_TIMER_ID);
    EnableWindow(g_state.hReplayPlay, playing ? FALSE : TRUE);
    EnableWindow(g_state.hReplayStop, playing ? TRUE : FALSE);
}

/* One playback frame: advance the virtual playhead and seek */
static void Replay_Tick(void)
{
    if (!g_state.replayLoaded || !g_state.replayPlaying) return;
    g_state.replayVt += 0.1 * g_state.replaySpeed;

    /* First sample whose time >= vt */
    int n = g_state.replay.count;
    int idx = n - 1;
    for (int i = 0; i < n; i++) {
        if (g_state.replay.samples[i].time >= g_state.replayVt - 1e-9) {
            idx = i;
            break;
        }
    }
    Replay_ApplyIndex(idx, true);
    if (idx >= n - 1) Replay_SetPlaying(false); /* end reached → reset buttons */
}

static void OnReplayBrowse(void)
{
    wchar_t wpath[MAX_PATH] = {0};
    char cur[MAX_PATH];
    if (UI_GetEditText(g_state.hReplayPath, cur, MAX_PATH) > 0)
        MultiByteToWideChar(CP_UTF8, 0, cur, -1, wpath, MAX_PATH);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_state.hMain;
    ofn.lpstrFile = wpath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = T(STR_FILTER_LOG);
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = T(STR_DLG_TITLE_SELECT_LOG);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn)) {
        char path[MAX_PATH] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH, NULL, NULL);
        SetEditUtf8(g_state.hReplayPath, path);
        /* Choosing a file IS loading it — there is no separate 载入 button. */
        OnReplayLoad();
    }
}

static void OnReplayLoad(void)
{
    char path[MAX_PATH];
    UI_GetEditText(g_state.hReplayPath, path, MAX_PATH);
    if (!path[0]) {
        SetWindowTextW(g_state.hReplayInfo, T(STR_MSG_SELECT_LOG_FILE));
        return;
    }

    Replay_SetPlaying(false);
    char err[256] = {0};
    if (!Replay_LoadFile(path, &g_state.replay, err, sizeof(err))) {
        g_state.replayLoaded = false;
        g_state.replayIdx = 0;
        Chart_Clear(&g_state.replayChart);
        Replay_UpdateSliderRange();
        Replay_UpdateLabels();
        InvalidateRect(g_state.hReplayChartWnd, NULL, TRUE);
        wchar_t werr[300], wmsg[360];
        MultiByteToWideChar(CP_UTF8, 0, err, -1, werr, 300);
        swprintf(wmsg, 360, T(STR_ERR_LOAD_FAILED), werr);
        SetWindowTextW(g_state.hReplayInfo, wmsg);
        return;
    }

    g_state.replayLoaded = true;
    /* Curve color follows the log role: client blue / server green */
    g_state.replayChart.lineColor = g_state.replay.isClient ? COLOR_CLIENT_LINE
                                                            : COLOR_SERVER_LINE;
    g_state.replayChart.areaColor = g_state.replay.isClient ? COLOR_CLIENT_AREA
                                                            : COLOR_SERVER_AREA;
    Replay_ReadSpeed();
    Replay_ReadUnit();   /* 让已保存的档位在加载时立即生效（默认 auto = Mb/s） */

    /* Decision #2: position at the end after load (full curve visible) */
    g_state.replayIdx = g_state.replay.count - 1;
    Replay_UpdateSliderRange();
    Replay_ApplyIndex(g_state.replayIdx, true);

    /* Info row: 来源 | 格式 · 切片数 · 时长 (+ 多流信息) */
    wchar_t w[256];
    int sc = g_state.replay.samples[g_state.replay.count - 1].streamCount;
    if (sc > 0) {
        if (g_state.replay.streamOverflow)
            swprintf(w, 256, T(STR_FMT_REPLAY_INFO_OVER),
                     g_state.replay.isClient ? T(STR_TAB_CLIENT) : T(STR_TAB_SERVER),
                     g_state.replay.isJson ? T(STR_MODE_JSON) : T(STR_MODE_TEXT),
                     g_state.replay.count, g_state.replay.duration, sc);
        else
            swprintf(w, 256, T(STR_FMT_REPLAY_INFO),
                     g_state.replay.isClient ? T(STR_TAB_CLIENT) : T(STR_TAB_SERVER),
                     g_state.replay.isJson ? T(STR_MODE_JSON) : T(STR_MODE_TEXT),
                     g_state.replay.count, g_state.replay.duration, sc);
    } else {
        swprintf(w, 256, T(STR_FMT_REPLAY_INFO_SHORT),
                 g_state.replay.isClient ? T(STR_TAB_CLIENT) : T(STR_TAB_SERVER),
                 g_state.replay.isJson ? T(STR_MODE_JSON) : T(STR_MODE_TEXT),
                 g_state.replay.count, g_state.replay.duration);
    }
    SetWindowTextW(g_state.hReplayInfo, w);
}

/* ===== AutoTest mode ===== */
/* When --autotest is passed on command line, the GUI window is created hidden.
   It reads test cases from test/autotest.ini, runs each case (set controls →
   start iperf3 → wait for completion → dump summary + raw output to files),
   then exits. The Python orchestrator (test/run_autotest.py) generates the
   INI, launches the exe, and produces an HTML report from the dumped files. */

/* Dump a window's text to a UTF-8 file (with BOM) */
static void AutoTest_DumpWindowText(HWND hwnd, const char* filePath)
{
    if (!hwnd || !filePath) return;
    int len = GetWindowTextLengthW(hwnd);
    if (len < 0) len = 0;
    wchar_t* wbuf = (wchar_t*)malloc((size_t)(len + 1) * sizeof(wchar_t));
    if (!wbuf) return;
    GetWindowTextW(hwnd, wbuf, len + 1);
    wbuf[len] = 0;

    /* Convert to UTF-8 */
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
    if (u8len <= 0) { free(wbuf); return; }
    char* u8buf = (char*)malloc(u8len);
    if (!u8buf) { free(wbuf); return; }
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, u8buf, u8len, NULL, NULL);

    FILE* fp = fopen(filePath, "wb");
    if (fp) {
        fwrite("\xEF\xBB\xBF", 1, 3, fp); /* UTF-8 BOM */
        fwrite(u8buf, 1, u8len - 1, fp);  /* exclude null terminator */
        fclose(fp);
    }
    free(u8buf);
    free(wbuf);
}

/* Set a combo box by format code string */
static int FormatCodeToIndex(const char* code)
{
    static const char* codes[] = {"k","m","g","t","K","M","G","T"};
    if (!code || !code[0]) return 8; /* auto */
    for (int i = 0; i < 8; i++) {
        if (stricmp(code, codes[i]) == 0) return i;
    }
    return 8;
}

/* Run one test case: read params from INI section [case_NN], set UI controls,
   start iperf3, and wait. The WM_PIPE_DONE handler will call AutoTest_DumpAndAdvance. */
static void AutoTest_StartNext(void)
{
    char ini[MAX_PATH];
    GetSettingsPath(ini, sizeof(ini));
    /* Replace settings.ini with test/autotest.ini path */
    char* p = strrchr(ini, '\\');
    if (p) {
        *p = 0;
        snprintf(ini, sizeof(ini), "%s\\test\\autotest.ini", ini);
    }

    if (s_autotestCaseIdx >= s_autotestCaseCount) {
        /* All done */
        PostMessageW(g_state.hMain, WM_AUTOTEST_DONE, 0, 0);
        return;
    }

    char section[32];
    snprintf(section, sizeof(section), "case_%02d", s_autotestCaseIdx);

    /* Read test case params */
    char mode[16], proto[8], fmt[8], ip[64], port[16], duration[16], interval[16];
    char bw[32], extra[256], num[64], omit[16];
    int reverse, bidir, json, getserver, oneoff, verbose, parallel;

    GetPrivateProfileStringA(section, "mode", "client", mode, sizeof(mode), ini);
    GetPrivateProfileStringA(section, "proto", "tcp", proto, sizeof(proto), ini);
    GetPrivateProfileStringA(section, "format", "", fmt, sizeof(fmt), ini);
    GetPrivateProfileStringA(section, "ip", s_autotestServerIP, ip, sizeof(ip), ini);
    GetPrivateProfileStringA(section, "port", "5201", port, sizeof(port), ini);
    GetPrivateProfileStringA(section, "duration", "5", duration, sizeof(duration), ini);
    GetPrivateProfileStringA(section, "interval", "1", interval, sizeof(interval), ini);
    GetPrivateProfileStringA(section, "bw", "", bw, sizeof(bw), ini);
    GetPrivateProfileStringA(section, "extra", "", extra, sizeof(extra), ini);
    GetPrivateProfileStringA(section, "num", "", num, sizeof(num), ini);
    GetPrivateProfileStringA(section, "omit", "0", omit, sizeof(omit), ini);
    reverse    = GetPrivateProfileIntA(section, "reverse", 0, ini);
    bidir      = GetPrivateProfileIntA(section, "bidir", 0, ini);
    json       = GetPrivateProfileIntA(section, "json", 0, ini);
    getserver  = GetPrivateProfileIntA(section, "getserver", 0, ini);
    oneoff     = GetPrivateProfileIntA(section, "oneoff", 0, ini);
    verbose    = GetPrivateProfileIntA(section, "verbose", 0, ini);
    /* 并发流数必须显式指定：不设的话会沿用 settings.ini 里用户上次的值，
       用例结果就跟着用户配置漂移了。下限 1、上限 8（与多流下拉的项数一致）。 */
    parallel   = GetPrivateProfileIntA(section, "parallel", 1, ini);
    if (parallel < 1) parallel = 1;
    if (parallel > 8) parallel = 8;

    /* Set UI controls */
    if (stricmp(mode, "server") == 0) {
        /* Server test — kill any existing iperf3.exe first to free port 5201 */
        {
            STARTUPINFOA si2 = { sizeof(si2) };
            si2.dwFlags = STARTF_USESHOWWINDOW;
            si2.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi2 = {0};
            CreateProcessA(NULL, "taskkill /IM iperf3.exe /F",
                           NULL, NULL, FALSE,
                           CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2);
            if (pi2.hProcess) {
                WaitForSingleObject(pi2.hProcess, 3000);
                CloseHandle(pi2.hProcess);
            }
            if (pi2.hThread) CloseHandle(pi2.hThread);
        }

        /* Server test */
        SetEditUtf8(g_state.hServerPort, port);
        SetEditUtf8(g_state.hServerExtra, extra);
        SendMessageW(g_state.hServerFormat, CB_SETCURSEL, FormatCodeToIndex(fmt), 0);
        SendMessageW(g_state.hServerOneOff, BM_SETCHECK, oneoff ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_state.hServerVerbose, BM_SETCHECK, verbose ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_state.hServerJson, BM_SETCHECK, json ? BST_CHECKED : BST_UNCHECKED, 0);
        g_state.serverOneOff = oneoff != 0;
        g_state.serverVerbose = verbose != 0;
        g_state.serverJson = json != 0;

        /* Start server — it runs until killed or one-off completes */
        OnStartServer();

        /* Launch a separate iperf3 client to connect to this server,
           so the server can complete its test and exit (one-off) or
           produce output (non-one-off with timer stop). */
        {
            char iperf3Path[MAX_PATH];
            GetModuleFileNameA(NULL, iperf3Path, MAX_PATH);
            /* Replace exe name with tools\iperf3.exe */
            char* slash = strrchr(iperf3Path, '\\');
            if (slash) {
                *slash = 0;
                strncat_s(iperf3Path, MAX_PATH, "\\tools\\iperf3.exe", _TRUNCATE);
            }

            char clientCmd[512];
            const char* pArg = (stricmp(proto, "udp") == 0) ? "-u" : "";
            snprintf(clientCmd, sizeof(clientCmd),
                     "\"%s\" -c 127.0.0.1 -p %s -t %s %s --forceflush",
                     iperf3Path, port, duration, pArg);

            /* Cygwin iperf3 crashes when stdout is NUL/invalid,
               causing "unknown control message" on the server side.
               Use an anonymous pipe instead — we don't read it, but
               iperf3's fd layer works correctly with a pipe. */
            HANDLE hClientPipeR = NULL, hClientPipeW = NULL;
            {
                SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
                CreatePipe(&hClientPipeR, &hClientPipeW, &sa, 65536);
                SetHandleInformation(hClientPipeR, HANDLE_FLAG_INHERIT, 0);
            }

            STARTUPINFOA si = { sizeof(si) };
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            si.hStdInput = NULL;
            si.hStdOutput = hClientPipeW;
            si.hStdError = hClientPipeW;
            PROCESS_INFORMATION pi = {0};
            CreateProcessA(NULL, clientCmd, NULL, NULL, TRUE,
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            if (pi.hProcess) CloseHandle(pi.hProcess);
            if (pi.hThread) CloseHandle(pi.hThread);
            if (hClientPipeW) CloseHandle(hClientPipeW);
            if (hClientPipeR) CloseHandle(hClientPipeR);
        }

        /* For server mode without one-off, we need to stop after a timeout.
           For one-off, the server exits after one test. */
        if (!oneoff) {
            /* Set a timer to stop after duration+5 seconds */
            SetTimer(g_state.hMain, 999, (atoi(duration) + 5) * 1000, NULL);
        }
        /* WM_PIPE_DONE will trigger AutoTest_DumpAndAdvance */
    } else {
        /* Client test */
        SetEditUtf8(g_state.hClientIP, ip);
        SetEditUtf8(g_state.hClientPort, port);
        SendMessageW(g_state.hClientProto, CB_SETCURSEL, (stricmp(proto, "udp") == 0) ? 1 : 0, 0);
        SetEditUtf8(g_state.hClientDuration, duration);
        SetEditUtf8(g_state.hClientInterval, interval);
        SetEditUtf8(g_state.hClientNum, num);
        SetEditUtf8(g_state.hClientOmit, omit);
        SetEditUtf8(g_state.hClientBw, bw);
        SetEditUtf8(g_state.hClientExtra, extra);
        SendMessageW(g_state.hClientFormat, CB_SETCURSEL, FormatCodeToIndex(fmt), 0);
        SendMessageW(g_state.hClientReverse, BM_SETCHECK, reverse ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_state.hClientBidir, BM_SETCHECK, bidir ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_state.hClientJson, BM_SETCHECK, json ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_state.hClientGetServer, BM_SETCHECK, getserver ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_state.hClientParallel, CB_SETCURSEL, parallel - 1, 0);
        g_state.clientReverse = reverse != 0;
        g_state.clientBidir = bidir != 0;
        g_state.clientJson = json != 0;
        g_state.clientGetServer = getserver != 0;
        UpdateProtocolDependentControls();

        /* Start client test */
        OnStartClient();
        /* WM_PIPE_DONE will trigger AutoTest_DumpAndAdvance */
    }
}

/* Dump summary + raw output to files, then advance to next case */
static void AutoTest_DumpAndAdvance(int mode)
{
    /* Dump summary box text */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\case_%02d_summary.txt",
             s_autotestResultsDir, s_autotestCaseIdx);

    HWND hSummary = (mode == MODE_CLIENT) ? g_state.hClientSummaryBox : g_state.hServerSummaryBox;
    AutoTest_DumpWindowText(hSummary, path);

    /* Dump raw output box text */
    snprintf(path, sizeof(path), "%s\\case_%02d_raw.txt",
             s_autotestResultsDir, s_autotestCaseIdx);
    HWND hOutput = (mode == MODE_CLIENT) ? g_state.hClientOutput : g_state.hServerOutput;
    AutoTest_DumpWindowText(hOutput, path);

    /* Dump command string */
    snprintf(path, sizeof(path), "%s\\case_%02d_cmd.txt",
             s_autotestResultsDir, s_autotestCaseIdx);
    HWND hCmd = (mode == MODE_CLIENT) ? g_state.hClientCmd : g_state.hServerCmd;
    AutoTest_DumpWindowText(hCmd, path);

    /* Advance */
    s_autotestCaseIdx++;
    PostMessageW(g_state.hMain, WM_AUTOTEST_NEXT, 0, 0);
}

/* ===== WinMain ===== */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR lpCmdLine, int nShow)
{
    /* Check for --autotest command line flag */
    bool autotest = (lpCmdLine && wcsstr(lpCmdLine, L"--autotest") != NULL);

    /* Init Common Controls */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    /* Register main window class */
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; /* we paint our own bg */
    /* Pick the best-matching frame out of the multi-size app.ico instead of
       letting the shell scale the default 32x32 frame down to 16x16.
       WNDCLASS has no hIconSm — the small icon is set on the window below. */
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
                                 IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON), LR_SHARED);
    RegisterClassW(&wc);

    /* Register panel class (custom container that forwards child notifications) */
    WNDCLASSW wcPanel = {0};
    wcPanel.lpfnWndProc = PanelWndProc;
    wcPanel.hInstance = hInstance;
    wcPanel.lpszClassName = PANEL_CLASS_NAME;
    wcPanel.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcPanel.hbrBackground = NULL; /* we paint our own bg in WM_ERASEBKGND */
    RegisterClassW(&wcPanel);

    /* Create main window */
    HWND hwnd = CreateWindowExW(0, MAIN_CLASS_NAME, L"iperf3 GUI V1.1",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 840,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    /* Small icon for the title bar / taskbar: load the native 16px frame so it
       stays crisp instead of being downscaled from the 32px one. */
    SendMessageW(hwnd, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
                                    IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                    GetSystemMetrics(SM_CYICON), LR_SHARED));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
                                    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON), LR_SHARED));

    if (autotest) {
        /* AutoTest mode: don't show window, set up test state */
        s_autotestMode = true;

        /* Determine results directory: <exe_dir>\test\results\ */
        wchar_t wexe[MAX_PATH];
        GetModuleFileNameW(NULL, wexe, MAX_PATH);
        wchar_t* slash = wcsrchr(wexe, L'\\');
        if (slash) *slash = 0;
        char exeDir[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wexe, -1, exeDir, MAX_PATH, NULL, NULL);
        snprintf(s_autotestResultsDir, MAX_PATH, "%s\\test\\results", exeDir);

        /* Create results directory if it doesn't exist */
        CreateDirectoryA(s_autotestResultsDir, NULL);

        /* Read case count from test/autotest.ini */
        char iniPath[MAX_PATH];
        snprintf(iniPath, sizeof(iniPath), "%s\\test\\autotest.ini", exeDir);
        s_autotestCaseCount = GetPrivateProfileIntA("autotest", "case_count", 0, iniPath);

        /* Read server IP from autotest.ini */
        char sip[64];
        GetPrivateProfileStringA("autotest", "server_ip", "127.0.0.1",
                                 sip, sizeof(sip), iniPath);
        strncpy(s_autotestServerIP, sip, sizeof(s_autotestServerIP) - 1);
        s_autotestServerIP[sizeof(s_autotestServerIP) - 1] = 0;

        /* Disable logging for autotest */
        g_state.logEnabled = false;
        SendMessageW(g_state.hLogEnable, BM_SETCHECK, BST_UNCHECKED, 0);

        if (s_autotestCaseCount <= 0) {
            /* No test cases — exit immediately */
            MessageBoxA(NULL, "No test cases found in test/autotest.ini",
                        "AutoTest", MB_ICONERROR);
            DestroyWindow(hwnd);
        } else {
            /* Start first test case after a short delay (let window init finish) */
            PostMessageW(hwnd, WM_AUTOTEST_NEXT, 0, 0);
        }
    } else {
        ShowWindow(hwnd, nShow);
        UpdateWindow(hwnd);
    }

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        /* Allow Tab key navigation */
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return (int)msg.wParam;
}
