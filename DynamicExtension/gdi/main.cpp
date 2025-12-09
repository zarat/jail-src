// main.cpp  (alles in EINER Datei)
// Build (Beispiel):
// g++ -c main.cpp -I. -L. -lJail -lgdiplus -lole32 -std=c++14
// g++ -shared main.o -o GDI.dll -I. -L. -lJail -lgdiplus -lole32 -static-libgcc -static-libstdc++ -std=c++14

#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "Jail.h"

using namespace Gdiplus;

// Ressourcen-Listen (Indices werden an Jail zurückgegeben)
static std::vector<std::unique_ptr<Graphics>>    g_graphics;
static std::vector<std::unique_ptr<Bitmap>>      g_bitmaps;
static std::vector<std::unique_ptr<Pen>>         g_pens;
static std::vector<std::unique_ptr<SolidBrush>>  g_brushes;
static std::vector<std::unique_ptr<Font>>        g_fonts;

// GDI+ einmal pro DLL-Lifetime initialisieren
class GDIPlusManager {
public:
    GDIPlusManager() {
        GdiplusStartupInput in;
        GdiplusStartup(&token, &in, nullptr);
    }
    ~GDIPlusManager() {
        GdiplusShutdown(token);
    }
private:
    ULONG_PTR token{};
};
static GDIPlusManager g_gdiplus;

// --- Helpers ---
static bool validGfx(int i)   { return i >= 0 && i < (int)g_graphics.size() && i < (int)g_bitmaps.size(); }
static bool validPen(int i)   { return i >= 0 && i < (int)g_pens.size(); }
static bool validBrush(int i) { return i >= 0 && i < (int)g_brushes.size(); }
static bool validFont(int i)  { return i >= 0 && i < (int)g_fonts.size(); }

// "x,y;x,y;..." -> vector<Point>
static std::vector<Point> parsePoints(const std::string& s) {
    std::vector<Point> pts;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, ';')) {
        item.erase(std::remove_if(item.begin(), item.end(), ::isspace), item.end());
        if (item.empty()) continue;

        auto comma = item.find(',');
        if (comma == std::string::npos) continue;

        int x = std::stoi(item.substr(0, comma));
        int y = std::stoi(item.substr(comma + 1));
        pts.emplace_back(x, y);
    }
    return pts;
}

// GraphicsPath NICHT by-value zurückgeben (MinGW: nicht kopierbar)
static void buildRoundedRectPath(GraphicsPath& path, REAL x, REAL y, REAL w, REAL h, REAL r) {
    path.Reset();

    REAL rr = std::max<REAL>(0, std::min<REAL>(r, std::min<REAL>(w, h) / 2));
    if (rr <= 0) {
        path.AddRectangle(RectF(x, y, w, h));
        return;
    }

    REAL d = rr * 2;
    path.AddArc(x,     y,     d, d, 180, 90); // TL
    path.AddArc(x+w-d, y,     d, d, 270, 90); // TR
    path.AddArc(x+w-d, y+h-d, d, d,   0, 90); // BR
    path.AddArc(x,     y+h-d, d, d,  90, 90); // BL
    path.CloseFigure();
}

static StringAlignment mapAlign(int a) {
    switch (a) {
        case 1: return StringAlignmentCenter;
        case 2: return StringAlignmentFar;
        default: return StringAlignmentNear;
    }
}

