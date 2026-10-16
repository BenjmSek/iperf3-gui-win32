#ifndef UI_H
#define UI_H

#include <windows.h>
#include "chart.h"
#include "iperf3.h"
#include "replay.h"

typedef struct {
    /* Tab state */
    int activeTab;

    /* Status */
    bool clientRunning;
    bool serverRunning;
    char currentServerIP[64];
    char iperf3Path[MAX_PATH];

    /* 右上角消息输出框：显示 iperf3 版本（始终）+ 错误（程序路径无效 / 版本过低 / 日志路径无效） */
    char msgErrorIperf3[256];
    char msgErrorLog[256];
    char iperf3Ver[32];   /* 探测到的 iperf3 版本号（如 "3.17"）；空串表示未检测到 */

    /* Client UI state (synced from checkboxes) */
    bool clientReverse;
    bool clientBidir;
    bool clientJson;
    bool clientGetServer;
    bool clientIsUdp;   /* true if current test is UDP (for chart dual-axis) */

    /* Server params */
    char serverPort[16];
    bool serverOneOff;
    bool serverVerbose;
    bool serverJson;
    bool serverIsUdp;   /* true if current connection is UDP */

    /* Client output */
    bool  clientAutoScroll;
    HANDLE clientPipeRead;        /* pipe read end for stdout+stderr */
    HANDLE clientPipeThread;      /* thread that reads the pipe */

    /* Server output */
    bool  serverAutoScroll;
    HANDLE serverPipeRead;
    HANDLE serverPipeThread;

    /* Client summary blocks: up to 2 blocks (bidir), each with sender + receiver */
    Iperf3Data clientSumSender[2];
    Iperf3Data clientSumReceiver[2];
    bool clientHasSender[2];
    bool clientHasReceiver[2];
    int clientSummaryBlockCount;     /* 0=none, 1=single, 2=bidir */

    /* Server summary blocks */
    Iperf3Data serverSumSender[2];
    Iperf3Data serverSumReceiver[2];
    bool serverHasSender[2];
    bool serverHasReceiver[2];
    int serverSummaryBlockCount;

    /* Parsing state: tracks which summary block we're in (0 or 1) */
    bool serverPastSeparator;
    bool clientPastSeparator;
    int clientSummaryBlockIdx;       /* 0 for first block, 1 for second */
    int serverSummaryBlockIdx;

    /* --get-server-output: client stdout contains "Server output:" section */
    bool clientHasServerOutput;
    bool clientInServerOutput;
    bool clientServerPastSeparator;
    int  clientServerSummaryBlockCount;
    int  clientServerSummaryBlockIdx;
    Iperf3Data clientServerSumSender[4];
    Iperf3Data clientServerSumReceiver[4];
    bool clientServerHasSender[4];
    bool clientServerHasReceiver[4];

    /* Window handles */
    HWND hMain;
    HWND hTabClient, hTabServer, hTabSettings, hTabReplay;
    HWND hSaveConfig, hLoadConfig, hRestoreDefault;
    HWND hClientPanel, hServerPanel, hSettingsPanel, hReplayPanel;
    HWND hStatusRight;
    /* Status bar left segments: 客户端：●运行   服务端：●空闲
       The dot label is colored by state (red=running, green=idle). */
    HWND hStatusClientTag, hStatusClientDot, hStatusClientVal;
    HWND hStatusServerTag, hStatusServerDot, hStatusServerVal;

    /* Client controls */
    HWND hClientIP, hClientPort, hClientProto, hClientDuration,
         hClientInterval, hClientBw, hClientExtra,
         hClientFormat, hClientNum, hClientOmit,
         hClientReverse, hClientBidir, hClientJson, hClientGetServer,
         hClientParallel,
         hClientStart, hClientStop,
         hClientCmd, hClientOutput,
         hClientClear, hClientCopy, hClientAutoScroll, hClientCopyHint,
         hClientChartWnd, hClientSummaryBox;

    /* Server controls */
    HWND hServerPort, hServerOneOff, hServerVerbose, hServerJson,
         hServerFormat, hServerExtra, hServerAf,
         hServerStart, hServerStop,
         hServerCmd, hServerOutput,
         hServerClear, hServerCopy, hServerAutoScroll, hServerCopyHint,
         hServerChartWnd, hServerSummaryBox;

    /* Settings controls */
    HWND hSettingsPath, hSettingsBrowse, hSettingsDetect;
    HWND hLicense;      /* 「License」按钮 -> MIT License 授权声明对话框（方案 §6） */
    HWND hHelpIperf3;   /* 「说明」按钮 -> iperf3 下载与自动检测说明对话框 */

    /* Log controls */
    HWND hLogEnable, hLogDir, hLogBrowse, hLogNaming, hLogMaxSize, hLogNamingHelp;

    /* Replay controls */
    HWND hReplayPath, hReplayBrowse, hReplayInfo,
         hReplaySlider, hReplayTime, hReplayPlay, hReplayStop,
         hReplaySpeed, hReplayUnit, hReplayValues, hReplayChartWnd;

    /* Replay state */
    ReplayData replay;      /* loaded log samples */
    bool  replayLoaded;     /* a log is loaded and the slider is usable */
    bool  replayPlaying;    /* playback timer running */
    int   replayIdx;        /* current sample index (slider position) */
    double replaySpeed;     /* 1.0 / 1.5 / 2.0 / 3.0 / 4.0 */
    double replayVt;        /* virtual playhead time (seconds), decoupled from interval */

    /* Log state */
    bool  logEnabled;
    int   logMaxSize;       /* KB */
    FILE* logFile;          /* current open log file handle */
    char  logFilePath[MAX_PATH]; /* full path to current log file */

    /* Chart GDI+ data */
    ChartData clientChart;
    ChartData serverChart;
    ChartData replayChart;

    /* Live multi-stream interval tracking (fixes -P n sawtooth: one point
       per interval). Maps an iperf3 streamId → stable slot per chart. */
    int liveSeenIds[2][CHART_MAX_STREAMS];
    int liveSeenCount[2];

    /* "显示各流" default (settings panel; persisted to settings.ini) */
    bool showStreams;
    HWND hShowStreams;

    /* "隐藏流汇总" (settings panel; persisted to settings.ini [Chart])
       仅当 showStreams == true 时生效，且控件在该条件下才可点。 */
    bool hideStreamSum;
    HWND hHideStreamSum;

    /* JSON mode parsing state */
    bool isJsonMode[2];            /* [0]=client, [1]=server */
    JsonParseState jsonState[2];   /* [0]=client, [1]=server */
    double jsonPendingBps[2];      /* accumulated bps for interval */
    double jsonPendingTime[2];     /* accumulated end time for interval */

    /* Dark mode */
    bool isDark;          /* effective dark flag, resolved from themeSel */
    int  themeSel;        /* 0=跟随系统 1=深色 2=浅色 (persisted [UI] Theme) */
} AppState;

/* Global state (single instance app) */
extern AppState g_state;

/* UI functions */
void UI_SwitchTab(int tab);
void UI_UpdateStatus(void);
void UI_AppendOutput(const char* text, int mode);
void UI_ClearOutput(int mode);
void UI_CopyOutput(int mode);
void UI_UpdateChart(int mode);
void UI_UpdateSummary(int mode);
void UI_EnableControls(void);
void UpdateProtocolDependentControls(void);
void UI_AutoDetectPath(void);
void UI_BrowsePath(void);
void UI_ShowLicenseDialog(void);   /* 模态「授权声明」对话框（方案 §6） */
void UI_ShowHelpDialog(void);      /* 模态「说明」对话框（iperf3 下载 + 自动检测） */

/* Global config operations */
void SaveAllConfig(void);
void LoadAllConfig(void);
void RestoreAllDefaults(void);
void InitDefaultConfig(void);  /* write defaults to INI if missing */

/* Get text from an edit control into a buffer. Returns the length. */
int UI_GetEditText(HWND hwnd, char* buf, int bufLen);

#endif /* UI_H */
