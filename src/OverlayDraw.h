// OverlayDraw.h — Kaplamanın çizim ilkelleri ve renkleri.
//
// Bunlar OverlayPaint.cpp'nin isimsiz uzayında duruyordu ve orası tek başına
// yeterliydi — ta ki büyüteç ve ipucu kutusu kendi dosyasına taşınana kadar.
// İki dosya da aynı dört yardımcıyı ve aynı renkleri kullanıyor; alternatif
// ikisinde de ayrı birer kopya tutmaktı, ve bir gün yalnızca birinin
// değiştirilmesi kaçınılmazdı.
//
// HEPSİ `inline`: başlıkta gövde var, ayrı bir çeviri birimi yok. Fonksiyonlar
// birkaç satır ve her boyamada onlarca kez çağrılıyor.
#pragma once

#include "Geometry.h"

#include <windows.h>

namespace crisp {
namespace draw {

constexpr COLORREF kAccent      = RGB(10, 132, 255);
constexpr COLORREF kPanelBack   = RGB(24, 24, 27);
constexpr COLORREF kPanelBorder = RGB(63, 63, 70);
constexpr COLORREF kTextPrimary = RGB(244, 244, 245);
constexpr COLORREF kTextDim     = RGB(161, 161, 170);
constexpr COLORREF kWhite       = RGB(255, 255, 255);

[[nodiscard]] inline LONG Scale(LONG value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

// Ekran koordinatını istemci koordinatına çevirir.
[[nodiscard]] inline RECT ToClient(const RECT& r, const RECT& screen) noexcept {
    return RECT{r.left - screen.left, r.top - screen.top, r.right - screen.left,
                r.bottom - screen.top};
}

[[nodiscard]] inline POINT ToClient(POINT p, const RECT& screen) noexcept {
    return POINT{p.x - screen.left, p.y - screen.top};
}

inline void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &r, brush);
    ::DeleteObject(brush);
}

// İçi boş dikdörtgen çerçeve. FrameRect fırça boyutunu kullanmadığı için
// kalınlık dört ayrı FillRect ile verilir.
inline void DrawFrame(HDC dc, const RECT& r, LONG thickness, COLORREF color) {
    if (thickness <= 0) {
        return;
    }
    FillRectColor(dc, RECT{r.left, r.top, r.right, r.top + thickness}, color);
    FillRectColor(dc, RECT{r.left, r.bottom - thickness, r.right, r.bottom}, color);
    FillRectColor(dc, RECT{r.left, r.top + thickness, r.left + thickness,
                           r.bottom - thickness}, color);
    FillRectColor(dc, RECT{r.right - thickness, r.top + thickness, r.right,
                           r.bottom - thickness}, color);
}

}  // namespace draw
}  // namespace crisp
