// ColorSpace.cpp — bkz. ColorSpace.h.
#include "ColorSpace.h"

#include <cmath>
#include <cwctype>

namespace crisp {
namespace {

[[nodiscard]] int HexDigit(wchar_t ch) noexcept {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f') {
        return ch - L'a' + 10;
    }
    if (ch >= L'A' && ch <= L'F') {
        return ch - L'A' + 10;
    }
    return -1;
}

[[nodiscard]] BYTE ToByte(double value) noexcept {
    const double scaled = value * 255.0 + 0.5;
    if (scaled <= 0.0) {
        return 0;
    }
    if (scaled >= 255.0) {
        return 255;
    }
    return static_cast<BYTE>(scaled);
}

}  // namespace

Hsv RgbToHsv(COLORREF color) noexcept {
    const double r = static_cast<double>(GetRValue(color)) / 255.0;
    const double g = static_cast<double>(GetGValue(color)) / 255.0;
    const double b = static_cast<double>(GetBValue(color)) / 255.0;

    const double high = (r > g ? (r > b ? r : b) : (g > b ? g : b));
    const double low = (r < g ? (r < b ? r : b) : (g < b ? g : b));
    const double span = high - low;

    Hsv hsv;
    hsv.value = high;
    hsv.saturation = high <= 0.0 ? 0.0 : span / high;

    // GRİ TONLARIN TONU YOKTUR ve 0 verilir. Hesaplamaya zorlamak sıfıra
    // bölme demek; kullanıcı açısından da siyahın "kırmızımsı" olması anlamsız.
    if (span <= 0.0) {
        hsv.hue = 0.0;
        return hsv;
    }

    double hue = 0.0;
    if (high == r) {
        hue = (g - b) / span;
    } else if (high == g) {
        hue = 2.0 + (b - r) / span;
    } else {
        hue = 4.0 + (r - g) / span;
    }
    hue *= 60.0;
    if (hue < 0.0) {
        hue += 360.0;
    }
    hsv.hue = hue;
    return hsv;
}

COLORREF HsvToRgb(const Hsv& hsv) noexcept {
    double hue = std::fmod(hsv.hue, 360.0);
    if (hue < 0.0) {
        hue += 360.0;
    }
    const double saturation = hsv.saturation < 0.0
                                  ? 0.0
                                  : (hsv.saturation > 1.0 ? 1.0 : hsv.saturation);
    const double value =
        hsv.value < 0.0 ? 0.0 : (hsv.value > 1.0 ? 1.0 : hsv.value);

    const double sector = hue / 60.0;
    const int index = static_cast<int>(sector) % 6;
    const double fraction = sector - std::floor(sector);

    const double p = value * (1.0 - saturation);
    const double q = value * (1.0 - saturation * fraction);
    const double t = value * (1.0 - saturation * (1.0 - fraction));

    double r = value;
    double g = t;
    double b = p;
    switch (index) {
        case 0: r = value; g = t;     b = p;     break;
        case 1: r = q;     g = value; b = p;     break;
        case 2: r = p;     g = value; b = t;     break;
        case 3: r = p;     g = q;     b = value; break;
        case 4: r = t;     g = p;     b = value; break;
        default: r = value; g = p;    b = q;     break;
    }
    return RGB(ToByte(r), ToByte(g), ToByte(b));
}

bool ParseHexColor(const wchar_t* text, COLORREF& out) noexcept {
    if (text == nullptr) {
        return false;
    }
    while (*text == L' ' || *text == L'\t' || *text == L'#') {
        ++text;
    }

    int digits[6] = {0, 0, 0, 0, 0, 0};
    int count = 0;
    while (count < 6 && text[count] != L'\0') {
        const int digit = HexDigit(text[count]);
        if (digit < 0) {
            break;   // hane bitti; kalanı aşağıdaki denetim inceler
        }
        digits[count] = digit;
        ++count;
    }
    // Kalanı boşluk olabilir ama başka bir şey OLAMAZ: "#1e90ffzz" kabul
    // edilseydi kullanıcı yazım hatasını hiç fark etmezdi.
    for (const wchar_t* rest = text + count; *rest != L'\0'; ++rest) {
        if (*rest != L' ' && *rest != L'\t') {
            return false;
        }
    }

    if (count == 3) {
        out = RGB(digits[0] * 17, digits[1] * 17, digits[2] * 17);
        return true;
    }
    if (count == 6) {
        out = RGB(digits[0] * 16 + digits[1], digits[2] * 16 + digits[3],
                  digits[4] * 16 + digits[5]);
        return true;
    }
    return false;
}

std::wstring FormatHexColor(COLORREF color) {
    wchar_t text[8];
    ::swprintf_s(text, L"#%02X%02X%02X", GetRValue(color), GetGValue(color),
                 GetBValue(color));
    return std::wstring(text);
}

int RelativeLuma(COLORREF color) noexcept {
    return (299 * GetRValue(color) + 587 * GetGValue(color) +
            114 * GetBValue(color)) /
           1000;
}

bool PrefersDarkInk(COLORREF background) noexcept {
    return RelativeLuma(background) > 140;
}

bool HasContrast(COLORREF a, COLORREF b) noexcept {
    // 55 EŞİĞİ DENEYEREK BULUNDU: koyu arayüz zemini (~32) ile kırmızı (116)
    // arasındaki fark 84 ve gözle rahat ayrılıyor; vurgu mavisi (109) ile aynı
    // kırmızı arasındaki 7 ise ayrılmıyor.
    const int difference = RelativeLuma(a) - RelativeLuma(b);
    return (difference < 0 ? -difference : difference) >= 55;
}

}  // namespace crisp
