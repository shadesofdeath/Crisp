// OverlaySession.cpp — Kaplama penceresinin sınıfı, kurulumu ve ömrü.
//
// AYRI DOSYA: mesaj yordamı ve tampon hazırlığıyla birlikte Overlay.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9).
#include "OverlayInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "OverlayPaint.h"
#include "Util.h"
#include "WindowPick.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispSelectionOverlay";
[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    // CS_DBLCLKS olmadan WM_LBUTTONDBLCLK hiç gelmez ve kelime/satır seçimi
    // çalışmaz.
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kWindowClass;
    // Arka plan fırçası YOK: WM_ERASEBKGND'i zaten yutuyoruz.
    wc.hbrBackground = nullptr;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

OverlayResult RunSelectionOverlay(HINSTANCE instance, const Settings& settings,
                                  OverlayMode mode, bool preferWindowPick,
                                  Image& frozen, const OcrLayout* layout) {
    OverlayResult result{};

    if (!EnsureWindowClass(instance)) {
        LogV(L"Kaplama pencere sınıfı kaydedilemedi");
        return result;
    }

    const RECT screen = VirtualScreenRect();
    if (!CaptureRect(screen, frozen)) {
        LogV(L"Ekran dondurulamadı");
        return result;
    }

    OverlayState state;
    state.mode = mode;
    state.visual.screen = screen;
    state.visual.showHint = true;
    state.visual.colorPick = (mode == OverlayMode::ColorPick);
    state.visual.textSelect = (mode == OverlayMode::TextSelect);

    if (mode == OverlayMode::TextSelect && layout != nullptr) {
        state.layout = *layout;
        state.visual.layout = &state.layout;
    }

    // Renk seçmenin tek yolu büyüteç: ayar kapalı olsa bile açılır, yoksa
    // kullanıcı hangi pikseli aldığını göremez.
    // Metin seçmede ise büyüteç KAPALIDIR: kelime kutularının üstünü örter ve
    // seçilen şey piksel değil metin olduğu için bir işe yaramaz.
    state.visual.showMagnifier =
        (mode != OverlayMode::TextSelect) &&
        (settings.showMagnifier || mode == OverlayMode::ColorPick);

    // Pencere vurgulaması yalnızca bölge kipinde anlamlı.
    state.allowHover = (mode == OverlayMode::Region) &&
                       (preferWindowPick || settings.showWindowHighlight);

    if (!BuildDimmedCopy(frozen, state.dimmed)) {
        return result;
    }
    // Dondurulmuş görüntünün sahipliği kaplamaya taşınır ve dönüşte geri
    // verilir. Kopyalamak 4K çok monitörlü bir masaüstünde onlarca megabayt
    // gereksiz tahsis olurdu.
    state.frozen = std::move(frozen);

    // WS_EX_TOOLWINDOW: Alt+Tab listesinde görünmesin.
    // WS_EX_TOPMOST: her şeyin üstünde; yakaladığı masaüstünün üstünde durmalı.
    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    const HWND window = ::CreateWindowExW(
        exStyle, kWindowClass, L"Crisp", WS_POPUP, screen.left, screen.top,
        static_cast<int>(geom::Width(screen)),
        static_cast<int>(geom::Height(screen)), nullptr, nullptr, instance,
        &state);

    if (window == nullptr) {
        LogV(L"Kaplama penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        frozen = std::move(state.frozen);
        return result;
    }

    ::ShowWindow(window, SW_SHOW);

    // Gösterimden SONRA en üste yeniden yerleştir. WS_EX_TOPMOST oluşturma
    // anında verilir ama görev çubuğu da en-üst bir penceredir ve iki en-üst
    // pencere arasında sıra en son yerleştirilene göre belirlenir. Bu çağrı
    // olmadan kaplama görev çubuğunun ALTINDA kalabilir; o hâlde kullanıcı
    // ekranın alt şeridinde karartılmamış, canlı bir görev çubuğu görür ve
    // oradan seçim yaptığında gördüğüyle yakaladığı uyuşmaz.
    ::SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    // SetForegroundWindow, süreç ön plan hakkına sahip değilse SESSİZCE
    // başarısız olur. Gerçek kullanımda hakkı global kısayol verir; başka bir
    // süreç tetiklerse (betik, otomasyon) pencere görünür ama klavye girdisi
    // almayabilir, bu yüzden odak ayrıca zorlanır.
    ::SetForegroundWindow(window);
    ::SetFocus(window);
    ::SetActiveWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    // Dondurulmuş görüntü çağırana geri verilir: seçim ondan kırpılacak.
    frozen = std::move(state.frozen);
    return state.result;
}

}  // namespace crisp