// --- Exported API ---
extern "C" {

// GDI.Graphics(width, height) -> graphicsIndex
__declspec(dllexport) void createContext(JAIL::JObject* c, void* data) {
    int width  = c->getParameter("width")->getInt();
    int height = c->getParameter("height")->getInt();

    auto bmp = std::make_unique<Bitmap>(width, height, PixelFormat32bppARGB);
    auto gfx = std::make_unique<Graphics>(bmp.get());

    gfx->SetSmoothingMode(SmoothingModeAntiAlias);
    gfx->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    g_bitmaps.push_back(std::move(bmp));
    g_graphics.push_back(std::move(gfx));

    c->getReturnVar()->setInt((int)g_graphics.size() - 1);
}

// GDI.Save(graphics, dst)
__declspec(dllexport) void saveContext(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    std::string dst = c->getParameter("dst")->getString();
    std::wstring wideDst(dst.begin(), dst.end());

    if (!validGfx(gfx)) {
        std::cerr << "Ungültiger Graphics-Index!\n";
        return;
    }

    CLSID clsid;
    wchar_t pngClsidStr[] = L"{557CF406-1A04-11D3-9A73-0000F81EF32E}";
    if (FAILED(CLSIDFromString(pngClsidStr, &clsid))) {
        std::cerr << "Fehler beim Abrufen des PNG-CLSID!\n";
        return;
    }

    if (g_bitmaps[gfx]->Save(wideDst.c_str(), &clsid, NULL) != Ok) {
        std::cerr << "Fehler beim Speichern der Bitmap!\n";
    }
	
	//delete &g_gdiplus;
}

// GDI.Pen(r,g,b,a,width) -> penIndex
__declspec(dllexport) void createPen(JAIL::JObject* c, void* data) {
    int r = c->getParameter("r")->getInt();
    int g = c->getParameter("g")->getInt();
    int b = c->getParameter("b")->getInt();
    int a = c->getParameter("a")->getInt();
    int w = c->getParameter("width") ? c->getParameter("width")->getInt() : 1;

    auto p = std::make_unique<Pen>(Color(a, r, g, b), (REAL)w);
    p->SetLineJoin(LineJoinRound);
    p->SetStartCap(LineCapRound);
    p->SetEndCap(LineCapRound);

    g_pens.push_back(std::move(p));
    c->getReturnVar()->setInt((int)g_pens.size() - 1);
}

// GDI.PenDash(pen, dash)  dash:0 solid,1 dash,2 dot,3 dashdot,4 dashdotdot
__declspec(dllexport) void setPenDash(JAIL::JObject* c, void* data) {
    int pen = c->getParameter("pen")->getInt();
    int dash = c->getParameter("dash")->getInt();

    if (!validPen(pen)) return;

    DashStyle ds = DashStyleSolid;
    switch (dash) {
        case 1: ds = DashStyleDash; break;
        case 2: ds = DashStyleDot; break;
        case 3: ds = DashStyleDashDot; break;
        case 4: ds = DashStyleDashDotDot; break;
        default: ds = DashStyleSolid; break;
    }
    g_pens[pen]->SetDashStyle(ds);
}

// GDI.Brush(r,g,b,a) -> brushIndex
__declspec(dllexport) void createBrush(JAIL::JObject* c, void* data) {
    int r = c->getParameter("r")->getInt();
    int g = c->getParameter("g")->getInt();
    int b = c->getParameter("b")->getInt();
    int a = c->getParameter("a")->getInt();

    g_brushes.push_back(std::make_unique<SolidBrush>(Color(a, r, g, b)));
    c->getReturnVar()->setInt((int)g_brushes.size() - 1);
}

// GDI.Font(name, size, style) -> fontIndex
// style bitmask: 0 regular, 1 bold, 2 italic, 4 underline, 8 strike
__declspec(dllexport) void createFont(JAIL::JObject* c, void* data) {
    std::string name = c->getParameter("name")->getString();
    float size = (float)c->getParameter("size")->getInt();
    int style = c->getParameter("style") ? c->getParameter("style")->getInt() : 0;

    std::wstring wname(name.begin(), name.end());
    auto f = std::make_unique<Font>(wname.c_str(), size, (FontStyle)style, UnitPixel);

    g_fonts.push_back(std::move(f));
    c->getReturnVar()->setInt((int)g_fonts.size() - 1);
}

// GDI.Clear(graphics, r,g,b,a)
__declspec(dllexport) void clear(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int r = c->getParameter("r")->getInt();
    int g = c->getParameter("g")->getInt();
    int b = c->getParameter("b")->getInt();
    int a = c->getParameter("a")->getInt();
    if (!validGfx(gfx)) return;
    g_graphics[gfx]->Clear(Color(a, r, g, b));
}

// --- Basic primitives ---

// GDI.DrawLine(graphics, pen, startX,startY,endX,endY)
__declspec(dllexport) void drawLine(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    int x1 = c->getParameter("startX")->getInt();
    int y1 = c->getParameter("startY")->getInt();
    int x2 = c->getParameter("endX")->getInt();
    int y2 = c->getParameter("endY")->getInt();

    if (!validGfx(gfx) || !validPen(pen)) return;
    g_graphics[gfx]->DrawLine(g_pens[pen].get(), x1, y1, x2, y2);
}

// GDI.DrawRect(graphics, pen, x,y,w,h)
__declspec(dllexport) void drawRect(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();

    if (!validGfx(gfx) || !validPen(pen)) return;
    g_graphics[gfx]->DrawRectangle(g_pens[pen].get(), x, y, w, h);
}

// GDI.FillRect(graphics, brush, x,y,w,h)
__declspec(dllexport) void fillRect(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int br = c->getParameter("brush")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();

    if (!validGfx(gfx) || !validBrush(br)) return;
    g_graphics[gfx]->FillRectangle(g_brushes[br].get(), x, y, w, h);
}

// GDI.DrawEllipse(graphics, pen, x,y,w,h)
__declspec(dllexport) void drawEllipse(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();

    if (!validGfx(gfx) || !validPen(pen)) return;
    g_graphics[gfx]->DrawEllipse(g_pens[pen].get(), x, y, w, h);
}

// GDI.FillEllipse(graphics, brush, x,y,w,h)
__declspec(dllexport) void fillEllipse(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int br = c->getParameter("brush")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();

    if (!validGfx(gfx) || !validBrush(br)) return;
    g_graphics[gfx]->FillEllipse(g_brushes[br].get(), x, y, w, h);
}

// points: "10,10; 50,10; 50,50; 10,50"
// GDI.DrawPolygon(graphics, pen, points)
__declspec(dllexport) void drawPolygon(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    std::string ptsStr = c->getParameter("points")->getString();

    if (!validGfx(gfx) || !validPen(pen)) return;
    auto pts = parsePoints(ptsStr);
    if (pts.size() < 2) return;

    g_graphics[gfx]->DrawPolygon(g_pens[pen].get(), pts.data(), (INT)pts.size());
}

// GDI.FillPolygon(graphics, brush, points)
__declspec(dllexport) void fillPolygon(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int br = c->getParameter("brush")->getInt();
    std::string ptsStr = c->getParameter("points")->getString();

    if (!validGfx(gfx) || !validBrush(br)) return;
    auto pts = parsePoints(ptsStr);
    if (pts.size() < 3) return;

    g_graphics[gfx]->FillPolygon(g_brushes[br].get(), pts.data(), (INT)pts.size());
}

// --- RoundedRect ---

// GDI.DrawRoundedRect(graphics, pen, x,y,w,h,radius)
__declspec(dllexport) void drawRoundedRect(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();
    int radius = c->getParameter("radius")->getInt();

    if (!validGfx(gfx) || !validPen(pen)) return;

    GraphicsPath path; // stack
    buildRoundedRectPath(path, (REAL)x, (REAL)y, (REAL)w, (REAL)h, (REAL)radius);
    g_graphics[gfx]->DrawPath(g_pens[pen].get(), &path);
}

// GDI.FillRoundedRect(graphics, brush, x,y,w,h,radius)
__declspec(dllexport) void fillRoundedRect(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int br = c->getParameter("brush")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();
    int radius = c->getParameter("radius")->getInt();

    if (!validGfx(gfx) || !validBrush(br)) return;

    GraphicsPath path;
    buildRoundedRectPath(path, (REAL)x, (REAL)y, (REAL)w, (REAL)h, (REAL)radius);
    g_graphics[gfx]->FillPath(g_brushes[br].get(), &path);
}

// --- Arc / Pie ---

// GDI.DrawArc(graphics, pen, x,y,w,h, startAngle, sweepAngle)
__declspec(dllexport) void drawArc(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();
    float startAngle = (float)c->getParameter("startAngle")->getInt();
    float sweepAngle = (float)c->getParameter("sweepAngle")->getInt();

    if (!validGfx(gfx) || !validPen(pen)) return;
    g_graphics[gfx]->DrawArc(g_pens[pen].get(), x, y, w, h, startAngle, sweepAngle);
}

// GDI.DrawPie(graphics, pen, x,y,w,h, startAngle, sweepAngle)
__declspec(dllexport) void drawPie(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int pen = c->getParameter("pen")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();
    float startAngle = (float)c->getParameter("startAngle")->getInt();
    float sweepAngle = (float)c->getParameter("sweepAngle")->getInt();

    if (!validGfx(gfx) || !validPen(pen)) return;
    g_graphics[gfx]->DrawPie(g_pens[pen].get(), x, y, w, h, startAngle, sweepAngle);
}

// GDI.FillPie(graphics, brush, x,y,w,h, startAngle, sweepAngle)
__declspec(dllexport) void fillPie(JAIL::JObject* c, void* data) {
    int gfx = c->getParameter("graphics")->getInt();
    int br = c->getParameter("brush")->getInt();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();
    float startAngle = (float)c->getParameter("startAngle")->getInt();
    float sweepAngle = (float)c->getParameter("sweepAngle")->getInt();

    if (!validGfx(gfx) || !validBrush(br)) return;
    g_graphics[gfx]->FillPie(g_brushes[br].get(), x, y, w, h, startAngle, sweepAngle);
}

// --- Text ---

// GDI.DrawText(graphics, font, brush, text, x,y,w,h, align, valign)
// align/valign: 0 near, 1 center, 2 far
__declspec(dllexport) void drawText(JAIL::JObject* c, void* data) {
    int gfx  = c->getParameter("graphics")->getInt();
    int font = c->getParameter("font")->getInt();
    int br   = c->getParameter("brush")->getInt();

    std::string text = c->getParameter("text")->getString();
    int x = c->getParameter("x")->getInt();
    int y = c->getParameter("y")->getInt();
    int w = c->getParameter("w")->getInt();
    int h = c->getParameter("h")->getInt();

    int align  = c->getParameter("align")  ? c->getParameter("align")->getInt()  : 0;
    int valign = c->getParameter("valign") ? c->getParameter("valign")->getInt() : 0;

    if (!validGfx(gfx) || !validFont(font) || !validBrush(br)) return;

    std::wstring wtext(text.begin(), text.end());

    StringFormat fmt;
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoClip);
    fmt.SetAlignment(mapAlign(align));
    fmt.SetLineAlignment(mapAlign(valign));

    RectF layout((REAL)x, (REAL)y, (REAL)w, (REAL)h);
    g_graphics[gfx]->DrawString(
        wtext.c_str(),
        (INT)wtext.size(),
        g_fonts[font].get(),
        layout,
        &fmt,
        g_brushes[br].get()
    );
}

