/*
 * iperf3.c — iperf3 process management and output parsing
 * Pure Win32 API + C runtime. No external dependencies.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <shlwapi.h>
#include <iphlpapi.h>
/* AF_INET comes from winsock2.h; define it inline to avoid linking ws2_32 */
#ifndef AF_INET
#define AF_INET 2
#endif
#include "iperf3.h"

/* ===== Static state ===== */
static HANDLE s_clientProcess = NULL;
static DWORD  s_clientPID = 0;
static HANDLE s_serverProcess = NULL;
static DWORD  s_serverPID = 0;

/* ===== Find iperf3.exe =====
   查找顺序（先找到先用）：
     1) 本程序所在目录，及其全部子目录
     2) 系统 PATH 环境变量中的各目录（平铺，不递归）
   档 1 用「FIFO 队列」实现按层 BFS ⇒ 浅层优先。这一点是必要的：
   同层内 FindFirstFile 的返回顺序无 API 保证，浅层优先 + 同层按名排序
   才能保证结果稳定可复现（否则深层备份目录里的同名旧文件可能先被选中）。
   junction / symlink（FILE_ATTRIBUTE_REPARSE_POINT）跳过：避免跟随链接跨盘
   扫描或自引用死循环。
   返回绝对路径（malloc，调用方 free）；找不到返回 NULL。 */

/* 可增长目录队列（FIFO = 按层 BFS） */
typedef struct {
    char** items;
    int    count;
    int    cap;
} DirQueue;

static bool DirQueuePush(DirQueue* q, const char* path)
{
    char* dup = _strdup(path);
    if (!dup) return false;
    if (q->count == q->cap) {
        int ncap = q->cap ? q->cap * 2 : 64;
        char** ni = (char**)realloc(q->items, (size_t)ncap * sizeof(char*));
        if (!ni) { free(dup); return false; }
        q->items = ni;
        q->cap = ncap;
    }
    q->items[q->count++] = dup;
    return true;
}

static void DirQueueFree(DirQueue* q)
{
    for (int i = 0; i < q->count; i++) free(q->items[i]);
    free(q->items);
    q->items = NULL;
    q->count = 0;
    q->cap = 0;
}

static int CmpNameAsc(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* 本程序所在目录（绝对路径，无尾反斜杠） */
static bool GetExeDir(char* out, int outLen)
{
    char  buf[MAX_PATH];
    char* p;
    DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) return false;
    p = strrchr(buf, '\\');
    if (!p) return false;
    *p = 0;
    if ((int)strlen(buf) >= outLen) return false;
    strcpy(out, buf);
    return true;
}

/* 档 1：root 及其全部子目录，浅层优先 */
static char* FindInTree(const char* root)
{
    DirQueue q = {0};
    char*    found = NULL;
    char     cand[MAX_PATH];

    if (!DirQueuePush(&q, root)) return NULL;

    for (int head = 0; head < q.count && !found; head++) {
        /* 注意：dir 指向队列元素，后续 DirQueuePush 可能 realloc ⇒
           本循环内只在入队之前使用 dir，入队之后不再触碰。 */
        const char* dir = q.items[head];
        DirQueue kids = {0};
        HANDLE   h;
        WIN32_FIND_DATAA fd;
        char     pattern[MAX_PATH];

        snprintf(cand, sizeof(cand), "%s\\iperf3.exe", dir);
        if (GetFileAttributesA(cand) != INVALID_FILE_ATTRIBUTES) {
            found = _strdup(cand);
            break;
        }

        snprintf(pattern, sizeof(pattern), "%s\\*", dir);
        h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char sub[MAX_PATH];
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (fd.cFileName[0] == '.') continue;                              /* . / .. */
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;  /* junction / symlink */
                if (snprintf(sub, sizeof(sub), "%s\\%s", dir, fd.cFileName) >= (int)sizeof(sub)) continue;
                DirQueuePush(&kids, sub);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        if (kids.count > 1) qsort(kids.items, kids.count, sizeof(char*), CmpNameAsc);
        for (int i = 0; i < kids.count; i++) DirQueuePush(&q, kids.items[i]);
        DirQueueFree(&kids);
    }

    DirQueueFree(&q);
    return found;
}

