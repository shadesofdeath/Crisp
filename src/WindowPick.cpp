// WindowPick.cpp — bkz. WindowPick.h.
#include "WindowPick.h"

#include "Geometry.h"

#include <dwmapi.h>

#include <iterator>

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

[[nodiscard]] bool BelongsToThisProcess(HWND window) noexcept {
    DWORD processId = 0;
    (void)::GetWindowThreadProcessId(window, &processId);
    return processId == ::GetCurrentProcessId();
}

// Kabuğun kendi yüzeyleri. Masaüstü, görev çubuğu ve taşma paneli görünür ve
// kapsamlı pencerelerdir ama hiçbiri "aktif pencere" sayılmaz.
[[nodiscard]] bool IsShellSurface(HWND window) noexcept {
    wchar_t className[64] = L"";
    if (::GetClassNameW(window, className,
                        static_cast<int>(std::size(className))) <= 0) {
        return false;
    }
    constexpr const wchar_t* kShellClasses[] = {
        L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow", L"TopLevelWindowForOverflowXamlIsland",
        L"Windows.UI.Core.CoreWindow", L"XamlExplorerHostIslandWindow"};
    for (const wchar_t* name : kShellClasses) {
        if (::wcscmp(className, name) == 0) {
            return true;
        }
    }
    return false;
}

// Ön plan penceresi doğrudan kabul edilebilir mi?
[[nodiscard]] bool IsForegroundCandidate(HWND window) noexcept {
    return IsCapturableWindow(window) && !BelongsToThisProcess(window) &&
           !IsShellSurface(window);
}

// Z sırasında arama yapılırken uygulanan DAHA SIKI ölçüt.
//
// Ön plandan gelen cevaba güvenilir — kullanıcı o pencereyle çalışıyordu.
// Tarama ise kör: başlıksız yardımcı pencereler, WS_EX_TOOLWINDOW paletleri ve
// görünmez çerçeveler de görünür ve kapsamlı olabiliyor. Alt+Tab listesinin
// ölçütü budur ve kullanıcının "aktif pencere" derken kastettiği de o liste.
[[nodiscard]] bool IsAltTabCandidate(HWND window) noexcept {
    if (!IsForegroundCandidate(window)) {
        return false;
    }
    const LONG_PTR exStyle = ::GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) != 0) {
        return false;
    }
    // Sahibi olan pencereler (iletişim kutuları) sahibinin yerine geçmez;
    // ama sahipsiz VE başlıksız olanlar da genellikle görünmez yardımcılardır.
    if (::GetWindowTextLengthW(window) <= 0) {
        return false;
    }
    return true;
}

BOOL CALLBACK TopmostCandidateProc(HWND window, LPARAM param) {
    if (!IsAltTabCandidate(window)) {
        return TRUE;
    }
    *reinterpret_cast<HWND*>(param) = window;
    return FALSE;   // ilk eşleşme en üsttekidir
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

HWND ForegroundCapturableWindow() noexcept {
    // 1) Kısayolla tetiklendiyse ön plan doğrudan doğru cevaptır.
    const HWND foreground = ::GetForegroundWindow();
    if (IsForegroundCandidate(foreground)) {
        return foreground;
    }

    // 2) Tepsi menüsünden tetiklendiyse ön plan bize ya da kabuğa geçmiştir;
    //    kullanıcının penceresi Z sırasının tepesinde durur.
    HWND found = nullptr;
    ::EnumWindows(TopmostCandidateProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

std::wstring WindowTitleText(HWND window) {
    if (window == nullptr) {
        return std::wstring();
    }
    const int length = ::GetWindowTextLengthW(window);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int written =
        ::GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    text.resize(written < 0 ? 0 : static_cast<size_t>(written));
    return text;
}

HWND WindowUnderPoint(POINT screenPoint, HWND ignore) noexcept {
    HitTestContext context{};
    context.point = screenPoint;
    context.ignore = ignore;

    ::EnumWindows(HitTestProc, reinterpret_cast<LPARAM>(&context));
    return context.found;
}

}  // namespace crisp
