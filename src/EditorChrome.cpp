// EditorChrome.cpp — Araç çubuğu düğmelerinin zeminleri ve glifleri.
//
// AYRI DOSYA: on araç, yedi renk, üç kalınlık ve on üç eylem için çizim kodu
// tek başına birkaç yüz satır. Yerleşim ve boyama ile aynı dosyada durunca
// EditorWindow.cpp ev kuralının 400 satır sınırını aşıyordu (docs §9).
#include "EditorInternal.h"

#include "Geometry.h"
#include "Theme.h"

#include <iterator>

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

// --- Simgeler ---------------------------------------------------------------
// Kodda çizilirler, kaynak olarak gömülmezler: dokuz araç × iki tema × dört DPI
// ölçeği 72 varlık demek olurdu ve hepsi birkaç çizgiden ibaret.

void DrawToolGlyph(HDC dc, const RECT& box, ToolKind tool, COLORREF color,
                   unsigned dpi) {
    const int cx = box.left + static_cast<int>(geom::Width(box)) / 2;
    const int cy = box.top + static_cast<int>(geom::Height(box)) / 2;
    const int r = Scale(8, dpi);
    const int penWidth = (std::max)(1, Scale(2, dpi));

    const HPEN pen = ::CreatePen(PS_SOLID, penWidth, color);
    if (pen == nullptr) {
        return;
    }
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));

    switch (tool) {
        case ToolKind::Arrow: {
            ::MoveToEx(dc, cx - r, cy + r, nullptr);
            ::LineTo(dc, cx + r - Scale(3, dpi), cy - r + Scale(3, dpi));
            const POINT head[3] = {
                {cx + r, cy - r},
                {cx + r - Scale(8, dpi), cy - r + Scale(2, dpi)},
                {cx + r - Scale(2, dpi), cy - r + Scale(8, dpi)}};
            const HBRUSH fill = ::CreateSolidBrush(color);
            if (fill != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, fill);
                ::Polygon(dc, head, 3);
                ::SelectObject(dc, previous);
                ::DeleteObject(fill);
            }
            break;
        }
        case ToolKind::Rectangle:
            ::Rectangle(dc, cx - r, cy - r + Scale(2, dpi), cx + r,
                        cy + r - Scale(2, dpi));
            break;
        case ToolKind::Ellipse:
            ::Ellipse(dc, cx - r, cy - r + 1, cx + r, cy + r - 1);
            break;
        case ToolKind::Pen:
            ::MoveToEx(dc, cx - r, cy + r - Scale(2, dpi), nullptr);
            ::LineTo(dc, cx - r / 3, cy - r);
            ::LineTo(dc, cx + r / 3, cy + r - Scale(2, dpi));
            ::LineTo(dc, cx + r, cy - r);
            break;
        case ToolKind::Highlighter: {
            const HPEN thick = ::CreatePen(PS_SOLID, Scale(6, dpi), color);
            if (thick != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, thick);
                ::MoveToEx(dc, cx - r, cy + r / 2, nullptr);
                ::LineTo(dc, cx + r, cy - r / 2);
                ::SelectObject(dc, previous);
                ::DeleteObject(thick);
            }
            break;
        }
        case ToolKind::Text:
        case ToolKind::StepNumber: {
            LOGFONTW font{};
            font.lfHeight = -Scale(16, dpi);
            font.lfWeight = FW_SEMIBOLD;
            font.lfCharSet = DEFAULT_CHARSET;
            ::wcscpy_s(font.lfFaceName, L"Segoe UI");
            const HFONT created = ::CreateFontIndirectW(&font);
            if (created != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, created);
                ::SetBkMode(dc, TRANSPARENT);
                ::SetTextColor(dc, color);
                RECT area = box;
                ::DrawTextW(dc, tool == ToolKind::Text ? L"T" : L"1", -1, &area,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                ::SelectObject(dc, previous);
                ::DeleteObject(created);
            }
            break;
        }
        case ToolKind::Blur:
            for (int i = 0; i < 3; ++i) {
                const int radius = r - i * Scale(3, dpi);
                if (radius > 0) {
                    ::Ellipse(dc, cx - radius, cy - radius, cx + radius,
                              cy + radius);
                }
            }
            break;
        case ToolKind::Crop: {
            // İki L: fotoğrafçı kadrajı. Dikdörtgen aracıyla karışmasın diye
            // kapalı bir çerçeve DEĞİL.
            ::MoveToEx(dc, cx - r, cy - r + Scale(4, dpi), nullptr);
            ::LineTo(dc, cx - r, cy + r);
            ::LineTo(dc, cx + r - Scale(4, dpi), cy + r);
            ::MoveToEx(dc, cx + r, cy + r - Scale(4, dpi), nullptr);
            ::LineTo(dc, cx + r, cy - r);
            ::LineTo(dc, cx - r + Scale(4, dpi), cy - r);
            break;
        }
        case ToolKind::Mosaic: {
            const int cell = (std::max)(2, r * 2 / 3);
            const HBRUSH fill = ::CreateSolidBrush(color);
            if (fill != nullptr) {
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 3; ++col) {
                        if (((row + col) % 2) != 0) {
                            continue;
                        }
                        const RECT cellRect{
                            cx - r + col * cell, cy - r + row * cell,
                            cx - r + (col + 1) * cell - 1,
                            cy - r + (row + 1) * cell - 1};
                        ::FillRect(dc, &cellRect, fill);
                    }
                }
                ::DeleteObject(fill);
            }
            break;
        }
        default:
            break;
    }

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

