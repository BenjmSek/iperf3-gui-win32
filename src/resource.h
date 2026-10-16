#ifndef RESOURCE_H
#define RESOURCE_H

/* ===== Application icon ===== */
#define IDI_APPICON        100

/* ===== Tab IDs (order matches the tab bar: 客户端 / 服务端 / 回放 / 设置) ===== */
#define TAB_CLIENT      0
#define TAB_SERVER      1
#define TAB_REPLAY      2
#define TAB_SETTINGS    3

/* ===== Tab buttons ===== */
#define BTN_TAB_CLIENT     1001
#define BTN_TAB_SERVER     1002
#define BTN_TAB_REPLAY     1003
#define BTN_TAB_SETTINGS   1004

/* ===== Client panel controls ===== */
#define EDT_CLIENT_IP       1100
#define EDT_CLIENT_PORT     1101
#define CBO_CLIENT_PROTO    1102
#define EDT_CLIENT_DURATION 1103
#define EDT_CLIENT_INTERVAL 1104
#define EDT_CLIENT_BW       1107
#define CHK_CLIENT_REVERSE  1108
#define CHK_CLIENT_BIDIR    1109
#define BTN_CLIENT_START    1110
#define BTN_CLIENT_STOP     1111
#define EDT_CLIENT_CMD      1112
#define EDT_CLIENT_OUTPUT   1113
#define BTN_CLIENT_CLEAR    1114
#define BTN_CLIENT_COPY     1115
#define CHK_CLIENT_AUTOSCROLL 1116
/* Client chart area — custom drawn child window */
#define WND_CLIENT_CHART    1117
/* Client summary labels */
#define LBL_CLIENT_SUMMARY  1118
/* ===== Global config buttons (right side of tab bar) ===== */
#define BTN_SAVE_CONFIG      1005
#define BTN_LOAD_CONFIG      1006
#define BTN_RESTORE_DEFAULT  1007
#define EDT_CLIENT_EXTRA    1121
#define CBO_CLIENT_FORMAT   1124
#define EDT_CLIENT_NUM      1125
#define EDT_CLIENT_OMIT     1126
#define CHK_CLIENT_JSON     1127
#define CHK_CLIENT_GETSERVER 1128
#define CBO_CLIENT_PARALLEL 1129
/* ===== Server panel controls ===== */
#define EDT_SERVER_PORT     1200
#define CHK_SERVER_ONEOFF   1201
#define CHK_SERVER_VERBOSE  1202
#define BTN_SERVER_START    1203
#define BTN_SERVER_STOP     1204
#define EDT_SERVER_CMD      1205
#define EDT_SERVER_OUTPUT   1206
#define BTN_SERVER_CLEAR    1207
#define BTN_SERVER_COPY     1208
#define CHK_SERVER_AUTOSCROLL 1209
#define WND_SERVER_CHART    1210
#define LBL_SERVER_SUMMARY  1211
#define CBO_SERVER_FORMAT   1212
#define EDT_SERVER_EXTRA    1213
#define CHK_SERVER_JSON     1214
#define CBO_SERVER_AF       1215

/* ===== Settings panel controls ===== */
#define EDT_SETTINGS_PATH   1300
#define BTN_SETTINGS_BROWSE 1304
#define BTN_SETTINGS_DETECT 1301

/* ===== Log controls ===== */
#define CHK_LOG_ENABLE      1305
#define LBL_LOG_DIR         1306
#define EDT_LOG_DIR         1307
#define BTN_LOG_BROWSE      1308
#define LBL_LOG_NAMING      1309
#define EDT_LOG_NAMING      1310
#define LBL_LOG_MAXSIZE     1311
#define EDT_LOG_MAXSIZE     1312
#define BTN_LOG_NAMING_HELP 1313
#define CHK_SHOW_STREAMS    1314
/* UI language combo (S3) */
#define CBO_LANG            1316
/* UI theme combo (S3) */
#define CBO_THEME           1317
/* 图表组：「隐藏流汇总」复选框（仅在勾选「显示各流」时可点） */
#define CHK_HIDE_STREAM_SUM 1318
/* License（设置页按钮 + 模态对话框内控件，方案 §6） */
#define BTN_LICENSE         1326
#define IDC_LICENSE_TEXT    1327
#define BTN_LICENSE_COPY    1328
#define BTN_LICENSE_CLOSE   1329

/* 「添加说明」按钮（设置页，紧跟「自动检测」）+ 模态说明对话框内控件 */
#define BTN_HELP_IPERF3     1321
#define IDC_HELP_TEXT       1322
#define BTN_HELP_CLOSE      1324

/* ===== Replay panel controls ===== */
#define EDT_REPLAY_PATH    1500
#define BTN_REPLAY_BROWSE  1501
#define LBL_REPLAY_INFO    1503
#define TRK_REPLAY_POS     1504
#define LBL_REPLAY_TIME    1505
#define BTN_REPLAY_PLAY    1506
#define BTN_REPLAY_STOP    1507
#define CBO_REPLAY_SPEED   1508
#define CBO_REPLAY_UNIT    1511
#define LBL_REPLAY_VALUES  1509
#define WND_REPLAY_CHART   1510

/* Replay playback timer (WM_TIMER id) */
#define REPLAY_TIMER_ID    1001

/* ===== Status bar segments ===== */
#define LBL_STATUS_CLIENT_TAG   1401
#define LBL_STATUS_CLIENT_DOT   1402
#define LBL_STATUS_CLIENT_VAL   1403
#define LBL_STATUS_SERVER_TAG   1404
#define LBL_STATUS_SERVER_DOT   1405
#define LBL_STATUS_SERVER_VAL   1406

/* ===== Copy feedback labels (output toolbar, right of 自动滚动) ===== */
#define LBL_CLIENT_COPY_HINT    1407
#define LBL_SERVER_COPY_HINT    1408

/* ===== Custom messages (for pipe reader thread → main thread) ===== */
#define WM_PIPE_LINE     (WM_USER + 100)  /* wParam = mode, lParam = malloc'd line ptr */
#define WM_PIPE_DONE     (WM_USER + 101)  /* wParam = mode, lParam = exit code */

/* ===== AutoTest mode messages ===== */
#define WM_AUTOTEST_NEXT (WM_USER + 200)  /* Advance to next test case */
#define WM_AUTOTEST_DONE (WM_USER + 201)  /* All test cases complete, exit */

#endif /* RESOURCE_H */
