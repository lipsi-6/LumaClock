# Luma Clock

一个轻量、离线运行的 Windows 桌面悬浮时钟。C++17 / Win32 / GDI+，不需要 Ruby、Node.js、WebView 或额外 GCC DLL。

## 开始使用

双击 `app\LumaClock.exe`。默认只显示可拖动的指针时钟，没有标题栏、工具条或任务栏按钮。

- 按住时钟拖动：移动位置，松手后自动保存。
- 右键点击时钟：打开设置。
- 点击系统托盘的 Luma 图标：打开设置。图标可能在托盘的“显示隐藏的图标”区域。
- 再次双击 EXE：打开现有实例的设置，不会创建第二个时钟。
- 快捷键：优先使用 **Ctrl + Alt + L**；如被其他程序占用，尝试 **Ctrl + Alt + Shift + L**。实际可用的快捷键显示在“关于”页；两者都占用时仍可通过托盘或再次启动打开设置。
- 关闭设置窗口或按 Esc：只关闭设置，时钟继续运行。
- 右键托盘图标 → **退出 Luma Clock**：完全退出。

## 设置

所有设置即时生效，约 400 毫秒后自动保存。

**外观**

- 指针钟面；云雾白、午夜蓝、暖砂三种配色。
- 大小 60%–160%，不透明度 25%–100%。
- 前台钟面和设置预览使用同一套矢量绘图代码。

**桌面行为**

- 显示秒针。
- 平滑秒针：默认关闭；开启后以 20 帧/秒更新。
- 始终置顶、鼠标穿透、锁定位置。
- 登录 Windows 后启动：默认关闭；用户开启后才在当前用户的 Run 注册表项中保存程序路径，不需要管理员权限。
- “把时钟移回屏幕”可以找回位置；断开显示器时也会自动把时钟约束到可见工作区。

开启鼠标穿透以后，鼠标会直接操作时钟下方的窗口，不能再直接拖动或右键时钟。请通过托盘、快捷键或再次启动 EXE 进入设置，关闭鼠标穿透。

应用直接读取 Windows 本地时间，不会修改系统时间或连接网络。每次绘制重新读取时间，因此睡眠唤醒不会累积计时误差。

## 配置与便携运行

配置文件是 EXE 同目录下的 `LumaClock.ini`。程序所在目录需要可写权限。

- 只复制 EXE：在新位置按默认设置运行。
- 同时复制 EXE 和 INI：保留外观和位置。
- 程序使用临时文件写入配置，再替换正式配置，减少意外退出造成半份文件的可能。
- 如果已经启用登录启动，移动 EXE 前先关闭该选项，移动后再开启，以更新路径。

目标系统：Windows 10 / 11 x64。界面针对高 DPI 绘制；实际验证以当前电脑为准。置顶是普通桌面的置顶窗口，不覆盖安全桌面或独占全屏应用。

## 构建

公开的构建入口是 `build-generic.ps1`。本机专用的 `build.ps1` 可保留用于日常构建，该文件已由 `.gitignore` 排除，不随源码提交。

需要 Windows PowerShell 5.1 或 PowerShell 7，以及 **x64 MinGW-w64 GCC 工具链**（包含 `g++.exe` 和 `windres.exe`）。脚本不下载依赖，也不假定工具链安装在任何固定目录。

将工具链的 `bin` 目录加入 PATH 后，在项目目录运行：

```powershell
.\build-generic.ps1
```

不修改 PATH 时，也可以显式指定编译器。以下路径仅为示例，请替换为实际安装位置：

```powershell
.\build-generic.ps1 -Compiler 'C:\Toolchains\mingw64\bin\g++.exe'
```

脚本优先使用编译器同目录下的 `windres`，也支持带目标前缀的工具名。如果它不在同一目录且未加入 PATH，可单独指定：

```powershell
.\build-generic.ps1 -Compiler 'C:\Toolchains\mingw64\bin\g++.exe' -ResourceCompiler 'C:\Toolchains\mingw64\bin\windres.exe'
```

脚本会校验编译目标为 `x86_64-w64-mingw32`，避免误用面向 Linux、MSYS 或 32 位 Windows 的编译器。对 PATH 的临时调整仅作用于当前 PowerShell 进程，构建结束后会恢复。

发布版使用 `-Os -s -static`，没有开启 LTO，没有下载依赖，也不编译第三方框架。三份源文件顺序编译，编译器自动移除临时目标文件；`build` 目录只保留资源目标文件，发布 EXE 位于 `app` 目录。

调试版：

```powershell
.\build-generic.ps1 -Debug
```

重编译前先退出正在运行的时钟，以免 EXE 被 Windows 锁定。

## 源码仓库包含哪些文件

保留 `src/`、`resources/`、`tools/`、`build-generic.ps1`、`.gitignore` 和说明文档。`resources/app.ico` 是构建输入，`resources/icon.svg` 和图标生成脚本方便后续修改，应随源码保留。

`.gitignore` 采用白名单：默认忽略全部文件，只放行明确列出的源码、资源、通用脚本和文档。本机专用的 `build.ps1`、生成的 `build/`、`app/` 及其他未列出的文件均被忽略。新增需要提交的源码、许可证或正式截图时，应同步添加白名单规则；位于新目录中的文件，需要先放行父目录，再放行文件。

EXE 可以通过 GitHub Releases 单独发布。`VALIDATION.md` 是可选的测试范围说明，保留或省略都不会影响构建。

## 验证

内置自检覆盖配置边界、DPI 尺寸计算、24 小时制时间格式，以及三种配色的实际绘制：

```powershell
New-Item -ItemType Directory .\build\checks -Force
$report = Join-Path (Get-Location).Path 'build\checks\report.txt'
Start-Process .\app\LumaClock.exe -ArgumentList @('--self-test', ('"' + $report + '"')) -Wait
```

报告和三张透明 PNG 预览写入所指定报告文件的目录。自检不修改正常配置，也不创建托盘图标。

供开发验证使用的 `--ui-test` 参数使钟面暂时出现在任务栏，方便测试工具定位拖动；日常启动不使用该参数。

## 文件布局

```text
app/LumaClock.exe         可直接运行的发布程序
src/main.cpp             窗口、托盘、配置、生命周期和自检
src/clock.cpp            指针钟面的绘制
src/settings.cpp         独立设置窗口
resources/               图标、清单和版本资源
tools/generate_icon.py   可选的图标重绘脚本（需要 Pillow）
build-generic.ps1                一步构建
```

## 设计参考

参考 [ElevenClock](https://github.com/marticliment/ElevenClock) 的托盘与设置入口，以及 [Desktop Clock](https://desktopclock.app/about/clock) 的指针钟面和主题选择思路。应用界面、钟面、图标和代码为本项目绘制和编写，没有复制第三方图标、字体或程序代码。显示字体使用 Windows 已安装的字体。

Windows 接口参考：[UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow)、[置顶窗口](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)、[窗口定时器](https://learn.microsoft.com/en-us/windows/win32/winmsg/using-timers)。
