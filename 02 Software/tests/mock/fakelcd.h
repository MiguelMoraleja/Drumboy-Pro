// A pixel buffer that behaves like the firmware's Lcd for the screens drawn by header-only templates
// (GeneratorScreens.h, FileScreens.h): the firmware's own font tables (Lcd/Font_*.h), a lane-area
// check on every pixel, and PPM output. Used by test_generatorscreens and test_filescreens.
#ifndef MOCK_FAKELCD_H
#define MOCK_FAKELCD_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Font_05x07.h"
#include "Font_07x09.h"
#include "Font_10x14.h"
#include "GeneratorScreens.h"  // GsFont, GsAlign and the lane area (kGsArea*)

// The font's character map (Lcd::getLetter): index 52 = no glyph.
inline uint8_t glyphIndex(char c) {
    static const uint8_t fontMap[128] = {
        /* 00-09 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 10-19 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 20-29 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 30-39 */ 52, 52, 52, 51, 52, 50, 52, 39, 52, 52,
        /* 40-49 */ 52, 52, 52, 38, 41, 37, 40, 36, 0, 1,
        /* 50-59 */ 2, 3, 4, 5, 6, 7, 8, 9, 42, 52,
        /* 60-69 */ 43, 52, 44, 49, 52, 10, 11, 12, 13, 14,
        /* 70-79 */ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        /* 80-89 */ 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
        /* 90-99 */ 35, 45, 52, 46, 52, 47, 52, 10, 11, 12,
        /*100-109*/ 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        /*110-119*/ 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
        /*120-129*/ 33, 34, 35, 52, 48, 52, 52, 52};
    const unsigned char u = static_cast<unsigned char>(c);
    return (u > 127) ? 52 : fontMap[u];
}

struct TextBox {
    int x, y, w, h;
    std::string text;
};

// A pixel buffer that behaves like the firmware's Lcd for what GeneratorScreens.h uses.
struct FakeLcd {
    static const int W = 960, H = 540;
    std::vector<uint16_t> pixels;
    std::vector<uint8_t> touched;
    GsFont font = GS_FONT_05x07;
    GsAlign align = GS_LEFT;
    uint16_t fore = 0xFFFF, back = 0x0000;
    int outsideArea = 0, offscreen = 0, unsupported = 0, ops = 0;
    std::string badChars;
    std::vector<TextBox> texts;

    FakeLcd() : pixels(W * H, 0x1234), touched(W * H, 0) {}

    void setFont(GsFont f) { font = f; }
    void setAlignment(GsAlign a) { align = a; }
    void setForeColor(uint16_t c) { fore = c; }
    void setBackColor(uint16_t c) { back = c; }

