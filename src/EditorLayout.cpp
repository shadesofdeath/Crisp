// EditorLayout.cpp — Tuval yerleşimi, koordinat dönüşümü ve yakınlaştırma.
// Araç çubuğu yerleşimi ve çizim EditorWindow.cpp'dedir.
#include "EditorInternal.h"

#include "Geometry.h"

#include <algorithm>

namespace crisp {
namespace editor {
namespace {

// Pencereye sığdıran ölçek. BÜYÜTME YOK: küçük bir yakalamayı pencereye
// yaymak pikselleri bulanıklaştırır ve kullanıcı çizdiği şeyin gerçek
// boyutunu yanlış tahmin eder. Kullanıcı isterse yakınlaştırma düğmesiyle
// büyütür — o zaman büyütmeyi kendisi istemiş olur.
[[nodiscard]] double FitScale(const State& state) noexcept {
    if (state.image == nullptr || !state.image->Valid()) {
        return 1.0;
    }
    const int width = static_cast<int>(geom::Width(state.viewport));
    const int height = static_cast<int>(geom::Height(state.viewport));
    if (width <= 0 || height <= 0) {
        return 1.0;
    }
    const double fit =
        (std::min)(static_cast<double>(width) / state.image->Width(),
                   static_cast<double>(height) / state.image->Height());
    return fit < 1.0 ? fit : 1.0;
}

}  // namespace

void ClampPan(State& state) {
    if (geom::IsEmpty(state.viewport)) {
        return;
    }
    const int viewWidth = static_cast<int>(geom::Width(state.viewport));
    const int viewHeight = static_cast<int>(geom::Height(state.viewport));
    const int imageWidth = static_cast<int>(state.image->Width() * state.scale);
    const int imageHeight = static_cast<int>(state.image->Height() * state.scale);

    // Görüntü görünür alandan KÜÇÜKSE kaydırma yoktur; ortada durur.
    // Serbest bırakmak, kullanıcının resmi köşeye itip "kayboldu" sanması
    // demek olurdu.
    const int limitX = imageWidth > viewWidth ? (imageWidth - viewWidth) / 2 : 0;
    const int limitY =
        imageHeight > viewHeight ? (imageHeight - viewHeight) / 2 : 0;

    state.pan.x = (std::clamp)(state.pan.x, static_cast<LONG>(-limitX),
                               static_cast<LONG>(limitX));
    state.pan.y = (std::clamp)(state.pan.y, static_cast<LONG>(-limitY),
                               static_cast<LONG>(limitY));
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

    state.scale = state.fitToWindow ? FitScale(state) : state.zoom;
    if (state.fitToWindow) {
        state.zoom = state.scale;
        state.pan = POINT{0, 0};
    }

    ClampPan(state);

    const int width = static_cast<int>(state.image->Width() * state.scale);
    const int height = static_cast<int>(state.image->Height() * state.scale);
    const int left = state.viewport.left +
                     (static_cast<int>(geom::Width(state.viewport)) - width) / 2 +
                     state.pan.x;
    const int top = state.viewport.top +
                    (static_cast<int>(geom::Height(state.viewport)) - height) / 2 +
                    state.pan.y;
    state.canvas = RECT{left, top, left + width, top + height};
}

POINT ToImage(const State& state, POINT client) noexcept {
    if (state.scale <= 0.0) {
        return POINT{0, 0};
    }
    return POINT{
        static_cast<LONG>((client.x - state.canvas.left) / state.scale),
        static_cast<LONG>((client.y - state.canvas.top) / state.scale)};
}

POINT ToClient(const State& state, POINT image) noexcept {
    return POINT{state.canvas.left + static_cast<LONG>(image.x * state.scale),
                 state.canvas.top + static_cast<LONG>(image.y * state.scale)};
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
    double target = (state.fitToWindow ? state.scale : state.zoom) * factor;
    target = (std::clamp)(target, kMinZoom, kMaxZoom);
    if (::fabs(target - previous) < 0.0001) {
        return;
    }

    // Çıpanın altındaki GÖRÜNTÜ NOKTASI, yakınlaştırmadan sonra da aynı ekran
    // noktasında kalmalı. Bunun için önce nokta hesaplanır, ölçek değişir,
    // sonra o noktanın nereye düştüğüne bakılıp fark kaydırmaya eklenir.
    const POINT before = ToImage(state, anchor);

    state.fitToWindow = false;
    state.zoom = target;

    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);

    const POINT after = ToClient(state, before);
    state.pan.x += anchor.x - after.x;
    state.pan.y += anchor.y - after.y;
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