/* 档 2：PATH 中各目录，平铺，不递归 */
static char* FindInPath(void)
{
    static char pathEnv[32768];
    DWORD n = GetEnvironmentVariableA("PATH", pathEnv, sizeof(pathEnv));
    char* p;

    if (n == 0 || n >= sizeof(pathEnv)) return NULL;

    for (p = pathEnv; *p; ) {
        char*  semi = strchr(p, ';');
        char*  dir  = p;
        char   cand[MAX_PATH];
        size_t len;

        if (semi) *semi = 0;

        while (*dir == ' ' || *dir == '\t' || *dir == '"') dir++;
        len = strlen(dir);
        while (len > 0) {
            char c = dir[len - 1];
            if (c == ' ' || c == '\t' || c == '"' || c == '\\') dir[--len] = 0;
            else break;
        }
        if (len > 0) {
            snprintf(cand, sizeof(cand), "%s\\iperf3.exe", dir);
            if (GetFileAttributesA(cand) != INVALID_FILE_ATTRIBUTES) return _strdup(cand);
        }

        if (!semi) break;
        p = semi + 1;
    }
    return NULL;
}

char* FindIperf3(void)
{
    char exeDir[MAX_PATH];

    /* 1) 本程序所在目录及其全部子目录 */
    if (GetExeDir(exeDir, sizeof(exeDir))) {
        char* r = FindInTree(exeDir);
        if (r) return r;
    }

    /* 2) 系统 PATH 环境变量中的各目录 */
    return FindInPath();
}

/* ===== Build command line for CreateProcess ===== */
static char* BuildCommandLine(const char* exePath, const char* const* args, int argCount)
{
    size_t total = strlen(exePath) + 4; /* quotes + space */
    for (int i = 0; i < argCount; i++) {
        total += strlen(args[i]) + 1;
    }
    total += 16; /* safety */

    char* cmd = (char*)malloc(total);
    if (!cmd) return NULL;
    cmd[0] = 0;

    snprintf(cmd, total, "\"%s\"", exePath);
    for (int i = 0; i < argCount; i++) {
        strcat(cmd, " ");
        strcat(cmd, args[i]);
    }
    return cmd;
}

/* ===== Check iperf3 version supports --forceflush (>= 3.7) ===== */
bool CheckIperf3Version(const char* iperf3Path, char* versionStr, int versionStrLen)
{
    if (!iperf3Path || !*iperf3Path) return false;

    /* Create pipe to capture "iperf3 --version" output */
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 4096)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    PROCESS_INFORMATION pi = {0};

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" --version", iperf3Path);

    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite); /* parent must close write end so ReadFile can hit EOF */
    if (!ok) {
        CloseHandle(hRead);
        return false;
    }
    WaitForSingleObject(pi.hProcess, 5000);

    char buf[4096] = {0};
    DWORD bytesRead = 0;
    ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, NULL);
    buf[bytesRead] = 0;

    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    /* Parse version: look for pattern like "iperf 3.21" or "iperf3 3.21" */
    int major = 0, minor = 0;
    const char* p = strstr(buf, "iperf");
    if (p) {
        p += 5;  /* skip "iperf" */
        /* If next char is '3' followed by whitespace, it's "iperf3" — skip it */
        if (*p == '3' && (p[1] == ' ' || p[1] == '\t')) p++;
        /* Skip whitespace to reach the version number */
        while (*p == ' ' || *p == '\t') p++;
        if (sscanf(p, "%d.%d", &major, &minor) >= 1) {
            if (versionStr && versionStrLen > 0) {
                snprintf(versionStr, versionStrLen, "%d.%d", major, minor);
            }
            /* --forceflush was introduced in iperf3 3.7 */
            if (major > 3 || (major == 3 && minor >= 7)) return true;
        }
    }

    return false;
}

/* ===== Create anonymous pipe for stdout+stderr capture ===== */
/* Creates a single inheritable anonymous pipe. The write end is set as
   both hStdOutput and hStdError for the child process. The read end is
   made non-inheritable so it stays in the parent.
   Returns true on success. */
static bool CreateOutputPipe(HANDLE* phRead, HANDLE* phWrite)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE }; /* inheritable */
    if (!CreatePipe(phRead, phWrite, &sa, 65536)) return false; /* 64KB buffer */
    /* Parent's read end must NOT be inherited by the child */
    SetHandleInformation(*phRead, HANDLE_FLAG_INHERIT, 0);
    return true;
}