    void put(int x, int y, uint16_t c) {
        if ((x < 0) || (y < 0) || (x >= W) || (y >= H)) {
            offscreen++;
            return;
        }
        if ((x < kGsAreaX) || (x >= kGsAreaX + kGsAreaW) || (y < kGsAreaY) || (y >= kGsAreaY + kGsAreaH)) outsideArea++;
        pixels[y * W + x] = c;
        touched[y * W + x] = 1;
    }
    void drawPixel(uint16_t x, uint16_t y) { put(x, y, fore); }
    void drawHLine(int x, int y, int l) {
        for (int i = 0; i < l; i++) put(x + i, y, fore);
    }
    void drawVLine(uint16_t x, uint16_t y, int16_t l) {
        ops++;
        for (int i = 0; i < l; i++) put(x, y + i, fore);
    }
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        ops++;
        int x = x0, y = y0;
        const int dx = std::abs(static_cast<int>(x1) - x), dy = -std::abs(static_cast<int>(y1) - y);
        const int sx = (x < x1) ? 1 : -1, sy = (y < y1) ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            put(x, y, fore);
            if ((x == x1) && (y == y1)) break;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y += sy;
            }
        }
    }
    void fillCircle(uint16_t cx, uint16_t cy, uint16_t r) {
        ops++;
        for (int dy = -static_cast<int>(r); dy <= static_cast<int>(r); dy++)
            for (int dx = -static_cast<int>(r); dx <= static_cast<int>(r); dx++)
                if (dx * dx + dy * dy <= static_cast<int>(r) * static_cast<int>(r)) put(cx + dx, cy + dy, fore);
    }
    void drawRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
        ops++;
        if ((w == 0) || (h == 0)) return;
        drawHLine(x, y, static_cast<int>(w));
        drawHLine(x, y + static_cast<int>(h) - 1, static_cast<int>(w));
        for (uint32_t i = 0; i < h; i++) {
            put(x, y + static_cast<int>(i), fore);
            put(x + static_cast<int>(w) - 1, y + static_cast<int>(i), fore);
        }
    }
    void fillRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
        ops++;
        for (uint32_t j = 0; j < h; j++)
            for (uint32_t i = 0; i < w; i++) put(x + static_cast<int>(i), y + static_cast<int>(j), fore);
    }
    void clearRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
        ops++;
        for (uint32_t j = 0; j < h; j++)
            for (uint32_t i = 0; i < w; i++) put(x + static_cast<int>(i), y + static_cast<int>(j), back);
    }
    void drawText(const char* text, uint16_t length, uint16_t x, uint16_t y) {
        ops++;
        const int fw = (font == GS_FONT_05x07) ? kFontWidth_05x07 : ((font == GS_FONT_07x09) ? kFontWidth_07x09 : kFontWidth_10x14);
        const int fh = (font == GS_FONT_05x07) ? kFontHeight_05x07 : ((font == GS_FONT_07x09) ? kFontHeight_07x09 : kFontHeight_10x14);
        const int fs = (font == GS_FONT_05x07) ? kFontSpacing_05x07 : ((font == GS_FONT_07x09) ? kFontSpacing_07x09 : kFontSpacing_10x14);
        const int w = length * (fw + fs);
        int xPos = x;
        if (align == GS_CENTER) xPos = x - w / 2;
        if (align == GS_RIGHT) xPos = x - w;
        std::string s(text, text + length);
        texts.push_back(TextBox{xPos, y, w, fh, s});
        for (int i = 0; i < length; i++) {
            const uint8_t idx = glyphIndex(text[i]);
            if ((idx == 52) && (text[i] != ' ')) {
                unsupported++;
                badChars += std::string("'") + text[i] + "' in '" + s + "'  ";
            }
            for (int row = 0; row < fh; row++) {
                uint16_t bits = 0;
                if (font == GS_FONT_05x07) bits = kFontData_05x07[idx][row];
                if (font == GS_FONT_07x09) bits = kFontData_07x09[idx][row];
                if (font == GS_FONT_10x14) bits = kFontData_10x14[idx][row];
                for (int col = 0; col < fw + fs; col++) {
                    const bool on = (col < fw) && (bits & (1u << (fw - 1 - col)));
                    put(xPos + i * (fw + fs) + col, y + row, on ? fore : back);
                }
            }
        }
    }

    // The area before a screen is drawn: a pattern, so pixels the screen never touches are visible.
    void dirtyArea() {
        for (int y = kGsAreaY; y < kGsAreaY + kGsAreaH; y++)
            for (int x = kGsAreaX; x < kGsAreaX + kGsAreaW; x++) pixels[y * W + x] = 0x1234;
    }
    void dump(const char* name) const {
        const char* dir = std::getenv("GS_DUMP_DIR");
        if (!dir) return;
        std::string path = std::string(dir) + "/" + name + ".ppm";
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return;
        std::fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W * H; i++) {
            const uint16_t c = pixels[i];
            const uint8_t r = static_cast<uint8_t>(((c >> 11) & 31) * 255 / 31), g = static_cast<uint8_t>(((c >> 5) & 63) * 255 / 63), b = static_cast<uint8_t>((c & 31) * 255 / 31);
            std::fputc(r, f);
            std::fputc(g, f);
            std::fputc(b, f);
        }
        std::fclose(f);
    }
};


inline bool fakeLcdOverlap(const TextBox& a, const TextBox& b) {
    return (a.x < b.x + b.w) && (b.x < a.x + a.w) && (a.y < b.y + b.h) && (b.y < a.y + a.h);
}

#endif  // MOCK_FAKELCD_H
