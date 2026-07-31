// EditorWindow.cpp — Yerleşim ve çizim. Mesajlar EditorInput.cpp'de.
#include "EditorInternal.h"

#include "EditorRender.h"
#include "Geometry.h"
#include "Theme.h"
#include "Util.h"

#include <algorithm>

namespace crisp {
namespace editor {
namespace {

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

    const wchar_t* glyph = L"";
    switch (action) {
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
    RECT area = box;
    ::DrawTextW(dc, glyph, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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

}  // namespace

int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

int RequiredToolbarWidth(unsigned dpi) noexcept {
    const int side = Scale(kButtonSide, dpi);
    const int gap = Scale(kButtonGap, dpi);
    const int group = Scale(kGroupGap, dpi);
    const int swatch = Scale(kSwatchSide, dpi);

    constexpr int kToolCount = 9;
    constexpr int kThicknessCount = 3;
    constexpr int kActionCount = 6;
    const int colorCount = static_cast<int>(std::size(kPalette));

    return group + kToolCount * (side + gap) +
           group + colorCount * (swatch + gap) +
           group + kThicknessCount * (side + gap) +
           group + kActionCount * (side + gap) + group;
}

void LayoutButtons(State& state, const RECT& client) {
    state.buttons.clear();

    const int side = Scale(kButtonSide, state.dpi);
    const int gap = Scale(kButtonGap, state.dpi);
    const int group = Scale(kGroupGap, state.dpi);
    const int top = (Scale(kToolbarHeight, state.dpi) - side) / 2;
    const unsigned dpiLocal = state.dpi;
    int x = group;

    for (const ToolKind tool :
         {ToolKind::Arrow, ToolKind::Rectangle, ToolKind::Ellipse, ToolKind::Pen,
          ToolKind::Highlighter, ToolKind::Text, ToolKind::StepNumber,
          ToolKind::Blur, ToolKind::Mosaic}) {
        Button button;
        button.kind = ButtonKind::Tool;
        button.tool = tool;
        button.bounds = RECT{x, top, x + side, top + side};
        state.buttons.push_back(button);
        x += side + gap;
    }

    auto addSeparator = [&]() {
        Button separator;
        separator.kind = ButtonKind::Separator;
        separator.bounds = RECT{x + group / 2, top, x + group / 2 + Scale(2, dpiLocal),
                                top + side};
        state.buttons.push_back(separator);
        x += group;
    };

    addSeparator();
    const int swatch = Scale(kSwatchSide, state.dpi);
    const int swatchTop = (Scale(kToolbarHeight, state.dpi) - swatch) / 2;
    for (const COLORREF color : kPalette) {
        Button button;
        button.kind = ButtonKind::Color;
        button.color = color;
        button.bounds = RECT{x, swatchTop, x + swatch, swatchTop + swatch};
        state.buttons.push_back(button);
        x += swatch + gap;
    }

    addSeparator();
    for (const int thickness : {2, 4, 7}) {
        Button button;
        button.kind = ButtonKind::Thickness;
        button.thickness = thickness;
        button.bounds = RECT{x, top, x + side, top + side};
        state.buttons.push_back(button);
        x += side + gap;
    }

    // Eylemler SAĞA yaslanır: araç sayısı değiştikçe yerlerinin kayması, kas
    // hafızasıyla çalışan kullanıcıyı her sürümde yanıltırdı.
    //
    // PENCERE DARSA soldan devam ederler. Sağa yaslamayı koşulsuz uygulamak,
    // dar bir pencerede eylem düğmelerini renk örneklerinin ÜSTÜNE bindirirdi
    // ve iki grup da tıklanamaz hâle gelirdi.
    const int actionsWidth = 6 * (side + gap) + group;
    int right = static_cast<int>(geom::Width(client)) - group;
    if (right - actionsWidth < x) {
        right = x + actionsWidth;
    }
    for (const int action : {kActionClose, kActionSave, kActionCopy, kActionClear,
                             kActionRedo, kActionUndo}) {
        Button button;
        button.kind = ButtonKind::Action;
        button.action = action;
        button.bounds = RECT{right - side, top, right, top + side};
        if (action == kActionUndo) {
            button.enabled = state.document.CanUndo();
        } else if (action == kActionRedo) {
            button.enabled = state.document.CanRedo();
        } else if (action == kActionClear) {
            button.enabled = !state.document.empty();
        }
        state.buttons.push_back(button);
        right -= side + gap;
    }
}

void LayoutCanvas(State& state, const RECT& client) {
    const int toolbar = Scale(kToolbarHeight, state.dpi);
    const int pad = Scale(12, state.dpi);

    const int availableWidth = static_cast<int>(geom::Width(client)) - pad * 2;
    const int availableHeight =
        static_cast<int>(geom::Height(client)) - toolbar - pad * 2;
    if (availableWidth <= 0 || availableHeight <= 0 || state.image == nullptr ||
        !state.image->Valid()) {
        state.canvas = RECT{};
        state.scale = 1.0;
        return;
    }

    // BÜYÜTME YOK: küçük bir yakalamayı pencereye yaymak pikselleri bulanık
    // gösterir ve kullanıcı çizdiği şeyin gerçek boyutunu yanlış tahmin eder.
    const double fit =
        (std::min)(static_cast<double>(availableWidth) / state.image->Width(),
                   static_cast<double>(availableHeight) / state.image->Height());
    state.scale = fit < 1.0 ? fit : 1.0;

    const int width = static_cast<int>(state.image->Width() * state.scale);
    const int height = static_cast<int>(state.image->Height() * state.scale);
    const int left = pad + (availableWidth - width) / 2;
    const int top = toolbar + pad + (availableHeight - height) / 2;
    state.canvas = RECT{left, top, left + width, top + height};
}

POINT ToImage(const State& state, POINT client) noexcept {
    if (state.scale <= 0.0) {
        return POINT{0, 0};
    }
    return POINT{static_cast<LONG>((client.x - state.canvas.left) / state.scale),
                 static_cast<LONG>((client.y - state.canvas.top) / state.scale)};
}

int ButtonAt(const State& state, POINT client) noexcept {
    for (size_t i = 0; i < state.buttons.size(); ++i) {
        // Ayraçlar tıklanamaz: aralarına düşen bir tıklama, yanındaki düğmenin
        // seçilmesini engellerdi.
        if (state.buttons[i].kind == ButtonKind::Separator) {
            continue;
        }
        if (::PtInRect(&state.buttons[i].bounds, client)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Rebuild(State& state) {
    if (state.image == nullptr || !state.original.Valid()) {
        return;
    }
    // HER SEFERİNDE ORİJİNALDEN: mevcut görüntünün üstüne boyamak, geri
    // alınan bir şeklin izini bırakırdı — geri alma yalnızca listeden siler,
    // piksellerden değil.
    Image fresh;
    if (!CropImage(state.original, 0, 0, state.original.Width(),
                   state.original.Height(), fresh)) {
        return;
    }
    RenderShapes(fresh, state.document.Shapes(), state.dpi);
    *state.image = std::move(fresh);
}

void Paint(HWND window, State& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);

    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer =
        ::CreateCompatibleBitmap(dc, geom::Width(client), geom::Height(client));
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const Palette& colors = theme::Colors();
    FillRectColor(memory, client, colors.surfaceAlt);

    const RECT toolbar{0, 0, geom::Width(client),
                       Scale(kToolbarHeight, state.dpi)};
    FillRectColor(memory, toolbar, colors.surface);
    FillRectColor(memory,
                  RECT{0, toolbar.bottom - 1, toolbar.right, toolbar.bottom},
                  colors.border);

    // 1. AŞAMA — zeminler tek bir alfa katmanına, sonra tek karıştırma.
    if (state.chrome.Prepare(memory, POINT{0, 0},
                             static_cast<int>(geom::Width(toolbar)),
                             static_cast<int>(geom::Height(toolbar)))) {
        for (size_t i = 0; i < state.buttons.size(); ++i) {
            const Button& button = state.buttons[i];
            DrawButtonBackground(state.chrome, state, button,
                                 IsSelected(state, button),
                                 state.hoverButton == static_cast<int>(i));
        }
        state.chrome.BlendTo(memory);
    }

    // 2. AŞAMA — glifler zeminlerin üstüne.
    for (const Button& button : state.buttons) {
        DrawButtonGlyph(memory, state, button, IsSelected(state, button));
    }

    // Tuval
    if (!geom::IsEmpty(state.canvas) && state.image != nullptr &&
        state.image->Valid()) {
        const HDC imageDc = ::CreateCompatibleDC(dc);
        const HGDIOBJ oldImage = ::SelectObject(imageDc, state.image->Handle());

        ::SetStretchBltMode(memory, HALFTONE);
        ::SetBrushOrgEx(memory, 0, 0, nullptr);
        ::StretchBlt(memory, state.canvas.left, state.canvas.top,
                     geom::Width(state.canvas), geom::Height(state.canvas),
                     imageDc, 0, 0, state.image->Width(), state.image->Height(),
                     SRCCOPY);

        ::SelectObject(imageDc, oldImage);
        ::DeleteDC(imageDc);

        FrameRectColor(memory, state.canvas, 1, colors.border);

        // Sürüklenen şekil ÖNİZLEMESİ tuvale, ölçeklenmiş koordinatlarda.
        if (state.dragging || state.typing) {
            const Shape& draft = state.dragging ? state.draft : state.textDraft;
            Shape scaled = draft;
            auto toClient = [&](POINT p) {
                return POINT{
                    state.canvas.left + static_cast<LONG>(p.x * state.scale),
                    state.canvas.top + static_cast<LONG>(p.y * state.scale)};
            };
            scaled.start = toClient(draft.start);
            scaled.end = toClient(draft.end);
            for (POINT& p : scaled.points) {
                p = toClient(p);
            }
            RenderPreview(memory, scaled, state.dpi);
        }
    }

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

}  // namespace editor
}  // namespace crisp
