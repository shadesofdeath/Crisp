// EditorSession.cpp — Düzenleyici penceresinin sınıfı ve ömrü.
//
// AYRI DOSYA: mesaj yordamıyla aynı dosyada EditorInput.cpp 400 satırı
// aşıyordu (docs §9).
#include "EditorInternal.h"

#include "Capture.h"
#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <shellapi.h>
#include <shellscalingapi.h>

#include <memory>
#include <string>

namespace crisp {
namespace editor {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispEditorWindow";

[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = EditorProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hbrBackground = nullptr;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace
}  // namespace editor

EditorResult RunEditor(HINSTANCE instance, const Settings& settings, Image& image) {
    using namespace editor;

    EditorResult result{};
    if (!image.Valid() || !EnsureWindowClass(instance)) {
        return result;
    }

    State state;
    state.instance = instance;
    state.image = &image;
    state.settings = settings;

    auto base = std::make_shared<Image>();
    if (!CropImage(image, 0, 0, image.Width(), image.Height(), *base)) {
        return result;
    }
    state.document.SetBase(base);

    POINT cursor{};
    ::GetCursorPos(&cursor);
    const HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1920, 1080};
    if (::GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (SUCCEEDED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        state.dpi = dpiX;
    }

    // Renk seçicideki "son kullanılanlar" boş başlamaz: ilk açılışta boş bir
    // sıra, orada ne olacağını anlatmıyordu. Varsayılan renk hem seçili hem
    // listenin ilk üyesi olur.
    state.recentColors.push_back(state.color);

    // Pencere görüntüyü sarar ama ekranın dışına taşmaz; büyük bir yakalama
    // çalışma alanına sığdırılır ve tuval küçültülerek gösterilir.
    const int chromeWidth = Scale(24, state.dpi);
    const int chromeHeight = Scale(kToolbarHeight + kStatusHeight + 24, state.dpi);
    int width = image.Width() + chromeWidth;
    int height = image.Height() + chromeHeight;

    // ARAÇ ÇUBUĞU SIĞMALI. Gerekli genişlik hesaplanır, tahmin edilmez:
    // sabit bir sayı bir sonraki araç eklendiğinde sessizce yetersiz kalır ve
    // dosya düğmeleri düzenleme düğmelerinin üstüne biner.
    width = (std::max)(width, RequiredToolbarWidth(state.dpi));
    width = (std::min)(width, static_cast<int>(geom::Width(work)));
    height = (std::min)(height, static_cast<int>(geom::Height(work)));

    const int x = work.left + (static_cast<int>(geom::Width(work)) - width) / 2;
    const int y = work.top + (static_cast<int>(geom::Height(work)) - height) / 2;

    const std::wstring title = Loc::Str(IDS_EDITOR_TITLE);
    const HWND window = ::CreateWindowExW(
        0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW, x, y, width, height,
        nullptr, nullptr, instance, &state);
    if (window == nullptr) {
        LogV(L"Düzenleyici penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        return result;
    }

    theme::ApplyToWindow(window);
    // Dosya bırakmayı kabul et: bir görüntüyü düzenlemek için önce
    // yakalamak gerekmiyor artık.
    ::DragAcceptFiles(window, TRUE);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    return state.result;
}


}  // namespace crisp
