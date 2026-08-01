// ColorPickerLayout.cpp — Panelin ölçüleri, isabet testi ve renk eşitlemesi.
//
// AYRI DOSYA: pencere ömrü, mesajlar, yerleşim ve çizim birlikte 400 satırı
// aşıyordu (docs §9). Buradaki hiçbir şey pencere tutamacına dokunmaz —
// yerleşim saf hesaptır ve mesaj yolundan ayrı durması, "düğme neden yanlış
// yerde" sorusunu mesaj koduna hiç bakmadan yanıtlanabilir kılar.
#include "ColorPickerInternal.h"

#include "Geometry.h"

namespace crisp {
namespace picker {

int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

void SetColor(State& state, COLORREF color) {
    state.color = color;
    const Hsv fresh = RgbToHsv(color);
    state.hsv.saturation = fresh.saturation;
    state.hsv.value = fresh.value;
    if (fresh.saturation > 0.0) {
        state.hsv.hue = fresh.hue;
    }
    state.hexText = FormatHexColor(color).substr(1);
}

void SyncFromHsv(State& state) {
    state.color = HsvToRgb(state.hsv);
    state.hexText = FormatHexColor(state.color).substr(1);
}

void BuildMetrics(State& state) {
    const unsigned dpi = state.dpi;
    Metrics m;
    const int pad = Scale(kPad, dpi);
    const int gap = Scale(kGap, dpi);
    const int row = Scale(kRow, dpi);
    m.swatch = Scale(kSwatch, dpi);
    m.cell = m.swatch + Scale(kSwatchGap, dpi);
    m.width = Scale(kPanelWidth, dpi);

    const int left = pad;
    const int right = m.width - pad;
    const int hueWidth = Scale(kHueWidth, dpi);
    const int squareHeight = Scale(kSquareHeight, dpi);

    int y = pad;
    m.square = RECT{left, y, right - hueWidth - gap, y + squareHeight};
    m.hue = RECT{right - hueWidth, y, right, y + squareHeight};
    y += squareHeight + Scale(12, dpi);

    m.preview = RECT{left, y, left + row, y + row};
    m.eyedropper = RECT{right - row, y, right, y + row};
    m.hex = RECT{m.preview.right + gap, y, m.eyedropper.left - gap, y + row};
    y += row + Scale(14, dpi);

    const int gridWidth = kColumns * m.cell - Scale(kSwatchGap, dpi);
    m.presets = RECT{left, y, left + gridWidth,
                     y + kPresetRows * m.cell - Scale(kSwatchGap, dpi)};
    y = m.presets.bottom + Scale(12, dpi);

    m.recent = RECT{left, y, left + gridWidth, y + m.swatch};
    y = m.recent.bottom + Scale(14, dpi);

    const int buttonWidth = Scale(kButtonWidth, dpi);
    m.accept = RECT{right - buttonWidth, y, right, y + row};
    m.cancel = RECT{m.accept.left - gap - buttonWidth, y, m.accept.left - gap,
                    y + row};
    m.height = y + row + pad;
    state.metrics = m;
}

Hit HitTest(const State& state, POINT point, int& index) noexcept {
    index = -1;
    const Metrics& m = state.metrics;
    if (::PtInRect(&m.square, point)) {
        return Hit::Square;
    }
    if (::PtInRect(&m.hue, point)) {
        return Hit::Hue;
    }
    if (::PtInRect(&m.hex, point)) {
        return Hit::Hex;
    }
    if (::PtInRect(&m.eyedropper, point)) {
        return Hit::Eyedropper;
    }
    if (::PtInRect(&m.accept, point)) {
        return Hit::Accept;
    }
    if (::PtInRect(&m.cancel, point)) {
        return Hit::Cancel;
    }

    // Izgaralarda BOŞLUKLAR SAYILMAZ: iki örnek arasına düşen tıklama
    // komşusunu seçseydi kullanıcı istemediği rengi alırdı.
    auto inGrid = [&](const RECT& area, int rows, int count) -> int {
        if (!::PtInRect(&area, point)) {
            return -1;
        }
        const int column = static_cast<int>(point.x - area.left) / m.cell;
        const int line = static_cast<int>(point.y - area.top) / m.cell;
        if (column >= kColumns || line >= rows) {
            return -1;
        }
        if (static_cast<int>(point.x - area.left) % m.cell >= m.swatch ||
            static_cast<int>(point.y - area.top) % m.cell >= m.swatch) {
            return -1;
        }
        const int slot = line * kColumns + column;
        return slot < count ? slot : -1;
    };

    const int preset = inGrid(m.presets, kPresetRows, kColumns * kPresetRows);
    if (preset >= 0) {
        index = preset;
        return Hit::Preset;
    }
    const int recent = inGrid(m.recent, 1, static_cast<int>(state.recent.size()));
    if (recent >= 0) {
        index = recent;
        return Hit::Recent;
    }
    return Hit::None;
}

}  // namespace picker
}  // namespace crisp
