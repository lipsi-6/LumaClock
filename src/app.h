#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace luma {
using namespace Gdiplus;
constexpr double PI = 3.14159265358979323846;
constexpr wchar_t CLOCK_CLASS[] = L"LumaClock.Floating.v1";
constexpr wchar_t SETTINGS_CLASS[] = L"LumaClock.Settings.v1";
constexpr UINT TRAY_MESSAGE = WM_APP + 1;
constexpr UINT SHOW_SETTINGS = WM_APP + 2;
constexpr UINT QUIT_APP = WM_APP + 3;
constexpr UINT SAVE_TIMER = 2;
constexpr int HOTKEY_ID = 44;

struct Config {
    int theme = 0, scale = 100, opacity = 100;
    bool seconds = true, smooth = false;
    bool topmost = true, clickThrough = false, locked = false;
    int x = INT_MIN, y = INT_MIN;
};
struct Palette {
    Color face, edge, ink, muted, accent, shadow;
};

extern HINSTANCE instance;
extern HWND clockWindow, settingsWindow;
extern Config config;
extern std::wstring exePath, iniPath;
extern bool clockVisible, hotkeyReady, settingsFocused, keyboardFocus;
extern std::wstring hotkeyText;
extern HICON appIcon;

Palette palette(int theme);
void roundedPath(GraphicsPath& p, RectF r, float radius);
void roundFill(Graphics& g, RectF r, float radius, Color color);
void text(Graphics& g, const std::wstring& s, RectF r, float size,
          Color color, bool bold = false, StringAlignment align = StringAlignmentNear,
          const wchar_t* family = L"Microsoft YaHei UI");
void drawClock(Graphics& g, const Config& c, const SYSTEMTIME& now);
SizeF logicalSize();
SIZE pixelSize(const Config& c, UINT dpi);
std::wstring timeText(const SYSTEMTIME& now, bool includeSeconds);
void renderClock();
void applyConfig(bool resize = true);
void saveConfig();
void scheduleSave();
void resetPosition();
void openSettings();
void refreshSettings();
bool startupEnabled();
bool setStartup(bool enabled);
LRESULT CALLBACK settingsProc(HWND, UINT, WPARAM, LPARAM);
void updateTray();
}
