# iperf3 GUI V1.1

[English](README_EN.md) | [简体中文](README.md)

> The repository carries two complete documents that mirror each other section by section:
> **README.md (Simplified Chinese)** and **README_EN.md (English)**.
> Any doc change must land in **both**, otherwise one language silently drifts behind.

A graphical front-end for **iperf3** on Windows, written in pure **Win32 API + GDI+** with no UI framework.
Compiles to a single statically linked `iperf3gui.exe` (~738 KB).

---

## Screenshots

| Client | Server |
|---|---|
| ![Client](screenshots/client.png) | ![Server](screenshots/server.png) |

| Replay | Settings |
|---|---|
| ![Replay](screenshots/replay.png) | ![Settings](screenshots/settings.png) |

> Screenshots live in `screenshots/` (same directory as this document).
> If you are reading this on the repository landing page, follow the links there instead.

---

## 1. Features

| Module | Capability |
|---|---|
| Client | Server IP / port, protocol, duration, interval, parallel streams, buffer, bandwidth; reverse `-R`, bidirectional `--bidir`, JSON output, `--get-server-output` |
| Server | Listen port, one-off `-1`, verbose `-V` |
| Live chart | GDI+ line + translucent area fill; bold sum curve + thin per-stream lines (`-P n`); UDP right axis (jitter / loss / datagrams); dual TX/RX curves in bidir mode |
| Replay | Load a past log, step through intervals with a slider, same chart as live; a "Unit" combo switches between 9 rate units (default *auto*) |
| Raw output | RichEdit terminal view with autoscroll / clear / copy |
| Summary parsing | Auto-parses sender / receiver summaries, supports bidir dual-channel |
| i18n | Simplified Chinese / Traditional Chinese / English (+ a "Follow system" entry) |
| Themes | Follow system / Dark / Light, including the DWM title bar |
| Persistence | Every setting written to `settings.ini`, **next to the exe** |
| Logging | Optionally write test output to `logs/`, with a size cap and a naming template |
| iperf3 path | Auto-detect / browse / save manually |

---

## 2. Tech Stack

| Component | Technology |
|---|---|
| Languages | C (main.c / iperf3.c / replay.c / lang.c) + C++ (chart.cpp, the GDI+ class) |
| GUI | Win32 API (`CreateWindowEx` + message loop), **no v6 manifest** (classic control painting) |
| Drawing | GDI+ |
| Compiler | MinGW32 GCC 16.2.0 (i686), installed at `C:\mingw32` |
| Rich text | RichEdit 5.0 (`Msftedit.dll`), falls back to RichEdit 2.0 |
| Zero dependencies | No Electron / Qt / duilib / any UI framework |

---

## 3. Directory Layout

### 3.1 Inside the repository (everything you see on this page)

```
iperf3-gui-win32/
├── README.md              Simplified Chinese full documentation
├── README_EN.md           English full documentation (mirrors the Chinese one)
├── CHANGELOG.md           Change log
├── CONTRIBUTING.md        Contributing guide (build / house rules / regression)
├── LICENSE                MIT license text
├── build.bat              Build script (windres + g++)
├── .gitignore             Excludes build artifacts, config, third-party binaries
├── .gitattributes         Normalizes to LF
├── screenshots/           UI screenshots
│   ├── client.png
│   ├── server.png
│   ├── replay.png
│   └── settings.png
├── test/
│   ├── run_autotest.py    Regression orchestrator (17 cases, auto-starts a server)
│   └── autotest.ini       Case parameters (case_00 … case_16)
└── src/
    ├── main.c             Main window / control creation / events / persistence / i18n / theme
    ├── iperf3.c           iperf3 process management + output parsing (incl. JSON)
    ├── iperf3.h           Data structures and API
    ├── chart.cpp          GDI+ chart rendering (the only C++ file)
    ├── chart.h            ChartData struct and chart API
    ├── replay.c           Replay: log parsing + per-interval advance
    ├── replay.h
    ├── lang.c             Three-language string table kStr[]
    ├── lang.h             String enum + language/theme selector enums
    ├── ui.h               AppState global state + control handles
    ├── resource.h         Control ID definitions
    ├── license_text.c     MIT license body (trilingual summary + always-English terms)
    ├── license_text.h
    ├── help_text.c        "Notes" body (trilingual)
    ├── help_text.h
    ├── app.rc             Resource script (icon + VERSIONINFO)
    └── app.ico            Application icon (multiple sizes)
```