/* ===== Start iperf3 client ===== */
bool StartIperf3Client(const char* iperf3Path,
                       const char* serverIP, const char* port,
                       const char* protocol, int duration, int interval,
                       int parallel,
                       bool reverse, bool bidir,
                       const char* bandwidth, const char* extraArgs,
                       const char* format,
                       const char* numBytes,
                       int omit,
                       bool json,
                       bool getServerOutput,
                       char* cmdStr, int cmdStrLen,
                       HANDLE* phPipeRead)
{
    if (s_clientProcess) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return false;
    }

    /* Build args — default values (5201/10/1) are omitted from command line */
    const char* args[32];
    int argc = 0;
    char durStr[32], intStr[32], parStr[32], portStr[32], omitStr[32];

    args[argc++] = "-c";
    args[argc++] = serverIP;

    if (port && *port && strcmp(port, "5201") != 0) {
        strncpy(portStr, port, sizeof(portStr) - 1);
        portStr[sizeof(portStr)-1] = 0;
        args[argc++] = "-p";
        args[argc++] = portStr;
    }
    if (protocol && stricmp(protocol, "udp") == 0) {
        args[argc++] = "-u";
    }
    if (duration > 0 && duration != 10) {
        snprintf(durStr, sizeof(durStr), "%d", duration);
        args[argc++] = "-t";
        args[argc++] = durStr;
    }
    if (interval > 0 && interval != 1) {
        snprintf(intStr, sizeof(intStr), "%d", interval);
        args[argc++] = "-i";
        args[argc++] = intStr;
    }
    if (parallel > 1) {
        snprintf(parStr, sizeof(parStr), "%d", parallel);
        args[argc++] = "-P";
        args[argc++] = parStr;
    }
    if (reverse) args[argc++] = "-R";
    if (bidir) args[argc++] = "--bidir";
    if (bandwidth && *bandwidth && protocol && stricmp(protocol, "udp") == 0) {
        args[argc++] = "-b";
        args[argc++] = bandwidth;
    }
    if (format && *format) {
        args[argc++] = "-f";
        args[argc++] = format;
    }
    if (numBytes && *numBytes) {
        args[argc++] = "-n";
        args[argc++] = numBytes;
    }
    if (omit > 0) {
        snprintf(omitStr, sizeof(omitStr), "%d", omit);
        args[argc++] = "-O";
        args[argc++] = omitStr;
    }
    if (json) args[argc++] = "-J";
    if (getServerOutput) args[argc++] = "--get-server-output";

    /* Save display arg count (before --forceflush) */
    int dispArgc = argc;

    /* Add --forceflush to actual execution args (internal, not shown to user).
       --forceflush forces iperf3 to fflush(stdout) after every interval report,
       which makes the anonymous pipe receive data in real-time. Without it,
       the C runtime switches to full buffering (4KB) when stdout is not a
       console, and the output box stays empty during the test. */
    args[argc++] = "--forceflush";

    /* Build display command: exe + base args + extra args (raw, no parsing).
       --forceflush is NOT included — it's an internal mechanism. */
    if (cmdStr && cmdStrLen > 0) {
        cmdStr[0] = 0;
        snprintf(cmdStr, cmdStrLen, "\"%s\"", iperf3Path);
        for (int i = 0; i < dispArgc; i++) {
            strncat_s(cmdStr, cmdStrLen, " ", 1);
            strncat_s(cmdStr, cmdStrLen, args[i], _TRUNCATE);
        }
        /* Append extra args raw — one leading space, then user text as-is */
        if (extraArgs && *extraArgs) {
            strncat_s(cmdStr, cmdStrLen, " ", 1);
            strncat_s(cmdStr, cmdStrLen, extraArgs, _TRUNCATE);
        }
    }

    /* Build full command line (includes --forceflush) */
    char* fullCmd = BuildCommandLine(iperf3Path, args, argc);
    if (!fullCmd) return false;

    /* Append extra args to actual execution command too (raw, as-is) */
    if (extraArgs && *extraArgs) {
        size_t curLen = strlen(fullCmd);
        size_t extraLen = strlen(extraArgs) + 2;
        char* bigger = (char*)realloc(fullCmd, curLen + extraLen);
        if (bigger) {
            fullCmd = bigger;
            strcat(fullCmd, " ");
            strcat(fullCmd, extraArgs);
        }
    }

    /* Create anonymous pipe for stdout+stderr capture (64KB buffer) */
    HANDLE hPipeRead = NULL, hPipeWrite = NULL;
    if (!CreateOutputPipe(&hPipeRead, &hPipeWrite)) {
        free(fullCmd);
        SetLastError(ERROR_GEN_FAILURE);
        return false;
    }

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = hPipeWrite;  /* stdout → pipe */
    si.hStdError = hPipeWrite;   /* stderr → same pipe (merged) */
    PROCESS_INFORMATION pi = {0};

    BOOL ok = CreateProcessA(NULL, fullCmd, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    DWORD lastErr = GetLastError();
    free(fullCmd);
    /* Close OUR copy of the write handle after CreateProcess.
       The child has its own inherited copy. If we keep ours open, ReadFile
       will never return 0 (EOF) when the child exits. */
    CloseHandle(hPipeWrite);
    if (!ok) {
        CloseHandle(hPipeRead);
        SetLastError(lastErr);
        return false;
    }

    s_clientProcess = pi.hProcess;
    s_clientPID = pi.dwProcessId;
    CloseHandle(pi.hThread);

    if (phPipeRead) *phPipeRead = hPipeRead;
    else CloseHandle(hPipeRead); /* caller doesn't want it — shouldn't happen */

    return true;
}

