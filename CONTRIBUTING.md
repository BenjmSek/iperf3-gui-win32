# Contributing / 贡献指南

## 构建

需要 MinGW32 (i686)，默认装在 `C:\mingw32`：

```cmd
build.bat
```

详见 `README.md` §4 编译方法。

## 改代码前请读

`README.md` §13 列了本项目的开发铁律，几条最容易踩的：

- `lang.h` 的枚举是**显式数字**，`lang.c` 的 `kStr[]` 是**按位置索引** —— 两者必须严格同序。
  **新增字符串只能追加到末尾**，插在中间会全局文案错位。
- 新增配置项要同时接 4 处：`SaveAllConfig` / `LoadAllConfig` / `InitDefaultConfig` / `RestoreAllDefaults`。
- 改 `README.md` 必须同步改 `README_EN.md`，两份 14 节逐节对应。

## 回归测试

```cmd
python test\run_autotest.py
```

用例定义在 `test/autotest.ini`，产物落在 `test/results/`（已 gitignore）。
基线：**16 PASS / 1 FAIL**（`case_05` UDP bidir，iperf3 3.21 Cygwin 构建自身限制，非 GUI 缺陷）。

## 提交

- 不要把 `settings.ini`、`logs/`、第三方二进制提交进来（已 gitignore）。
- 提交信息用英文或中文均可，一句话说清改了什么。
