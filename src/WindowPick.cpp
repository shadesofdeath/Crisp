// WindowPick.cpp — bkz. WindowPick.h.
#include "WindowPick.h"

#include "Geometry.h"

#include <dwmapi.h>

namespace crisp {
namespace {

struct HitTestContext {
    POINT point{};
    HWND ignore = nullptr;
    HWND found = nullptr;
};

// EnumWindows üst düzey pencereleri Z SIRASINA göre, en üstteki ilk olacak
// biçimde verir. İlk eşleşme doğru cevaptır; devam etmenin anlamı yok.
BOOL CALLBACK HitTestProc(HWND window, LPARAM param) {
    auto* context = reinterpret_cast<HitTestContext*>(param);

    if (window == context->ignore || !IsCapturableWindow(window)) {
        return TRUE;
    }

    RECT bounds{};
    if (!WindowFrameBounds(window, bounds)) {
        return TRUE;
    }

    if (::PtInRect(&bounds, context->point)) {
        context->found = window;
        return FALSE;   // taramayı bitir
    }
    return TRUE;
}

}  // namespace

bool IsCapturableWindow(HWND window) noexcept {
    if (window == nullptr || !::IsWindow(window)) {
        return false;
    }
    if (!::IsWindowVisible(window) || ::IsIconic(window)) {
        return false;
    }

    // WS_EX_TRANSPARENT pencereler fare girdisini geçirir; kullanıcı onların
    // "üzerine" gelemez, dolayısıyla seçilmeleri de beklenmez.
    const LONG_PTR exStyle = ::GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TRANSPARENT) != 0) {
        return false;
    }

    BOOL cloaked = FALSE;
    const HRESULT hr = ::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked,
                                               sizeof(cloaked));
    if (SUCCEEDED(hr) && cloaked != FALSE) {
        return false;
    }

    RECT bounds{};
    if (!::GetWindowRect(window, &bounds) || geom::IsEmpty(bounds)) {
        return false;
    }
    return true;
}

bool WindowFrameBounds(HWND window, RECT& out) noexcept {
    if (window == nullptr || !::IsWindow(window)) {
        return false;
    }

    RECT bounds{};
    const HRESULT hr = ::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                               &bounds, sizeof(bounds));
    if (FAILED(hr) || geom::IsEmpty(bounds)) {
        if (!::GetWindowRect(window, &bounds)) {
            return false;
        }
    }

    if (geom::IsEmpty(bounds)) {
        return false;
    }
    out = bounds;
    return true;
}

HWND WindowUnderPoint(POINT screenPoint, HWND ignore) noexcept {
    HitTestContext context{};
    context.point = screenPoint;
    context.ignore = ignore;

    ::EnumWindows(HitTestProc, reinterpret_cast<LPARAM>(&context));
    return context.found;
}

}  // namespace crisp