**The repository contains no third-party binaries.** iperf3 and its Cygwin runtime, the
compiled exe, local configuration and test logs are all kept out.

### 3.2 Local workspace only (not in the repository)

The development machine also carries the following. All of it is **machine-specific**
and excluded by `.gitignore`:

| Workspace item | Measured | Why it stays out |
|---|---|---|
| `tools/` + `iperf3.21/` | 8.9 MB each, 17.8 MB total | Third-party binaries (iperf3 is BSD-3-Clause; the Cygwin runtime carries its own licenses) — not the same provenance as this project's MIT |
| `iperf3gui.exe` / `iperf3gui.lnk` | — | The exe goes to a GitHub Release asset instead; the .lnk carries an absolute path |
| `settings.ini` | — | Contains an absolute local path |
| `logs/*.log` | — | Contains internal IP addresses (e.g. `192.168.x.x`) and port numbers |
| `src/app.res`, `test/results/` | — | Build / run artifacts, regenerated every time |
| `*_备份_*` | 6 files | Working-habit noise |

> Pre-edit backups (`*.c_备份_YYYYMMDD`, `README.md_备份_*`) are kept in the workspace per house rules;
> delete them by hand once you are satisfied.

After cleaning, the repository is about **0.60 MB** (31 files: sources, docs, regression
scripts and screenshots). Users supply their own iperf3 (≥ 3.7) — see
[§10](#10-iperf3-version-requirements). The iperf3 path configured on the Settings page is
never distributed with the repository.

---

## 4. Building

### 4.1 Using build.bat (recommended)

```cmd
cd /d d:\Tools\Network_Tools\Flow\iperf3-GUI-Win32
build.bat
```

Two steps:
1. `windres src\app.rc -O coff -o src\app.res`
2. `g++` (8 sources + app.res) → `iperf3gui.exe`

### 4.2 Manual build (cmd, same as the bat)

```cmd
C:\mingw32\bin\windres.exe src\app.rc -O coff -o src\app.res

C:\mingw32\bin\g++.exe -O2 -Wall ^
    -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN ^
    src\main.c src\iperf3.c src\chart.cpp src\replay.c src\lang.c src\license_text.c src\help_text.c src\app.res ^
    -o iperf3gui.exe ^
    -lgdiplus -lcomctl32 -lcomdlg32 -lshlwapi -liphlpapi ^
    -luser32 -lkernel32 -lgdi32 -lshell32 -lole32 ^
    -mwindows -municode -static -static-libgcc
```

### 4.3 Manual build (bash / PowerShell, single line, no `^` continuation)

```bash
C:/mingw32/bin/windres.exe src/app.rc -O coff -o src/app.res

C:/mingw32/bin/g++.exe -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN \
  src/main.c src/iperf3.c src/chart.cpp src/replay.c src/lang.c src/license_text.c src/help_text.c src/app.res \
  -o iperf3gui.exe \
  -lgdiplus -lcomctl32 -lcomdlg32 -lshlwapi -liphlpapi -luser32 -lkernel32 -lgdi32 -lshell32 -lole32 \
  -mwindows -municode -static -static-libgcc
```

### 4.4 Linked Libraries

| Library / switch | Purpose |
|---|---|
| gdiplus | GDI+ chart drawing |
| comctl32 | Common controls / subclassing (`SetWindowSubclass`) |
| comdlg32 | File open dialog |
| shlwapi | Path helpers |
| iphlpapi | Port-in-use detection (`GetExtendedTcpTable`) |
| shell32 / ole32 | Shell and COM basics |
| `-mwindows` | Windows subsystem (no console) |
| `-municode` | Unicode entry point (`wWinMain`) |
| `-static -static-libgcc` | Static linking, no runtime dependencies |

> **`uxtheme` was removed on 2026-09-23** (`SetWindowTheme` is a no-op without a v6 manifest;
> `-luxtheme` and the related dead code are gone).

### 4.5 Current Warning Baseline

With `-Wall -Wextra`: **2 unused parameters in main.c** (`textLen`, `hPrev`), 0 elsewhere.
Plus a handful of `-Wstringop-truncation` (`strncpy` length equals source length — known and harmless).
**A change must not add any new warning.**

> `-Wall` alone (what `build.bat` and §4.2 use) does **not** report unused parameters —
> you need `-Wextra` to reproduce the baseline.

### 4.6 Verifying the exe Was Really Rebuilt

**Do not use md5, and do not judge by size alone:**
- Two builds of identical source produce different md5s (the PE header carries a timestamp);
- Sizes collide across versions (several builds all landed on exactly 736007 B).

**Trust mtime.**

---

## 5. settings.ini

**Location: the exe's own directory** (`GetModuleFileNameA` dir + `\settings.ini`).
Older docs claiming `%APPDATA%\iperf3gui\` are **wrong**.

| Section | Keys | Notes |
|---|---|---|
| `[client]` | `ip` `port` `proto` `duration` `interval` `format` `num` `omit` `bw` `extra` `parallel` `reverse` `bidir` `json` `getserveroutput` | Client parameters; saved on "Apply" or exit |
| `[server]` | `port` `format` `af` `extra` `oneoff` `verbose` `json` | Server parameters |
| `[settings]` | `iperf3Path` | Path to iperf3.exe, default `.\tools\iperf3.exe` |
| `[log]` | `Enabled` `Dir` `Naming` `MaxSizeKB` | Log to disk; `Naming` supports `%mode` `%ip` `%Y%M%D` `%h%m%s` |
| `[UI]` | `Theme` = `auto`\|`dark`\|`light`, `Language` = language entry | Theme and language. Written by "Restore defaults" or on exit |
| `[Chart]` | `ShowStreams` (per-stream thin lines), `HideStreamSum` (hide the sum in multi-stream) | Both default `0`; `HideStreamSum` only takes effect when `ShowStreams=1` |
| `[replay]` | `unit` = `0`–`8` | Replay display-unit index (`0`=Kb/s `1`=Mb/s `2`=Gb/s `3`=Tb/s `4`=KB/s `5`=MB/s `6`=GB/s `7`=TB/s `8`=auto, default `8`). *auto* is equivalent to Mb/s |

> `WritePrivateProfileString` **preserves existing blank lines verbatim** — it neither heals nor worsens them.
> Legacy blank lines can only be compacted by hand.

---

## 6. Regression Testing

The orchestrator **starts an iperf3 server itself** (iperf3 3.x `-s` serves multiple clients in sequence,
verified empirically), so no manual server is needed:

```bash
python test/run_autotest.py --duration 5
```

| Item | Value |
|---|---|
| Case count | 17 (`case_00` … `case_16`), covering TCP/UDP × `-f` unit × forward/reverse/bidir × JSON × server one-off/verbose |
| **Measured baseline** | **16 PASS / 0 WARN / 1 FAIL** (2026-10-16, iperf3 3.21 Cygwin build, loopback, duration=5). The FAIL is `case_05` UDP bidir — an iperf3-side limitation, see below |
| How it works | The orchestrator rewrites `test/autotest.ini` for the selected cases (the GUI always reads that file next to the exe and always starts at `case_00`; the original is restored afterwards) → launches `iperf3gui.exe --autotest` (hidden window) → parses the per-case `cmd` / `raw` / `summary` dumps under `test/results/` → judges PASS/WARN/FAIL → writes `test/results/report.html` |
| Judging | ① dump missing/empty; ② `[error]` in raw output; ③ text mode must contain interval/summary lines; ④ summary-box anchor (client `[发送]`, server `[接收]`); ⑤ **numeric cross-check** — convert both the summary-box rate and the raw `[SUM] … sender` line to bps and compare with a 2% tolerance |
| Options | `--duration`, `--server-ip`, `--server-port`, `--start-case N`, `--max-cases N`, `--no-server` |

**Two environment constraints** (verified on real hardware, 2026-10-16):

1. **The UI language must be Simplified Chinese.** The orchestrator anchors on the window text
   `[发送]` / `[接收]` / `方向:`; Traditional Chinese renders `[傳送]` and English renders `[Sender]`.
   A wrong language is reported explicitly (it does not silently turn everything red).
   The language entry lives in `settings.ini` under `[UI] Language`.
2. **UDP cases always use `parallel=1`.** The Cygwin iperf3 3.21 build fails outright when `-u` is
   combined with `-P>1`: `unable to read from stream socket: Resource temporarily unavailable`, exit code 1
   (reproduced at `-P 2/4/8`; `-P 1` is fine). This is an **iperf3 limitation, not a GUI defect**,
   so UDP multi-stream is excluded from the regression matrix.

#### Known FAIL: `case_05` UDP + `--bidir`

**Not a GUI defect.** It reproduces with bare command lines, no GUI involved:

```cmd
iperf3.exe -s -p 15211
iperf3.exe -c 127.0.0.1 -p 15211 -u -t 3 --bidir
  → iperf3: error - unable to read from stream socket: Resource temporarily unavailable
```

Control experiment (same machine, same exe):

| Combination | Result |
|---|---|
| `-u -t 3` (plain UDP) | OK |
| `-u -t 3 -R` (UDP reverse) | OK |
| `-u -t 3 -b 10M --bidir` | Same failure (not bandwidth-related) |
| `-u -t 6 --bidir` | Same failure (not duration-related) |
| `-t 3 --bidir` (TCP bidir) | OK |

The server log shows `Accepted connection` and then the client fails to read its control socket —
it is stuck inside iperf3's own control-channel handshake, before the GUI ever parses anything.
The FAIL is therefore attributed to **iperf3 3.21 Cygwin not supporting UDP + bidir** and is excluded
from the GUI regression gate.

Each case must state `parallel` explicitly: if AutoTest omits it, it inherits whatever the user last had
in `settings.ini` (e.g. `parallel=8`) and results drift with user config. The GUI sets the stream combo
from the `parallel` key (`CB_SETCURSEL, parallel-1`, clamped to 1–8).

---

## 7. Source Module Responsibilities

| File | Lines | Responsibility |
|---|---|---|
| `main.c` | 5487 | Main window, creation and layout of the four panels (`CreateAllControls`), `UI_SwitchTab`, message handling, persistence (LoadAll/SaveAll/InitDefault/RestoreDefaults), `Theme_Apply`, i18n application, dark-mode subclasses, replay UI wiring |
| `iperf3.c` | 1067 | Build the command line, `CreateProcess` the child, read the anonymous pipe, two parsing paths (text and JSON) |
| `chart.cpp` | 735 | GDI+ drawing: axes, grid, sum line/area, per-stream thin lines, UDP right axis, legend (click to toggle), `niceMax` range |
| `replay.c` | 448 | Parse a historical log into an interval array, advance via slider/playback, feed the same `ChartData` |
| `lang.c` / `lang.h` | 243 / 193 | Trilingual string table `kStr[]` (138 entries) and enum; language and theme selector enums |
| `license_text.c` / `help_text.c` | 85 / 52 | MIT body (trilingual summary + always-English terms), notes body (trilingual) |
| `ui.h` | 193 | `AppState` global state + all control handles |
| `resource.h` | 137 | Control ID segments (see below) |

---

## 8. Control ID Segments (`resource.h`)

| Segment | Range | Notes |
|---|---|---|
| Tab buttons | 1001–1004 | Client / Server / Replay / Settings |
| Global buttons | 1005–1007 | Save config / Load config / Restore defaults |
| Client panel | 1100–1129 | Inputs, checkboxes, command box, output box, chart window, summary label |
| Server panel | 1200–1215 | Port, one-off, verbose, output, etc. |
| Settings panel | 1300–1329 | Path, browse/detect, log items, **1314 show streams, 1316 language combo, 1317 theme combo, 1318 hide stream sum, 1321 notes, 1326 License** |
| Replay panel | 1500–1511 | Path / browse / info / slider / time / play / stop / speed / **unit (1511)** / values / chart |
| Status bar | 1401–1408 | Two segments (client, server) each label + dot + value, plus the copy hint |
| Custom messages | — | `WM_PIPE_LINE` (WM_USER+100) / `WM_PIPE_DONE` (+101), `WM_AUTOTEST_NEXT` (+200) / `WM_AUTOTEST_DONE` (+201) |

> The replay panel owns the 1500 segment, not 1400 — 1400 belongs to the status bar.
> The timer `REPLAY_TIMER_ID = 1001` shares its numeric value with a Tab button ID but lives in a different namespace.

---

## 9. House Rules (read before touching code)

### 9.1 Parsing rules (get these wrong ⇒ blank chart)

- **The summary-line test is `direction[0] != 0`** (interval lines carry no sender/receiver).
  **Never** use the `- - - -` separator line.
- **In bidir, never filter on `streamId == -1`**: with a single stream pair iperf3 emits no `[SUM]` line,
  so such a filter blanks the whole curve.
- For new server connections, **only match `"Accepted connection from"`**.
- The legend is GDI+-drawn ⇒ **must `InvalidateRect`** after a language or theme switch.
- Multi-stream (`-P n`) interval lines arrive as `[4] [7] … [19] [SUM]`:
  **the first point of a new interval must be seeded with the previous interval's sum**, then overwritten
  when `[SUM]` arrives — otherwise the curve head dives to a single-stream value (roughly 1/N of total).
  Same for the bidir branch.

### 9.2 Dark mode (constraints of having no v6 manifest)

- `SetWindowTheme` **is a no-op**; a plain push button **ignores `WM_CTLCOLORBTN`** ⇒ dark buttons need
  `BS_OWNERDRAW` + `WM_DRAWITEM`.
- The combo drop-down arrow's `rcButton` **is already in client coordinates**.
- Control borders are drawn in the client area ⇒ repaint them with `FieldChromeSubclass`.
- The multi-line command box is a RichEdit: background via `EM_SETBKGNDCOLOR`,
  **text colour must go through `EM_SETCHARFORMAT`**.
- Title bar: `DWMWA_USE_IMMERSIVE_DARK_MODE` (20; 19 is the legacy value).
  **The title bar font cannot be changed** (DWM draws it).
- `[UI] Theme=auto|dark|light`; `themeSel` (the selection) and `isDark` (the effective value) **must stay separate**.
- **Darkening the combo drop-down list's scrollbar is impossible — abandoned.**
  `WM_CTLCOLORSCROLLBAR` is simply never sent for a ListBox's built-in scrollbar.
  The dead code is deleted; do not retry this path.

### 9.3 Language table

- The `lang.h` enum uses **explicit numbers** and `lang.c`'s `kStr[]` is **positionally indexed** —
  the two **must stay strictly in the same order**. Deleting any middle item requires renumbering everything
  after it; being off by one scrambles every string in the app. **New items may only be appended at the end.**
- **All three columns** of a language row must be filled in (otherwise "Simplified Chinese" shows up
  as Traditional text in a Traditional UI).
- Language combo entries 1–3 are **hard-coded literals in `main.c` and do not go through `T()`**;
  only entry 0 ("Follow system") is translated.
- All three theme combo entries **do** follow the UI language (entries 0/1/2 are replaced on language switch).
  **This is the opposite of the language combo — do not copy one from the other.**
- **Never hard-code legend widths**: measure with `MeasureString` for the active language
  (English is far wider than Chinese; hard-coded widths overlap).

### 9.4 Cross-process UI verification (32-bit exe + 64-bit Python)

- `SendMessage` **does not marshal pointers** ⇒ to read combo items use only `CB_GETLBTEXTLEN`
  (`CB_GETLBTEXT` always returns `CB_ERR`).
- To trigger a real switch: `CB_SETCURSEL` plus posting `WM_COMMAND(MAKEWPARAM(id, CBN_SELCHANGE))`
  to the **combo's parent window**.
- ctypes calls **must declare `argtypes` explicitly**, or handles get truncated and
  `ReadProcessMemory` silently reads all zeros.
- Plain-text checks can use UTF-16LE binary substring search.

### 9.5 Adding a setting means wiring 4 places

`SaveAllConfig` / `LoadAllConfig` / `InitDefaults` / `RestoreAllDefaults` — miss one and you get
"settings lost after restart" or "restore defaults did nothing".

---

## 10. iperf3 Version Requirements

| Feature | Minimum version |
|---|---|
| Basic test | iperf3 3.0+ |
| `--bidir` | 3.7+ |
| `[Role]` field (TX-C / RX-C) | 3.17+ |

iperf3 lookup order (`FindIperf3`, first hit wins): ① the program's own directory and its subdirectories
(level-order BFS, skipping reparse points); ② directories on the system `PATH` (no subdirectories).
"Auto-detect" follows this order; "Browse" sets a path manually. Below 3.7, ticking "Bidirectional"
warns but does not block the run.

In bidir mode iperf3 v3.21 emits one separator line followed by 4 summary lines
(TX-C sender/receiver + RX-C sender/receiver); the program distinguishes the forward and reverse
channels by the `[Role]` field.

---

## 11. Antivirus False Positives

A statically linked MinGW exe has no digital signature, and the program `CreateProcess`es iperf3.exe,
redirects pipes and writes files — a pattern that trips heuristic detection in 360 Security Guard. Remedies:

1. Add `iperf3gui.exe` to 360's trusted list;
2. File a false-positive report at https://soft.360.cn/report/ (usually resolved in 1–3 days);
3. `app.rc` already embeds VERSIONINFO (CompanyName / ProductName / FileVersion) —
   right-click the exe → Properties → Details to inspect it.

---

## 12. Architecture

### 12.1 Process Model

- `CreateProcessA` launches iperf3.exe as a child process;
- stdout and stderr are **merged into a single inheritable anonymous pipe** (64 KB,
  `CreatePipe` + `STARTF_USESTDHANDLES`);
- the command line **internally appends `--forceflush`** (iperf3 3.7+) to force `fflush(stdout)`
  every interval; without it the C runtime switches to 4 KB full buffering when stdout is not a console,
  and the pipe receives nothing in real time;
  **`--forceflush` does not appear in the command line shown in the UI** (`BuildCommandLine` keeps
  separate display and exec parameter sets);
- `PipeReaderThread` uses an 8 KB buffer, splits on `\n`, strips `\r`, and posts each line to the main
  thread as `WM_PIPE_LINE`; on process exit it posts `WM_PIPE_DONE`;
- each line goes through `OnProcessLine` → `OnIperf3Line` (JSON state machine when the line starts with `{`,
  text parsing otherwise).

> An older version wrote a temp file via `--logfile` and tailed it with a 200 ms timer
> (`TIMER_CLIENT_TAIL` / `TIMER_SERVER_TAIL` / `TailLineCallback`). All of that is removed and those
> symbols no longer appear in the source. This document was corrected accordingly.

### 12.2 Chart Rendering

`chart.cpp` draws with GDI+ in the child window's `WM_PAINT`:

- Line + translucent fill; blue = client, green = server;
- `ChartData` is independent of `AppState` — one instance each for client, server and replay;
- Three data sources: `values[]` (sum; TX in bidir), `values2[]` (RX in bidir), `series[i]` (per-stream thin lines);
- Y range: by default from `values[]`; when the sum is hidden it scans the visible `series[]` for the max
  instead (otherwise the thin lines all bunch up in the bottom 1/N);
- The legend is owner-drawn and clickable to toggle each stream; English text is laid out using measured
  `MeasureString` widths.

### 12.3 Theme

- Reads `HKCU\Software\Microsoft\Windows\Themes\Personalize\AppsUseLightTheme` for the system theme;
- The settings-page combo (`auto` / `dark` / `light`) overrides it;
- On switch, `Theme_Apply` walks `EnumChildWindows` (recursively) + `RedrawWindow` for a full refresh,
  so grandchildren (e.g. the replay slider) are covered and new controls need no extra wiring;
- Colours come in two independent light/dark branches — **do not break dark while editing light**.

### 12.4 Replay Display Unit and the "Now:" Value

Replay Row 4 has a "Unit" combo after the speed combo (`CBO_REPLAY_UNIT=1511`), with the same 9 entries
as the client/server pages.

Data flow (`unit` / `yScale` travel all the way down; the log is never re-parsed):

```
CBO_REPLAY_UNIT (CBN_SELCHANGE)
  → Replay_ReadUnit()      writes g_state.replay.unit / .yScale
  → Replay_ApplyIndex(idx) → Replay_Seek()   copies unit/yScale into ChartData on every seek
  → chart.cpp              draws the Y-axis title from cd->yUnit, divides every point by cd->yScale
```

The "Now:" value row goes through `Replay_FormatRate()`, which converts using the selected unit.

> **Decision record (2026-10-16, user's call): the summary box does NOT follow `-f`.**
> The summary-box rate is derived from iperf3's raw bps via `FormatSpeed()` auto-ranging
> (G/M/K/bps) and **does not change its unit expression with `-f`**, whereas the chart Y axis and the raw
> output **do** follow `-f`. For this reason the replay value row deliberately **avoids the shared
> `FormatSpeed()`** (changing it would affect the summary box) and uses the local `Replay_FormatRate()`.
> This is a product decision, not a defect — do not "fix" it into consistency.

---

## 13. License

This program (iperf3 GUI) is licensed under the **MIT License**, **Copyright (c) 2026 BenjmSek**.

- Full terms in `LICENSE` at the repository root; the in-app "License" dialog embeds the same English
  text (trilingual summary + always-English terms).

### Third-party dependencies (not in this repo — bring your own)

This program **neither includes nor distributes** iperf3 or its runtime dependencies
(`iperf3.exe`, `cygwin1.dll`, `cygcrypto-3.dll`, `cygz.dll`). Obtain iperf3 yourself
(≥ 3.7 recommended, for `--bidir` support) and point the GUI at it on the Settings page.

| Component | License | Copyright holder |
|---|---|---|
| iperf3 | BSD-3-Clause | The Regents of the University of California, through **Lawrence Berkeley National Laboratory (LBNL)** |
| Cygwin runtime (`cygwin1.dll` etc.) | LGPL-3.0-or-later (with the Cygwin linking exception) | Red Hat / the Cygwin project |

Both are independent of this program's MIT license and neither infects it. Full terms:
[esnet/iperf](https://github.com/esnet/iperf) and [cygwin.com](https://cygwin.com/).

---

## 14. Change Log

| Item | Detail |
|---|---|
| Font sizes | Tab buttons 7 → 22 px (`s_hFontGroupNormal` / `s_hFontGroup` when selected); the 6 status-bar controls go back to `CreateLabel`'s default 20 px, dropping the dynamic bolding tied to the active Tab |
| Replay slider | New `SliderThemeSubclass` + `PaintReplaySlider` owner-draw; follows light theme (the dark branch stays dark, no white background) |
| Curve head | Fixed the multi-stream live sum "head dives down" bug: the new point is seeded with the previous interval's sum; covers both the non-bidir and bidir branches |
| New option | Settings page gained "Hide stream sum" (`CHK_HIDE_STREAM_SUM=1318`), greyed out unless "Show streams" is ticked; when active the sum line/area/legend entry are not drawn and the Y range comes from the streams |
| Legend | Replaced 6 hard-coded widths with measured `MeasureString`; English labels `Summary`→`Sum`, `Stream`→`STRM`, `Bandwidth`→`BW` |
| Dead code | Removed `JsonGetBool()`, `seriesName[]`, `LBL_SETTINGS_DETECTED`; fixed stale comments in `chart.h` and section headers in `main.c` |
| Cleanup | Temp files / backups / one-off probes removed (`test/` keeps only `run_autotest.py` + `autotest.ini`) |

### 2026-10-16

| Item | Detail |
|---|---|
| License | Unified on **MIT License** (previously `license_text.c` claimed BSD-3-Clause while `app.rc` said "MIT License"). Dialog body, trilingual summary and verbatim terms all replaced with MIT |
| Attribution | `2BorNot2B` → **BenjmSek** (window title, VERSIONINFO, License dialog, README) |
| Default IP | `DEF_CLIENT_IP` changed from an internal address to **`127.0.0.1`** (control initial value and help-text examples included) |
| Docs | README split into **two complete documents**: `README.md` (Chinese) and `README_EN.md` (English). Previously English had only a top Quick Start and all 14 sections were Chinese-only |
| Regression | Rebuilt `test/autotest.ini` (17 cases) + `test/run_autotest.py` orchestrator; measured 16 PASS / 0 WARN / 1 FAIL |
| Regression env | AutoTest gained a `parallel` case key (previously it inherited the concurrency from `settings.ini`, so results drifted with user config); UDP cases all use `parallel=1` |
| Replay unit | Replay page gained a "Unit" combo `CBO_REPLAY_UNIT=1511` (9 entries, default auto = Mb/s), persisted as `[replay] unit`; the "Now:" value row follows it |
| Doc corrections | §12.1 process model (the old `--logfile` + tail approach is gone), §5 settings.ini path, §7 line counts, §10 lookup order, §3 directory tree |
