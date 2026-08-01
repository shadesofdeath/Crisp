// EditorStatus.cpp — Durum çubuğu: ölçü, imleç konumu, onay şeridi ve
// yakınlaştırma kaydırıcısı.
//
// NEDEN VAR: yakınlaştırma eklendiği anda "şu an %kaçtayım" sorusu doğdu ve
// bunun cevabı hiçbir yerde yazmıyordu. Aynı şeritte ölçü ve imleç konumu da
// bedavaya geliyor — ikisi de kırpma yaparken sorulan sorular.
//
// KAYDIRICI DA BURADA: yakınlaştırma düğmeleri araç çubuğunda dururken "şu an
// neredeyim, sınırın neresindeyim" sorusunun görsel bir cevabı yoktu ve %10
// ile %800 arasında düğmeyle gitmek onlarca tıklama demekti.
#include "EditorInternal.h"

#include "Geometry.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace crisp {
namespace editor {
namespace {

// Kaydırıcı LOGARİTMİK. Doğrusal olsaydı %10–%100 aralığı olukta yalnızca
// sekizde bir yer kaplar, kullanışlı yakınlaştırmaların tamamı ilk birkaç
// piksele sıkışırdı.
[[nodiscard]] double ZoomToFraction(double zoom) noexcept {
    const double low = std::log(kMinZoom);
    const double high = std::log(kMaxZoom);
    const double value = std::log(zoom < kMinZoom ? kMinZoom : zoom);
    const double t = (value - low) / (high - low);
    return t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
}

[[nodiscard]] double FractionToZoom(double t) noexcept {
    const double low = std::log(kMinZoom);
    const double high = std::log(kMaxZoom);
    return std::exp(low + (high - low) * (t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t)));
}

void DrawSlider(HDC dc, const State& state, const RECT& slider) {
    const Palette& colors = theme::Colors();
    const RECT track = ZoomTrack(state, slider);
    const int centre = (track.top + track.bottom) / 2;
    const int thickness = (std::max)(2, Scale(2, state.dpi));

    FillRectColor(dc,
                  RECT{track.left, centre - thickness / 2, track.right,
                       centre - thickness / 2 + thickness},
                  colors.border);

    const int knobX =
        track.left + static_cast<int>(ZoomToFraction(state.scale) *
                                      static_cast<double>(geom::Width(track)));
    const int radius = Scale(6, state.dpi);

    // Dolu kısım vurgu renginde: kaydırıcının hangi yöne gittiği, tutamağın
    // konumundan önce bu şeritten okunur.
    FillRectColor(dc,
                  RECT{track.left, centre - thickness / 2, knobX,
                       centre - thickness / 2 + thickness},
                  colors.accent);

    const HBRUSH brush = ::CreateSolidBrush(colors.accent);
    if (brush != nullptr) {
        const HGDIOBJ oldBrush = ::SelectObject(dc, brush);
        const HGDIOBJ oldPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        ::Ellipse(dc, knobX - radius, centre - radius, knobX + radius,
                  centre + radius);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(brush);
    }
}

}  // namespace

RECT ZoomTrack(const State& state, const RECT& slider) noexcept {
    const int inset = Scale(10, state.dpi);
    return RECT{slider.left + inset, slider.top, slider.right - inset,
                slider.bottom};
}

void ZoomFromSlider(HWND window, State& state, int x) {
    for (const Button& button : state.buttons) {
        if (button.kind != ButtonKind::ZoomSlider) {
            continue;
        }
        const RECT track = ZoomTrack(state, button.bounds);
        const int span = static_cast<int>(geom::Width(track));
        if (span <= 0) {
            return;
        }
        const double t = static_cast<double>(x - track.left) /
                         static_cast<double>(span);
        ApplyZoom(window, state, FractionToZoom(t));
        return;
    }
}

void DrawStatusBar(HDC dc, const State& state, const RECT& client) {
    const Palette& colors = theme::Colors();
    const int height = Scale(kStatusHeight, state.dpi);
    const RECT bar{0, static_cast<LONG>(geom::Height(client)) - height,
                   static_cast<LONG>(geom::Width(client)),
                   static_cast<LONG>(geom::Height(client))};

    FillRectColor(dc, bar, colors.surface);
    FillRectColor(dc, RECT{bar.left, bar.top, bar.right, bar.top + 1},
                  colors.border);

    const HFONT created = CreateUiFont(state.dpi, 9, FW_NORMAL);
    if (created == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, created);
    ::SetBkMode(dc, TRANSPARENT);

    const int pad = Scale(12, state.dpi);
    wchar_t text[96];

    // ONAY ŞERİDİ ÖLÇÜNÜN YERİNE GEÇER: ikisi aynı anda gösterilseydi şerit ya
    // ortadaki imleç konumuna binerdi ya da dar pencerede hiç sığmazdı. Ölçü
    // birkaç saniye kaybolur, onay ise görülmesi gereken şeydir.
    if (!state.flashText.empty()) {
        ::SetTextColor(dc, colors.accent);
        RECT area{bar.left + pad, bar.top, bar.right - pad, bar.bottom};
        ::DrawTextW(dc, state.flashText.c_str(), -1, &area,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
                        DT_END_ELLIPSIS);
    } else if (state.image != nullptr && state.image->Valid()) {
        ::SetTextColor(dc, colors.textDim);
        ::swprintf_s(text, L"%d × %d px", state.image->Width(),
                     state.image->Height());
        RECT area{bar.left + pad, bar.top, bar.right - pad, bar.bottom};
        ::DrawTextW(dc, text, -1, &area,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    if (state.hoverImage.x >= 0 && state.flashText.empty()) {
        ::SetTextColor(dc, colors.textDim);
        ::swprintf_s(text, L"%ld, %ld", state.hoverImage.x, state.hoverImage.y);
        RECT area{bar.left, bar.top, bar.right, bar.bottom};
        ::DrawTextW(dc, text, -1, &area,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    ::swprintf_s(text, L"%%%d", static_cast<int>(state.scale * 100.0 + 0.5));
    RECT percent{bar.right - Scale(56, state.dpi), bar.top,
                 bar.right - Scale(12, state.dpi), bar.bottom};
    ::SetTextColor(dc, state.fitToWindow ? colors.textDim : colors.accent);
    ::DrawTextW(dc, text, -1, &percent,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::SelectObject(dc, oldFont);
    ::DeleteObject(created);

    for (const Button& button : state.buttons) {
        if (button.kind == ButtonKind::ZoomSlider) {
            DrawSlider(dc, state, button.bounds);
        }
    }
}

}  // namespace editor
}  // namespace crisp