/* ===== Start iperf3 server ===== */
bool StartIperf3Server(const char* iperf3Path,
                       const char* port, bool oneOff, bool verbose,
                       const char* format,
                       const char* extraArgs,
                       bool json,
                       const char* afVersion,
                       char* cmdStr, int cmdStrLen,
                       HANDLE* phPipeRead)
{
    if (s_serverProcess) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return false;
    }

    const char* args[20];
    int argc = 0;
    char portStr[32];

    args[argc++] = "-s";

    /* Only add -p if non-default port */
    if (port && *port && strcmp(port, "5201") != 0) {
        strncpy(portStr, port, sizeof(portStr) - 1);
        portStr[sizeof(portStr)-1] = 0;
        args[argc++] = "-p";
        args[argc++] = portStr;
    }

    if (oneOff) args[argc++] = "-1";
    if (verbose) args[argc++] = "-V";
    if (afVersion && afVersion[0]) {
        if (strcmp(afVersion, "-4") == 0) args[argc++] = "-4";
        else if (strcmp(afVersion, "-6") == 0) args[argc++] = "-6";
    }
    if (format && *format) {
        args[argc++] = "-f";
        args[argc++] = format;
    }
    if (json) args[argc++] = "-J";

    /* Save display arg count (before --forceflush) */
    int dispArgc = argc;

    /* --forceflush: internal, not shown in display command */
    args[argc++] = "--forceflush";

    /* Build display command (without --forceflush) */
    if (cmdStr && cmdStrLen > 0) {
        cmdStr[0] = 0;
        snprintf(cmdStr, cmdStrLen, "\"%s\"", iperf3Path);
        for (int i = 0; i < dispArgc; i++) {
            strncat_s(cmdStr, cmdStrLen, " ", 1);
            strncat_s(cmdStr, cmdStrLen, args[i], _TRUNCATE);
        }
        /* Append extra args raw */
        if (extraArgs && *extraArgs) {
            strncat_s(cmdStr, cmdStrLen, " ", 1);
            strncat_s(cmdStr, cmdStrLen, extraArgs, _TRUNCATE);
        }
    }

    char* fullCmd = BuildCommandLine(iperf3Path, args, argc);
    if (!fullCmd) return false;

    /* Append extra args to actual execution command too (raw, as-is) */
    if (extraArgs && *extraArgs) {
        size_t curLen = strlen(fullCmd);
        size_t extraLen = strlen(extraArgs) + 2;
        char* bigger = (char*)realloc(fullCmd, curLen + extraLen);
        if (bigger) {
            fullCmd = bigger;
            strcat(fullCmd, " ");
            strcat(fullCmd, extraArgs);
        }
    }

    /* Create anonymous pipe for stdout+stderr capture */
    HANDLE hPipeRead = NULL, hPipeWrite = NULL;
    if (!CreateOutputPipe(&hPipeRead, &hPipeWrite)) {
        free(fullCmd);
        SetLastError(ERROR_GEN_FAILURE);
        return false;
    }

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    PROCESS_INFORMATION pi = {0};

    BOOL ok = CreateProcessA(NULL, fullCmd, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    DWORD lastErr = GetLastError();
    free(fullCmd);
    CloseHandle(hPipeWrite); /* parent closes write end so EOF works */
    if (!ok) {
        CloseHandle(hPipeRead);
        SetLastError(lastErr);
        return false;
    }

    s_serverProcess = pi.hProcess;
    s_serverPID = pi.dwProcessId;
    CloseHandle(pi.hThread);

    if (phPipeRead) *phPipeRead = hPipeRead;
    else CloseHandle(hPipeRead);

    return true;
}

/* ===== Stop processes ===== */
static void StopProcess(HANDLE* phProcess, DWORD pid)
{
    if (*phProcess) {
        TerminateProcess(*phProcess, 1);
        WaitForSingleObject(*phProcess, 2000);
        CloseHandle(*phProcess);
        *phProcess = NULL;
    } else if (pid) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (h) {
            TerminateProcess(h, 1);
            WaitForSingleObject(h, 2000);
            CloseHandle(h);
        }
    }
}

