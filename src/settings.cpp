#include "app.h"
#include <cwchar>
#include <dwmapi.h>

namespace luma {
namespace {
constexpr int NAV_APPEARANCE = 100, NAV_BEHAVIOR = 101, NAV_ABOUT = 102;
constexpr int THEME_FIRST = 210, SIZE_SLIDER = 220, OPACITY_SLIDER = 221;
constexpr int OPT_SECONDS = 300, OPT_SMOOTH = 303;
constexpr int OPT_TOPMOST = 304, OPT_THROUGH = 305, OPT_LOCKED = 306, OPT_STARTUP = 307;
constexpr int RESET_POSITION = 320, RESET_APPEARANCE = 321, CLOSE_SETTINGS = 400;
constexpr int CLIENT_W = 860, CLIENT_H = 580;
constexpr float RAIL_W = 176;
constexpr float CONTENT_X = 208, CONTENT_W = 612;

int page = 0;
float uiScale = 1;
HFONT controlFont = nullptr;
HBRUSH background = nullptr;
struct Child {
    HWND hwnd;
    int page;
};
std::vector<Child> controls;

Color paper(255, 242, 239, 232);
Color rail(255, 233, 229, 219);
Color line(255, 214, 208, 194);
Color hair(255, 229, 224, 213);
Color ink(255, 30, 29, 26);
Color muted(255, 111, 106, 94);
Color faint(255, 118, 110, 96);
Color accent(255, 166, 61, 42);
Color stage(255, 250, 248, 242);
Color press(255, 224, 219, 208);
Color selected(255, 229, 224, 211);
Color onInk(255, 249, 246, 238);

int px(float n) { return static_cast<int>(std::lround(n * uiScale)); }
void outline(Graphics& g, RectF r, float radius, Color color, float width = 1) {
    GraphicsPath p;
    roundedPath(p, r, radius);
    Pen pen(color, width);
    g.DrawPath(&pen, &p);
}
void rule(Graphics& g, float x1, float y1, float x2, float y2, Color color, float width = 1) {
    Pen pen(color, width);
    g.DrawLine(&pen, PointF(x1, y1), PointF(x2, y2));
}
void makeButton(int id, const wchar_t* title, float x, float y, float w, float h, int ownerPage) {
    HWND b = CreateWindowExW(0, L"BUTTON", title,
        WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        px(x), px(y), px(w), px(h), settingsWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), TRUE);
    controls.push_back({b, ownerPage});
}
void slider(int id, const wchar_t* title, int min, int max, int value,
            float x, float y, float w, int ownerPage) {
    HWND s = CreateWindowExW(0, TRACKBAR_CLASSW, title,
        WS_CHILD | WS_TABSTOP | TBS_NOTICKS | TBS_HORZ,
        px(x), px(y), px(w), px(28), settingsWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(s, TBM_SETRANGE, TRUE, MAKELPARAM(min, max));
    SendMessageW(s, TBM_SETPOS, TRUE, value);
    SendMessageW(s, TBM_SETPAGESIZE, 0, 10);
    SendMessageW(s, WM_SETFONT, reinterpret_cast<WPARAM>(controlFont), TRUE);
    controls.push_back({s, ownerPage});
}
void syncOptionEnabled() {
    for (auto& child : controls)
        if (GetDlgCtrlID(child.hwnd) == OPT_SMOOTH)
            EnableWindow(child.hwnd, config.seconds);
}
void createControls() {
    SendMessageW(settingsWindow, WM_SETREDRAW, FALSE, 0);
    for (auto& child : controls) DestroyWindow(child.hwnd);
    controls.clear();
    makeButton(NAV_APPEARANCE, L"外观", 0, 112, RAIL_W, 34, -1);
    makeButton(NAV_BEHAVIOR, L"桌面行为", 0, 150, RAIL_W, 34, -1);
    makeButton(NAV_ABOUT, L"关于", 0, 188, RAIL_W, 34, -1);

    const wchar_t* themes[] = {L"云雾白", L"午夜蓝", L"暖砂"};
    for (int i = 0; i < 3; ++i)
        makeButton(THEME_FIRST + i, themes[i], CONTENT_X + i * 206, 390, 200, 46, 0);
    slider(SIZE_SLIDER, L"大小", 60, 160, config.scale, CONTENT_X, 480, 296, 0);
    slider(OPACITY_SLIDER, L"不透明度", 25, 100, config.opacity, 524, 480, 296, 0);
    makeButton(RESET_APPEARANCE, L"还原默认外观", CONTENT_X, 528, 150, 28, 0);

    makeButton(OPT_SECONDS, L"显示秒针", CONTENT_X, 102, CONTENT_W, 44, 1);
    makeButton(OPT_SMOOTH, L"平滑秒针", CONTENT_X, 146, CONTENT_W, 44, 1);
    makeButton(OPT_TOPMOST, L"始终置顶", CONTENT_X, 252, CONTENT_W, 44, 1);
    makeButton(OPT_THROUGH, L"鼠标穿透", CONTENT_X, 296, CONTENT_W, 44, 1);
    makeButton(OPT_LOCKED, L"锁定位置", CONTENT_X, 340, CONTENT_W, 44, 1);
    makeButton(OPT_STARTUP, L"登录 Windows 后启动", CONTENT_X, 384, CONTENT_W, 44, 1);
    makeButton(RESET_POSITION, L"把时钟移回屏幕", CONTENT_X, 460, 170, 30, 1);
    makeButton(CLOSE_SETTINGS, L"完成", 710, 460, 110, 30, 1);

    makeButton(CLOSE_SETTINGS, L"回到桌面", CONTENT_X, 460, 130, 34, 2);
    syncOptionEnabled();
    SendMessageW(settingsWindow, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(settingsWindow, nullptr, FALSE);
}
void showPage(int next) {
    page = next;
    for (auto& child : controls)
        ShowWindow(child.hwnd, (child.page == -1 || child.page == page) ? SW_SHOW : SW_HIDE);
    refreshSettings();
    UpdateWindow(settingsWindow);
}
void invalidateControl(int id) {
    for (auto& child : controls)
        if (GetDlgCtrlID(child.hwnd) == id) InvalidateRect(child.hwnd, nullptr, FALSE);
}
void invalidateStage() {
    RECT r{px(CONTENT_X), px(84), px(CONTENT_X + CONTENT_W + 1), px(355)};
    InvalidateRect(settingsWindow, &r, FALSE);
}
void invalidateValue(bool size) {
    RECT r{px(size ? CONTENT_X + 196 : 720), px(458),
           px(size ? CONTENT_X + 296 : 820), px(478)};
    InvalidateRect(settingsWindow, &r, FALSE);
}
bool optionValue(int id) {
    switch (id) {
    case OPT_SECONDS: return config.seconds;
    case OPT_SMOOTH: return config.smooth;
    case OPT_TOPMOST: return config.topmost;
    case OPT_THROUGH: return config.clickThrough;
    case OPT_LOCKED: return config.locked;
    case OPT_STARTUP: return startupEnabled();
    default: return false;
    }
}
bool optionEnabled(int id) {
    return id != OPT_SMOOTH || config.seconds;
}
void syncSliders() {
    for (auto& child : controls) {
        if (child.page != 0) continue;
        int id = GetDlgCtrlID(child.hwnd);
        if (id == SIZE_SLIDER) SendMessageW(child.hwnd, TBM_SETPOS, TRUE, config.scale);
        else if (id == OPACITY_SLIDER) SendMessageW(child.hwnd, TBM_SETPOS, TRUE, config.opacity);
    }
}
void checkMark(Graphics& g, RectF box, bool on, bool enabled) {
    Color border = enabled ? (on ? ink : Color(255, 168, 160, 145)) : Color(255, 196, 190, 178);
    outline(g, box, 2, border, 1);
    if (on) {
        SolidBrush fill(enabled ? ink : Color(255, 168, 160, 145));
        GraphicsPath path;
        roundedPath(path, box, 2);
        g.FillPath(&fill, &path);
        Pen check(onInk, 1.9f);
        check.SetStartCap(LineCapRound);
        check.SetEndCap(LineCapRound);
        g.DrawLine(&check, PointF(box.X + 4.5f, box.Y + 9.5f), PointF(box.X + 7.6f, box.Y + 12.8f));
        g.DrawLine(&check, PointF(box.X + 7.6f, box.Y + 12.8f), PointF(box.X + 13.6f, box.Y + 6.2f));
    }
}
void drawButton(DRAWITEMSTRUCT* item) {
    int id = item->CtlID;
    Graphics g(item->hDC);
    g.ScaleTransform(uiScale, uiScale);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    float w = (item->rcItem.right - item->rcItem.left) / uiScale;
    float h = (item->rcItem.bottom - item->rcItem.top) / uiScale;
    bool down = (item->itemState & ODS_SELECTED) != 0;

    if (id >= NAV_APPEARANCE && id <= NAV_ABOUT) {
        SolidBrush base(rail);
        g.FillRectangle(&base, RectF(0, 0, w, h));
        if (down) {
            SolidBrush pressed(press);
            g.FillRectangle(&pressed, RectF(0, 0, w, h));
        }
        bool selected = page == id - NAV_APPEARANCE;
        const wchar_t* labels[] = {L"外观", L"桌面行为", L"关于"};
        text(g, labels[id - NAV_APPEARANCE], RectF(26, 0, w - 36, h), 15,
             selected ? ink : muted, selected);
    } else if (id >= THEME_FIRST && id < THEME_FIRST + 3) {
        int theme = id - THEME_FIRST;
        bool chosen = theme == config.theme;
        if (chosen) {
            roundFill(g, RectF(0, 0, w, h), 2, down ? press : selected);
        } else {
            SolidBrush base(paper);
            g.FillRectangle(&base, RectF(0, 0, w, h));
            if (down) {
                SolidBrush pressed(press);
                g.FillRectangle(&pressed, RectF(0, 0, w, h));
            }
        }
        outline(g, RectF(0.5f, 0.5f, w - 1, h - 1), 2, line, 1);
        auto colors = palette(theme);
        const wchar_t* names[] = {L"云雾白", L"午夜蓝", L"暖砂"};
        Font font(L"Microsoft YaHei UI", 15, chosen ? FontStyleBold : FontStyleRegular, UnitPixel);
        StringFormat measure;
        measure.SetAlignment(StringAlignmentNear);
        measure.SetLineAlignment(StringAlignmentCenter);
        measure.SetFormatFlags(StringFormatFlagsNoWrap);
        RectF bounds;
        g.MeasureString(names[theme], -1, &font, RectF(0, 0, 1000, 1000), &measure, &bounds);
        float groupWidth = 18 + 12 + bounds.Width;
        float left = (w - groupWidth) / 2;
        float centerY = h / 2;
        SolidBrush swatch(colors.face);
        g.FillEllipse(&swatch, RectF(left, centerY - 9, 18, 18));
        Pen ring(colors.edge, 1);
        g.DrawEllipse(&ring, RectF(left, centerY - 9, 18, 18));
        SolidBrush dot(colors.accent);
        g.FillEllipse(&dot, RectF(left + 12, centerY + 2, 6, 6));
        text(g, names[theme], RectF(left + 30, 0, bounds.Width + 4, h), 15,
             chosen ? ink : muted, chosen, StringAlignmentNear);
    } else if (id >= OPT_SECONDS && id <= OPT_STARTUP) {
        bool on = optionValue(id);
        bool enabled = optionEnabled(id);
        SolidBrush base(down ? press : paper);
        g.FillRectangle(&base, RectF(0, 0, w, h));
        if (id != OPT_SMOOTH && id != OPT_STARTUP)
            rule(g, 0, h - 0.5f, w, h - 0.5f, hair);
        wchar_t title[128];
        GetWindowTextW(item->hwndItem, title, 128);
        text(g, title, RectF(0, 0, w - 110, h), 15, enabled ? ink : faint);
        if (id == OPT_STARTUP)
            text(g, L"仅当前用户", RectF(210, 0, 140, h), 14, faint);
        checkMark(g, RectF(w - 28, (h - 18) / 2, 18, 18), on, enabled);
    } else {
        bool primary = id == CLOSE_SETTINGS;
        bool plain = id == RESET_APPEARANCE;
        if (primary) {
            SolidBrush solid(down ? Color(255, 52, 50, 45) : ink);
            g.FillRectangle(&solid, RectF(0, 0, w, h));
        } else {
            SolidBrush base(paper);
            g.FillRectangle(&base, RectF(0, 0, w, h));
            if (down) {
                SolidBrush pressed(press);
                g.FillRectangle(&pressed, RectF(0, 0, w, h));
            }
        }
        if (!primary && !plain) outline(g, RectF(0.5f, 0.5f, w - 1, h - 1), 2, line, 1);
        wchar_t title[128];
        GetWindowTextW(item->hwndItem, title, 128);
        text(g, title, RectF(plain ? 0 : 2, 0, w - 4, h), 14,
             primary ? onInk : (plain ? muted : ink), primary,
             plain ? StringAlignmentNear : StringAlignmentCenter);
    }
    if (keyboardFocus && GetActiveWindow() == settingsWindow && GetFocus() == item->hwndItem) {
        Color focusColor = id == CLOSE_SETTINGS
            ? Color(210, onInk.GetR(), onInk.GetG(), onInk.GetB())
            : Color(200, 120, 112, 100);
        Pen pen(focusColor, 1);
        pen.SetDashStyle(DashStyleDot);
        GraphicsPath path;
        roundedPath(path, RectF(3, 3, w - 6, h - 6), 2);
        g.DrawPath(&pen, &path);
    }
}

void paintSlider(HDC dc, HWND sliderWindow) {
    RECT rc;
    GetClientRect(sliderWindow, &rc);
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.ScaleTransform(uiScale, uiScale);
    float w = rc.right / uiScale, h = rc.bottom / uiScale;
    SolidBrush base(paper);
    g.FillRectangle(&base, RectF(0, 0, w, h));
    int minValue = static_cast<int>(SendMessageW(sliderWindow, TBM_GETRANGEMIN, 0, 0));
    int maxValue = static_cast<int>(SendMessageW(sliderWindow, TBM_GETRANGEMAX, 0, 0));
    int value = static_cast<int>(SendMessageW(sliderWindow, TBM_GETPOS, 0, 0));
    RECT channel{};
    SendMessageW(sliderWindow, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    float left = channel.left / uiScale, right = channel.right / uiScale;
    if (right <= left) { left = 6; right = w - 6; }
    float cy = h / 2;
    float ratio = maxValue > minValue ? static_cast<float>(value - minValue) / (maxValue - minValue) : 0;
    float thumb = left + (right - left) * std::clamp(ratio, 0.f, 1.f);
    rule(g, left, cy, right, cy, line, 2);
    rule(g, left, cy, thumb, cy, accent, 2);
    roundFill(g, RectF(thumb - 4, cy - 8, 8, 16), 2, ink);
}

void paintStage(Graphics& g) {
    const RectF box(CONTENT_X, 84, CONTENT_W, 270);
    SolidBrush fill(stage);
    g.FillRectangle(&fill, box);
    Pen border(line, 1);
    g.DrawRectangle(&border, box);
    Config preview = config;
    auto logical = logicalSize();
    float scale = std::min(552.f / logical.Width, 240.f / logical.Height);
    GraphicsState state = g.Save();
    g.TranslateTransform(CONTENT_X + CONTENT_W / 2 - logical.Width * scale / 2,
                         219 - logical.Height * scale / 2);
    g.ScaleTransform(scale, scale);
    SYSTEMTIME now;
    GetLocalTime(&now);
    drawClock(g, preview, now);
    g.Restore(state);
}

void paintSettings(HDC dc, int width, int height) {
    Bitmap buffer(width, height, PixelFormat32bppPARGB);
    Graphics g(&buffer);
    g.Clear(paper);
    g.ScaleTransform(uiScale, uiScale);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    SolidBrush railBrush(rail);
    g.FillRectangle(&railBrush, RectF(0, 0, RAIL_W, height / uiScale));
    rule(g, RAIL_W - 0.5f, 0, RAIL_W - 0.5f, height / uiScale, line);

    SolidBrush mark(accent);
    g.FillRectangle(&mark, RectF(24, 29, 6, 6));
    text(g, L"Luma", RectF(38, 20, 110, 28), 20, ink, true);
    text(g, L"桌面时钟", RectF(24, 48, 110, 18), 14, muted);
    text(g, L"更改会自动保存", RectF(24, 552, 150, 18), 14, faint);

    const wchar_t* titles[] = {L"外观", L"桌面行为", L"关于"};
    text(g, titles[page], RectF(CONTENT_X, 24, 400, 30), 22, ink, true);

    if (page == 0) {
        paintStage(g);
        text(g, L"配色", RectF(CONTENT_X, 368, 120, 18), 14, muted);
        wchar_t sizeValue[32], opacityValue[32];
        swprintf(sizeValue, 32, L"%d%%", config.scale);
        swprintf(opacityValue, 32, L"%d%%", config.opacity);
        text(g, L"大小", RectF(CONTENT_X, 459, 120, 18), 15, muted);
        text(g, sizeValue, RectF(CONTENT_X + 196, 459, 100, 18), 15, ink, true,
             StringAlignmentFar);
        text(g, L"不透明度", RectF(524, 459, 140, 18), 15, muted);
        text(g, opacityValue, RectF(720, 459, 100, 18), 15, ink, true,
             StringAlignmentFar);
    } else if (page == 1) {
        text(g, L"显示", RectF(CONTENT_X, 84, 120, 18), 14, muted);
        text(g, L"窗口", RectF(CONTENT_X, 230, 120, 18), 14, muted);
        std::wstring recovery = hotkeyReady
            ? L"开启鼠标穿透后，通过快捷键或托盘图标回到设置。"
            : L"开启鼠标穿透后，点击系统托盘里的 Luma 图标回到设置。";
        g.FillRectangle(&mark, RectF(CONTENT_X, 512, 2, 44));
        text(g, recovery.c_str(), RectF(CONTENT_X + 12, 512, CONTENT_W - 12, 20), 14, muted);
        text(g, L"穿透时鼠标会直接操作下方的窗口。", RectF(CONTENT_X + 12, 534, CONTENT_W - 12, 20), 14, faint);
    } else {
        text(g, L"Luma Clock", RectF(CONTENT_X, 84, 420, 34), 26, ink, true);
        text(g, L"版本 1.0", RectF(CONTENT_X, 118, 220, 18), 14, muted);
        text(g, L"一个浮在桌面上的指针时钟，由 Windows GDI+ 直接绘制，不依赖浏览器或网络。",
             RectF(CONTENT_X, 152, 590, 24), 15, ink);
        text(g, L"设置保存在程序同目录的 LumaClock.ini，移动程序时请一并复制。",
             RectF(CONTENT_X, 182, 590, 20), 13, muted);

        const wchar_t* keys[] = {L"打开设置", L"移动时钟", L"退出程序", L"配置文件", L"时间来源"};
        std::wstring vals[] = {
            hotkeyReady ? hotkeyText : L"托盘图标",
            L"直接拖动时钟",
            L"托盘菜单 → 退出 Luma Clock",
            L"LumaClock.ini",
            L"Windows 本地时间"};
        for (int i = 0; i < 5; ++i) {
            float y = 220 + i * 36;
            text(g, keys[i], RectF(CONTENT_X, y, 110, 18), 14, faint);
            text(g, vals[i], RectF(CONTENT_X + 112, y - 2, CONTENT_W - 112, 20), 14, ink);
            if (i < 4) rule(g, CONTENT_X, y + 28, CONTENT_X + CONTENT_W, y + 28, hair);
        }
    }

    Graphics screen(dc);
    screen.DrawImage(&buffer, 0, 0);
}

void handleCommand(int id) {
    if (keyboardFocus) {
        keyboardFocus = false;
        for (auto& child : controls) InvalidateRect(child.hwnd, nullptr, FALSE);
    }
    if (id >= NAV_APPEARANCE && id <= NAV_ABOUT) {
        int next = id - NAV_APPEARANCE;
        if (next != page) showPage(next);
        return;
    }
    if (id == IDCANCEL || id == CLOSE_SETTINGS) {
        DestroyWindow(settingsWindow);
        return;
    }
    if (id >= THEME_FIRST && id < THEME_FIRST + 3) {
        config.theme = id - THEME_FIRST;
        applyConfig();
        for (int i = 0; i < 3; ++i) invalidateControl(THEME_FIRST + i);
        invalidateStage();
        return;
    }
    switch (id) {
    case OPT_SECONDS: config.seconds = !config.seconds; break;
    case OPT_SMOOTH:
        if (config.seconds) config.smooth = !config.smooth;
        break;
    case OPT_TOPMOST: config.topmost = !config.topmost; break;
    case OPT_THROUGH: config.clickThrough = !config.clickThrough; break;
    case OPT_LOCKED: config.locked = !config.locked; break;
    case OPT_STARTUP:
        if (!setStartup(!startupEnabled())) MessageBoxW(settingsWindow,
            L"无法更新自动启动设置。请检查当前用户的写入权限。", L"Luma Clock", MB_OK | MB_ICONWARNING);
        break;
    case RESET_POSITION: resetPosition(); return;
    case RESET_APPEARANCE:
        config.theme = 0;
        config.scale = config.opacity = 100;
        config.seconds = true;
        config.smooth = false;
        applyConfig();
        syncSliders();
        syncOptionEnabled();
        refreshSettings();
        return;
    default: return;
    }
    applyConfig();
    if (id == OPT_SECONDS) syncOptionEnabled();
    invalidateControl(id);
    if (id == OPT_SECONDS) {
        invalidateControl(OPT_SMOOTH);
        invalidateStage();
    } else if (id == OPT_SMOOTH) {
        invalidateStage();
    }
}
}

void refreshSettings() {
    if (!settingsWindow) return;
    syncOptionEnabled();
    InvalidateRect(settingsWindow, nullptr, FALSE);
    for (auto& child : controls)
        if (IsWindowVisible(child.hwnd)) InvalidateRect(child.hwnd, nullptr, FALSE);
}
void activateSettings(HWND window) {
    HWND foreground = GetForegroundWindow();
    DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    DWORD currentThread = GetCurrentThreadId();
    bool attached = foregroundThread && foregroundThread != currentThread &&
                    AttachThreadInput(currentThread, foregroundThread, TRUE);
    SetForegroundWindow(window);
    BringWindowToTop(window);
    SetFocus(window);
    if (attached) AttachThreadInput(currentThread, foregroundThread, FALSE);
}
void openSettings() {
    if (settingsWindow) {
        SetWindowPos(clockWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(settingsWindow, SW_RESTORE);
        activateSettings(settingsWindow);
        settingsFocused = true;
        applyConfig(false);
        return;
    }
    UINT dpi = GetDpiForWindow(clockWindow);
    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(clockWindow, MONITOR_DEFAULTTONEAREST), &monitor);
    uiScale = std::min(dpi / 96.f, std::min((monitor.rcWork.right - monitor.rcWork.left - 32) / static_cast<float>(CLIENT_W),
                                          (monitor.rcWork.bottom - monitor.rcWork.top - 64) / static_cast<float>(CLIENT_H)));
    uiScale = std::max(.6f, uiScale);
    RECT rect{0, 0, px(CLIENT_W), px(CLIENT_H)};
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_APPWINDOW);
    int w = rect.right - rect.left, h = rect.bottom - rect.top;
    int x = monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - w) / 2;
    int y = monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - h) / 2;
    settingsWindow = CreateWindowExW(WS_EX_APPWINDOW, SETTINGS_CLASS, L"Luma Clock",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x, y, w, h, nullptr, nullptr, instance, nullptr);
    if (settingsWindow) {
        SetWindowPos(clockWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(settingsWindow, SW_SHOW);
        activateSettings(settingsWindow);
        settingsFocused = true;
        applyConfig(false);
    }
}

LRESULT CALLBACK settingsProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        settingsWindow = window;
        controlFont = CreateFontW(-px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        background = CreateSolidBrush(RGB(242, 239, 232));
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIcon));
        BOOL dark = FALSE;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        createControls();
        showPage(0);
        SetTimer(window, 3, 1000, nullptr);
        return 0;
    }
    case WM_SIZE:
        if (settingsWindow && clockWindow) applyConfig(false);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window, &ps);
        RECT r;
        GetClientRect(window, &r);
        paintSettings(dc, r.right, r.bottom);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_ACTIVATE:
        settingsFocused = LOWORD(wParam) != WA_INACTIVE;
        keyboardFocus = false;
        SetFocus(window);
        for (auto& child : controls) InvalidateRect(child.hwnd, nullptr, FALSE);
        InvalidateRect(window, nullptr, FALSE);
        if (clockWindow) applyConfig(false);
        return 0;
    case WM_LBUTTONDOWN:
        keyboardFocus = false;
        SetFocus(window);
        for (auto& child : controls) InvalidateRect(child.hwnd, nullptr, FALSE);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_PARENTNOTIFY:
        if (LOWORD(wParam) == WM_LBUTTONDOWN && keyboardFocus) {
            keyboardFocus = false;
            for (auto& child : controls) InvalidateRect(child.hwnd, nullptr, FALSE);
        }
        break;
    case WM_DRAWITEM: drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE;
    case WM_NOTIFY: {
        auto header = reinterpret_cast<LPNMHDR>(lParam);
        if (header->code == NM_CUSTOMDRAW) {
            auto draw = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
            if (draw->dwDrawStage == CDDS_PREPAINT) {
                int id = GetDlgCtrlID(header->hwndFrom);
                if (id == SIZE_SLIDER || id == OPACITY_SLIDER) {
                    paintSlider(draw->hdc, header->hwndFrom);
                    return CDRF_SKIPDEFAULT;
                }
            }
        }
        break;
    }
    case WM_COMMAND: handleCommand(LOWORD(wParam)); return 0;
    case WM_HSCROLL: {
        keyboardFocus = false;
        auto sliderWindow = reinterpret_cast<HWND>(lParam);
        int id = GetDlgCtrlID(sliderWindow);
        int value = static_cast<int>(SendMessageW(sliderWindow, TBM_GETPOS, 0, 0));
        if (id == SIZE_SLIDER) config.scale = value;
        else if (id == OPACITY_SLIDER) config.opacity = value;
        else return 0;
        applyConfig(id == SIZE_SLIDER);
        InvalidateRect(sliderWindow, nullptr, FALSE);
        invalidateValue(id == SIZE_SLIDER);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(242, 239, 232));
        return reinterpret_cast<LRESULT>(background);
    case WM_DPICHANGED: {
        RECT* suggested = reinterpret_cast<RECT*>(lParam);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST), &monitor);
        uiScale = std::max(.6f, std::min(HIWORD(wParam) / 96.f,
            std::min((monitor.rcWork.right - monitor.rcWork.left - 32) / static_cast<float>(CLIENT_W),
                     (monitor.rcWork.bottom - monitor.rcWork.top - 64) / static_cast<float>(CLIENT_H))));
        RECT frame{0, 0, px(CLIENT_W), px(CLIENT_H)};
        AdjustWindowRectEx(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_APPWINDOW);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
                     frame.right - frame.left, frame.bottom - frame.top, SWP_NOZORDER | SWP_NOACTIVATE);
        createControls();
        showPage(page);
        return 0;
    }
    case WM_TIMER:
        if (page == 0) {
            RECT r{px(CONTENT_X), px(84), px(CONTENT_X + CONTENT_W + 1), px(355)};
            InvalidateRect(window, &r, FALSE);
        }
        return 0;
    case WM_NCLBUTTONDOWN:
        keyboardFocus = false;
        SetFocus(window);
        for (auto& child : controls) InvalidateRect(child.hwnd, nullptr, FALSE);
        InvalidateRect(window, nullptr, FALSE);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE: DestroyWindow(window); return 0;
    case WM_DESTROY:
        KillTimer(window, 3);
        controls.clear();
        if (controlFont) DeleteObject(controlFont);
        controlFont = nullptr;
        if (background) DeleteObject(background);
        background = nullptr;
        settingsFocused = false;
        settingsWindow = nullptr;
        if (clockWindow) applyConfig(false);
        saveConfig();
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
