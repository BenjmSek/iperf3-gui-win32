# Changelog

本项目遵循 [Keep a Changelog](https://keepachangelog.com/) 格式。

## [1.1.0] - 2026-10-16

### Added

- 回放页「显示单位」下拉：Kb/s / Mb/s / Gb/s / Tb/s / KB/s / MB/s / GB/s / TB-s / auto，
  位于倍速选择框之后；切换即时重绘曲线与 Y 轴，并写入 `settings.ini` 的 `[replay] unit`
- 英文完整文档 `README_EN.md`，与中文版 14 节逐节对应
- `test/` 回归测试编排器（17 用例）与 `CONTRIBUTING.md`

### Changed

- 窗口标题与 VERSIONINFO 的 `FileDescription` / `ProductName` 统一为 `iperf3 GUI V1.1`

### Fixed

- 许可证统一为 **MIT**（原先 `license_text.c` 声明 BSD-3-Clause 与 `app.rc` 的 MIT 自相矛盾）
- 清理死代码 `JsonGetBool()`、`seriesName[]`、`LBL_SETTINGS_DETECTED`
- 修正 `chart.h` 过时注释与若干文档数字

### Removed

- `--logfile` 相关残留、`TIMER_CLIENT_TAIL` / `TailLineCallback` 轮询机制（已由管道线程取代）

## [1.0.0]

- 首个可用版本：客户端 / 服务端 / 实时图表 / 三语 / 深色主题 / `settings.ini` 持久化
