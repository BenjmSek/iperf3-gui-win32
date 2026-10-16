#ifndef IPERF3_H
#define IPERF3_H

#include <windows.h>
#include <stdbool.h>

/* ===== Data structures ===== */

typedef struct {
    int   streamId;
    double intervalStart;
    double intervalEnd;
    char  transfer[64];
    char  bandwidth[64];
    double bps;
    double bytes;
    char  formatSpeed[32];
    /* TCP extras */
    int   retransmits;
    char  cwnd[64];
    /* UDP extras */
    double jitter;
    int   lostPackets;
    int   totalPackets;
    double lossRate;
    /* direction */
    char  direction[16];  /* "sender" / "receiver" / "" */
    /* role (bidir only): "TX-C", "RX-C", "TX-S", "RX-S", or "" */
    char  role[16];
    /* type */
    bool  isSummary;
} Iperf3Data;

typedef enum {
    MODE_CLIENT = 0,
    MODE_SERVER = 1
} IperfMode;

/* ===== API ===== */

/* Find iperf3.exe in PATH or common locations.
   Returns malloc'd string (caller must free) or NULL. */
char* FindIperf3(void);

/* Start iperf3 in client mode.
   Returns true on success.
   cmdStr (if non-NULL) receives the display command string (without --forceflush).
   hPipeRead (if non-NULL) receives the read end of the anonymous pipe that
   captures iperf3's stdout+stderr (combined). The caller must CloseHandle it. */
bool StartIperf3Client(const char* iperf3Path,
                       const char* serverIP, const char* port,
                       const char* protocol, int duration, int interval,
                       int parallel,
                       bool reverse, bool bidir,
                       const char* bandwidth, const char* extraArgs,
                       const char* format,       /* -f: "k"/"m"/"g"/"t"/"K"/"M"/"G"/"T" or NULL(auto) */
                       const char* numBytes,     /* -n: bytes to transmit, or NULL */
                       int omit,                 /* -O: seconds to omit, 0=skip */
                       bool json,                /* -J */
                       bool getServerOutput,     /* --get-server-output */
                       /* outputs: */
                       char* cmdStr, int cmdStrLen,
                       HANDLE* hPipeRead);

/* Start iperf3 in server mode.
   Returns true on success.
   cmdStr/hPipeRead same as client. */
bool StartIperf3Server(const char* iperf3Path,
                       const char* port, bool oneOff, bool verbose,
                       const char* format,       /* -f: same as client */
                       const char* extraArgs,    /* manual extra args */
                       bool json,                /* -J */
                       const char* afVersion,    /* "-4", "-6", or NULL/"" for both */
                       char* cmdStr, int cmdStrLen,
                       HANDLE* hPipeRead);

/* Stop client or server process (TerminateProcess + wait) */
void StopIperf3Client(void);
void StopIperf3Server(void);

/* Kill any process listening on the given port (IP Helper API) */
void KillPortListeners(const char* port);

/* Parse a single iperf3 text output line into Iperf3Data.
   Returns true if parsed successfully (line matched data pattern). */
bool ParseIperf3Line(const char* line, bool isSummary, Iperf3Data* out);

/* ===== JSON mode parser ===== */

/* JSON parser state machine */
typedef enum {
    JS_IDLE,            /* looking for "sum"/"sum_sent"/"sum_received" */
    JS_IN_INTERVAL_SUM, /* inside "sum":{ of an interval */
    JS_IN_SUM_SENT,     /* inside "sum_sent":{ */
    JS_IN_SUM_RECEIVED  /* inside "sum_received":{ */
} JsonState;

typedef struct {
    JsonState state;
    bool   inServerOutput;  /* inside "server_output_json" */
    /* Accumulated fields for current summary object */
    double bytes;
    double bps;
    int    retransmits;
    double jitter;
    int    lostPackets;
    int    totalPackets;
    double lossRate;
    bool   isSender;
    double intervalEnd;
} JsonParseState;

/* Parse a single JSON output line.
   For interval lines: calls callback cbAddPoint(time, bps) if non-NULL.
   For summary lines: fills outSummary and returns true (caller checks direction).
   Returns true if a summary object was completed. */
bool ParseIperf3JsonLine(const char* line, JsonParseState* jps,
                         Iperf3Data* outSummary,
                         double* pTime, double* pBps, bool* pIsInterval,
                         double* pJitter, double* pLossRate,
                         double* pDatagrams);

/* JSON helper: find "key" : <number> inside a JSON fragment.
   Shared with the replay JSON scanner (replay.c). */
bool JsonGetDouble(const char* line, const char* key, double* out);
bool JsonGetInt(const char* line, const char* key, int* out);

/* Format bytes → human readable transfer string (e.g. "1.23 MBytes") */
void FormatBytes(double bytes, char* out, int outLen);

/* Format bps → human readable string */
void FormatSpeed(double bps, char* out, int outLen);

/* Clean control chars from a line (in-place) */
void CleanOutput(char* line);

/* Get the process handle for client/server (for exit code detection) */
HANDLE GetClientProcess(void);
HANDLE GetServerProcess(void);
DWORD  GetClientPID(void);
DWORD  GetServerPID(void);

/* Release the stale process handle/PID after the process exited by itself.
   MUST be called before starting the same mode again, or the start
   functions will report "already running" and fail. */
void ResetClientProcess(void);
void ResetServerProcess(void);

/* Check if process has exited and get exit code. Returns true if exited. */
bool CheckProcessExited(HANDLE hProcess, DWORD* exitCode);

/* Check iperf3 version supports --forceflush (>= 3.7).
   Runs "iperf3.exe --version" and parses the version string.
   Returns true if version is sufficient, false otherwise.
   If versionStr is non-NULL, receives the parsed version string. */
bool CheckIperf3Version(const char* iperf3Path, char* versionStr, int versionStrLen);

#endif /* IPERF3_H */
