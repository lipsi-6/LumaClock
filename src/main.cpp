#include "app.h"
#include <cwchar>

namespace luma {
HINSTANCE instance = nullptr;
HWND clockWindow = nullptr, settingsWindow = nullptr;
Config config;
std::wstring exePath, iniPath;
bool clockVisible = true, hotkeyReady = false, settingsFocused = false, keyboardFocus = false;
std::wstring hotkeyText = L"Ctrl + Alt + L";
HICON appIcon = nullptr;
namespace {
HANDLE mutexHandle = nullptr;
NOTIFYICONDATAW tray{};
UINT taskbarCreated = 0;
bool dragging = false, iniWarningShown = false;
POINT dragStart{}, windowStart{};
constexpr int MENU_SETTINGS = 501, MENU_VISIBLE = 502, MENU_THROUGH = 503, MENU_EXIT = 504;

void normalize(Config& c) {
    c.theme = std::clamp(c.theme, 0, 2);
    c.scale = std::clamp(c.scale, 60, 160); c.opacity = std::clamp(c.opacity, 25, 100);
    if (!c.seconds) c.smooth = false;
}
void loadConfig() {
    auto number = [](const wchar_t* key, int fallback) {
        return static_cast<int>(GetPrivateProfileIntW(L"Clock", key, fallback, iniPath.c_str()));
    };
    config.theme = number(L"Theme", 0);
    config.scale = number(L"Scale", 100); config.opacity = number(L"Opacity", 100);
    config.seconds = number(L"Seconds", 1) != 0;
    config.smooth = number(L"Smooth", 0) != 0;
    config.topmost = number(L"Topmost", 1) != 0;
    config.clickThrough = number(L"ClickThrough", 0) != 0;
    config.locked = number(L"Locked", 0) != 0;
    config.x = number(L"X", INT_MIN); config.y = number(L"Y", INT_MIN);
    normalize(config);
}
RECT workArea(HWND window) {
    MONITORINFO info{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info);
    return info.rcWork;
}
void clampPosition() {
    RECT r; GetWindowRect(clockWindow, &r);
    RECT area = workArea(clockWindow);
    int x = std::clamp(static_cast<int>(r.left), static_cast<int>(area.left),
                       std::max(static_cast<int>(area.left), static_cast<int>(area.right - (r.right - r.left))));
    int y = std::clamp(static_cast<int>(r.top), static_cast<int>(area.top),
                       std::max(static_cast<int>(area.top), static_cast<int>(area.bottom - (r.bottom - r.top))));
    SetWindowPos(clockWindow, nullptr, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    config.x = x; config.y = y;
}
void addTray() {
    tray = {}; tray.cbSize = sizeof(tray); tray.hWnd = clockWindow; tray.uID = 1;
    tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray.uCallbackMessage = TRAY_MESSAGE; tray.hIcon = appIcon;
    wcscpy_s(tray.szTip, L"Luma Clock · 右键打开设置");
    Shell_NotifyIconW(NIM_ADD, &tray);
    tray.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &tray);
}
void trayMenu() {
    HMENU menu = CreatePopupMenu();
    std::wstring menuTitle = L"打开设置";
    if (hotkeyReady) menuTitle += L"\t" + hotkeyText;
    AppendMenuW(menu, MF_STRING, MENU_SETTINGS, menuTitle.c_str());
    AppendMenuW(menu, MF_STRING, MENU_VISIBLE, clockVisible ? L"隐藏时钟" : L"显示时钟");
    AppendMenuW(menu, MF_STRING | (config.clickThrough ? MF_CHECKED : 0), MENU_THROUGH, L"鼠标穿透");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MENU_EXIT, L"退出 Luma Clock");
    SetMenuDefaultItem(menu, MENU_SETTINGS, FALSE);
    POINT pt; GetCursorPos(&pt);
    // A hidden settings-independent owner ensures the tray menu dismisses properly.
    SetForegroundWindow(clockWindow);
    int command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                 pt.x, pt.y, 0, clockWindow, nullptr);
    DestroyMenu(menu); PostMessageW(clockWindow, WM_NULL, 0, 0);
    if (command == MENU_SETTINGS) openSettings();
    else if (command == MENU_VISIBLE) {
        clockVisible = !clockVisible;
        ShowWindow(clockWindow, clockVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
        renderClock();
    } else if (command == MENU_THROUGH) { config.clickThrough = !config.clickThrough; applyConfig(false); refreshSettings(); }
    else if (command == MENU_EXIT) DestroyWindow(clockWindow);
}

LRESULT CALLBACK clockProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreated && taskbarCreated != 0) { addTray(); return 0; }
    switch (message) {
    case WM_NCHITTEST:
        return config.clickThrough ? HTTRANSPARENT : HTCLIENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
        if (!config.locked && !config.clickThrough) {
            dragging = true; GetCursorPos(&dragStart);
            RECT r; GetWindowRect(window, &r); windowStart = {r.left, r.top}; SetCapture(window);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (dragging) {
            POINT pt; GetCursorPos(&pt);
            SetWindowPos(window, nullptr, windowStart.x + pt.x - dragStart.x,
                         windowStart.y + pt.y - dragStart.y, 0, 0,
                         SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging) { dragging = false; ReleaseCapture(); clampPosition(); scheduleSave(); }
        return 0;
    case WM_CAPTURECHANGED:
        if (dragging) { dragging = false; clampPosition(); scheduleSave(); }
        return 0;
    case WM_RBUTTONUP: openSettings(); return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, config.locked ? IDC_ARROW : IDC_SIZEALL)); return TRUE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(window, &ps); EndPaint(window, &ps); renderClock(); return 0; }
    case WM_TIMER:
        if (wParam == SAVE_TIMER) { KillTimer(window, SAVE_TIMER); saveConfig(); }
        else { renderClock(); updateTray(); }
        return 0;
    case WM_TIMECHANGE: renderClock(); return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) renderClock();
        return TRUE;
    case WM_DISPLAYCHANGE: clampPosition(); renderClock(); scheduleSave(); return 0;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) { clampPosition(); renderClock(); scheduleSave(); }
        return 0;
    case WM_DPICHANGED: {
        RECT* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        applyConfig(); return 0;
    }
    case WM_HOTKEY: if (wParam == HOTKEY_ID) openSettings(); return 0;
    case TRAY_MESSAGE:
        if (LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_RBUTTONUP) trayMenu();
        else if (LOWORD(lParam) == NIN_SELECT || LOWORD(lParam) == NIN_KEYSELECT) openSettings();
        return 0;
    case SHOW_SETTINGS: openSettings(); return 0;
    case QUIT_APP: DestroyWindow(window); return 0;
    case WM_QUERYENDSESSION: saveConfig(); return TRUE;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        saveConfig();
        if (settingsWindow) DestroyWindow(settingsWindow);
        KillTimer(window, 1); KillTimer(window, SAVE_TIMER);
        if (hotkeyReady) UnregisterHotKey(window, HOTKEY_ID);
        Shell_NotifyIconW(NIM_DELETE, &tray); clockWindow = nullptr; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool writeUtf8(const std::wstring& path, const std::wstring& content) {
    int bytes = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> utf8(bytes);
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written; bool ok = WriteFile(file, utf8.data(), bytes - 1, &written, nullptr) && written == static_cast<DWORD>(bytes - 1);
    CloseHandle(file); return ok;
}

