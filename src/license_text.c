/*
 * license_text.c — License 对话框正文（编译进 exe，自包含）
 *
 * 方案：README/ui-settings-plan.md §6.2（逐段定稿）
 * 许可 = MIT License；署名 = Copyright (c) 2026 BenjmSek
 *
 * 前提：GUI 不分发 iperf3.exe / cygwin1.dll，用户自行获取 iperf3 由本程序调用，
 *       因此本对话框只含 GUI 自身的授权声明，不含任何第三方组件许可全文。
 * MIT 条款原文恒英文（法律以英文为准）；摘要与附注按当前 UI 语言显示。
 */
#include "license_text.h"

/* 第 1 段｜版权行（固定，不随语言变） */
const wchar_t kLicCopyright[] =
    L"Copyright (c) 2026 BenjmSek";

/* 第 2 段｜授权摘要（简体） */
const wchar_t kLicSummaryZh[] =
    L"本程序以 MIT 许可证授权。\r\n"
    L"你可以自由使用、复制、修改、合并、发布、分发、再授权及销售本程序，\r\n"
    L"但必须在所有副本或重要部分中包含本版权声明和许可声明。\r\n"
    L"本程序按\"现状\"提供，不提供任何担保。";

/* 第 2 段｜授权摘要（繁體，中国台湾用语，非字形映射） */
const wchar_t kLicSummaryTw[] =
    L"本程式以 MIT 授權條款釋出。\r\n"
    L"你可以自由使用、複製、修改、合併、發布、散布、再授權及銷售本程式，\r\n"
    L"但必須在所有副本或重要部分中包含本著作權聲明與授權聲明。\r\n"
    L"本程式按「現狀」提供，不提供任何擔保。";

/* 第 2 段｜授权摘要（英文） */
const wchar_t kLicSummaryEn[] =
    L"This software is licensed under the MIT License.\r\n"
    L"You are free to use, copy, modify, merge, publish, distribute,\r\n"
    L"sublicense, and/or sell copies of this software, provided that\r\n"
    L"this copyright notice and permission notice are included in all\r\n"
    L"copies or substantial portions of the Software.\r\n"
    L"This software is provided \"as is\", without warranty of any kind.";

/* 第 3 段｜MIT License 完整英文原文（恒英文） */
const wchar_t kLicMitFull[] =
    L"MIT License\r\n"
    L"\r\n"
    L"Copyright (c) 2026 BenjmSek\r\n"
    L"\r\n"
    L"Permission is hereby granted, free of charge, to any person obtaining a copy\r\n"
    L"of this software and associated documentation files (the \"Software\"), to deal\r\n"
    L"in the Software without restriction, including without limitation the rights\r\n"
    L"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\r\n"
    L"copies of the Software, and to permit persons to whom the Software is\r\n"
    L"furnished to do so, subject to the following conditions:\r\n"
    L"\r\n"
    L"The above copyright notice and this permission notice shall be included in all\r\n"
    L"copies or substantial portions of the Software.\r\n"
    L"\r\n"
    L"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\r\n"
    L"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\r\n"
    L"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\r\n"
    L"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\r\n"
    L"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\r\n"
    L"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\r\n"
    L"SOFTWARE.";

/* 第 4 段｜附注（简体） */
const wchar_t kLicNoteZh[] =
    L"附注：\r\n"
    L"本程序本身不包含也不分发 iperf3 及其运行时依赖。\r\n"
    L"用户需自行获取 iperf3（版本 ≥ 3.7）并由本程序调用。\r\n"
    L"iperf3 采用 BSD-3-Clause 许可证，其许可声明请参阅 iperf3 官方源。";

/* 第 4 段｜附注（繁體，中国台湾用语） */
const wchar_t kLicNoteTw[] =
    L"附註：\r\n"
    L"本程式本身不包含也不散布 iperf3 及其執行階段相依元件。\r\n"
    L"使用者需自行取得 iperf3（版本 ≥ 3.7）並由本程式呼叫。\r\n"
    L"iperf3 採用 BSD-3-Clause 授權條款，其授權聲明請參閱 iperf3 官方來源。";

/* 第 4 段｜附注（英文） */
const wchar_t kLicNoteEn[] =
    L"Note:\r\n"
    L"This program does not contain or distribute iperf3 or its runtime\r\n"
    L"dependencies. Users must obtain iperf3 (version >= 3.7) separately\r\n"
    L"for this program to invoke.\r\n"
    L"iperf3 is licensed under BSD-3-Clause; refer to the iperf3 official\r\n"
    L"source for its license notice.";
