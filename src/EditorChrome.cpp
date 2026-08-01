// EditorChrome.cpp — Araç çubuğu düğmelerinin zeminleri, grup etiketleri ve
// düğme içerikleri. Simgeler EditorGlyphs.cpp'de.
#include "EditorInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"

#include <algorithm>
#include <string>

namespace crisp {
namespace editor {

void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &r, brush);
    ::DeleteObject(brush);
}

void FrameRectColor(HDC dc, const RECT& r, int thickness, COLORREF color) {
    FillRectColor(dc, RECT{r.left, r.top, r.right, r.top + thickness}, color);
    FillRectColor(dc, RECT{r.left, r.bottom - thickness, r.right, r.bottom}, color);
    FillRectColor(dc, RECT{r.left, r.top, r.left + thickness, r.bottom}, color);
    FillRectColor(dc, RECT{r.right - thickness, r.top, r.right, r.bottom}, color);
}

HFONT CreateUiFont(unsigned dpi, int points, int weight) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(points, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

namespace {

// Renk düğmesinin içindeki örnek. Düğmenin tamamını boyamak, hangi rengin
// seçili olduğunu gösterirdi ama düğmenin kendisini kaybettirirdi: koyu bir
// renkte düğme araç çubuğunun içinde bir deliğe dönüşüyordu.
[[nodiscard]] RECT SwatchRect(const Button& button, unsigned dpi) noexcept {
    const int side = Scale(18, dpi);
    const int left = button.bounds.left + Scale(8, dpi);
    const int top = (button.bounds.top + button.bounds.bottom) / 2 - side / 2;
    return RECT{left, top, left + side, top + side};
}

}  // namespace

// Düğme ZEMİNLERİ alfa katmanına, GLİFLERİ doğrudan DC'ye çizilir. Zeminler
// yuvarlatılmış ve kenar yumuşatmalı olmalı; GDI'nin RoundRect'i yumuşatma
// yapmaz ve köşeler tırtıklı çıkar — düz kare düğmelerin ucuz durmasının
// sebebi de buydu.
void DrawButtonBackground(AlphaLayer& layer, const State& state,
                          const Button& button, bool selected, bool hovered) {
    if (button.kind == ButtonKind::ZoomSlider) {
        return;   // kaydırıcının kendi çizimi durum çubuğunda
    }

    const Palette& colors = theme::Colors();
    const int radius = Scale(8, state.dpi);

    if (selected) {
        layer.FillRoundRect(button.bounds, colors.accent, 255, radius);
    } else if (hovered && button.enabled) {
        layer.FillRoundRect(button.bounds, colors.text, 26, radius);
    }

    // Açılır düğmeler seçili olmasalar da bir kutuları olmalı: yanındaki araç
    // düğmelerinden farklı davrandıklarını (liste açtıklarını) ancak böyle
    // belli ediyorlar.
    if (button.dropdown && !selected && !hovered) {
        layer.StrokeRoundRect(button.bounds, colors.border, 170, radius, 1);
    }

    if (button.kind == ButtonKind::Color) {
        const RECT swatch = SwatchRect(button, state.dpi);
        const int swatchRadius = Scale(5, state.dpi);
        layer.FillRoundRect(swatch, state.color, 255, swatchRadius);
        // İnce çerçeve: beyaz örnek açık zeminde, koyu örnek koyu zeminde
        // kaybolurdu.
        layer.StrokeRoundRect(swatch, colors.border, 190, swatchRadius, 1);
    }
}

bool IsSelected(const State& state, const Button& button) noexcept {
    // RENK VE KALINLIK ARTIK SEÇİLİ OLMUYOR: tek düğmeler ve geçerli değeri
    // zaten üstlerinde gösteriyorlar. Vurgu zemini vermek, "bu araç etkin"
    // anlamına gelen araç düğmeleriyle karıştırırdı.
    // DOLGU DÜĞMESİ BİR AÇMA-KAPAMA: eylem düğmesi gibi görünüyor ama durumu
    // var ve o durumu göstermezse kullanıcı dolgunun açık olup olmadığını
    // ancak bir şekil çizerek öğrenir.
    if (button.kind == ButtonKind::Action && button.action == kActionFill) {
        return state.fillShapes;
    }
    return button.kind == ButtonKind::Tool && button.tool == state.tool &&
           !state.ocr.active;
}

void DrawButtonGlyph(HDC dc, const State& state, const Button& button,
                     bool selected) {
    if (button.kind == ButtonKind::ZoomSlider) {
        return;
    }

    const Palette& colors = theme::Colors();
    const COLORREF ink =
        button.enabled ? (selected ? RGB(255, 255, 255) : colors.text)
                       : colors.textDim;

    switch (button.kind) {
        case ButtonKind::Tool:
            DrawToolGlyph(dc, button.bounds, button.tool, ink, state.dpi);
            break;
        case ButtonKind::Thickness: {
            // ÖNİZLEME, NOKTA DEĞİL: eskiden kalınlığı bir dairenin çapı
            // anlatıyordu ve 2 ile 4 piksel arasındaki fark 38 piksellik bir
            // düğmede ayırt edilemiyordu. Kısa bir çizgi, çizildiğinde ne
            // olacağını doğrudan gösterir.
            const int stroke = (std::max)(1, Scale(state.thickness, state.dpi));
            const int cy = (button.bounds.top + button.bounds.bottom) / 2;
            const HPEN pen = ::CreatePen(PS_SOLID, stroke, ink);
            if (pen != nullptr) {
                const HGDIOBJ oldPen = ::SelectObject(dc, pen);
                ::MoveToEx(dc, button.bounds.left + Scale(9, state.dpi), cy,
                           nullptr);
                ::LineTo(dc, button.bounds.right - Scale(20, state.dpi), cy);
                ::SelectObject(dc, oldPen);
                ::DeleteObject(pen);
            }
            break;
        }
        case ButtonKind::Action:
            DrawActionGlyph(dc, button.bounds, button.action, ink, state.dpi);
            break;
        default:
            break;   // renk örneği zeminde çizildi
    }

    if (button.dropdown) {
        DrawChevron(dc,
                    POINT{button.bounds.right - Scale(10, state.dpi),
                          (button.bounds.top + button.bounds.bottom) / 2},
                    button.enabled ? colors.textDim : colors.textDim, state.dpi);
    }
}

void DrawGroupChrome(HDC dc, const State& state) {
    const Palette& colors = theme::Colors();
    const HFONT created = CreateUiFont(state.dpi, 8, FW_NORMAL);
    if (created == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, created);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, colors.textDim);

    const int labelHeight = Scale(kLabelHeight, state.dpi);

    for (int i = 0; i < kGroupCount; ++i) {
        const ToolbarGroup& group = state.groups[i];
        if (geom::IsEmpty(group.bounds)) {
            continue;
        }

        RECT label{group.bounds.left, group.bounds.bottom + Scale(2, state.dpi),
                   group.bounds.right,
                   group.bounds.bottom + Scale(2, state.dpi) + labelHeight};
        ::DrawTextW(dc, Loc::Str(group.labelId).c_str(), -1, &label,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // AYRAÇ, GRUPLAR ARASINDAKİ BOŞLUĞUN ORTASINDA ve yalnızca aralarında
        // gerçekten boşluk varsa. Dar bir pencerede dosya grubu düzenleme
        // grubuna yaslanır ve oraya çizgi koymak iki grubu birbirine yapıştırmış
        // gibi gösterirdi.
        if (i + 1 >= kGroupCount || geom::IsEmpty(state.groups[i + 1].bounds)) {
            continue;
        }
        const int gap = static_cast<int>(state.groups[i + 1].bounds.left -
                                         group.bounds.right);
        if (gap < Scale(kGroupGap, state.dpi)) {
            continue;
        }
        const int x = group.bounds.right + gap / 2;
        const int inset = Scale(5, state.dpi);
        FillRectColor(dc,
                      RECT{x, group.bounds.top + inset,
                           x + (std::max)(1, Scale(1, state.dpi)),
                           group.bounds.bottom - inset},
                      colors.border);
    }

    ::SelectObject(dc, oldFont);
    ::DeleteObject(created);
}

}  // namespace editor
}  // namespace crisp