void StopIperf3Client(void)
{
    StopProcess(&s_clientProcess, s_clientPID);
    s_clientPID = 0;
}

void StopIperf3Server(void)
{
    StopProcess(&s_serverProcess, s_serverPID);
    s_serverPID = 0;
}

/* ===== Kill port listeners ===== */
/* Uses IP Helper API (in-memory) to find and terminate processes
   listening on the given port. */
void KillPortListeners(const char* port)
{
    if (!port || !*port) return;
    int portNum = atoi(port);
    if (portNum <= 0) return;

    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL;
    DWORD dwSize = 0;
    DWORD ret = GetExtendedTcpTable(NULL, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (ret != ERROR_INSUFFICIENT_BUFFER || dwSize == 0) return;

    pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
    if (!pTcpTable) return;

    ret = GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET,
                              TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (ret != NO_ERROR) {
        free(pTcpTable);
        return;
    }

    DWORD selfPid = GetCurrentProcessId();
    for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID* row = &pTcpTable->table[i];
        /* dwLocalPort is in network byte order */
        int localPort = (int)(((row->dwLocalPort & 0xFF) << 8) |
                              ((row->dwLocalPort >> 8) & 0xFF));
        if (localPort != portNum) continue;

        DWORD pid = row->dwOwningPid;
        if (pid == 0 || pid == selfPid || pid == 4) continue; /* never kill self/System */

        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (h) {
            TerminateProcess(h, 1);
            CloseHandle(h);
        }
    }
    free(pTcpTable);
}

/* ===== Process accessors ===== */
HANDLE GetClientProcess(void) { return s_clientProcess; }
HANDLE GetServerProcess(void) { return s_serverProcess; }
DWORD  GetClientPID(void) { return s_clientPID; }
DWORD  GetServerPID(void) { return s_serverPID; }

/* ===== Reset process state after natural exit ===== */
void ResetClientProcess(void)
{
    if (s_clientProcess) {
        CloseHandle(s_clientProcess);
        s_clientProcess = NULL;
    }
    s_clientPID = 0;
}

void ResetServerProcess(void)
{
    if (s_serverProcess) {
        CloseHandle(s_serverProcess);
        s_serverProcess = NULL;
    }
    s_serverPID = 0;
}

bool CheckProcessExited(HANDLE hProcess, DWORD* exitCode)
{
    if (!hProcess) { if (exitCode) *exitCode = 0; return true; }
    DWORD code = 0;
    if (GetExitCodeProcess(hProcess, &code)) {
        if (code != STILL_ACTIVE) {
            if (exitCode) *exitCode = code;
            return true;
        }
    }
    return false;
}

/* ===== Clean output (remove control chars) ===== */
void CleanOutput(char* line)
{
    if (!line) return;
    char* dst = line;
    char* src = line;
    while (*src) {
        unsigned char c = (unsigned char)*src;
        if (c >= 0x20 && c != 0x7f) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = 0;
}

/* ===== String parsing helpers ===== */

/* Skip whitespace */
static const char* SkipWs(const char* p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Case-insensitive prefix match */
static bool StrStartsWithCI(const char* str, const char* prefix)
{
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix))
            return false;
        str++; prefix++;
    }
    return true;
}

/* ===== Parse iperf3 text line ===== */
/*
 * Expected format:
 * [ID]  START-END  sec  TRANSFER  BANDWIDTH  [extra]
 *
 * Examples:
 * [  5]   0.00-1.00   sec  1.23 MBytes  10.3 Mbits/sec
 * [  5]   9.00-10.00  sec  2.34 MBytes  19.6 Mbits/sec    1024
 * [SUM]   9.00-10.00  sec  2.34 MBytes  19.6 Mbits/sec    sender
 * [  3]   0.00-1.00   sec  1.23 MBytes  10.3 Mbits/sec   0.123 ms  12/100 (12%)
 */