int selfTest(const std::wstring& reportPath) {
    int tests = 0, failed = 0;
    std::wstring details;
    auto check = [&](bool ok, const wchar_t* name) {
        ++tests; if (!ok) ++failed;
        details += std::wstring(ok ? L"PASS " : L"FAIL ") + name + L"\n";
    };
    Config c; c.scale = -99; c.opacity = 999; c.theme = 20; normalize(c);
    check(c.scale == 60 && c.opacity == 100 && c.theme == 2, L"configuration bounds");
    c = Config{}; auto size = pixelSize(c, 144);
    check(size.cx == 450 && size.cy == 450, L"150 percent display scaling");
    SYSTEMTIME t{}; t.wYear = 2026; t.wMonth = 10; t.wDay = 26; t.wDayOfWeek = 1;
    t.wHour = 0; t.wMinute = 5; t.wSecond = 9;
    check(timeText(t, true) == L"00:05:09", L"24 hour midnight formatting");
    check(timeText(t, false) == L"00:05", L"24 hour formatting");
    CLSID pngEncoder{};
    UINT count = 0, length = 0; GetImageEncodersSize(&count, &length);
    std::vector<BYTE> codecs(length);
    GetImageEncoders(count, length, reinterpret_cast<ImageCodecInfo*>(codecs.data()));
    for (UINT i = 0; i < count; ++i) {
        auto& codec = reinterpret_cast<ImageCodecInfo*>(codecs.data())[i];
        if (wcscmp(codec.MimeType, L"image/png") == 0) pngEncoder = codec.Clsid;
    }
    auto outDir = reportPath.substr(0, reportPath.find_last_of(L"\\/"));
    SYSTEMTIME renderTime{};
    renderTime.wYear = 2026; renderTime.wMonth = 10; renderTime.wDay = 26;
    renderTime.wHour = 10; renderTime.wMinute = 10; renderTime.wSecond = 30;
    for (int theme = 0; theme < 3; ++theme) {
        c = Config{}; c.theme = theme;
        auto s = logicalSize();
        Bitmap image(static_cast<int>(s.Width), static_cast<int>(s.Height), PixelFormat32bppPARGB);
        Status status;
        {
            Graphics graphics(&image); graphics.Clear(Color(0, 0, 0, 0));
            drawClock(graphics, c, renderTime); status = graphics.GetLastStatus();
        }
        auto path = outDir + L"\\clock-theme-" + std::to_wstring(theme) + L".png";
        auto saved = image.Save(path.c_str(), &pngEncoder, nullptr);
        Color center; image.GetPixel(static_cast<int>(s.Width / 2), static_cast<int>(s.Height / 2), &center);
        check(status == Ok && saved == Ok && center.GetA() > 0,
              (L"render theme " + std::to_wstring(theme)).c_str());
    }
    details += L"\n" + std::to_wstring(tests - failed) + L"/" + std::to_wstring(tests) + L" passed\n";
    return writeUtf8(reportPath, details) && failed == 0 ? 0 : 1;
}
}

