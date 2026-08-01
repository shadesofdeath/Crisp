// MessageWindow.cpp — bkz. MessageWindow.h.
#include "MessageWindow.h"

#include "MessageInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <commctrl.h>
#include <shellscalingapi.h>
#include <uxtheme.h>

namespace crisp {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispMessageBox";


}  // namespace

int MsgScale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

[[nodiscard]] HFONT MsgCreateUiFont(unsigned dpi, int pointSize, int weight) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(pointSize, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

namespace {

// Simge glifi ve rengi. Renk temadan DEĞİL sabit gelir: bir hata simgesinin
// koyu temada da kırmızı olması gerekir, yoksa uyarıyla hatanın farkı kalmaz.

LRESULT CALLBACK MessageProc(HWND window, UINT message, WPARAM wParam,
                             LPARAM lParam) {
    auto* state = reinterpret_cast<MessageState*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return ::DefWindowProcW(window, message, wParam, lParam);
        }

        case WM_CREATE:
            if (state != nullptr) {
                MsgBuildButtons(window, *state);
            }
            return 0;

        case WM_PAINT:
            if (state != nullptr) {
                MsgPaint(window, *state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSTATIC: {
            if (state == nullptr) {
                break;
            }
            const HDC dc = reinterpret_cast<HDC>(wParam);
            ::SetTextColor(dc, theme::Colors().text);
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(state->background);
        }

        case WM_COMMAND: {
            if (state == nullptr) {
                break;
            }
            const int id = LOWORD(wParam);
            // IsDialogMessage Esc için IDCANCEL gönderir; kutunun kendi
            // WM_KEYDOWN dalı, odak düğmedeyken hiç çalışmaz.
            if (id == IDCANCEL) {
                state->result = state->buttons == MessageButtons::YesNo
                                    ? MessageResult::No
                                    : MessageResult::Ok;
                ::DestroyWindow(window);
                return 0;
            }
            if (id == kIdPrimary) {
                state->result = state->buttons == MessageButtons::YesNo
                                    ? MessageResult::No
                                    : MessageResult::Ok;
                ::DestroyWindow(window);
                return 0;
            }
            if (id == kIdSecondary) {
                state->result = MessageResult::Yes;
                ::DestroyWindow(window);
                return 0;
            }
            break;
        }

        // Esc, sistem kutusundaki gibi OLUMSUZ yanıttır: kapatmak onaylamak
        // anlamına gelemez.
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && state != nullptr) {
                state->result = state->buttons == MessageButtons::YesNo
                                    ? MessageResult::No
                                    : MessageResult::Ok;
                ::DestroyWindow(window);
                return 0;
            }
            break;

        case WM_CLOSE:
            if (state != nullptr) {
                state->result = state->buttons == MessageButtons::YesNo
                                    ? MessageResult::No
                                    : MessageResult::Ok;
            }
            ::DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

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
    wc.lpfnWndProc = MessageProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hbrBackground = nullptr;
    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

MessageResult ShowMessage(HINSTANCE instance, HWND owner,
                          const std::wstring& text, MessageIcon icon,
                          MessageButtons buttons) {
    MessageState state;
    state.text = text;
    state.icon = icon;
    state.buttons = buttons;
    state.result = buttons == MessageButtons::YesNo ? MessageResult::No
                                                    : MessageResult::Ok;

    if (!EnsureWindowClass(instance)) {
        // Kendi kutumuz açılamıyorsa sessiz kalmaktansa sistem kutusu.
        const UINT flags = buttons == MessageButtons::YesNo ? MB_YESNO : MB_OK;
        return ::MessageBoxW(owner, text.c_str(),
                             Loc::Str(IDS_APP_TITLE).c_str(), flags) == IDYES
                   ? MessageResult::Yes
                   : state.result;
    }

    // Kutu SAHİBİNİN monitöründe açılır; sahip yoksa imlecin bulunduğu
    // monitörde. Birincil ekranda açmak, üç monitörlü bir kurulumda
    // kullanıcının bakmadığı yerde uyarı göstermek olurdu.
    POINT anchor{};
    if (owner != nullptr) {
        RECT ownerRect{};
        ::GetWindowRect(owner, &ownerRect);
        anchor = POINT{(ownerRect.left + ownerRect.right) / 2,
                       (ownerRect.top + ownerRect.bottom) / 2};
    } else {
        ::GetCursorPos(&anchor);
    }
    const HMONITOR monitor = ::MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96;
    }
    state.dpi = dpiX;
    state.font = MsgCreateUiFont(state.dpi, 10, FW_NORMAL);
    state.background = ::CreateSolidBrush(theme::Colors().surfaceAlt);

    const int pad = MsgScale(kMsgPad, state.dpi);
    const int textWidth = MsgScale(kMsgWidth, state.dpi) - pad * 2 -
                          MsgScale(kMsgIconSide + kMsgIconGap, state.dpi);
    state.textHeight = MsgMeasureText(text, textWidth, state.font);
    if (state.textHeight < MsgScale(kMsgMinTextHeight, state.dpi)) {
        state.textHeight = MsgScale(kMsgMinTextHeight, state.dpi);
    }

    RECT desired{0, 0, MsgScale(kMsgWidth, state.dpi),
                 pad * 2 + state.textHeight +
                     MsgScale(kMsgButtonHeight + kMsgPad * 2, state.dpi)};
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    ::AdjustWindowRectEx(&desired, style, FALSE, WS_EX_DLGMODALFRAME);
    const int width = static_cast<int>(geom::Width(desired));
    const int height = static_cast<int>(geom::Height(desired));

    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1920, 1080};
    if (::GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }
    int x = anchor.x - width / 2;
    int y = anchor.y - height / 2;
    x = (x < work.left) ? work.left : ((x + width > work.right)
                                           ? static_cast<int>(work.right) - width
                                           : x);
    y = (y < work.top) ? work.top : ((y + height > work.bottom)
                                         ? static_cast<int>(work.bottom) - height
                                         : y);

    const HWND window = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME, kWindowClass, Loc::Str(IDS_APP_TITLE).c_str(), style,
        x, y, width, height, owner, nullptr, instance, &state);

    if (window == nullptr) {
        if (state.font != nullptr) {
            ::DeleteObject(state.font);
        }
        if (state.background != nullptr) {
            ::DeleteObject(state.background);
        }
        return state.result;
    }

    // KİPSELLİK: sahip pencere devre dışı bırakılmazsa kullanıcı arkadaki
    // düğmeye tekrar basıp aynı işlemi ikinci kez başlatabilir.
    const bool disableOwner = owner != nullptr && ::IsWindowEnabled(owner);
    if (disableOwner) {
        ::EnableWindow(owner, FALSE);
    }

    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!::IsDialogMessageW(window, &message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    if (disableOwner) {
        ::EnableWindow(owner, TRUE);
        ::SetForegroundWindow(owner);
    }
    if (state.font != nullptr) {
        ::DeleteObject(state.font);
    }
    if (state.background != nullptr) {
        ::DeleteObject(state.background);
    }
    return state.result;
}

}  // namespace crisp