__declspec(dllexport) void scCloseGDI(JAIL::JObject* c, void* data) {
	
	delete &g_gdiplus;
	
}

// --- Registration ---
__declspec(dllexport) void registerLib(JAIL::JInterpreter* interpreter) {
    interpreter->addNative("function GDI.Graphics(width, height)", createContext, 0);
    interpreter->addNative("function GDI.Save(graphics, dst)", saveContext, 0);

    interpreter->addNative("function GDI.Pen(r, g, b, a, width)", createPen, 0);
    interpreter->addNative("function GDI.PenDash(pen, dash)", setPenDash, 0);

    interpreter->addNative("function GDI.Brush(r, g, b, a)", createBrush, 0);
    interpreter->addNative("function GDI.Font(name, size, style)", createFont, 0);

    interpreter->addNative("function GDI.Clear(graphics, r, g, b, a)", clear, 0);

    interpreter->addNative("function GDI.DrawLine(graphics, pen, startX, startY, endX, endY)", drawLine, 0);

    interpreter->addNative("function GDI.DrawRect(graphics, pen, x, y, w, h)", drawRect, 0);
    interpreter->addNative("function GDI.FillRect(graphics, brush, x, y, w, h)", fillRect, 0);

    interpreter->addNative("function GDI.DrawEllipse(graphics, pen, x, y, w, h)", drawEllipse, 0);
    interpreter->addNative("function GDI.FillEllipse(graphics, brush, x, y, w, h)", fillEllipse, 0);

    interpreter->addNative("function GDI.DrawPolygon(graphics, pen, points)", drawPolygon, 0);
    interpreter->addNative("function GDI.FillPolygon(graphics, brush, points)", fillPolygon, 0);

    interpreter->addNative("function GDI.DrawRoundedRect(graphics, pen, x, y, w, h, radius)", drawRoundedRect, 0);
    interpreter->addNative("function GDI.FillRoundedRect(graphics, brush, x, y, w, h, radius)", fillRoundedRect, 0);

    interpreter->addNative("function GDI.DrawArc(graphics, pen, x, y, w, h, startAngle, sweepAngle)", drawArc, 0);

    interpreter->addNative("function GDI.DrawPie(graphics, pen, x, y, w, h, startAngle, sweepAngle)", drawPie, 0);
    interpreter->addNative("function GDI.FillPie(graphics, brush, x, y, w, h, startAngle, sweepAngle)", fillPie, 0);

    interpreter->addNative("function GDI.DrawText(graphics, font, brush, text, x, y, w, h, align, valign)", drawText, 0);
	
	interpreter->addNative("function GDI.Destroy(graphics)", scCloseGDI, 0);
}

} // extern "C"