bool ParseIperf3Line(const char* line, bool isSummary, Iperf3Data* out)
{
    if (!line || !out) return false;
    memset(out, 0, sizeof(*out));
    out->isSummary = isSummary;

    const char* p = SkipWs(line);

    /* Match [ID] or [SUM] */
    if (*p != '[') return false;
    p++;
    const char* bracketEnd = strchr(p, ']');
    if (!bracketEnd) return false;

    /* Parse stream ID (may be "SUM") */
    if (StrStartsWithCI(p, "SUM")) {
        out->streamId = -1; /* SUM */
    } else {
        out->streamId = atoi(p);
    }
    p = bracketEnd + 1;

    /* In --bidir mode, iperf3 prints a second bracket after [ID]: [Role]
       e.g. [  6][TX-C]   0.00-2.00   sec ...
       Skip it if present so the START-END parser sees the numbers. */
    p = SkipWs(p);
    if (*p == '[') {
        const char* roleEnd = strchr(p, ']');
        if (roleEnd) {
            const char* rp = p + 1;
            int roleLen = (int)(roleEnd - rp);
            if (roleLen > 0 && roleLen < 15) {
                memcpy(out->role, rp, roleLen);
                out->role[roleLen] = 0;
            }
            p = roleEnd + 1;
        }
    }

    /* Parse START-END */
    p = SkipWs(p);
    out->intervalStart = strtod(p, (char**)&p);
    p = SkipWs(p);
    if (*p == '-') p++;
    p = SkipWs(p);
    out->intervalEnd = strtod(p, (char**)&p);

    /* Parse "sec" keyword */
    p = SkipWs(p);
    if (!StrStartsWithCI(p, "sec")) return false;
    p += 3;

    /* Parse transfer (e.g. "1.23 MBytes" or "1.23 KBytes" or "1.23 GBytes") */
    p = SkipWs(p);
    {
        double val = strtod(p, (char**)&p);
        p = SkipWs(p);
        /* Unit: MBytes, KBytes, GBytes, Bytes */
        const char* unitStart = p;
        while (*p && *p != ' ') p++;
        int unitLen = (int)(p - unitStart);
        char unit[16] = {0};
        if (unitLen > 0 && unitLen < 15) {
            memcpy(unit, unitStart, unitLen);
            unit[unitLen] = 0;
        }
        /* Build transfer string */
        snprintf(out->transfer, sizeof(out->transfer), "%.2f %s", val, unit);
        out->bytes = val;
        if (unit[0] == 'T' || unit[0] == 't') out->bytes *= 1e12;
        else if (unit[0] == 'G' || unit[0] == 'g') out->bytes *= 1e9;
        else if (unit[0] == 'M' || unit[0] == 'm') out->bytes *= 1e6;
        else if (unit[0] == 'K' || unit[0] == 'k') out->bytes *= 1e3;
    }

    /* Parse bandwidth (e.g. "10.3 Mbits/sec") */
    p = SkipWs(p);
    {
        double val = strtod(p, (char**)&p);
        p = SkipWs(p);
        const char* unitStart = p;
        while (*p && *p != ' ') p++;
        int unitLen = (int)(p - unitStart);
        char unit[16] = {0};
        if (unitLen > 0 && unitLen < 15) {
            memcpy(unit, unitStart, unitLen);
            unit[unitLen] = 0;
        }
        snprintf(out->bandwidth, sizeof(out->bandwidth), "%.2f %s", val, unit);
        /* Convert to bps (bits/sec).
           iperf3 -f options: lowercase = bits (Kbits/Mbits/Gbits/Tbits),
                              uppercase = Bytes (KBytes/MBytes/GBytes/TBytes).
           Unit string always starts with capital prefix (K/M/G/T),
           unit[1] distinguishes: 'b' = bits, 'B' = Bytes. */
        out->bps = val;
        {
            double mult = 1;
            switch (unit[0]) {
                case 'K': case 'k': mult = 1e3; break;
                case 'M': case 'm': mult = 1e6; break;
                case 'G': case 'g': mult = 1e9; break;
                case 'T': case 't': mult = 1e12; break;
            }
            if (unit[1] == 'B') mult *= 8;  /* Bytes → bits */
            out->bps = val * mult;
        }

        FormatSpeed(out->bps, out->formatSpeed, sizeof(out->formatSpeed));
    }

    /* Parse extra (retransmits, cwnd, jitter, lost/total, direction) */
    p = SkipWs(p);
    if (*p) {
        /* Try TCP extra: "1024" or "1024 3.45 MBytes" (integer, not float) */
        if (isdigit((unsigned char)*p) && p[1] != '.') {
            int retrans = atoi(p);
            while (isdigit((unsigned char)*p)) p++;
            out->retransmits = retrans;
            p = SkipWs(p);
            /* Check for cwnd */
            if (*p && isdigit((unsigned char)*p)) {
                double cwndVal = strtod(p, (char**)&p);
                p = SkipWs(p);
                const char* u = p;
                while (*p && *p != ' ') p++;
                int ul = (int)(p - u);
                if (ul > 0 && ul < 63) {
                    snprintf(out->cwnd, sizeof(out->cwnd), "%.2f %.*s", cwndVal, ul, u);
                }
            }
        }
        /* Try UDP extra: "0.123 ms  12/100 (12%)" */
        else if (*p && (isdigit((unsigned char)*p) || *p == '.')) {
            double jitterVal = strtod(p, (char**)&p);
            p = SkipWs(p);
            if (StrStartsWithCI(p, "ms")) {
                out->jitter = jitterVal;
                p += 2;
                p = SkipWs(p);
                /* lost/total */
                out->lostPackets = atoi(p);
                while (*p && *p != '/') p++;
                if (*p == '/') p++;
                out->totalPackets = atoi(p);
                while (*p && *p != '(') p++;
                if (*p == '(') p++;
                out->lossRate = strtod(p, (char**)&p);
                /* skip past "%" and ")" to reach direction */
                while (*p == '%' || *p == ')') p++;
                p = SkipWs(p);
            }
        }
        /* Check direction: sender / receiver */
        if (StrStartsWithCI(p, "sender")) {
            strcpy(out->direction, "sender");
        } else if (StrStartsWithCI(p, "receiver")) {
            strcpy(out->direction, "receiver");
        }
    }

    return true;
}

