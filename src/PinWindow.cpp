// PinWindow.cpp — bkz. PinWindow.h.
#include "PinWindow.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "Theme.h"
#include "resource.h"
#include "Util.h"

#include <commdlg.h>
// windowsx.h: GET_X_LPARAM / GET_Y_LPARAM. Elle kaydırmak yerine makro
// kullanılır çünkü koordinatlar İŞARETLİDİR ve çok monitörlü kurulumda
// negatif olabilir; LOWORD ile çıkarmak onları 65000 gibi değerlere çevirir.
#include <windowsx.h>

#include <memory>
#include <vector>

namespace crisp {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispPinWindow";


// Menü komutları yalnızca bu dosyada anlamlı; resource.h'yi kirletmezler.
enum PinCommand {
    kPinCopy = 1,
    kPinSaveAs,
    kPinActualSize,
    kPinOpacityFull,
    kPinOpacityHalf,
    kPinClose,
};

struct PinState {
    Image image;
    unique_hdc imageDc;
    int zoom = 100;
    BYTE opacity = 255;
    HWND window = nullptr;
};

// Açık iğneler. Dosya kapsamlı static: ev kuralı global değişkeni yasaklar ama
// .cpp içindeki static'e izin verir. unique_ptr, PinState'in adresinin liste
// büyüdükçe değişmemesini garanti eder — pencere verisi o adrese işaret ediyor.
std::vector<std::unique_ptr<PinState>>& Pins() {
    static std::vector<std::unique_ptr<PinState>> pins;
    return pins;
}

void ForgetPin(PinState* state) noexcept {
    auto& pins = Pins();
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        if (it->get() == state) {
            pins.erase(it);
            return;
        }
    }
}

void ApplyZoom(PinState& state, int newZoom, POINT anchorScreen) {
    if (newZoom == state.zoom) {
        return;
    }

    RECT bounds{};
    if (!::GetWindowRect(state.window, &bounds)) {
        return;
    }

    const SIZE oldSize{geom::Width(bounds), geom::Height(bounds)};
    const SIZE newSize = geom::ScaledSize(
        SIZE{state.image.Width(), state.image.Height()}, newZoom);
    const POINT origin = geom::ZoomAnchoredOrigin(
        POINT{bounds.left, bounds.top}, anchorScreen, oldSize, newSize);

    state.zoom = newZoom;
    ::SetWindowPos(state.window, HWND_TOPMOST, origin.x, origin.y, newSize.cx,
                   newSize.cy, SWP_NOACTIVATE);
    ::InvalidateRect(state.window, nullptr, FALSE);
}

void SaveAs(const PinState& state) {
    wchar_t path[MAX_PATH] = L"";
    std::wstring suggestion = L"Crisp " + TimestampForFileName() + L".png";
    ::wcscpy_s(path, suggestion.c_str());

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = state.window;
    dialog.lpstrFilter = L"PNG görüntü\0*.png\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!::GetSaveFileNameW(&dialog)) {
        return;   // kullanıcı iptal etti; hata değil
    }
    if (!SavePng(state.image, path)) {
        ::MessageBoxW(state.window, Loc::Str(IDS_SAVE_FAILED).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONERROR);
    }
}

void ShowContextMenu(PinState& state) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const std::wstring copyText = Loc::MenuText(IDS_PIN_COPY, IDS_ACCEL_COPY);
    const std::wstring saveText = Loc::MenuText(IDS_PIN_SAVE_AS, IDS_ACCEL_SAVE);
    const std::wstring sizeText = Loc::Str(IDS_PIN_ACTUAL_SIZE);
    const std::wstring opaqueText = Loc::Str(IDS_PIN_OPAQUE);
    const std::wstring translucentText = Loc::Str(IDS_PIN_TRANSLUCENT);
    const std::wstring closeText = Loc::MenuText(IDS_PIN_CLOSE, IDS_ACCEL_ESC);

    ::AppendMenuW(menu, MF_STRING, kPinCopy, copyText.c_str());
    ::AppendMenuW(menu, MF_STRING, kPinSaveAs, saveText.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kPinActualSize, sizeText.c_str());
    ::AppendMenuW(menu, MF_STRING | (state.opacity == 255 ? MF_CHECKED : 0),
                  kPinOpacityFull, opaqueText.c_str());
    ::AppendMenuW(menu, MF_STRING | (state.opacity != 255 ? MF_CHECKED : 0),
                  kPinOpacityHalf, translucentText.c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kPinClose, closeText.c_str());

    POINT cursor{};
    ::GetCursorPos(&cursor);
    ::SetForegroundWindow(state.window);

    const int command = ::TrackPopupMenuEx(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y,
        state.window, nullptr);
    ::DestroyMenu(menu);

    switch (command) {
        case kPinCopy:
            if (!CopyImageToClipboard(state.image, state.window)) {
                LogV(L"İğneden panoya kopyalama başarısız");
            }
            break;
        case kPinSaveAs:
            SaveAs(state);
            break;
        case kPinActualSize:
            ApplyZoom(state, 100, cursor);
            break;
        case kPinOpacityFull:
            state.opacity = 255;
            ::SetLayeredWindowAttributes(state.window, 0, state.opacity, LWA_ALPHA);
            break;
        case kPinOpacityHalf:
            state.opacity = 140;
            ::SetLayeredWindowAttributes(state.window, 0, state.opacity, LWA_ALPHA);
            break;
        case kPinClose:
            ::DestroyWindow(state.window);
            break;
        default:
            break;
    }
}