void DrawActionGlyph(HDC dc, const RECT& box, int action, COLORREF color,
                     unsigned dpi) {
    LOGFONTW font{};
    font.lfHeight = -Scale(16, dpi);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    // Segoe MDL2 Assets Windows 10/11'de daima vardır ve simgeleri tek
    // renklidir; metin yazı tipiyle çizilen oklar sürümden sürüme kayar.
    ::wcscpy_s(font.lfFaceName, L"Segoe MDL2 Assets");
    const HFONT created = ::CreateFontIndirectW(&font);
    if (created == nullptr) {
        return;
    }

    // SOLA DÖNDÜRME SİMGESİ YOKTUR: Segoe MDL2'de saat yönünde dönen bir ok
    // (E7AD) var, aynası yok. Başka bir kavisli ok seçmek geri al/yinele
    // düğmeleriyle karışırdı; aynı glifi yatayda çevirmek iki düğmeyi
    // birbirinin tam yansıması yapar.
    const wchar_t* glyph = L"";
    bool mirror = false;
    switch (action) {
        case kActionZoomOut:     glyph = L""; break;   // ZoomOut
        case kActionZoomIn:      glyph = L""; break;   // ZoomIn
        case kActionZoomFit:     glyph = L""; break;   // FitPage
        case kActionOcr:         glyph = L""; break;   // TextField
        case kActionRotateLeft:  glyph = L""; mirror = true; break;
        case kActionRotateRight: glyph = L""; break;
        case kActionScale:       glyph = L""; break;   // FullScreen
        case kActionUndo:  glyph = L""; break;   // Undo
        case kActionRedo:  glyph = L""; break;   // Redo
        case kActionClear: glyph = L""; break;   // Delete
        case kActionCopy:  glyph = L""; break;   // Copy
        case kActionSave:  glyph = L""; break;   // Save
        case kActionClose: glyph = L""; break;   // Cancel
        default: break;
    }

    const HGDIOBJ previous = ::SelectObject(dc, created);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, color);

    int previousMode = 0;
    XFORM previousTransform{};
    if (mirror) {
        // Düğmenin kendi orta ekseninde yansıt: x' = (left + right) - x.
        previousMode = ::SetGraphicsMode(dc, GM_ADVANCED);
        ::GetWorldTransform(dc, &previousTransform);
        const XFORM flip{-1.0f, 0.0f, 0.0f, 1.0f,
                         static_cast<FLOAT>(box.left + box.right), 0.0f};
        ::SetWorldTransform(dc, &flip);
    }

    RECT area = box;
    ::DrawTextW(dc, glyph, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (mirror) {
        ::SetWorldTransform(dc, &previousTransform);
        ::SetGraphicsMode(dc, previousMode);
    }
    ::SelectObject(dc, previous);
    ::DeleteObject(created);
}