/* ===== Format speed ===== */
void FormatSpeed(double bps, char* out, int outLen)
{
    if (!out || outLen <= 0) return;
    if (bps < 0) bps = 0;
    if (bps >= 1e9) snprintf(out, outLen, "%.2f Gbps", bps / 1e9);
    else if (bps >= 1e6) snprintf(out, outLen, "%.2f Mbps", bps / 1e6);
    else if (bps >= 1e3) snprintf(out, outLen, "%.2f Kbps", bps / 1e3);
    else snprintf(out, outLen, "%.0f bps", bps);
}

/* ===== Format bytes → transfer string ===== */
void FormatBytes(double bytes, char* out, int outLen)
{
    if (!out || outLen <= 0) return;
    if (bytes >= 1e12) snprintf(out, outLen, "%.2f TBytes", bytes / 1e12);
    else if (bytes >= 1e9) snprintf(out, outLen, "%.2f GBytes", bytes / 1e9);
    else if (bytes >= 1e6) snprintf(out, outLen, "%.2f MBytes", bytes / 1e6);
    else if (bytes >= 1e3) snprintf(out, outLen, "%.2f KBytes", bytes / 1e3);
    else snprintf(out, outLen, "%.0f Bytes", bytes);
}

/* ===== JSON mode parser ===== */

/* Helper: check if line contains a key (simple substring search) */
static bool JsonContainsKey(const char* line, const char* key)
{
    /* Search for "key" : pattern (with quotes) */
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(line, pattern) != NULL;
}

/* Helper: extract double value for a key from a JSON fragment.
   Searches for "key" : <number> and returns the number.
   Returns true if found.
   Non-static: also used by the replay JSON scanner (see replay.c). */
bool JsonGetDouble(const char* line, const char* key, double* out)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(line, pattern);
    if (!p) return false;
    p += strlen(pattern);
    /* Skip whitespace and colon */
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p != ':') return false;
    p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p == 'n') return false; /* null */
    *out = strtod(p, NULL);
    return true;
}

/* Helper: extract int value (non-static: also used by replay.c) */
bool JsonGetInt(const char* line, const char* key, int* out)
{
    double d;
    if (!JsonGetDouble(line, key, &d)) return false;
    *out = (int)d;
    return true;
}

