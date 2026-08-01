// ColorPickerInternal.h — Renk seçicinin İÇ paylaşımı.
//
// AYRI DOSYA: pencere ömrü/mesajlar (ColorPicker.cpp) ile çizim
// (ColorPickerPaint.cpp) arasında paylaşılır; ikisi tek dosyada 400 satırı
// aşıyordu (docs §9).
#pragma once

#include "AlphaLayer.h"
#include "Capture.h"
#include "ColorSpace.h"
#include "Settings.h"

#include <string>
#include <vector>

#include <windows.h>

namespace crisp {
namespace picker {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kPad = 12;
inline constexpr int kSquareWidth = 216;
inline constexpr int kSquareHeight = 140;
inline constexpr int kHueWidth = 16;
inline constexpr int kGap = 8;
inline constexpr int kRow = 30;
inline constexpr int kSwatch = 16;
inline constexpr int kSwatchGap = 4;
inline constexpr int kColumns = 12;
inline constexpr int kPresetRows = 3;
inline constexpr int kButtonWidth = 82;
inline constexpr int kPanelWidth = kPad * 2 + kSquareWidth + kGap + kHueWidth;

// Hazır renkler: koyu, orta ve açık olmak üzere üç sıra.
//
// ÜÇ SIRA ÇÜNKÜ İKİSİ YETMİYOR: işaretlemede hem koyu bir zemine yazılacak
// açık renk hem açık bir zemine yazılacak koyu renk gerekir; tek sıra ton
// gösteren bir palet her seferinde birini eksik bırakıyordu.
inline constexpr COLORREF kPresets[kColumns * kPresetRows] = {
    RGB(0, 0, 0),     RGB(64, 64, 64),   RGB(128, 128, 128), RGB(191, 191, 191),
    RGB(255, 255, 255), RGB(127, 29, 29), RGB(146, 64, 14),  RGB(133, 77, 14),
    RGB(63, 98, 18),  RGB(6, 95, 70),    RGB(21, 94, 117),   RGB(30, 64, 175),

    RGB(239, 68, 68), RGB(249, 115, 22), RGB(245, 158, 11),  RGB(234, 179, 8),
    RGB(132, 204, 22), RGB(34, 197, 94), RGB(16, 185, 129),  RGB(6, 182, 212),
    RGB(59, 130, 246), RGB(99, 102, 241), RGB(168, 85, 247), RGB(236, 72, 153),

    RGB(252, 165, 165), RGB(253, 186, 116), RGB(252, 211, 77), RGB(253, 224, 71),
    RGB(190, 242, 100), RGB(134, 239, 172), RGB(110, 231, 183), RGB(103, 232, 249),
    RGB(147, 197, 253), RGB(165, 180, 252), RGB(216, 180, 254), RGB(249, 168, 212),
};

enum class Hit {
    None,
    Square,
    Hue,
    Hex,
    Eyedropper,
    Preset,
    Recent,
    Accept,
    Cancel,
};

struct Metrics {
    int width = 0;
    int height = 0;
    RECT square{};
    RECT hue{};
    RECT preview{};
    RECT hex{};
    RECT eyedropper{};
    RECT presets{};
    RECT recent{};
    RECT accept{};
    RECT cancel{};
    int cell = 0;      // örnek kenarı + boşluk
    int swatch = 0;
};

struct State {
    HINSTANCE instance = nullptr;
    Settings settings;
    unsigned dpi = 96;
    Metrics metrics;

    COLORREF color = 0;
    COLORREF original = 0;
    Hsv hsv;

    // Hex alanı YAZILIRKEN ham dize tutulur: her tuş sonrası renge çevirip
    // geri biçimlendirmek, kullanıcının yazdığı yarım değeri silerdi.
    std::wstring hexText;
    bool hexFocus = false;

    std::vector<COLORREF> recent;

    Hit pressed = Hit::None;   // fare basılı tutulurken sürüklenen bölge
    Hit hover = Hit::None;
    int hoverIndex = -1;

    bool accepted = false;

    // Damlalık çalışırken panel gizlenir ve pencere ETKİNLİĞİNİ KAYBEDER;
    // bu bayrak olmasaydı "dışarı tıklandı" sanılıp panel kapanırdı.
    bool suspended = false;

    // Ton değişince yeniden üretilen doygunluk-parlaklık karesi. Her boyamada
    // otuz bin piksel hesaplamak, fareyi sürüklerken gözle görülür şekilde
    // takılırdı.
    Image squareBitmap;
    Image hueBitmap;
    double squareHue = -1.0;

    // Yuvarlatılmış zeminler (örnekler, alanlar, düğmeler) tek bir alfa
    // katmanına çizilip bir kerede karıştırılır; GDI'nin RoundRect'i kenar
    // yumuşatmaz ve elli küçük kare tırtıklı çıkardı.
    AlphaLayer chrome;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;
void BuildMetrics(State& state);

// Fare noktasının hangi bölgeye düştüğü. Preset/Recent için `index` doldurulur.
[[nodiscard]] Hit HitTest(const State& state, POINT point, int& index) noexcept;

// Panel içeriğini `dc`ye çizer.
void Paint(HWND window, State& state);

// RGB'den gelen bir renk: HSV ile hex metnini tazeler.
//
// TON KORUNUR: siyah ve gri için RgbToHsv ton olarak 0 verir ve o değeri
// yazmak, kullanıcı kareyi siyah köşeye sürükleyip geri döndüğünde seçtiği
// tonu kırmızıya çevirirdi.
void SetColor(State& state, COLORREF color);

// Kare ya da şerit sürüklendi: HSV'den rengi ve hex metnini üretir.
void SyncFromHsv(State& state);

}  // namespace picker
}  // namespace crisp
