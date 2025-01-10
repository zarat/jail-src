#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include "Jail.h"

using namespace Gdiplus;

// Listen für Graphics-, Bitmap- und Pen-Objekte
std::vector<std::unique_ptr<Graphics>> graphics;
std::vector<std::unique_ptr<Bitmap>> bitmaps;
std::vector<std::unique_ptr<Pen>> pens; // Hier wird pens deklariert

// GDI+ Initialisierung
class GDIPlusManager {
public:
    GDIPlusManager() {
        GdiplusStartupInput gdiInput;
        GdiplusStartup(&gdiplusToken, &gdiInput, nullptr);
    }
    ~GDIPlusManager() {
        GdiplusShutdown(gdiplusToken);
    }

private:
    ULONG_PTR gdiplusToken;
};

GDIPlusManager gdiManager;

extern "C" {

    __declspec(dllexport) void createContext(JAIL::JObject *c, void *data) {
        int width = c->getParameter("width")->getInt();
        int height = c->getParameter("height")->getInt();

        // Bitmap erstellen
        auto bitmap = std::make_unique<Bitmap>(width, height, PixelFormat32bppARGB);
        auto graphicsObj = std::make_unique<Graphics>(bitmap.get());

        // Bitmap und Graphics speichern
        bitmaps.push_back(std::move(bitmap));
        graphics.push_back(std::move(graphicsObj));

        // Index zurückgeben
        int index = static_cast<int>(graphics.size()) - 1;
        c->getReturnVar()->setInt(index);
    }

    __declspec(dllexport) void saveContext(JAIL::JObject *c, void *data) {
        int gfx = c->getParameter("graphics")->getInt();
        std::string dst = c->getParameter("dst")->getString();

        // Pfad in wstring konvertieren
        std::wstring wideDst(dst.begin(), dst.end());

        if (gfx < 0 || gfx >= bitmaps.size()) {
            std::cerr << "Ungültiger Graphics-Index!" << std::endl;
            return;
        }

        // PNG-Encoder CLSID setzen
        CLSID clsid;
        wchar_t pngClsidStr[] = L"{557CF406-1A04-11D3-9A73-0000F81EF32E}"; // CLSID für PNG
        if (FAILED(CLSIDFromString(pngClsidStr, &clsid))) {
            std::cerr << "Fehler beim Abrufen des PNG-CLSID!" << std::endl;
            return;
        }

        // Bitmap speichern
        if (bitmaps[gfx]->Save(wideDst.c_str(), &clsid, NULL) != Ok) {
            std::cerr << "Fehler beim Speichern der Bitmap!" << std::endl;
        }
		
		delete &gdiManager;
		
    }

    __declspec(dllexport) void createPen(JAIL::JObject *c, void *data) {
        int r = c->getParameter("r")->getInt();
        int g = c->getParameter("g")->getInt();
        int b = c->getParameter("b")->getInt();
        int a = c->getParameter("a")->getInt();

        // Pen mit Farbe erstellen
        Color color(a, r, g, b);
        pens.push_back(std::make_unique<Pen>(color)); // Verwendung von pens

        // Index zurückgeben
        int index = static_cast<int>(pens.size()) - 1;
        c->getReturnVar()->setInt(index);
    }

    __declspec(dllexport) void Draw(JAIL::JObject *c, void *data) {
        int graphicsIndex = c->getParameter("graphics")->getInt();
        int penIndex = c->getParameter("pen")->getInt();

        int startX = c->getParameter("startX")->getInt();
        int startY = c->getParameter("startY")->getInt();
        int endX = c->getParameter("endX")->getInt();
        int endY = c->getParameter("endY")->getInt();

        if (graphicsIndex < 0 || graphicsIndex >= graphics.size() || penIndex < 0 || penIndex >= pens.size()) {
            std::cerr << "Ungültige Grafik- oder Pen-Indizes!" << std::endl;
            return;
        }

        graphics[graphicsIndex]->DrawLine(pens[penIndex].get(), startX, startY, endX, endY);
    }

    __declspec(dllexport) void registerLib(JAIL::JInterpreter *interpreter) {
        interpreter->addNative("function GDI.Graphics(width, height)", createContext, 0);
        interpreter->addNative("function GDI.Save(graphics, dst)", saveContext, 0);
        interpreter->addNative("function GDI.Pen(r, g, b, a)", createPen, 0);
        interpreter->addNative("function GDI.DrawLine(graphics, pen, startX, startY, endX, endY)", Draw, 0);
    }
}
