// EditorLayout.cpp — Tuval yerleşimi, koordinat dönüşümü ve yakınlaştırma.
//
// HESAP BURADA DEĞİL: sığdırma ölçeği, kaydırma sınırı, çıpalı yakınlaştırma
// ve koordinat dönüşümleri geom:: altında, crisp_core'da. Burada kalan tek şey
// bunları pencere durumuna bağlamak. Ayrım bilinçli — matematik pencere
// açmadan sınanabilmeli, ve bu dosya bir zamanlar o kuralın tek istisnasıydı.
#include "EditorInternal.h"

#include "Geometry.h"

#include <cmath>

namespace crisp {
namespace editor {

void ClampPan(State& state) {
    if (geom::IsEmpty(state.viewport) || state.image == nullptr) {
        return;
    }
    state.pan = geom::ClampPan(
        state.pan, static_cast<int>(state.image->Width() * state.scale),
        static_cast<int>(state.image->Height() * state.scale),
        static_cast<int>(geom::Width(state.viewport)),
        static_cast<int>(geom::Height(state.viewport)));
}

void LayoutCanvas(State& state, const RECT& client) {
    const int toolbar = Scale(kToolbarHeight, state.dpi);
    const int status = Scale(kStatusHeight, state.dpi);
    const int pad = Scale(10, state.dpi);

    state.viewport = RECT{pad, toolbar + pad,
                          static_cast<LONG>(geom::Width(client)) - pad,
                          static_cast<LONG>(geom::Height(client)) - status - pad};

    if (state.image == nullptr || !state.image->Valid() ||
        geom::IsEmpty(state.viewport)) {
        state.canvas = RECT{};
        state.scale = 1.0;
        return;
    }

    if (state.fitToWindow) {
        state.scale = geom::FitScale(state.image->Width(), state.image->Height(),
                                     static_cast<int>(geom::Width(state.viewport)),
                                     static_cast<int>(geom::Height(state.viewport)));
        state.zoom = state.scale;
        state.pan = POINT{0, 0};
    } else {
        state.scale = state.zoom;
    }

    ClampPan(state);
    state.canvas = geom::CanvasRect(state.viewport, state.image->Width(),
                                    state.image->Height(), state.scale, state.pan);
}

POINT ToImage(const State& state, POINT client) noexcept {
    return geom::ViewToImage(client, POINT{state.canvas.left, state.canvas.top},
                             state.scale);
}

POINT ToClient(const State& state, POINT image) noexcept {
    return geom::ImageToView(image, POINT{state.canvas.left, state.canvas.top},
                             state.scale);
}

RECT ToClientRect(const State& state, const RECT& image) noexcept {
    const POINT topLeft = ToClient(state, POINT{image.left, image.top});
    const POINT bottomRight = ToClient(state, POINT{image.right, image.bottom});
    return RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

void ZoomAt(HWND window, State& state, double factor, POINT anchor) {
    if (state.image == nullptr || !state.image->Valid() || factor <= 0.0) {
        return;
    }

    const double previous = state.scale;
    const double target = geom::ClampZoom(previous * factor, kMinZoom, kMaxZoom);
    if (::fabs(target - previous) < 0.0001) {
        return;
    }

    state.pan = geom::PanForZoomAnchor(anchor, state.viewport,
                                       state.image->Width(),
                                       state.image->Height(), previous, state.pan,
                                       target);
    state.fitToWindow = false;
    state.zoom = target;

    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

void ZoomToFit(HWND window, State& state) {
    state.fitToWindow = true;
    state.pan = POINT{0, 0};
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

void ZoomToActual(HWND window, State& state) {
    state.fitToWindow = false;
    state.zoom = 1.0;
    state.pan = POINT{0, 0};
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace editor
}  // namespace crisp
