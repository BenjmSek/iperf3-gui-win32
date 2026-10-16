/*
 * license_text.h — License 对话框正文（编译进 exe，自包含，不依赖外部文件）
 *
 * 方案：README/ui-settings-plan.md §6.2
 * 许可 = MIT License；署名 = Copyright (c) 2026 BenjmSek
 * MIT 条款原文恒英文（法律以英文为准）；摘要与附注按当前 UI 语言显示。
 */
#ifndef LICENSE_TEXT_H
#define LICENSE_TEXT_H

/* 第 1 段｜版权行（固定，不随语言变） */
extern const wchar_t kLicCopyright[];

/* 第 2 段｜授权摘要（简/繁/英，按当前界面语言取一） */
extern const wchar_t kLicSummaryZh[];   /* 简体 */
extern const wchar_t kLicSummaryTw[];   /* 繁體（中国台湾用语） */
extern const wchar_t kLicSummaryEn[];

/* 第 3 段｜MIT License 完整英文原文（恒英文） */
extern const wchar_t kLicMitFull[];

/* 第 4 段｜附注（简/繁/英，按当前界面语言取一） */
extern const wchar_t kLicNoteZh[];
extern const wchar_t kLicNoteTw[];
extern const wchar_t kLicNoteEn[];

#endif /* LICENSE_TEXT_H */