// Düğme ZEMİNLERİ alfa katmanına, GLİFLERİ doğrudan DC'ye çizilir. Zeminler
// yuvarlatılmış ve kenar yumuşatmalı olmalı; GDI'nin RoundRect'i yumuşatma
// yapmaz ve köşeler tırtıklı çıkar — düz kare düğmelerin ucuz durmasının
// sebebi de buydu.
void DrawButtonBackground(AlphaLayer& layer, const State& state,
                          const Button& button, bool selected, bool hovered) {
    const Palette& colors = theme::Colors();
    const int radius = Scale(8, state.dpi);

    if (button.kind == ButtonKind::Separator) {
        // Grup ayracı: tam yükseklikte değil, ortada kısa bir çizgi. Tam boy
        // bir çizgi araç çubuğunu bölmelere ayırıp ağırlaştırırdı.
        const int inset = Scale(9, state.dpi);
        const RECT line{button.bounds.left, button.bounds.top + inset,
                        button.bounds.left + (std::max)(1, Scale(1, state.dpi)),
                        button.bounds.bottom - inset};
        layer.FillRoundRect(line, colors.border, 190, 0);
        return;
    }

    if (button.kind == ButtonKind::Color) {
        // Renk örnekleri DAİRE: kare örnekler araç simgeleriyle aynı siluete
        // sahip olduğu için göz onları da düğme sanıyordu.
        RECT swatch = button.bounds;
        const int r = static_cast<int>(geom::Width(swatch)) / 2;
        if (selected) {
            // Seçili renk bir halkayla işaretlenir; örneğin kendi rengini
            // değiştirmek onu tanınmaz hâle getirirdi.
            RECT ring = swatch;
            ::InflateRect(&ring, Scale(4, state.dpi), Scale(4, state.dpi));
            layer.StrokeRoundRect(ring, colors.accent, 255,
                                  static_cast<int>(geom::Width(ring)) / 2,
                                  (std::max)(2, Scale(2, state.dpi)));
        }
        layer.FillRoundRect(swatch, button.color, 255, r);
        // İnce çerçeve: beyaz örnek açık zeminde, koyu örnek koyu zeminde
        // kaybolurdu.
        layer.StrokeRoundRect(swatch, colors.border, 150, r, 1);
        return;
    }

    if (selected) {
        layer.FillRoundRect(button.bounds, colors.accent, 255, radius);
    } else if (hovered && button.enabled) {
        layer.FillRoundRect(button.bounds, colors.text, 26, radius);
    }
}

[[nodiscard]] bool IsSelected(const State& state, const Button& button) noexcept {
    switch (button.kind) {
        case ButtonKind::Tool:      return button.tool == state.tool;
        case ButtonKind::Color:     return button.color == state.color;
        case ButtonKind::Thickness: return button.thickness == state.thickness;
        default:                    return false;
    }
}

void DrawButtonGlyph(HDC dc, const State& state, const Button& button,
                     bool selected) {
    const Palette& colors = theme::Colors();
    const COLORREF ink =
        button.enabled ? (selected ? RGB(255, 255, 255) : colors.text)
                       : colors.textDim;

    switch (button.kind) {
        case ButtonKind::Tool:
            DrawToolGlyph(dc, button.bounds, button.tool, ink, state.dpi);
            break;
        case ButtonKind::Thickness: {
            const int dot = Scale(button.thickness + 3, state.dpi);
            const int cx = button.bounds.left +
                           static_cast<int>(geom::Width(button.bounds)) / 2;
            const int cy = button.bounds.top +
                           static_cast<int>(geom::Height(button.bounds)) / 2;
            const HBRUSH brush = ::CreateSolidBrush(ink);
            if (brush != nullptr) {
                const HGDIOBJ oldBrush = ::SelectObject(dc, brush);
                const HGDIOBJ oldPen = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
                ::Ellipse(dc, cx - dot, cy - dot, cx + dot + 1, cy + dot + 1);
                ::SelectObject(dc, oldPen);
                ::SelectObject(dc, oldBrush);
                ::DeleteObject(brush);
            }
            break;
        }
        case ButtonKind::Action:
            DrawActionGlyph(dc, button.bounds, button.action, ink, state.dpi);
            break;
        default:
            break;   // renk ve ayraç yalnızca zeminde
    }
}

// Durum çubuğu: solda görüntü ölçüsü, ortada imlecin konumu, sağda
// yakınlaştırma oranı.
//
// NEDEN VAR: yakınlaştırma eklendiği anda "şu an %kaçtayım" sorusu doğdu ve
// bunun cevabı hiçbir yerde yazmıyordu. Aynı şeritte ölçü ve imleç konumu da

}  // namespace editor
}  // namespace crisp
