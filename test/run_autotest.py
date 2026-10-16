#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_autotest.py — iperf3 GUI 回归测试编排器

流程：
  1. 读 test/autotest.ini 里的全部 [case_NN] 作为用例源；
  2. 按 --start-case / --max-cases 切片，重编号成 case_00… 后**回写** autotest.ini
     （GUI 固定读 <exe目录>\\test\\autotest.ini，且总是从 case_00 开始跑），
     并按 --duration 重写时长；运行结束（含异常）在 finally 里还原原文件；
  3. 为 client 用例拉起常驻 iperf3 服务端（iperf3 3.x 的 -s 可连续服务多个客户端，
     已实测确认）；
  4. 启动 iperf3gui.exe --autotest（隐藏窗口），跑完自动退出；
  5. 解析 test/results/ 下每用例的 cmd / raw / summary 三个 dump，判定 PASS/WARN/FAIL，
     输出控制台表格 + report.html。

校验锚点说明：汇总框正文取自 lang.c 的 STR_FMT_BLOCK_SENDER，简体中文是
"[发送]\\r\\n方向: …"。所以界面语言必须是简体中文（繁体是"[傳送]"、英文是"[Sender]"），
否则文本模式用例会判 FAIL —— 这里会直接检出并给出明确原因，而不是默默全红。

用法：
  python test/run_autotest.py [--duration 5] [--server-ip 127.0.0.1]
                              [--start-case 0] [--max-cases 17]
                              [--server-port 5201] [--no-server]
