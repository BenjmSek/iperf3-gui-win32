/*
 * lang.c — 多语言串表与接口
 *
 * 本文件由 build-i18n/gen_lang.py 自动生成，请勿手改。
 * 串表数据源：build-i18n/i18n_strings.py   （生成模式：简/繁/英 三语）
 */
#include "lang.h"

/* kStr[id][lang]：lang 顺序 = LANG_ZH_CN / LANG_ZH_TW / LANG_EN */
static const wchar_t* kStr[STR_COUNT][LANG_COUNT] = {
    /* STR_TAB_CLIENT                   */ { L"客户端", L"用戶端", L"Client" },
    /* STR_TAB_SERVER                   */ { L"服务端", L"伺服器", L"Server" },
    /* STR_TAB_REPLAY                   */ { L"回放", L"重播", L"Replay" },
    /* STR_TAB_SETTINGS                 */ { L"设置", L"設定", L"Settings" },
    /* STR_BTN_SAVE_CONFIG              */ { L"保存配置", L"儲存設定", L"Save Config" },
    /* STR_BTN_LOAD_CONFIG              */ { L"读取配置", L"讀取設定", L"Load Config" },
    /* STR_BTN_RESTORE_DEFAULT          */ { L"恢复默认", L"恢復預設", L"Restore Defaults" },
    /* STR_STATUS_CLIENT                */ { L"客户端:", L"用戶端：", L"Client:" },
    /* STR_STATUS_SERVER                */ { L"服务端:", L"伺服器：", L"Server:" },
    /* STR_STATUS_RUNNING               */ { L"运行", L"執行中", L"Running" },
    /* STR_STATUS_IDLE                  */ { L"空闲", L"閒置", L"Idle" },
    /* STR_LBL_SERVER_IP                */ { L"服务端 IP", L"伺服器 IP", L"Server IP" },
    /* STR_LBL_PORT                     */ { L"端口", L"連接埠", L"Port" },
    /* STR_LBL_PROTO                    */ { L"协议", L"協定", L"Protocol" },
    /* STR_LBL_UNIT                     */ { L"显示单位", L"顯示單位", L"Unit" },
    /* STR_LBL_TRANSFER                 */ { L"传输量(Byte)", L"傳輸量(Byte)", L"Transfer (Byte)" },
    /* STR_LBL_DURATION                 */ { L"时长(秒)", L"時長(秒)", L"Duration (s)" },
    /* STR_LBL_INTERVAL                 */ { L"间隔(秒)", L"間隔(秒)", L"Interval (s)" },
    /* STR_LBL_OMIT                     */ { L"防抖(秒)", L"防抖(秒)", L"Omit (s)" },
    /* STR_LBL_BANDWIDTH                */ { L"带宽", L"頻寬", L"Bandwidth" },
    /* STR_LBL_PARALLEL                 */ { L"多流", L"多流", L"Streams" },
    /* STR_LBL_EXTRA                    */ { L"手工参数", L"手動參數", L"Extra Args" },
    /* STR_CHK_REVERSE                  */ { L"反向", L"反向", L"Reverse" },
    /* STR_CHK_BIDIR                    */ { L"双向", L"雙向", L"Bidir" },
    /* STR_CHK_GETSERVER                */ { L"服务端报告", L"伺服器報告", L"Server Report" },
    /* STR_BTN_START                    */ { L"开始测试", L"開始測試", L"Start" },
    /* STR_BTN_STOP                     */ { L"停止", L"停止", L"Stop" },
    /* STR_LBL_CMD                      */ { L"命令:", L"命令：", L"Command:" },
    /* STR_LBL_SUMMARY                  */ { L"汇总", L"彙總", L"Sum" },
    /* STR_LBL_RAW_OUTPUT               */ { L"原始输出", L"原始輸出", L"Raw Output" },
    /* STR_BTN_CLEAR                    */ { L"清空", L"清除", L"Clear" },
    /* STR_BTN_COPY_ALL                 */ { L"复制全部", L"複製全部", L"Copy All" },
    /* STR_CHK_AUTOSCROLL               */ { L"自动滚动", L"自動捲動", L"Auto-scroll" },
    /* STR_LBL_LISTEN_PORT              */ { L"监听端口", L"監聽連接埠", L"Listen Port" },
    /* STR_LBL_AF                       */ { L"协议版本", L"IP 版本", L"IP Version" },
    /* STR_CHK_ONEOFF                   */ { L"仅一次", L"僅一次", L"One-off" },
    /* STR_CHK_VERBOSE                  */ { L"详细输出", L"詳細輸出", L"Verbose" },
    /* STR_BTN_START_SERVER             */ { L"启动服务端", L"啟動伺服器", L"Start Server" },
    /* STR_BTN_STOP_SERVER              */ { L"停止服务端", L"停止伺服器", L"Stop Server" },
    /* STR_LBL_IPERF3_PATH              */ { L"iperf3路径", L"iperf3 路徑", L"iperf3 Path" },
    /* STR_BTN_BROWSE                   */ { L"浏览", L"瀏覽", L"Browse" },
    /* STR_BTN_AUTODETECT               */ { L"自动检测", L"自動偵測", L"Auto-detect" },
    /* STR_LBL_LOG_SECTION              */ { L"日志", L"記錄", L"Logging" },
    /* STR_CHK_LOG_ENABLE               */ { L"启用日志", L"啟用記錄", L"Enable Logging" },
    /* STR_LBL_LOG_DIR                  */ { L"日志目录", L"記錄資料夾", L"Log Folder" },
    /* STR_BTN_BROWSE_DIR               */ { L"浏览文件夹", L"瀏覽資料夾", L"Browse Folder" },
    /* STR_NAMING_RULE                  */ { L"命名规则", L"命名規則", L"Naming Rule" },
    /* STR_LBL_LOG_MAXSIZE              */ { L"最大大小", L"大小上限", L"Max Size" },
    /* STR_LBL_CHART_SECTION            */ { L"图表", L"圖表", L"Chart" },
    /* STR_CHK_SHOW_STREAMS             */ { L"显示各流（多流测试时叠加各条流细线）", L"顯示各流（多流測試時疊加各條流細線）", L"Show streams (overlay per-stream lines for multi-stream tests)" },
    /* STR_BTN_BROWSE_ELLIPSIS          */ { L"浏览…", L"瀏覽…", L"Browse…" },
    /* STR_REPLAY_NO_FILE               */ { L"未载入日志文件", L"未載入記錄檔", L"No log file loaded" },
    /* STR_BTN_PLAY                     */ { L"▶ 播放", L"▶ 播放", L"▶ Play" },
    /* STR_BTN_STOP_PLAY                */ { L"■ 停止", L"■ 停止", L"■ Stop" },
    /* STR_LBL_SPEED                    */ { L"倍速:", L"倍速：", L"Speed:" },
    /* STR_MSG_SELECT_LOG_FILE          */ { L"请先选择或输入日志文件路径", L"請先選擇或輸入記錄檔路徑", L"Please select or enter a log file path" },
    /* STR_REPLAY_TIME_INIT             */ { L"时间: 0.0 / 0.0 s", L"時間：0.0 / 0.0 s", L"Time: 0.0 / 0.0 s" },
    /* STR_COPIED_TO_CLIPBOARD          */ { L"已复制输出到剪贴板", L"已複製輸出到剪貼簿", L"Output copied to clipboard" },
    /* STR_COPY_FAILED                  */ { L"复制失败：无法访问剪贴板", L"複製失敗：無法存取剪貼簿", L"Copy failed: cannot access the clipboard" },
    /* STR_HELP_NAMING_TITLE            */ { L"命名规则说明", L"命名規則說明", L"Naming Rule Help" },
    /* STR_HELP_NAMING_BODY             */ { L"命名规则占位符:\n  %ip   - IP 地址\n  %mode - 模式 (client/server)\n  %y    - 两位数年 (26)\n  %Y    - 四位数年 (2026)\n  %M    - 月 (01-12)\n  %D    - 日 (01-31)\n  %h    - 时 (00-23)\n  %m    - 分 (00-59)\n  %s    - 秒 (00-59)\n\n其他字符原样保留, 如 - . _\n\n示例: %mode-%ip-%Y%M%D-%h%m%s.log\n      -> server-127.0.0.1-20260919-2146.log", L"命名規則佔位符:\n  %ip   - IP 位址\n  %mode - 模式 (client/server)\n  %y    - 兩位數年 (26)\n  %Y    - 四位數年 (2026)\n  %M    - 月 (01-12)\n  %D    - 日 (01-31)\n  %h    - 時 (00-23)\n  %m    - 分 (00-59)\n  %s    - 秒 (00-59)\n\n其他字元原樣保留, 如 - . _\n\n示例: %mode-%ip-%Y%M%D-%h%m%s.log\n      -> server-127.0.0.1-20260919-2146.log", L"Naming Rule Placeholders:\n  %ip   - IP address\n  %mode - mode (client/server)\n  %y    - 2-digit year (26)\n  %Y    - 4-digit year (2026)\n  %M    - month (01-12)\n  %D    - day (01-31)\n  %h    - hour (00-23)\n  %m    - minute (00-59)\n  %s    - second (00-59)\n\nOther characters are kept as-is, e.g. - . _\n\nExample: %mode-%ip-%Y%M%D-%h%m%s.log\n      -> server-127.0.0.1-20260919-2146.log" },
    /* STR_FMT_STREAM                   */ { L"流%d", L"流 %d", L"STRM %d" },
    /* STR_FMT_BANDWIDTH                */ { L"带宽 %ls", L"頻寬 %ls", L"BW %ls" },
    /* STR_FMT_PROGRAM_PATH             */ { L"程序路径: %ls", L"程式路徑：%ls", L"Program path: %ls" },
    /* STR_FMT_PROGRAM_PATH_MISSING     */ { L"程序路径: 未找到 iperf3.exe", L"程式路徑：找不到 iperf3.exe", L"Program path: iperf3.exe not found" },
    /* STR_FMT_SUMMARY_HDR              */ { L"=== %ls (通道 %d) ===\r\n", L"=== %ls（通道 %d）===\r\n", L"=== %ls (stream %d) ===\r\n" },
    /* STR_FMT_SUMMARY_HDR2             */ { L"=== 通道 %d ===\r\n", L"=== 通道 %d ===\r\n", L"=== Stream %d ===\r\n" },
    /* STR_FMT_BLOCK_SENDER             */ { L"[发送]\r\n方向: %ls\r\n速率: %ls\r\n传输量: %ls\r\n重传: %d\r\nJitter: %.3f ms\r\n丢包: %d/%d (%.1f%%)\r\n", L"[傳送]\r\n方向: %ls\r\n速率: %ls\r\n傳輸量: %ls\r\n重傳: %d\r\nJitter: %.3f ms\r\n封包遺失: %d/%d (%.1f%%)\r\n", L"[Sender]\r\nDir: %ls\r\nRate: %ls\r\nTransfer: %ls\r\nRetr: %d\r\nJitter: %.3f ms\r\nLoss: %d/%d (%.1f%%)\r\n" },
    /* STR_FMT_BLOCK_RECEIVER           */ { L"[接收]\r\n方向: %ls\r\n速率: %ls\r\n传输量: %ls\r\n重传: %d\r\nJitter: %.3f ms\r\n丢包: %d/%d (%.1f%%)\r\n", L"[接收]\r\n方向: %ls\r\n速率: %ls\r\n傳輸量: %ls\r\n重傳: %d\r\nJitter: %.3f ms\r\n封包遺失: %d/%d (%.1f%%)\r\n", L"[Receiver]\r\nDir: %ls\r\nRate: %ls\r\nTransfer: %ls\r\nRetr: %d\r\nJitter: %.3f ms\r\nLoss: %d/%d (%.1f%%)\r\n" },
    /* STR_FMT_REPLAY_TIME              */ { L"时间: %.1f / %.1f s", L"時間：%.1f / %.1f s", L"Time: %.1f / %.1f s" },
    /* STR_FMT_CUR_UDP                  */ { L"当前: %ls   抖动 %.2f ms   丢包 %.1f%%", L"目前：%ls   抖動 %.2f ms   封包遺失 %.1f%%", L"Now: %ls   Jitter %.2f ms   Loss %.1f%%" },
    /* STR_FMT_CUR_DGRAM                */ { L"当前: %ls   数据报 %.0f", L"目前：%ls   資料包 %.0f", L"Now: %ls   Datagrams %.0f" },
    /* STR_FMT_CUR_PLAIN                */ { L"当前: %ls", L"目前：%ls", L"Now: %ls" },
    /* STR_FMT_REPLAY_INFO_OVER         */ { L"%ls | %ls · %d 个切片 · %.1f s · %d 流(超限仅汇总)", L"%ls | %ls · %d 個切片 · %.1f s · %d 流（超限僅彙總）", L"%ls | %ls · %d samples · %.1f s · %d streams (summary only)" },
    /* STR_FMT_REPLAY_INFO              */ { L"%ls | %ls · %d 个切片 · %.1f s · %d 流", L"%ls | %ls · %d 個切片 · %.1f s · %d 流", L"%ls | %ls · %d samples · %.1f s · %d streams" },
    /* STR_FMT_REPLAY_INFO_SHORT        */ { L"%ls | %ls · %d 个切片 · %.1f s", L"%ls | %ls · %d 個切片 · %.1f s", L"%ls | %ls · %d samples · %.1f s" },
    /* STR_MODE_JSON                    */ { L"JSON 模式", L"JSON 模式", L"JSON mode" },
    /* STR_MODE_TEXT                    */ { L"文本模式", L"文字模式", L"Text mode" },
    /* STR_ERR_VERSION_LOW              */ { L"[error] iperf3 版本 %s 过低，需 3.7+（支持 --forceflush）", L"[error] iperf3 版本 %s 過低，需 3.7+（支援 --forceflush）", L"[error] iperf3 version %s is too old; 3.7+ required (for --forceflush)" },
    /* STR_ERR_SPAWN_CLIENT             */ { L"[error] 启动 iperf3 失败 (Win32 错误码 %lu)", L"[error] 啟動 iperf3 失敗 (Win32 錯誤碼 %lu)", L"[error] Failed to start iperf3 (Win32 error %lu)" },
    /* STR_ERR_SPAWN_SERVER             */ { L"[error] 启动 iperf3 服务端失败 (Win32 错误码 %lu)", L"[error] 啟動 iperf3 伺服器失敗 (Win32 錯誤碼 %lu)", L"[error] Failed to start iperf3 server (Win32 error %lu)" },
    /* STR_INFO_CLEANUP_PORT            */ { L"[cleanup] 已清理端口 %s 上的残留进程", L"[cleanup] 已清理連接埠 %s 上的殘留處理程序", L"[cleanup] Killed stale process on port %s" },
    /* STR_ERR_LOAD_FAILED              */ { L"载入失败：%ls", L"載入失敗：%ls", L"Load failed: %ls" },
    /* STR_ERR_OOM                      */ { L"内存不足", L"記憶體不足", L"Out of memory" },
    /* STR_ERR_JSON_NO_INTERVALS        */ { L"JSON 中未找到 intervals 数组", L"JSON 中找不到 intervals 陣列", L"\"intervals\" array not found in JSON" },
    /* STR_ERR_JSON_EMPTY_INTERVALS     */ { L"JSON 中 intervals 数组为空", L"JSON 中 intervals 陣列為空", L"\"intervals\" array is empty in JSON" },
    /* STR_ERR_JSON_NO_DATA             */ { L"JSON 中未解析到 interval 数据", L"JSON 中未解析到 interval 資料", L"No interval data parsed from JSON" },
    /* STR_ERR_PATH_EMPTY               */ { L"路径为空", L"路徑為空", L"Path is empty" },
    /* STR_ERR_OPEN_FILE                */ { L"无法打开文件", L"無法開啟檔案", L"Cannot open file" },
    /* STR_ERR_FILE_EMPTY               */ { L"文件为空", L"檔案為空", L"File is empty" },
    /* STR_ERR_NO_INTERVAL_DATA         */ { L"未解析到 interval 数据", L"未解析到 interval 資料", L"No interval data parsed" },
    /* STR_OUT_HDR_CLIENT_SUM           */ { L"═══ 客户端汇总 ═══\r\n", L"═══ 用戶端彙總 ═══\r\n", L"═══ Client Summary ═══\r\n" },
    /* STR_OUT_HDR_SERVER_REPORT        */ { L"═══ 服务端报告 ═══\r\n", L"═══ 伺服器報告 ═══\r\n", L"═══ Server Report ═══\r\n" },
    /* STR_HINT_SELECTED_BY_BROWSE      */ { L"已通过浏览选择", L"已透過瀏覽選擇", L"Selected via Browse" },
    /* STR_HINT_NOT_FOUND               */ { L"未找到", L"找不到", L"Not found" },
    /* STR_WARN_BW_INVALID              */ { L"[warn] 带宽(-b) 无效，已忽略（应形如 100M）", L"[warn] 頻寬(-b) 無效，已忽略（應形如 100M）", L"[warn] Invalid bandwidth (-b), ignored (expected e.g. 100M)" },
    /* STR_ERR_NO_SERVER_IP             */ { L"[error] 请填写服务端 IP", L"[error] 請填寫伺服器 IP", L"[error] Server IP is required" },
    /* STR_INFO_BIDIR_37                */ { L"[info] --bidir 需要 iperf3 3.7+；若启动失败请取消该选项", L"[info] --bidir 需要 iperf3 3.7+；若啟動失敗請取消該選項", L"[info] --bidir requires iperf3 3.7+; clear it if startup fails" },
    /* STR_WARN_BW_UDP_ONLY             */ { L"[warn] 带宽(-b) 仅 UDP 生效，当前 TCP 测试已忽略", L"[warn] 頻寬(-b) 僅 UDP 生效，目前 TCP 測試已忽略", L"[warn] Bandwidth (-b) applies to UDP only; ignored for this TCP test" },
    /* STR_ERR_NO_IPERF3_SETTINGS       */ { L"[error] 未找到 iperf3.exe，请在设置中指定路径", L"[error] 找不到 iperf3.exe，請在設定中指定路徑", L"[error] iperf3.exe not found; set its path in Settings" },
    /* STR_ERR_NO_VERSION               */ { L"[error] 无法获取 iperf3 版本信息（需 3.7+）", L"[error] 無法取得 iperf3 版本資訊（需 3.7+）", L"[error] Cannot determine iperf3 version (3.7+ required)" },
    /* STR_ERR_NO_IPERF3                */ { L"[error] 未找到 iperf3.exe", L"[error] 找不到 iperf3.exe", L"[error] iperf3.exe not found" },
    /* STR_STOPPED_BY_USER              */ { L"[stopped] 用户手动停止", L"[stopped] 使用者手動停止", L"[stopped] Stopped by user" },
    /* STR_INFO_TEST_ABORTED            */ { L"[info] 测试被中止：iperf3 只有完整跑完才会输出汇总（sender/receiver）", L"[info] 測試被中止：iperf3 只有完整跑完才會輸出彙總（sender/receiver）", L"[info] Test aborted: iperf3 prints a summary (sender/receiver) only after a full run" },
    /* STR_LEGEND_SUM_TX                */ { L"汇总-TX", L"彙總-TX", L"Sum-TX" },
    /* STR_LEGEND_SUM_RX                */ { L"汇总-RX", L"彙總-RX", L"Sum-RX" },
    /* STR_AXIS_JITTER                  */ { L"抖动(ms)", L"抖動(ms)", L"Jitter (ms)" },
    /* STR_AXIS_LOSS                    */ { L"丢包率(%)", L"封包遺失率(%)", L"Loss (%)" },
    /* STR_AXIS_DATAGRAM                */ { L"数据报(个)", L"資料包(個)", L"Datagrams" },
    /* STR_DLG_TITLE_SELECT_EXE         */ { L"选择 iperf3.exe", L"選擇 iperf3.exe", L"Select iperf3.exe" },
    /* STR_DLG_TITLE_SELECT_DIR         */ { L"选择日志目录", L"選擇記錄資料夾", L"Select Log Folder" },
    /* STR_DLG_TITLE_SELECT_LOG         */ { L"选择 iperf3 日志文件", L"選擇 iperf3 記錄檔", L"Select iperf3 Log File" },
    /* STR_FILTER_EXE                   */ { L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0", L"執行檔 (*.exe)\0*.exe\0所有檔案 (*.*)\0*.*\0", L"Executable (*.exe)\0*.exe\0All Files (*.*)\0*.*\0" },
    /* STR_FILTER_LOG                   */ { L"日志文件 (*.log;*.txt;*.json)\0*.log;*.txt;*.json\0所有文件 (*.*)\0*.*\0", L"記錄檔 (*.log;*.txt;*.json)\0*.log;*.txt;*.json\0所有檔案 (*.*)\0*.*\0", L"Log Files (*.log;*.txt;*.json)\0*.log;*.txt;*.json\0All Files (*.*)\0*.*\0" },
    /* STR_EMPTY                        */ { L"", L"", L"" },
    /* STR_DOT                          */ { L"●", L"●", L"●" },
    /* STR_KB                           */ { L"KB", L"KB", L"KB" },
    /* STR_JSON                         */ { L"JSON", L"JSON", L"JSON" },
    /* STR_LBL_LANG                     */ { L"语言", L"語言", L"Language" },
    /* STR_LANG_OPT_AUTO                */ { L"跟随系统", L"跟隨系統", L"Follow System" },
    /* 以下三条 = 语言下拉里的语言名，三列**必须同值**：它们是各语言的原生写法，
       不随界面语言翻译。（此处「繁体列」曾误填「簡體中文」，导致繁体界面下该项
       显示成繁体；现 Lang_FillCombo 已改为固定字面量，同值仅为兜底与可读性。） */
    /* STR_LANG_OPT_ZH_CN               */ { L"简体中文", L"简体中文", L"简体中文" },
    /* STR_LANG_OPT_ZH_TW               */ { L"繁體中文", L"繁體中文", L"繁體中文" },
    /* STR_LANG_OPT_EN                  */ { L"English", L"English", L"English" },
    /* STR_LBL_THEME                    */ { L"主题", L"主題", L"Theme" },
    /* STR_THEME_OPT_AUTO               */ { L"跟随系统", L"跟隨系統", L"Follow System" },
    /* STR_THEME_OPT_DARK               */ { L"深色", L"深色", L"Dark" },
    /* STR_THEME_OPT_LIGHT              */ { L"浅色", L"淺色", L"Light" },
    /* STR_LICENSE_TITLE                */ { L"关于授权", L"關於授權", L"License" },
    /* STR_BTN_LICENSE                  */ { L"许可说明", L"授權說明", L"License" },
    /* STR_BTN_COPY                     */ { L"复制", L"複製", L"Copy" },
    /* STR_BTN_CLOSE                    */ { L"关闭", L"關閉", L"Close" },
    /* STR_INFO_IPERF3_VER              */ { L"[info] iperf3 版本 %s", L"[info] iperf3 版本 %s", L"[info] iperf3 version %s" },
    /* STR_BTN_HELP_IPERF3              */ { L"添加说明", L"新增說明", L"Add Help" },
    /* STR_LBL_APPEARANCE_SECTION      */ { L"外观", L"外觀", L"Appearance" },
    /* STR_LBL_OTHER_SECTION           */ { L"其它", L"其它", L"Other" },
    /* STR_FMT_VERSION                 */ { L"iperf3 版本: %ls", L"iperf3 版本：%ls", L"iperf3 version: %ls" },
    /* STR_ERR_LOG_DIR                 */ { L"日志路径无效，无法写入日志：%s", L"日誌路徑無效，無法寫入日誌：%s", L"Invalid log path; cannot write log: %s" },
    /* STR_CHK_HIDE_STREAM_SUM         */ { L"隐藏流汇总（多流时仅显示各流）", L"隱藏流彙總（多流時僅顯示各流）", L"Hide stream sum (per-stream lines only)" },
};

/* ---- 当前状态 ---- */
static int g_langSel = LANG_SEL_AUTO;   /* 用户档位 */
static int g_langAct = LANG_ZH_CN;      /* 实际生效语言 */

/* ---- 取串 ---- */
const wchar_t* T(int id)
{
    if (id < 0 || id >= STR_COUNT) return L"";
    return kStr[id][g_langAct];
}

/* 窄版：内部转 UTF-8。环形缓冲，支持同一条语句里多次调用。 */
#define TA_SLOTS 8
#define TA_BYTES 1024
const char* TA(int id)
{
    static char s_buf[TA_SLOTS][TA_BYTES];
    static int  s_slot = 0;
    char* b;
    if (id < 0 || id >= STR_COUNT) return "";
    b = s_buf[s_slot];
    s_slot = (s_slot + 1) % TA_SLOTS;
    if (WideCharToMultiByte(CP_UTF8, 0, kStr[id][g_langAct], -1,
                            b, TA_BYTES, NULL, NULL) <= 0) {
        b[0] = 0;
    }
    return b;
}

/* ---- 档位 -> 生效语言 ---- */
int Lang_ResolveSel(int sel)
{
    if (sel == LANG_SEL_ZH_CN) return LANG_ZH_CN;
    if (sel == LANG_SEL_ZH_TW) return LANG_ZH_TW;
    if (sel == LANG_SEL_EN)    return LANG_EN;

    /* AUTO：查用户首选 UI 语言；非中文一律回落 English */
    {
        LANGID lid = GetUserDefaultUILanguage();
        if (PRIMARYLANGID(lid) == LANG_CHINESE) {
            /* sublang 在**高 6 位**：SUBLANGID(lid) == lid >> 10。
               切勿写成 (lid & 0x3FF) —— 那是 PRIMARYLANGID，对任何 zh 变体
               都等于 4(LANG_CHINESE)，会导致全部中文系统被误判为繁体。 */
            WORD sub = SUBLANGID(lid);
            if (sub == SUBLANG_CHINESE_TRADITIONAL ||   /* 0x01 中国台湾 */
                sub == SUBLANG_CHINESE_HONGKONG     ||   /* 0x03 中国香港 */
                sub == SUBLANG_CHINESE_MACAU)            /* 0x05 中国澳门 */
                return LANG_ZH_TW;
            return LANG_ZH_CN;          /* 0x02 中国 / 0x04 新加坡 -> 简体 */
        }
        return LANG_EN;
    }
}

int  Lang_GetSel(void) { return g_langSel; }
int  Lang_GetAct(void) { return g_langAct; }

void Lang_SetSel(int sel)
{
    g_langSel = sel;
    g_langAct = Lang_ResolveSel(sel);
    Lang_Refresh();
}

/* ---- 控件登记表 ---- */
#define LANG_MAX_BIND 320
typedef struct { HWND hwnd; int strId; } LangBind;
static LangBind s_bind[LANG_MAX_BIND];
static int s_bindCount = 0;

void Lang_Bind(HWND hwnd, int strId)
{
    if (!hwnd) return;
    if (strId < 0 || strId >= STR_COUNT) return;
    /* 空串 / 状态圆点：由动态代码负责设置，不参与语言刷新 */
    if (strId == STR_EMPTY || strId == STR_DOT) return;
    if (s_bindCount >= LANG_MAX_BIND) return;
    s_bind[s_bindCount].hwnd   = hwnd;
    s_bind[s_bindCount].strId  = strId;
    s_bindCount++;
}

void Lang_Refresh(void)
{
    int i;
    for (i = 0; i < s_bindCount; i++) {
        SetWindowTextW(s_bind[i].hwnd, kStr[s_bind[i].strId][g_langAct]);
    }
    /* 图表面板是 GDI+ 自绘（SetWindowTextW 无效），由调用方负责重绘 */
}
