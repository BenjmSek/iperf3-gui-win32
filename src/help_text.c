/*
 * help_text.c — 「添加说明」对话框正文（三语）
 *
 * 内容由用户定稿提供，逐字照录，段间空行同样保留（不得删改）。
 *   · 简体：用户原稿
 *   · 繁體：用户原稿（中国台湾用语：程式 / 偵測 / 儲存設定 / 設定檔）
 *   · English：用户原稿
 *
 * 引用的按钮名与 lang.c 保持一致：
 *   浏览 / 瀏覽 / Browse             （STR_BTN_BROWSE）
 *   保存配置 / 儲存設定 / Save Config  （STR_BTN_SAVE_CONFIG）
 *   自动检测 / 自動偵測 / Auto-detect   （STR_BTN_AUTODETECT）
 */
#include <windows.h>
#include "help_text.h"

/* 简体中文 */
const wchar_t kHelpIperf3Zh[] =
    L"本程序不随包分发iperf3，请自行到https://iperf.fr/iperf-download.php下载3.7或更新的windows版本安装\r\n"
    L"\r\n"
    L"「自动检测」的查找顺序为（使用先找到的）：\r\n"
    L"1.本程序所在目录及其子目录\r\n"
    L"2.系统PATH环境变量中的目录（不含子目录）\r\n"
    L"\r\n"
    L"也可用「浏览」自行指定iperf3所在路径\r\n"
    L"\r\n"
    L"需点「保存配置」才会生效并写入配置文件";

/* 繁體中文（中国台湾用语） */
const wchar_t kHelpIperf3Tw[] =
    L"本程式不隨附iperf3，請自行到https://iperf.fr/iperf-download.php下載3.7或更新的Windows版本安裝\r\n"
    L"\r\n"
    L"「自動偵測」的查找順序為（採用先找到的）：\r\n"
    L"1.本程式所在目錄及其子目錄\r\n"
    L"2.系統 PATH 環境變數中的目錄（不含子目錄）\r\n"
    L"\r\n"
    L"也可用「瀏覽」自行指定 iperf3 所在路徑\r\n"
    L"\r\n"
    L"需點「儲存設定」才會生效並寫入設定檔";

/* English */
const wchar_t kHelpIperf3En[] =
    L"This program does not bundle iperf3. Download and install iperf3 for Windows 3.7 or later from "
    L"https://iperf.fr/iperf-download.php\r\n"
    L"\r\n"
    L"\"Auto-detect\" searches in this order (first match wins):\r\n"
    L"1. The folder containing this program and its subfolders\r\n"
    L"2. Folders in the system PATH variable (no subfolders)\r\n"
    L"\r\n"
    L"You can also use \"Browse\" to specify the iperf3 path yourself.\r\n"
    L"\r\n"
    L"Click \"Save Config\" to apply and write it to the config file.";
