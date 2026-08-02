// EditorWindow.cpp — Boyama. Yerleşim EditorLayout.cpp'de, mesajlar
// EditorInput.cpp'de.
#include "EditorInternal.h"

#include "EditorRender.h"
#include "Geometry.h"
#include "Theme.h"
#include "Util.h"

#include <algorithm>

namespace crisp {
namespace editor {
namespace {

// Araç çubuğu ile durum çubuğunun düğme zeminlerini kendi alfa katmanlarına
// çizip karıştırır.
void BlendChrome(HDC dc, State& state, AlphaLayer& layer, const RECT& strip) {
    if (!layer.Prepare(dc, POINT{strip.left, strip.top},
                       static_cast<int>(geom::Width(strip)),
                       static_cast<int>(geom::Height(strip)))) {
        return;
    }
    for (size_t i = 0; i < state.buttons.size(); ++i) {
        const Button& button = state.buttons[i];
        if (button.bounds.top < strip.top || button.bounds.bottom > strip.bottom) {
            continue;
        }
        DrawButtonBackground(layer, state, button, IsSelected(state, button),
                             state.hoverButton == static_cast<int>(i));
    }
    layer.BlendTo(dc);
}

void DrawCanvas(HDC dc, HDC reference, State& state) {
    const Palette& colors = theme::Colors();

    // GÖRÜNÜR ALANA KIRPILIR: yakınlaştırıldığında tuval görünür alandan taşar
    // ve kırpılmasaydı araç çubuğunun ve durum çubuğunun üstüne taşardı.
    const int saved = ::SaveDC(dc);
    ::IntersectClipRect(dc, state.viewport.left, state.viewport.top,
                        state.viewport.right, state.viewport.bottom);

    const HDC imageDc = ::CreateCompatibleDC(reference);
    const HGDIOBJ oldImage = ::SelectObject(imageDc, state.image->Handle());

    // BÜYÜTÜRKEN KOMŞU PİKSEL, küçültürken HALFTONE. Yakınlaştırmanın amacı
    // tek pikseli görmek; HALFTONE onu bulanıklaştırıp bu amacı ortadan
    // kaldırırdı.
    ::SetStretchBltMode(dc, state.scale > 1.0 ? COLORONCOLOR : HALFTONE);
    ::SetBrushOrgEx(dc, 0, 0, nullptr);
    ::StretchBlt(dc, state.canvas.left, state.canvas.top,
                 geom::Width(state.canvas), geom::Height(state.canvas), imageDc,
                 0, 0, state.image->Width(), state.image->Height(), SRCCOPY);

    ::SelectObject(imageDc, oldImage);
    ::DeleteDC(imageDc);

    FrameRectColor(dc, state.canvas, 1, colors.border);
    DrawOcrOverlay(dc, state);
    ::RestoreDC(dc, saved);

    // Sürüklenen şekil ÖNİZLEMESİ tuvale, ölçeklenmiş koordinatlarda.
    if (!state.ocr.active && state.dragging) {
        Shape scaled = state.draft;
        auto toClient = [&](POINT p) {
            return POINT{state.canvas.left + static_cast<LONG>(p.x * state.scale),
                         state.canvas.top + static_cast<LONG>(p.y * state.scale)};
        };
        scaled.start = toClient(state.draft.start);
        scaled.end = toClient(state.draft.end);
        for (POINT& p : scaled.points) {
            p = toClient(p);
        }
        RenderPreview(dc, scaled, state.dpi, state.canvas);
    }
    if (!state.ocr.active && state.typing) {
        DrawTextDraft(dc, state);
    }
    if (!state.ocr.active) {
        DrawSelectionFrame(dc, state);
    }
}

}  // namespace

int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

int ButtonAt(const State& state, POINT client) noexcept {
    for (size_t i = 0; i < state.buttons.size(); ++i) {
        if (::PtInRect(&state.buttons[i].bounds, client)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool CurrentFlattened(const State& state, Image& out) {
    if (state.image == nullptr || !state.image->Valid()) {
        return false;
    }
    return CropImage(*state.image, 0, 0, state.image->Width(),
                     state.image->Height(), out);
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

    const RECT statusStrip{0,
                           static_cast<LONG>(geom::Height(client)) -
                               Scale(kStatusHeight, state.dpi),
                           static_cast<LONG>(geom::Width(client)),
                           static_cast<LONG>(geom::Height(client))};

    // 1. AŞAMA — zeminler alfa katmanlarına, sonra tek karıştırma.
    BlendChrome(memory, state, state.chrome, toolbar);
    BlendChrome(memory, state, state.statusChrome, statusStrip);

    // 2. AŞAMA — glifler zeminlerin üstüne.
    for (const Button& button : state.buttons) {
        DrawButtonGlyph(memory, state, button, IsSelected(state, button));
    }
    DrawGroupChrome(memory, state);

    if (!geom::IsEmpty(state.canvas) && state.image != nullptr &&
        state.image->Valid()) {
        DrawCanvas(memory, dc, state);
    }

    DrawStatusBar(memory, state, client);
    // İPUCU EN SON: araç çubuğunun altına taştığı için tuvalin ve durum
    // çubuğunun üstünde kalmalı.
    DrawTooltip(memory, state, client);

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

}  // namespace editor
}  // namespace crisp
