/*
 * lang.h — 多语言支持（简中 / 繁中 / English）
 *
 * 本文件由 build-i18n/gen_lang.py 自动生成，请勿手改。
 * 串表数据源：build-i18n/i18n_strings.py
 *
 * 用法：
 *   T(STR_XXX)          -> const wchar_t*，控件文本 / 对话框 / swprintf 格式串
 *   TA(STR_XXX)         -> const char*  (UTF-8)，UI_AppendOutput / Log_WriteLine / snprintf
 */
#ifndef LANG_H
#define LANG_H

#include <windows.h>

/* ---- 用户档位（设置页可选值）---- */
enum {
    LANG_SEL_AUTO = 0,   /* 跟随系统 */
    LANG_SEL_ZH_CN = 1,   /* 简体中文 */
    LANG_SEL_ZH_TW = 2,   /* 繁體中文 */
    LANG_SEL_EN = 3,   /* English */
    LANG_SEL_COUNT
};

/* ---- 实际生效语言（由档位解析而来）---- */
enum { LANG_ZH_CN = 0, LANG_ZH_TW, LANG_EN, LANG_COUNT };

/* ---- 字符串 ID（顺序 = kStr 行序，由生成器保证）---- */
enum {
    STR_TAB_CLIENT = 0,
    STR_TAB_SERVER = 1,
    STR_TAB_REPLAY = 2,
    STR_TAB_SETTINGS = 3,
    STR_BTN_SAVE_CONFIG = 4,
    STR_BTN_LOAD_CONFIG = 5,
    STR_BTN_RESTORE_DEFAULT = 6,
    STR_STATUS_CLIENT = 7,
    STR_STATUS_SERVER = 8,
    STR_STATUS_RUNNING = 9,
    STR_STATUS_IDLE = 10,
    STR_LBL_SERVER_IP = 11,
    STR_LBL_PORT = 12,
    STR_LBL_PROTO = 13,
    STR_LBL_UNIT = 14,
    STR_LBL_TRANSFER = 15,
    STR_LBL_DURATION = 16,
    STR_LBL_INTERVAL = 17,
    STR_LBL_OMIT = 18,
    STR_LBL_BANDWIDTH = 19,
    STR_LBL_PARALLEL = 20,
    STR_LBL_EXTRA = 21,
    STR_CHK_REVERSE = 22,
    STR_CHK_BIDIR = 23,
    STR_CHK_GETSERVER = 24,
    STR_BTN_START = 25,
    STR_BTN_STOP = 26,
    STR_LBL_CMD = 27,
    STR_LBL_SUMMARY = 28,
    STR_LBL_RAW_OUTPUT = 29,
    STR_BTN_CLEAR = 30,
    STR_BTN_COPY_ALL = 31,
    STR_CHK_AUTOSCROLL = 32,
    STR_LBL_LISTEN_PORT = 33,
    STR_LBL_AF = 34,
    STR_CHK_ONEOFF = 35,
    STR_CHK_VERBOSE = 36,
    STR_BTN_START_SERVER = 37,
    STR_BTN_STOP_SERVER = 38,
    STR_LBL_IPERF3_PATH = 39,
    STR_BTN_BROWSE = 40,
    STR_BTN_AUTODETECT = 41,
    STR_LBL_LOG_SECTION = 42,
    STR_CHK_LOG_ENABLE = 43,
    STR_LBL_LOG_DIR = 44,
    STR_BTN_BROWSE_DIR = 45,
    STR_NAMING_RULE = 46,
    STR_LBL_LOG_MAXSIZE = 47,
    STR_LBL_CHART_SECTION = 48,
    STR_CHK_SHOW_STREAMS = 49,
    STR_BTN_BROWSE_ELLIPSIS = 50,
    STR_REPLAY_NO_FILE = 51,
    STR_BTN_PLAY = 52,
    STR_BTN_STOP_PLAY = 53,
    STR_LBL_SPEED = 54,
    STR_MSG_SELECT_LOG_FILE = 55,
    STR_REPLAY_TIME_INIT = 56,
    STR_COPIED_TO_CLIPBOARD = 57,
    STR_COPY_FAILED = 58,
    STR_HELP_NAMING_TITLE = 59,
    STR_HELP_NAMING_BODY = 60,
    STR_FMT_STREAM = 61,
    STR_FMT_BANDWIDTH = 62,
    STR_FMT_PROGRAM_PATH = 63,
    STR_FMT_PROGRAM_PATH_MISSING = 64,
    STR_FMT_SUMMARY_HDR = 65,
    STR_FMT_SUMMARY_HDR2 = 66,
    STR_FMT_BLOCK_SENDER = 67,
    STR_FMT_BLOCK_RECEIVER = 68,
    STR_FMT_REPLAY_TIME = 69,
    STR_FMT_CUR_UDP = 70,
    STR_FMT_CUR_DGRAM = 71,
    STR_FMT_CUR_PLAIN = 72,
    STR_FMT_REPLAY_INFO_OVER = 73,
    STR_FMT_REPLAY_INFO = 74,
    STR_FMT_REPLAY_INFO_SHORT = 75,
    STR_MODE_JSON = 76,
    STR_MODE_TEXT = 77,
    STR_ERR_VERSION_LOW = 78,
    STR_ERR_SPAWN_CLIENT = 79,
    STR_ERR_SPAWN_SERVER = 80,
    STR_INFO_CLEANUP_PORT = 81,
    STR_ERR_LOAD_FAILED = 82,
    STR_ERR_OOM = 83,
    STR_ERR_JSON_NO_INTERVALS = 84,
    STR_ERR_JSON_EMPTY_INTERVALS = 85,
    STR_ERR_JSON_NO_DATA = 86,
    STR_ERR_PATH_EMPTY = 87,
    STR_ERR_OPEN_FILE = 88,
    STR_ERR_FILE_EMPTY = 89,
    STR_ERR_NO_INTERVAL_DATA = 90,
    STR_OUT_HDR_CLIENT_SUM = 91,
    STR_OUT_HDR_SERVER_REPORT = 92,
    STR_HINT_SELECTED_BY_BROWSE = 93,
    STR_HINT_NOT_FOUND = 94,
    STR_WARN_BW_INVALID = 95,
    STR_ERR_NO_SERVER_IP = 96,
    STR_INFO_BIDIR_37 = 97,
    STR_WARN_BW_UDP_ONLY = 98,
    STR_ERR_NO_IPERF3_SETTINGS = 99,
    STR_ERR_NO_VERSION = 100,
    STR_ERR_NO_IPERF3 = 101,
    STR_STOPPED_BY_USER = 102,
    STR_INFO_TEST_ABORTED = 103,
    STR_LEGEND_SUM_TX = 104,
    STR_LEGEND_SUM_RX = 105,
    STR_AXIS_JITTER = 106,
    STR_AXIS_LOSS = 107,
    STR_AXIS_DATAGRAM = 108,
    STR_DLG_TITLE_SELECT_EXE = 109,
    STR_DLG_TITLE_SELECT_DIR = 110,
    STR_DLG_TITLE_SELECT_LOG = 111,
    STR_FILTER_EXE = 112,
    STR_FILTER_LOG = 113,
    STR_EMPTY = 114,
    STR_DOT = 115,
    STR_KB = 116,
    STR_JSON = 117,
    STR_LBL_LANG = 118,
    STR_LANG_OPT_AUTO = 119,
    STR_LANG_OPT_ZH_CN = 120,
    STR_LANG_OPT_ZH_TW = 121,
    STR_LANG_OPT_EN = 122,
    STR_LBL_THEME = 123,
    STR_THEME_OPT_AUTO = 124,
    STR_THEME_OPT_DARK = 125,
    STR_THEME_OPT_LIGHT = 126,
    STR_LICENSE_TITLE = 127,
    STR_BTN_LICENSE = 128,
    STR_BTN_COPY = 129,
    STR_BTN_CLOSE = 130,
    STR_INFO_IPERF3_VER = 131,
    STR_BTN_HELP_IPERF3 = 132,
    STR_LBL_APPEARANCE_SECTION = 133,
    STR_LBL_OTHER_SECTION = 134,
    STR_FMT_VERSION = 135,
    STR_ERR_LOG_DIR = 136,
    STR_CHK_HIDE_STREAM_SUM = 137,
    STR_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif

/* 取串：宽 / 窄(UTF-8) */
const wchar_t* T(int id);
const char*    TA(int id);

/* 档位 <-> 生效语言 */
int  Lang_ResolveSel(int sel);   /* 纯函数：档位 -> 生效语言（AUTO 时查系统） */
void Lang_SetSel(int sel);       /* 设档位并立即应用（刷新所有已绑定控件） */
int  Lang_GetSel(void);
int  Lang_GetAct(void);

/* 控件文本登记表：CreateLabel/Button/Checkbox 内部自动登记 */
void Lang_Bind(HWND hwnd, int strId);
void Lang_Refresh(void);         /* 按当前生效语言重设所有已绑定控件文本 */

#ifdef __cplusplus
}
#endif

#endif /* LANG_H */