bool ParseIperf3JsonLine(const char* line, JsonParseState* jps,
                         Iperf3Data* outSummary,
                         double* pTime, double* pBps, bool* pIsInterval,
                         double* pJitter, double* pLossRate,
                         double* pDatagrams)
{
    if (!line || !jps) return false;

    /* Detect server_output_json section */
    if (JsonContainsKey(line, "server_output_json")) {
        jps->inServerOutput = true;
    }
    /* Detect end of server output (closing of server_output_json) */
    if (jps->inServerOutput && strstr(line, "server_output_text")) {
        jps->inServerOutput = false;
    }

    /* Detect closing brace — finalize current summary object */
    if (strchr(line, '}')) {
        if (jps->state == JS_IN_SUM_SENT || jps->state == JS_IN_SUM_RECEIVED) {
            /* Build Iperf3Data from accumulated values */
            if (outSummary) {
                memset(outSummary, 0, sizeof(*outSummary));
                outSummary->isSummary = true;
                outSummary->bps = jps->bps;
                outSummary->bytes = jps->bytes;
                outSummary->retransmits = jps->retransmits;
                outSummary->jitter = jps->jitter;
                outSummary->lostPackets = jps->lostPackets;
                outSummary->totalPackets = jps->totalPackets;
                outSummary->lossRate = jps->lossRate;
                strcpy(outSummary->direction, jps->isSender ? "sender" : "receiver");
                FormatSpeed(jps->bps, outSummary->formatSpeed, sizeof(outSummary->formatSpeed));
                FormatBytes(jps->bytes, outSummary->transfer, sizeof(outSummary->transfer));
                snprintf(outSummary->bandwidth, sizeof(outSummary->bandwidth), "%.2f", jps->bps);
            }
            jps->state = JS_IDLE;
            /* Reset accumulators for next summary */
            jps->bytes = 0;
            jps->bps = 0;
            jps->retransmits = 0;
            jps->jitter = 0;
            jps->lostPackets = 0;
            jps->totalPackets = 0;
            jps->lossRate = 0;
            jps->isSender = false;
            /* Signal: a summary was completed */
            if (pIsInterval) *pIsInterval = false;
            return true;
        }
        if (jps->state == JS_IN_INTERVAL_SUM) {
            jps->state = JS_IDLE;
        }
    }

    /* Detect entering sum_sent / sum_received */
    if (JsonContainsKey(line, "sum_sent")) {
        jps->state = JS_IN_SUM_SENT;
        jps->isSender = true;
        /* Reset accumulators */
        jps->bytes = 0; jps->bps = 0; jps->retransmits = 0;
        jps->jitter = 0; jps->lostPackets = 0; jps->totalPackets = 0;
        jps->lossRate = 0;
        return false;
    }
    if (JsonContainsKey(line, "sum_received")) {
        jps->state = JS_IN_SUM_RECEIVED;
        jps->isSender = false;
        jps->bytes = 0; jps->bps = 0; jps->retransmits = 0;
        jps->jitter = 0; jps->lostPackets = 0; jps->totalPackets = 0;
        jps->lossRate = 0;
        return false;
    }

    /* Inside a summary object — extract fields */
    if (jps->state == JS_IN_SUM_SENT || jps->state == JS_IN_SUM_RECEIVED) {
        double dval;
        int ival;
        if (JsonGetDouble(line, "bytes", &dval)) jps->bytes = dval;
        if (JsonGetDouble(line, "bits_per_second", &dval)) jps->bps = dval;
        if (JsonGetInt(line, "retransmits", &ival)) jps->retransmits = ival;
        if (JsonGetDouble(line, "jitter_ms", &dval)) jps->jitter = dval;
        if (JsonGetInt(line, "lost_packets", &ival)) jps->lostPackets = ival;
        if (JsonGetInt(line, "packets", &ival)) jps->totalPackets = ival;
        if (JsonGetDouble(line, "lost_percent", &dval)) jps->lossRate = dval;
        /* Note: do NOT read "sender" field from JSON — iperf3 v3.21 has a bug
           where sum_received.sender is true for TCP. Direction is determined
           by structural position (sum_sent vs sum_received), not the field. */
        return false;
    }

    /* Inside interval sum — extract bps, end time, jitter, loss, datagrams for chart */
    if (jps->state == JS_IN_INTERVAL_SUM) {
        double dval;
        int ival;
        if (JsonGetDouble(line, "bits_per_second", &dval)) {
            if (pBps) *pBps = dval;
        }
        if (JsonGetDouble(line, "end", &dval)) {
            if (pTime) *pTime = dval;
        }
        /* UDP interval fields */
        if (JsonGetDouble(line, "jitter_ms", &dval)) {
            if (pJitter) *pJitter = dval;
        }
        if (JsonGetDouble(line, "lost_percent", &dval)) {
            if (pLossRate) *pLossRate = dval;
        }
        if (JsonGetInt(line, "packets", &ival)) {
            if (pDatagrams) *pDatagrams = (double)ival;
        }
        /* If both found, signal interval */
        /* Check if line has both or we accumulate across lines */
        if (pBps && *pBps > 0 && pTime && *pTime >= 0) {
            if (pIsInterval) *pIsInterval = true;
            /* Don't return true here — let caller check pBps */
        }
        return false;
    }

    /* IDLE state — look for "sum": (interval summary) */
    if (jps->state == JS_IDLE) {
        if (JsonContainsKey(line, "sum")) {
            jps->state = JS_IN_INTERVAL_SUM;
            jps->intervalEnd = 0;
            if (pBps) *pBps = 0;
            if (pTime) *pTime = 0;
            if (pJitter) *pJitter = 0;
            if (pLossRate) *pLossRate = 0;
            if (pDatagrams) *pDatagrams = 0;
            return false;
        }
    }

    return false;
}
