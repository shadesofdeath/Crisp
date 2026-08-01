// ColorPickerPaint.cpp — Renk seçici panelinin çizimi. Mesajlar
// ColorPicker.cpp'de.
#include "ColorPickerInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <cmath>

namespace crisp {
namespace picker {
namespace {

[[nodiscard]] HFONT CreateUiFont(unsigned dpi, int points, int weight) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(points, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

void FillRectColor(HDC dc, const RECT& area, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &area, brush);
    ::DeleteObject(brush);
}

// Doygunluk-parlaklık karesi: soldan sağa doygunluk, yukarıdan aşağı
// parlaklık. Yalnızca TON DEĞİŞTİĞİNDE üretilir; her boyamada otuz bin piksel
// hesaplamak fareyi sürüklerken takılma olarak hissedilirdi.
void EnsureSquare(State& state) {
    const int width = static_cast<int>(geom::Width(state.metrics.square));
    const int height = static_cast<int>(geom::Height(state.metrics.square));
    if (width <= 0 || height <= 0) {
        return;
    }
    if (state.squareBitmap.Valid() && state.squareBitmap.Width() == width &&
        state.squareBitmap.Height() == height &&
        std::fabs(state.squareHue - state.hsv.hue) < 0.05) {
        return;
    }
    if (!state.squareBitmap.Create(width, height)) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        const double value =
            1.0 - static_cast<double>(y) / static_cast<double>(height - 1);
        for (int x = 0; x < width; ++x) {
            const double saturation =
                static_cast<double>(x) / static_cast<double>(width - 1);
            const COLORREF rgb = HsvToRgb(Hsv{state.hsv.hue, saturation, value});
            state.squareBitmap.SetPixel(
                x, y,
                0xFF000000u | (static_cast<uint32_t>(GetRValue(rgb)) << 16) |
                    (static_cast<uint32_t>(GetGValue(rgb)) << 8) |
                    static_cast<uint32_t>(GetBValue(rgb)));
        }
    }
    state.squareHue = state.hsv.hue;
}

void EnsureHueStrip(State& state) {
    const int width = static_cast<int>(geom::Width(state.metrics.hue));
    const int height = static_cast<int>(geom::Height(state.metrics.hue));
    if (width <= 0 || height <= 0 ||
        (state.hueBitmap.Valid() && state.hueBitmap.Height() == height)) {
        return;
    }
    if (!state.hueBitmap.Create(width, height)) {
        return;
    }
    for (int y = 0; y < height; ++y) {
        const double hue =
            static_cast<double>(y) / static_cast<double>(height - 1) * 360.0;
        const COLORREF rgb = HsvToRgb(Hsv{hue, 1.0, 1.0});
        const uint32_t packed =
            0xFF000000u | (static_cast<uint32_t>(GetRValue(rgb)) << 16) |
            (static_cast<uint32_t>(GetGValue(rgb)) << 8) |
            static_cast<uint32_t>(GetBValue(rgb));
        for (int x = 0; x < width; ++x) {
            state.hueBitmap.SetPixel(x, y, packed);
        }
    }
}

void BlitImage(HDC dc, const Image& image, const RECT& target) {
    if (!image.Valid()) {
        return;
    }
    const HDC memory = ::CreateCompatibleDC(dc);
    if (memory == nullptr) {
        return;
    }
    const HGDIOBJ old = ::SelectObject(memory, image.Handle());
    ::BitBlt(dc, target.left, target.top, image.Width(), image.Height(), memory,
             0, 0, SRCCOPY);
    ::SelectObject(memory, old);
    ::DeleteDC(memory);
}

// İki halka: koyu bir zeminde beyaz, açık bir zeminde siyah kaybolur, ikisi
// birden her zeminde görünür.
void DrawRing(HDC dc, POINT centre, int radius, unsigned dpi) {
    const int width = (std::max)(1, Scale(1, dpi));
    for (int pass = 0; pass < 2; ++pass) {
        const int r = radius + pass;
        const HPEN pen =
            ::CreatePen(PS_SOLID, width, pass == 0 ? RGB(255, 255, 255) : RGB(0, 0, 0));
        if (pen == nullptr) {
            continue;
        }
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Ellipse(dc, centre.x - r, centre.y - r, centre.x + r, centre.y + r);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
    }
}

// Damlalık simgesi ELLE ÇİZİLİR: Segoe MDL2'de adı ezberden yazılabilecek bir
// damlalık glifi yok ve yanlış bir kod noktası, kutu karakteri olarak çıkardı.
void DrawEyedropper(HDC dc, const RECT& box, COLORREF ink, unsigned dpi) {
    const int cx = box.left + static_cast<int>(geom::Width(box)) / 2;
    const int cy = box.top + static_cast<int>(geom::Height(box)) / 2;
    const int r = Scale(7, dpi);
    const int stroke = (std::max)(1, Scale(2, dpi));

    const HPEN pen = ::CreatePen(PS_SOLID, stroke, ink);
    if (pen == nullptr) {
        return;
    }
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    // Gövde: sağ üstten sol alta inen kalın çizgi.
    ::MoveToEx(dc, cx + r, cy - r, nullptr);
    ::LineTo(dc, cx - r / 2, cy + r / 2);
    // Başlık: gövdenin üst ucundaki kısa çapraz.
    ::MoveToEx(dc, cx + r / 3, cy - r, nullptr);
    ::LineTo(dc, cx + r, cy - r / 3);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);

