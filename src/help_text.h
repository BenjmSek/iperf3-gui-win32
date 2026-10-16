#ifndef HELP_TEXT_H
#define HELP_TEXT_H

/* 「说明」对话框正文 —— 编译进 exe，不依赖外部文件。
   三语各一份，按当前界面语言取用（繁体按中国台湾用语，非字形映射）。
   正文里的按钮名必须与 lang.c 实际值一致：
   自动检测 / 瀏覽 / 儲存設定 / Auto-detect / Browse / Save Config。 */

extern const wchar_t kHelpIperf3Zh[];
extern const wchar_t kHelpIperf3Tw[];
extern const wchar_t kHelpIperf3En[];

#endif /* HELP_TEXT_H */
