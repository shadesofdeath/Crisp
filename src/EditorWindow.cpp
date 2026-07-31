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

// Durum çubuğu: solda görüntü ölçüsü, ortada imlecin konumu, sağda
// yakınlaştırma oranı.
//
// NEDEN VAR: yakınlaştırma eklendiği anda "şu an %kaçtayım" sorusu doğdu ve
// bunun cevabı hiçbir yerde yazmıyordu. Aynı şeritte ölçü ve imleç konumu da
// bedavaya geliyor — ikisi de kırpma yaparken sorulan sorular.
void DrawStatusBar(HDC dc, const State& state, const RECT& client) {
    const Palette& colors = theme::Colors();
    const int height = Scale(kStatusHeight, state.dpi);
    const RECT bar{0, static_cast<LONG>(geom::Height(client)) - height,
                   static_cast<LONG>(geom::Width(client)),
                   static_cast<LONG>(geom::Height(client))};

    FillRectColor(dc, bar, colors.surface);
    FillRectColor(dc, RECT{bar.left, bar.top, bar.right, bar.top + 1},
                  colors.border);

    LOGFONTW font{};
    font.lfHeight = -::MulDiv(8, static_cast<int>(state.dpi), 72);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    const HFONT created = ::CreateFontIndirectW(&font);
    if (created == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, created);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, colors.textDim);

    const int pad = Scale(12, state.dpi);
    wchar_t text[96];

    if (state.image != nullptr && state.image->Valid()) {
        ::swprintf_s(text, L"%d × %d", state.image->Width(),
                     state.image->Height());
        RECT area{bar.left + pad, bar.top, bar.right - pad, bar.bottom};
        ::DrawTextW(dc, text, -1, &area,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    if (state.hoverImage.x >= 0) {
        ::swprintf_s(text, L"%ld, %ld", state.hoverImage.x, state.hoverImage.y);
        RECT area{bar.left, bar.top, bar.right, bar.bottom};
        ::DrawTextW(dc, text, -1, &area,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    ::swprintf_s(text, L"%%%d", static_cast<int>(state.scale * 100.0 + 0.5));
    RECT area{bar.left + pad, bar.top, bar.right - pad, bar.bottom};
    ::SetTextColor(dc, state.fitToWindow ? colors.textDim : colors.accent);
    ::DrawTextW(dc, text, -1, &area,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::SelectObject(dc, oldFont);
    ::DeleteObject(created);
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

    constexpr int kToolCount = 10;
    constexpr int kThicknessCount = 3;
    constexpr int kActionCount = 13;
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
          ToolKind::Blur, ToolKind::Mosaic, ToolKind::Crop}) {
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
    const int actionsWidth = 13 * (side + gap) + group * 2;
    int right = static_cast<int>(geom::Width(client)) - group;
    if (right - actionsWidth < x) {
        right = x + actionsWidth;
    }
    for (const int action : {kActionClose, kActionSave, kActionCopy, kActionClear,
                             kActionRedo, kActionUndo, kActionScale,
                             kActionRotateRight, kActionRotateLeft, kActionOcr,
                             kActionZoomFit, kActionZoomIn, kActionZoomOut}) {
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
    const std::shared_ptr<const Image>& base = state.document.Base();
    if (state.image == nullptr || !base || !base->Valid()) {
        return;
    }
    // HER SEFERİNDE TABANDAN: mevcut görüntünün üstüne boyamak, geri alınan
    // bir şeklin izini bırakırdı — geri alma yalnızca listeden siler,
    // piksellerden değil. Taban belgede durur, çünkü kırpma ve döndürme onu
    // değiştirir ve o değişiklikler de geri alınabilir olmalı.
    Image fresh;
    if (!CropImage(*base, 0, 0, base->Width(), base->Height(), fresh)) {
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
        // GÖRÜNÜR ALANA KIRPILIR: yakınlaştırıldığında tuval görünür alandan
        // taşar ve kırpılmasaydı araç çubuğunun ve durum çubuğunun üstüne
        // taşardı.
        const int saved = ::SaveDC(memory);
        ::IntersectClipRect(memory, state.viewport.left, state.viewport.top,
                            state.viewport.right, state.viewport.bottom);

        const HDC imageDc = ::CreateCompatibleDC(dc);
        const HGDIOBJ oldImage = ::SelectObject(imageDc, state.image->Handle());

        // BÜYÜTÜRKEN KOMŞU PİKSEL, küçültürken HALFTONE. Yakınlaştırmanın
        // amacı tek pikseli görmek; HALFTONE onu bulanıklaştırıp bu amacı
        // ortadan kaldırırdı.
        ::SetStretchBltMode(memory, state.scale > 1.0 ? COLORONCOLOR : HALFTONE);
        ::SetBrushOrgEx(memory, 0, 0, nullptr);
        ::StretchBlt(memory, state.canvas.left, state.canvas.top,
                     geom::Width(state.canvas), geom::Height(state.canvas),
                     imageDc, 0, 0, state.image->Width(), state.image->Height(),
                     SRCCOPY);

        ::SelectObject(imageDc, oldImage);
        ::DeleteDC(imageDc);

        FrameRectColor(memory, state.canvas, 1, colors.border);
        DrawOcrOverlay(memory, state);
        ::RestoreDC(memory, saved);

        // Sürüklenen şekil ÖNİZLEMESİ tuvale, ölçeklenmiş koordinatlarda.
        if (!state.ocr.active && (state.dragging || state.typing)) {
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

    DrawStatusBar(memory, state, client);

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

}  // namespace editor
}  // namespace crisp