"""
import argparse
import html
import re
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "iperf3gui.exe"
INI = ROOT / "test" / "autotest.ini"
RESULTS = ROOT / "test" / "results"
TOOLS = ROOT / "tools"
IPERF3 = TOOLS / "iperf3.exe"

DEFAULTS = {
    "mode": "client", "proto": "tcp", "format": "", "ip": "", "port": "5201",
    "duration": "5", "interval": "1", "bw": "", "extra": "", "num": "",
    "omit": "0", "reverse": "0", "bidir": "0", "json": "0",
    "getserver": "0", "oneoff": "0", "verbose": "0",
}


# ---------------------------------------------------------------- ini 读写
def parse_cases(text):
    """返回 ([autotest] dict, [(段名, case dict) …])，保持文件里的原始顺序。"""
    meta, cases = {}, []
    cur = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith(("#", ";")):
            continue
        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            if name == "autotest":
                cur = meta
            elif re.fullmatch(r"case_\d+", name):
                cur = {}
                cases.append((name, cur))
            else:
                cur = None
        elif cur is not None and "=" in line:
            k, v = line.split("=", 1)
            cur[k.strip()] = v.strip()
    return meta, cases


def render_ini(meta, cases):
    out = ["[autotest]",
           "case_count = %d" % len(cases),
           "server_ip = %s" % meta.get("server_ip", "127.0.0.1"),
           ""]
    for idx, (_name, c) in enumerate(cases):
        out.append("[case_%02d]" % idx)
        for k in ("mode", "proto", "format", "ip", "port", "duration", "interval",
                  "bw", "extra", "num", "omit", "reverse", "bidir", "json",
                  "getserver", "oneoff", "verbose"):
            v = c.get(k, DEFAULTS[k])
            if v:
                out.append("%s = %s" % (k, v))
        out.append("")
    return "\n".join(out) + "\n"


def field(case, key):
    return case.get(key, DEFAULTS[key])


# ------------------------------------------------------------ iperf3 服务端
def wait_port(port, timeout=15.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection(("127.0.0.1", int(port)), 0.5):
                return True
        except OSError:
            time.sleep(0.3)
    return False


class ServerFarm:
    """为 client 用例拉起常驻 iperf3 服务端（按端口去重）。"""

    def __init__(self, ports):
        self.procs = []
        self.ports = ports

    def start(self):
        for port in self.ports:
            # stdin 给管道而不是 DEVNULL：Cygwin 版 iperf3 见到无效 stdin 会抱怨
            # 「不支持输入重新定向」（不致命，但没必要招惹）。
            p = subprocess.Popen(
                [str(IPERF3), "-s", "-p", str(port)],
                cwd=str(TOOLS), stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            threading.Thread(target=self._drain, args=(p,), daemon=True).start()
            self.procs.append(p)
            if not wait_port(port):
                raise RuntimeError("iperf3 服务端未能在端口 %s 起来" % port)

    @staticmethod
    def _drain(p):
        try:
            for _ in p.stdout:
                pass
        except Exception:
            pass

    def stop(self):
        for p in self.procs:
            if p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    p.kill()
            for stream in (p.stdin, p.stdout):
                try:
                    if stream:
                        stream.close()
                except Exception:
                    pass


# ------------------------------------------------------------------- 判定
def read_dump(path):
    if not path.exists():
        return None
    return path.read_text(encoding="utf-8-sig", errors="replace")


def detect_lang(summary):
    """从汇总框正文反推界面语言；返回 (语言, 锚点是否可用)。

    注意「接收」在简体与繁体里字形相同，只有「发送 / 傳送」「Sender」能区分语言。
    服务端只做接收，汇总框里没有发送块，因此单看服务端 dump 分不出简繁 ——
    这时按「锚点可用」处理（[接收] 两语通用），不做语言强判定。
    """
    if not summary:
        return "unknown", False
    if "[發送]" in summary or "[发送]" in summary:
        return "zh-CN", True
    if "[傳送]" in summary:
        return "zh-TW", True          # 繁体：锚点用 [接收]，与简体同形，仍可用
    if "[接收]" in summary:
        return "zh-CN/zh-TW", True    # 服务端：两语同形，够用
    if "[Sender]" in summary or "[Receiver]" in summary:
        return "en", False            # 英文下 [发送]/方向: 锚点不可用
    return "unknown", False


# iperf3 原文单位 → bps（键一律小写；iperf3 会随 -f 输出 K/M/G/Tbits/sec）
RAW_UNITS = {"bits/sec": 1.0, "kbits/sec": 1e3, "mbits/sec": 1e6,
             "gbits/sec": 1e9, "tbits/sec": 1e12}
# GUI 汇总框单位 → bps（FormatSpeed 恒自动换算，与 iperf3 的 -f 无关）
SUM_UNITS = {"bps": 1.0, "kbps": 1e3, "mbps": 1e6, "gbps": 1e9, "tbps": 1e12}

RAW_SUM_RE = re.compile(
    r"\[SUM\](?:\[([A-Za-z\-]+)\])?[^\r\n]*?\s([\d.]+)\s"
    r"(bits/sec|Kbits/sec|Mbits/sec|Gbits/sec|Tbits/sec)\s+sender", re.I)
# 单流（-P 1）时 iperf3 不输出 [SUM] 行，只有 [ ID] 那条流本身。
RAW_STREAM_RE = re.compile(
    r"\[\s*\d+\](?:\[([A-Za-z\-]+)\])?[^\r\n]*?\s([\d.]+)\s"
    r"(bits/sec|Kbits/sec|Mbits/sec|Gbits/sec|Tbits/sec)\s+sender", re.I)
SUM_RATE_RE = re.compile(r"速率:\s*([\d.]+)\s*(bps|Kbps|Mbps|Gbps|Tbps)")


def raw_sender_bps(raw, role=None):
    """取原始输出里 sender 侧的汇总速率（bps）。

    优先 [SUM] … sender；单流（parallel=1）时 iperf3 不打 [SUM]，退回最后一条
    [ ID] … sender（此时该流即全部流量，与 README 铁律「单对流不打 [SUM]」一致）。
    bidir 时原文有多条 [SUM][TX-C] / [SUM][RX-C]，必须按角色挑对应的那条，
    否则会拿反向通道的值去比正向通道的汇总框。role=None 表示非 bidir。
    """
    hits = RAW_SUM_RE.findall(raw or "")
    if not hits:
        hits = RAW_STREAM_RE.findall(raw or "")
    if not hits:
        return None
    if role:
        hits = [h for h in hits if (h[0] or "").upper() == role.upper()] or hits
    val, unit = hits[-1][1], hits[-1][2]
    return float(val) * RAW_UNITS[unit.lower()]


def json_sender_bps(raw):
    """JSON 模式：取 "sum_sent" 里的 bits_per_second（bps）。

    只认 sum_sent 之后的第一个 bits_per_second；不扫 "sum_received" 之后的部分
    （iperf3 3.21 的 sum_received.sender 在 TCP 下恒为 true，方向得靠结构区分）。
    """
    i = (raw or "").find('"sum_sent"')
    if i < 0:
        return None
    j = (raw or "").find('"sum_received"', i)
    seg = (raw or "")[i: j if j > 0 else None]
    m = re.search(r'"bits_per_second"\s*:\s*([-\d.eE+]+)', seg)
    return float(m.group(1)) if m else None


def judge(case, dumps):
    """返回 (status, errors, warns)，status ∈ PASS / WARN / FAIL。"""
    errors, warns = [], []
    cmd, raw, summary = dumps
    json_mode = field(case, "json") == "1"

    for name, text in (("cmd", cmd), ("raw", raw)):
        if text is None:
            errors.append("%s dump 缺失" % name)
        elif not text.strip():
            errors.append("%s dump 为空" % name)

    if raw is None:
        return "FAIL", errors, warns

    if "[error]" in raw:
        errors.append("原始输出里出现 [error]")

    if json_mode:
        if '"intervals"' not in raw:
            errors.append("JSON 输出里没有 intervals 字段")
    elif "bits/sec" not in raw and "Bytes" not in raw:
        errors.append("原始输出里没有区间/汇总行（bits/sec 或 Bytes）")

    # 汇总框：实测 JSON 模式也会填汇总框（走 ParseIperf3JsonLine 的 sum_sent /
    # sum_received），所以两种模式统一校验，只有取速率的源不同。
    lang, anchor_ok = detect_lang(summary)
    is_server = field(case, "mode") == "server"
    if not summary or not summary.strip():
        errors.append("汇总框为空（界面语言=%s）" % lang)
    elif not anchor_ok:
        errors.append("界面语言不支持（检出 %s），锚点 [发送]/[接收] 不可用" % lang)
    elif not is_server and lang != "zh-CN":
        errors.append("客户端用例要求界面语言为简体中文（检出 %s）；"
                      "繁体是「傳送」、英文是「Sender」，锚点对不上" % lang)
        # 服务端只做接收，汇总框天然只有 [接收] 块（见 case_14 实测输出），
        # 因此服务端用 [接收] 作锚点，客户端才要求 [发送]。
        anchor = "[接收]" if is_server else "[发送]"
        if anchor not in summary or "方向:" not in summary:
            errors.append("汇总框缺 %s 或 方向: 锚点" % anchor)
        if field(case, "bidir") == "1" and summary.count("[发送]") < 2:
            warns.append("bidir 用例只解析出 %d 个发送块（期望 ≥2）"
                         % summary.count("[发送]"))
        # 数值交叉校验：汇总框里该锚点块的速率 vs 原始输出的对应值，两边换算到
        # bps 再比，容差 2%（兜住各自的 %.2f / 5 位有效数字取整）。
        #   文本模式：取 [SUM] … sender，单流时退回 [ ID] … sender；
        #   JSON 模式：取 "sum_sent" 里的 bits_per_second。
        m = SUM_RATE_RE.search(summary)
        rb = (json_sender_bps(raw) if json_mode
              else raw_sender_bps(raw, "TX-C" if field(case, "bidir") == "1" else None))
        if m and rb:
            sb = float(m.group(1)) * SUM_UNITS[m.group(2).lower()]
            if abs(sb - rb) / rb > 0.02:
                errors.append("汇总速率 %.4g bps 与原文 %.4g bps 偏差 >2%%"
                              % (sb, rb))
        elif m and raw and ("sender" in raw or "sum_sent" in raw):
            warns.append("原始输出里取不到 sender 侧速率，跳过数值交叉校验")

    return ("FAIL" if errors else ("WARN" if warns else "PASS")), errors, warns


# ------------------------------------------------------------------- 报告
def write_report(rows, path):
    cells = []
    for r in rows:
        detail = html.escape("；".join(r["errors"] + r["warns"]) or "-")
        cells.append(
            "<tr class='%s'><td>case_%02d</td><td>%s</td><td>%s</td><td><b>%s</b></td>"
            "<td>%s</td></tr>" % (
                r["status"].lower(), r["idx"], html.escape(r["desc"]),
                html.escape(r["mode"]), r["status"], detail))
    # CSS 里带 %，这里不能走 %-formatting，只能拼字符串
    css = ("<style>body{font-family:'Segoe UI',system-ui,sans-serif;margin:24px}"
           "table{border-collapse:collapse;width:100%}"
           "th,td{border:1px solid #ccc;padding:6px 10px;text-align:left;"
           "vertical-align:top}th{background:#f2f2f2}"
           "tr.pass td{background:#eaf7ec}tr.fail td{background:#fdeaea}"
           "tr.warn td{background:#fff7e6}</style>")
    path.write_text(
        "<!doctype html><meta charset='utf-8'><title>iperf3 GUI 回归测试</title>"
        + css +
        "<h2>iperf3 GUI 回归测试</h2><p>"
        + html.escape(time.strftime("%Y-%m-%d %H:%M:%S"))
        + "</p>"
        "<table><tr><th>用例</th><th>参数</th><th>模式</th><th>结果</th><th>说明</th></tr>"
        + "".join(cells) + "</table>",
        encoding="utf-8")


# -------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description="iperf3 GUI 回归测试编排器")
    ap.add_argument("--duration", type=int, default=5, help="每个用例的时长秒（默认 5）")
    ap.add_argument("--server-ip", default=None, help="覆盖 autotest.ini 的 server_ip")
    ap.add_argument("--server-port", default=None, help="覆盖 client 用例的端口")
    ap.add_argument("--start-case", type=int, default=0, help="从哪个用例开始（默认 0）")
    ap.add_argument("--max-cases", type=int, default=None, help="最多跑几个用例")
    ap.add_argument("--no-server", action="store_true", help="不自动拉起 iperf3 服务端")
    args = ap.parse_args()

    if not EXE.exists():
        sys.exit("找不到 %s，先执行 build.bat" % EXE)
    if not IPERF3.exists():
        sys.exit("找不到 %s；GUI 默认路径与 server 用例都要用它" % IPERF3)

    original = INI.read_text(encoding="utf-8")
    meta, all_cases = parse_cases(original)
    if not all_cases:
        sys.exit("autotest.ini 里没有 [case_NN] 用例")

    if args.server_ip:
        meta["server_ip"] = args.server_ip

    selected = all_cases[args.start_case:]
    if args.max_cases is not None:
        selected = selected[:args.max_cases]
    if not selected:
        sys.exit("--start-case / --max-cases 切片后没有用例")

    # 时长重写：-n 与 -t 互斥（iperf3: only one test end condition），num 用例只能保留
    # duration=10 —— GUI 在 duration==10 时不输出 -t。
    for _name, c in selected:
        if not c.get("num"):
            c["duration"] = str(args.duration)
        if args.server_port:
            c["port"] = args.server_port

    client_ports = sorted({field(c, "port") for _n, c in selected
                           if field(c, "mode") == "client"})
    farm = ServerFarm(client_ports)

    # 清掉上一轮的 dump（只删自己命名的三类文件）
    RESULTS.mkdir(parents=True, exist_ok=True)
    for pat in ("case_*_summary.txt", "case_*_raw.txt", "case_*_cmd.txt"):
        for f in RESULTS.glob(pat):
            f.unlink()

    timeout = 60
    for _name, c in selected:
        dur = int(field(c, "duration")) if field(c, "duration").isdigit() else 10
        timeout += dur + (25 if field(c, "mode") == "server" else 12)

    try:
        INI.write_text(render_ini(meta, selected), encoding="utf-8")
        if client_ports and not args.no_server:
            print("[setup] 起 iperf3 服务端，端口 %s" % ",".join(client_ports))
            farm.start()
        print("[run] %s --autotest （%d 用例，超时 %ds）"
              % (EXE.name, len(selected), timeout))
        t0 = time.time()
        proc = subprocess.run([str(EXE), "--autotest"], cwd=str(ROOT),
                              timeout=timeout,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print("[run] GUI 退出码 %d，用时 %.1fs" % (proc.returncode, time.time() - t0))
    except subprocess.TimeoutExpired:
        print("[run] 超时！GUI 未能在 %ds 内跑完" % timeout)
    finally:
        farm.stop()
        INI.write_text(original, encoding="utf-8")

    rows = []
    for idx, (_name, c) in enumerate(selected):
        dumps = (read_dump(RESULTS / ("case_%02d_cmd.txt" % idx)),
                 read_dump(RESULTS / ("case_%02d_raw.txt" % idx)),
                 read_dump(RESULTS / ("case_%02d_summary.txt" % idx)))
        status, errors, warns = judge(c, dumps)
        desc = " ".join(x for x in (
            field(c, "proto").upper(),
            "-f " + field(c, "format") if field(c, "format") else "",
            "-b " + field(c, "bw") if field(c, "bw") else "",
            "-n " + field(c, "num") if field(c, "num") else "",
            "reverse" if field(c, "reverse") == "1" else "",
            "bidir" if field(c, "bidir") == "1" else "",
            "json" if field(c, "json") == "1" else "",
            "getserver" if field(c, "getserver") == "1" else "",
            "omit=" + field(c, "omit") if field(c, "omit") not in ("", "0") else "",
            "oneoff" if field(c, "oneoff") == "1" else "",
            "verbose" if field(c, "verbose") == "1" else "",
        ) if x)
        rows.append(dict(idx=idx, desc=desc or "-", mode=field(c, "mode"),
                         status=status, errors=errors, warns=warns))
        print("  case_%02d  %-4s %-52s %s%s" % (
            idx, status, desc or "-", errors[0] if errors else "",
            (" | " + warns[0]) if warns and not errors else ""))

    n_pass = sum(r["status"] == "PASS" for r in rows)
    n_warn = sum(r["status"] == "WARN" for r in rows)
    n_fail = sum(r["status"] == "FAIL" for r in rows)
    print("\n合计 %d 用例：%d PASS / %d WARN / %d FAIL"
          % (len(rows), n_pass, n_warn, n_fail))

    report = RESULTS / "report.html"
    write_report(rows, report)
    print("报告：%s" % report)
    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