LRESULT CALLBACK PinProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state =
        reinterpret_cast<PinState*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const HDC dc = ::BeginPaint(window, &paint);
            if (dc != nullptr && state != nullptr) {
                RECT client{};
                ::GetClientRect(window, &client);

                // COLORONCOLOR yerine HALFTONE: iğne küçültüldüğünde metin
                // okunabilir kalsın. Büyütmede piksel bloklarını korumak için
                // 100%'ün üstünde ham kopyaya düşülür.
                if (state->zoom > 100) {
                    ::SetStretchBltMode(dc, COLORONCOLOR);
                } else {
                    ::SetStretchBltMode(dc, HALFTONE);
                    ::SetBrushOrgEx(dc, 0, 0, nullptr);
                }

                ::StretchBlt(dc, 0, 0, geom::Width(client), geom::Height(client),
                             state->imageDc.get(), 0, 0, state->image.Width(),
                             state->image.Height(), SRCCOPY);

                // İnce çerçeve: iğnenin nerede bitip masaüstünün nerede
                // başladığı, benzer renkli bir arka planda belirsiz kalırdı.
                const HBRUSH brush = ::CreateSolidBrush(theme::Colors().accent);
                if (brush != nullptr) {
                    ::FrameRect(dc, &client, brush);
                    ::DeleteObject(brush);
                }
            }
            ::EndPaint(window, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        // Pencerenin her yeri başlık çubuğu gibi davranır: kullanıcı görüntünün
        // herhangi bir yerinden tutup sürükleyebilir.
        case WM_NCHITTEST:
            return HTCAPTION;

        case WM_MOUSEWHEEL: {
            if (state == nullptr) {
                break;
            }
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ApplyZoom(*state, geom::ZoomStep(state->zoom, delta), cursor);
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            if (state != nullptr) {
                POINT cursor{};
                ::GetCursorPos(&cursor);
                ApplyZoom(*state, 100, cursor);
            }
            return 0;
        }

        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            if (state != nullptr) {
                ShowContextMenu(*state);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (state == nullptr) {
                break;
            }
            if (wParam == VK_ESCAPE) {
                ::DestroyWindow(window);
                return 0;
            }
            const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (control && wParam == 'C') {
                (void)CopyImageToClipboard(state->image, window);
                return 0;
            }
            if (control && wParam == 'S') {
                SaveAs(*state);
                return 0;
            }
            break;
        }

        case WM_NCDESTROY: {
            // Durum burada silinir: WM_DESTROY'da silmek, sonrasında gelen
            // mesajların serbest bırakılmış belleğe erişmesine yol açardı.
            if (state != nullptr) {
                ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
                ForgetPin(state);
            }
            return 0;
        }

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    // CS_DBLCLKS olmadan WM_LBUTTONDBLCLK hiç gelmez.
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = PinProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

bool PinImageToScreen(HINSTANCE instance, const Image& image, POINT topLeft) {
    if (!image.Valid() || !EnsureWindowClass(instance)) {
        return false;
    }

    auto state = std::make_unique<PinState>();

    // Görüntü kopyalanır: çağıranın Image'ı bu çağrıdan sonra yok olabilir.
    if (!CropImage(image, 0, 0, image.Width(), image.Height(), state->image)) {
        return false;
    }

    const window_dc screenDc{nullptr};
    if (!screenDc.valid()) {
        return false;
    }
    state->imageDc.reset(::CreateCompatibleDC(screenDc.get()));
    if (!state->imageDc) {
        return false;
    }
    ::SelectObject(state->imageDc.get(), state->image.Handle());

    // Ekran dışına düşmesin: yakalama sağ kenardan yapıldıysa iğne tamamen
    // görünmez bir yere açılabilirdi.
    const RECT screen = VirtualScreenRect();
    RECT placement{topLeft.x, topLeft.y, topLeft.x + state->image.Width(),
                   topLeft.y + state->image.Height()};
    if (placement.right > screen.right) {
        placement.left -= placement.right - screen.right;
    }
    if (placement.bottom > screen.bottom) {
        placement.top -= placement.bottom - screen.bottom;
    }
    if (placement.left < screen.left) {
        placement.left = screen.left;
    }
    if (placement.top < screen.top) {
        placement.top = screen.top;
    }

    const std::wstring pinTitle = Loc::Str(IDS_PIN_TITLE);
    PinState* raw = state.get();
    const HWND window = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, kWindowClass,
        pinTitle.c_str(), WS_POPUP, placement.left, placement.top,
        state->image.Width(), state->image.Height(), nullptr, nullptr, instance,
        raw);

    if (window == nullptr) {
        LogV(L"İğne penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        return false;
    }

    raw->window = window;
    theme::ApplyToWindow(window);
    ::SetLayeredWindowAttributes(window, 0, raw->opacity, LWA_ALPHA);
    Pins().push_back(std::move(state));

    ::ShowWindow(window, SW_SHOWNOACTIVATE);
    ::UpdateWindow(window);
    return true;
}

void CloseAllPins() noexcept {
    // Kopya üzerinden gezilir: DestroyWindow, WM_NCDESTROY üzerinden listeyi
    // değiştirir ve canlı listede yineleme geçersiz yineleyiciye düşerdi.
    std::vector<HWND> windows;
    for (const auto& pin : Pins()) {
        windows.push_back(pin->window);
    }
    for (const HWND window : windows) {
        if (window != nullptr) {
            ::DestroyWindow(window);
        }
    }
}

}  // namespace crisp