    // Uç: dolu üçgen — damlanın düştüğü nokta.
    const HBRUSH brush = ::CreateSolidBrush(ink);
    if (brush != nullptr) {
        const HGDIOBJ oldBrush = ::SelectObject(dc, brush);
        const HGDIOBJ noPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        const POINT tip[3] = {{cx - r, cy + r},
                              {cx - r + Scale(6, dpi), cy + r - Scale(2, dpi)},
                              {cx - r + Scale(2, dpi), cy + r - Scale(6, dpi)}};
        ::Polygon(dc, tip, 3);
        ::SelectObject(dc, noPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(brush);
    }
}

void DrawSwatchGrid(State& state, const RECT& area, const COLORREF* colors,
                    int count, Hit kind) {
    const Palette& palette = theme::Colors();
    const Metrics& m = state.metrics;
    const int radius = Scale(4, state.dpi);

    for (int i = 0; i < count; ++i) {
        const int column = i % kColumns;
        const int line = i / kColumns;
        const RECT cell{area.left + column * m.cell, area.top + line * m.cell,
                        area.left + column * m.cell + m.swatch,
                        area.top + line * m.cell + m.swatch};

        if (state.hover == kind && state.hoverIndex == i) {
            RECT ring = cell;
            ::InflateRect(&ring, Scale(2, state.dpi), Scale(2, state.dpi));
            state.chrome.StrokeRoundRect(ring, palette.accent, 255,
                                         radius + Scale(2, state.dpi),
                                         (std::max)(1, Scale(2, state.dpi)));
        }
        state.chrome.FillRoundRect(cell, colors[i], 255, radius);
        state.chrome.StrokeRoundRect(cell, palette.border, 160, radius, 1);
    }
}

void DrawButton(State& state, const RECT& box, bool primary, bool hovered) {
    const Palette& colors = theme::Colors();
    const int radius = Scale(6, state.dpi);
    if (primary) {
        state.chrome.FillRoundRect(box, colors.accent, hovered ? 255 : 230, radius);
    } else {
        state.chrome.FillRoundRect(box, colors.text, hovered ? 40 : 22, radius);
        state.chrome.StrokeRoundRect(box, colors.border, 200, radius, 1);
    }
}

void DrawLabels(HDC dc, const State& state) {
    const Palette& colors = theme::Colors();
    const HFONT font = CreateUiFont(state.dpi, 9, FW_NORMAL);
    if (font == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);

    // Hex alanı: "#" ile birlikte, odaktayken sonunda ince bir imleç.
    std::wstring hex = L"#" + state.hexText;
    RECT hexArea = state.metrics.hex;
    hexArea.left += Scale(8, state.dpi);
    ::SetTextColor(dc, colors.text);
    ::DrawTextW(dc, hex.c_str(), -1, &hexArea,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (state.hexFocus) {
        RECT measure{0, 0, 0, 0};
        ::DrawTextW(dc, hex.c_str(), -1, &measure,
                    DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        const int caretX = hexArea.left + static_cast<int>(geom::Width(measure)) +
                           Scale(2, state.dpi);
        FillRectColor(dc,
                      RECT{caretX, hexArea.top + Scale(7, state.dpi),
                           caretX + (std::max)(1, Scale(1, state.dpi)),
                           hexArea.bottom - Scale(7, state.dpi)},
                      colors.accent);
    }

    ::SetTextColor(dc, RGB(255, 255, 255));
    RECT accept = state.metrics.accept;
    ::DrawTextW(dc, Loc::Str(IDS_SET_OK).c_str(), -1, &accept,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SetTextColor(dc, colors.text);
    RECT cancel = state.metrics.cancel;
    ::DrawTextW(dc, Loc::Str(IDS_SET_CANCEL).c_str(), -1, &cancel,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
}

}  // namespace

void Paint(HWND window, State& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const Palette& colors = theme::Colors();

    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer =
        ::CreateCompatibleBitmap(dc, geom::Width(client), geom::Height(client));
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    FillRectColor(memory, client, colors.surface);

    EnsureSquare(state);
    EnsureHueStrip(state);
    BlitImage(memory, state.squareBitmap, state.metrics.square);
    BlitImage(memory, state.hueBitmap, state.metrics.hue);

    // 1. AŞAMA — yuvarlatılmış zeminler tek alfa katmanına.
    const Metrics& m = state.metrics;
    if (state.chrome.Prepare(memory, POINT{0, 0},
                             static_cast<int>(geom::Width(client)),
                             static_cast<int>(geom::Height(client)))) {
        const int radius = Scale(6, state.dpi);
        state.chrome.FillRoundRect(m.preview, state.color, 255, radius);
        state.chrome.StrokeRoundRect(m.preview, colors.border, 200, radius, 1);

        state.chrome.FillRoundRect(m.hex, colors.text, 20, radius);
        state.chrome.StrokeRoundRect(
            m.hex, state.hexFocus ? colors.accent : colors.border, 220, radius,
            state.hexFocus ? (std::max)(1, Scale(2, state.dpi)) : 1);

        state.chrome.FillRoundRect(m.eyedropper, colors.text,
                                   state.hover == Hit::Eyedropper ? 40 : 20,
                                   radius);

        DrawSwatchGrid(state, m.presets, kPresets, kColumns * kPresetRows,
                       Hit::Preset);
        if (!state.recent.empty()) {
            DrawSwatchGrid(state, m.recent, state.recent.data(),
                           static_cast<int>(state.recent.size()), Hit::Recent);
        }

        DrawButton(state, m.accept, true, state.hover == Hit::Accept);
        DrawButton(state, m.cancel, false, state.hover == Hit::Cancel);
        state.chrome.BlendTo(memory);
    }

    // 2. AŞAMA — işaretler ve metin zeminlerin üstüne.
    const POINT squareMarker{
        m.square.left +
            static_cast<int>(state.hsv.saturation *
                             static_cast<double>(geom::Width(m.square) - 1)),
        m.square.top +
            static_cast<int>((1.0 - state.hsv.value) *
                             static_cast<double>(geom::Height(m.square) - 1))};
    DrawRing(memory, squareMarker, Scale(6, state.dpi), state.dpi);

    const int hueY =
        m.hue.top + static_cast<int>(state.hsv.hue / 360.0 *
                                     static_cast<double>(geom::Height(m.hue) - 1));
    const int hueBand = (std::max)(1, Scale(2, state.dpi));
    FillRectColor(memory, RECT{m.hue.left - Scale(2, state.dpi), hueY - hueBand,
                               m.hue.right + Scale(2, state.dpi), hueY + hueBand},
                  RGB(255, 255, 255));

    DrawEyedropper(memory, m.eyedropper, colors.text, state.dpi);
    DrawLabels(memory, state);

    // Panelin kendi kenarlığı: kenarsız bir açılır pencere, arkasındaki koyu
    // düzenleyiciyle sınırsız kaynaşırdı.
    const HBRUSH edge = ::CreateSolidBrush(colors.border);
    if (edge != nullptr) {
        ::FrameRect(memory, &client, edge);
        ::DeleteObject(edge);
    }

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

}  // namespace picker
}  // namespace crisp