void saveConfig() {
    if (iniPath.empty()) return;
    if (clockWindow) { RECT r; GetWindowRect(clockWindow, &r); config.x = r.left; config.y = r.top; }
    std::wstring ini = L"\xFEFF[Clock]\r\n";
    auto entry = [&](const wchar_t* key, int value) { ini += std::wstring(key) + L"=" + std::to_wstring(value) + L"\r\n"; };
    entry(L"Theme", config.theme);
    entry(L"Scale", config.scale); entry(L"Opacity", config.opacity);
    entry(L"Seconds", config.seconds);
    entry(L"Smooth", config.smooth);
    entry(L"Topmost", config.topmost); entry(L"ClickThrough", config.clickThrough);
    entry(L"Locked", config.locked); entry(L"X", config.x); entry(L"Y", config.y);
    std::wstring temp = iniPath + L".tmp";
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = false;
    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytes = static_cast<DWORD>(ini.size() * sizeof(wchar_t)), written = 0;
        ok = WriteFile(file, ini.data(), bytes, &written, nullptr) && written == bytes;
        CloseHandle(file);
        if (ok) ok = MoveFileExW(temp.c_str(), iniPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
    if (!ok && !iniWarningShown) {
        iniWarningShown = true;
        MessageBoxW(settingsWindow, L"设置暂时无法保存。请把程序放在可写入的文件夹，然后重新启动。",
                    L"Luma Clock", MB_OK | MB_ICONWARNING);
    }
}
void scheduleSave() { if (clockWindow) SetTimer(clockWindow, SAVE_TIMER, 400, nullptr); }
void resetPosition() {
    if (!clockWindow) return;
    auto area = workArea(clockWindow); auto size = pixelSize(config, GetDpiForWindow(clockWindow));
    int margin = MulDiv(32, GetDpiForWindow(clockWindow), 96);
    int x = std::max(static_cast<int>(area.left), static_cast<int>(area.right - size.cx - margin));
    int y = area.top + margin;
    SetWindowPos(clockWindow, nullptr, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    config.x = x; config.y = y; renderClock(); scheduleSave();
}
void applyConfig(bool resize) {
    normalize(config);
    LONG_PTR styles = GetWindowLongPtrW(clockWindow, GWL_EXSTYLE);
    if (config.clickThrough) styles |= WS_EX_TRANSPARENT;
    else styles &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(clockWindow, GWL_EXSTYLE, styles);
    auto size = pixelSize(config, GetDpiForWindow(clockWindow));
    DWORD flags = SWP_NOMOVE | SWP_NOACTIVATE | (resize ? 0 : SWP_NOSIZE);
    bool settingsOpen = settingsFocused && settingsWindow && IsWindowVisible(settingsWindow) && !IsIconic(settingsWindow);
    if (settingsOpen) {
        SetWindowPos(clockWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetWindowPos(clockWindow, settingsWindow, 0, 0, size.cx, size.cy, flags);
    } else {
        SetWindowPos(clockWindow, config.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, size.cx, size.cy, flags);
    }
    if (resize) clampPosition();
    KillTimer(clockWindow, 1);
    SetTimer(clockWindow, 1, config.seconds && config.smooth ? 50 : 1000, nullptr);
    renderClock(); scheduleSave();
}
bool startupEnabled() {
    wchar_t value[32768]; DWORD size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        L"LumaClock", RRF_RT_REG_SZ, nullptr, value, &size) == ERROR_SUCCESS;
}
bool setStartup(bool enabled) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS result;
    if (enabled) {
        std::wstring value = L"\"" + exePath + L"\"";
        result = RegSetValueExW(key, L"LumaClock", 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, L"LumaClock"); if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key); return result == ERROR_SUCCESS;
}
void updateTray() {
    static int lastMinute = -1;
    SYSTEMTIME now; GetLocalTime(&now);
    if (lastMinute == now.wMinute) return;
    lastMinute = now.wMinute;
    std::wstring tip = L"Luma Clock · " + timeText(now, false) + L"\n右键打开设置";
    wcsncpy_s(tray.szTip, tip.c_str(), _TRUNCATE);
    tray.uFlags = NIF_TIP; Shell_NotifyIconW(NIM_MODIFY, &tray);
}

int run(HINSTANCE current, const std::vector<std::wstring>& args) {
    instance = current;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t exe[32768]; GetModuleFileNameW(nullptr, exe, 32768); exePath = exe;
    iniPath = exePath.substr(0, exePath.find_last_of(L"\\/")) + L"\\LumaClock.ini";
    GdiplusStartupInput gdiplusInput; ULONG_PTR token;
    if (GdiplusStartup(&token, &gdiplusInput, nullptr) != Ok) return 2;
    if (args.size() >= 3 && args[1] == L"--self-test") {
        int result = selfTest(args[2]); GdiplusShutdown(token); return result;
    }
    HWND existing = FindWindowW(CLOCK_CLASS, nullptr);
    if (args.size() >= 2 && args[1] == L"--quit") {
        if (existing) PostMessageW(existing, QUIT_APP, 0, 0);
        GdiplusShutdown(token); return 0;
    }
    mutexHandle = CreateMutexW(nullptr, TRUE, L"Local\\LumaClock.DesktopClock.v1");
    if (!mutexHandle || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (existing) PostMessageW(existing, SHOW_SETTINGS, 0, 0);
        if (mutexHandle) CloseHandle(mutexHandle);
        GdiplusShutdown(token); return 0;
    }
    loadConfig();
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&common);
    appIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR));
    WNDCLASSEXW clockClass{}; clockClass.cbSize = sizeof(clockClass);
    clockClass.lpfnWndProc = clockProc; clockClass.hInstance = instance;
    clockClass.lpszClassName = CLOCK_CLASS; clockClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    clockClass.hIcon = appIcon; RegisterClassExW(&clockClass);
    WNDCLASSEXW settingsClass{}; settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = settingsProc; settingsClass.hInstance = instance;
    settingsClass.lpszClassName = SETTINGS_CLASS; settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hIcon = appIcon; RegisterClassExW(&settingsClass);
    UINT dpi = GetDpiForSystem(); auto size = pixelSize(config, dpi);
    int x = config.x == INT_MIN ? 100 : config.x, y = config.y == INT_MIN ? 100 : config.y;
    bool uiTest = std::find(args.begin(), args.end(), L"--ui-test") != args.end();
    clockWindow = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE | (uiTest ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW),
        CLOCK_CLASS, L"Luma Clock", WS_POPUP, x, y, size.cx, size.cy, nullptr, nullptr, instance, nullptr);
    if (!clockWindow) { ReleaseMutex(mutexHandle); CloseHandle(mutexHandle); GdiplusShutdown(token); return 3; }
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated"); addTray();
    hotkeyReady = RegisterHotKey(clockWindow, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'L') != FALSE;
    if (!hotkeyReady) {
        hotkeyText = L"Ctrl + Alt + Shift + L";
        hotkeyReady = RegisterHotKey(clockWindow, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'L') != FALSE;
    }
    if (config.x == INT_MIN || config.y == INT_MIN) resetPosition();
    ShowWindow(clockWindow, SW_SHOWNOACTIVATE);
    applyConfig();
    renderClock();
    if (std::find(args.begin(), args.end(), L"--settings") != args.end()) openSettings();
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (settingsWindow && msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_UP || msg.wParam == VK_DOWN ||
             msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT || msg.wParam == VK_TAB)) {
            keyboardFocus = true;
        }
        if (settingsWindow && msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            HWND focused = GetFocus();
            wchar_t className[16]{};
            if (focused && GetParent(focused) == settingsWindow &&
                GetClassNameW(focused, className, 16) > 0 &&
                wcscmp(className, L"Button") == 0) {
                SendMessageW(focused, BM_CLICK, 0, 0);
                continue;
            }
        }
        if (settingsWindow && IsDialogMessageW(settingsWindow, &msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (appIcon) DestroyIcon(appIcon);
    ReleaseMutex(mutexHandle); CloseHandle(mutexHandle);
    GdiplusShutdown(token); return 0;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int count = 0; LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::wstring> args;
    if (raw) { for (int i = 0; i < count; ++i) args.emplace_back(raw[i]); LocalFree(raw); }
    return luma::run(instance, args);
}
