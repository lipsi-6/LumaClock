#include "app.h"
#include <cwchar>

namespace luma {
Palette palette(int theme) {
    if (theme == 1) return {Color(255, 18, 26, 35), Color(255, 44, 56, 68),
        Color(255, 236, 238, 240), Color(255, 138, 150, 162),
        Color(255, 196, 160, 92), Color(70, 0, 0, 0)};
    if (theme == 2) return {Color(255, 232, 221, 202), Color(255, 199, 183, 157),
        Color(255, 43, 36, 29), Color(255, 128, 113, 94),
        Color(255, 124, 59, 37), Color(34, 50, 35, 20)};
    return {Color(255, 247, 244, 236), Color(255, 214, 208, 194),
        Color(255, 28, 27, 24), Color(255, 122, 115, 103),
        Color(255, 166, 61, 42), Color(30, 28, 27, 24)};
}

void roundedPath(GraphicsPath& path, RectF r, float radius) {
    radius = std::min(radius, std::min(r.Width, r.Height) / 2.f);
    float d = std::max(0.1f, radius * 2);
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
}
void roundFill(Graphics& g, RectF r, float radius, Color color) {
    GraphicsPath path;
    roundedPath(path, r, radius);
    SolidBrush brush(color);
    g.FillPath(&brush, &path);
}
void text(Graphics& g, const std::wstring& s, RectF r, float size,
          Color color, bool bold, StringAlignment align, const wchar_t* family) {
    Font f(family, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush brush(color);
    StringFormat format;
    format.SetAlignment(align);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(s.c_str(), static_cast<int>(s.size()), &f, r, &format, &brush);
}
SizeF logicalSize() {
    return SizeF(300, 300);
}
SIZE pixelSize(const Config& c, UINT dpi) {
    auto s = logicalSize();
    double factor = c.scale / 100. * dpi / 96.;
    return {static_cast<LONG>(std::lround(s.Width * factor)),
            static_cast<LONG>(std::lround(s.Height * factor))};
}
std::wstring timeText(const SYSTEMTIME& t, bool seconds) {
    wchar_t out[32];
    if (seconds) swprintf(out, 32, L"%02d:%02d:%02d", t.wHour, t.wMinute, t.wSecond);
    else swprintf(out, 32, L"%02d:%02d", t.wHour, t.wMinute);
    return out;
}

namespace {
Color lift(Color c, int amount) {
    auto up = [amount](BYTE v) { return static_cast<BYTE>(std::min(255, v + amount)); };
    return Color(c.GetA(), up(c.GetR()), up(c.GetG()), up(c.GetB()));
}
PointF polar(float cx, float cy, float length, double angle) {
    return {cx + length * static_cast<float>(std::sin(angle)),
            cy - length * static_cast<float>(std::cos(angle))};
}
void hand(Graphics& g, double angle, float length, float width, Color color, float tail = 0) {
    Pen pen(color, width);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    auto tip = polar(150, 150, length, angle);
    auto end = polar(150, 150, -tail, angle);
    g.DrawLine(&pen, end, tip);
}
}

static void analog(Graphics& g, const Config& c, const SYSTEMTIME& now) {
    auto p = palette(c.theme);
    const RectF face(15, 15, 270, 270);
    for (int i = 5; i >= 1; --i) {
        SolidBrush soft(Color(8, p.shadow.GetR(), p.shadow.GetG(), p.shadow.GetB()));
        g.FillEllipse(&soft, RectF(face.X - i, face.Y - i + 2, face.Width + i * 2, face.Height + i * 2));
    }
    LinearGradientBrush fill(PointF(60, 30), PointF(240, 280), lift(p.face, 5), p.face);
    g.FillEllipse(&fill, face);
    Pen edge(p.edge, 1);
    g.DrawEllipse(&edge, face);

    for (int i = 0; i < 60; ++i) {
        if (i % 5 == 0) continue;
        double angle = i * PI / 30;
        Pen tick(Color(165, p.muted.GetR(), p.muted.GetG(), p.muted.GetB()), 1);
        g.DrawLine(&tick, polar(150, 150, 124, angle), polar(150, 150, 129, angle));
    }
    for (int i = 0; i < 12; ++i) {
        double angle = i * PI / 6;
        Pen tick(p.ink, 2);
        tick.SetStartCap(LineCapRound);
        tick.SetEndCap(LineCapRound);
        g.DrawLine(&tick, polar(150, 150, 117, angle), polar(150, 150, 129, angle));
    }
    const wchar_t* numbers[] = {L"12", L"3", L"6", L"9"};
    const PointF places[] = {{150, 51}, {249, 150}, {150, 249}, {51, 150}};
    for (int i = 0; i < 4; ++i)
        text(g, numbers[i], RectF(places[i].X - 22, places[i].Y - 14, 44, 28), 21,
             p.ink, false, StringAlignmentCenter, L"Bahnschrift Light");

    double seconds = now.wSecond + (c.smooth ? now.wMilliseconds / 1000. : 0);
    double minutes = now.wMinute + seconds / 60.;
    double hours = now.wHour % 12 + minutes / 60.;
    hand(g, hours * PI / 6, 62, 7, p.ink, 10);
    hand(g, minutes * PI / 30, 104, 3.6f, p.ink, 14);
    if (c.seconds) hand(g, seconds * PI / 30, 112, 1.5f, p.accent, 28);
    SolidBrush pin(p.accent);
    g.FillEllipse(&pin, RectF(145.5f, 145.5f, 9, 9));
    SolidBrush core(p.face);
    g.FillEllipse(&core, RectF(148, 148, 4, 4));
}

void drawClock(Graphics& g, const Config& c, const SYSTEMTIME& now) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    analog(g, c, now);
}

void renderClock() {
    if (!clockWindow || !clockVisible) return;
    UINT dpi = GetDpiForWindow(clockWindow);
    SIZE size = pixelSize(config, dpi);
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !dc) {
        if (dib) DeleteObject(dib);
        if (dc) DeleteDC(dc);
        ReleaseDC(nullptr, screen);
        return;
    }
    HGDIOBJ old = SelectObject(dc, dib);
    {
        Bitmap bitmap(size.cx, size.cy, size.cx * 4, PixelFormat32bppPARGB,
                      static_cast<BYTE*>(bits));
        Graphics graphics(&bitmap);
        graphics.Clear(Color(0, 0, 0, 0));
        auto logical = logicalSize();
        graphics.ScaleTransform(size.cx / logical.Width, size.cy / logical.Height);
        SYSTEMTIME now; GetLocalTime(&now);
        drawClock(graphics, config, now);
    }
    RECT rect; GetWindowRect(clockWindow, &rect);
    POINT dest{rect.left, rect.top}, origin{};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(config.opacity * 255 / 100), AC_SRC_ALPHA};
    UpdateLayeredWindow(clockWindow, screen, &dest, &size, dc, &origin, 0, &blend, ULW_ALPHA);
    SelectObject(dc, old); DeleteObject(dib); DeleteDC(dc); ReleaseDC(nullptr, screen);
}
}
