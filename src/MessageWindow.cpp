// MessageWindow.cpp — bkz. MessageWindow.h.
#include "MessageWindow.h"

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

// Tasarım ölçüleri (96 DPI mantıksal piksel).
constexpr int kWidth = 420;
constexpr int kPad = 22;
constexpr int kIconSide = 34;
constexpr int kIconGap = 18;
constexpr int kButtonWidth = 104;
constexpr int kButtonHeight = 32;
constexpr int kButtonGap = 10;
constexpr int kMinTextHeight = 34;

enum ControlId { kIdPrimary = 100, kIdSecondary };

struct MessageState {
    std::wstring text;
    MessageIcon icon = MessageIcon::Information;
    MessageButtons buttons = MessageButtons::Ok;
    MessageResult result = MessageResult::Ok;
    unsigned dpi = 96;
    int textHeight = 0;
    HFONT font = nullptr;
    HBRUSH background = nullptr;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

[[nodiscard]] HFONT CreateUiFont(unsigned dpi, int pointSize, int weight) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(pointSize, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

// Simge glifi ve rengi. Renk temadan DEĞİL sabit gelir: bir hata simgesinin
// koyu temada da kırmızı olması gerekir, yoksa uyarıyla hatanın farkı kalmaz.
[[nodiscard]] const wchar_t* IconGlyph(MessageIcon icon) noexcept {
    switch (icon) {
        case MessageIcon::Warning:  return L"";
        case MessageIcon::Error:    return L"";
        case MessageIcon::Question: return L"";
        default:                    return L"";
    }
}

[[nodiscard]] COLORREF IconColor(MessageIcon icon) noexcept {
    switch (icon) {
        case MessageIcon::Warning:  return RGB(255, 170, 20);
        case MessageIcon::Error:    return RGB(232, 72, 62);
        default:                    return theme::Colors().accent;
    }
}

// Metnin verilen genişlikte kaplayacağı yükseklik. Pencere oluşturulmadan
// önce gerekli, bu yüzden ekran DC'siyle ölçülür.
[[nodiscard]] int MeasureText(const std::wstring& text, int width, HFONT font) {
    const HDC screen = ::GetDC(nullptr);
    if (screen == nullptr) {
        return 0;
    }
    const HGDIOBJ old = ::SelectObject(screen, font);
    RECT area{0, 0, width, 0};
    ::DrawTextW(screen, text.c_str(), -1, &area,
                DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    ::SelectObject(screen, old);
    ::ReleaseDC(nullptr, screen);
    return static_cast<int>(geom::Height(area));
}

void Paint(HWND window, const MessageState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;
    const int pad = Scale(kPad, dpi);
    const int iconSide = Scale(kIconSide, dpi);

    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer = ::CreateCompatibleBitmap(dc, geom::Width(client),
                                                    geom::Height(client));
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const HBRUSH surface = ::CreateSolidBrush(colors.surface);
    if (surface != nullptr) {
        ::FillRect(memory, &client, surface);
        ::DeleteObject(surface);
    }

    // DÜĞME ŞERİDİ AYRI ZEMİNDE: sistem ileti kutusunun da yaptığı gibi, metin
    // alanıyla eylem alanını ayırmak kutuyu okunur kılıyor.
    const int stripTop = static_cast<int>(geom::Height(client)) -
                         Scale(kButtonHeight + kPad * 2, dpi);
    const RECT strip{0, stripTop, static_cast<LONG>(geom::Width(client)),
                     static_cast<LONG>(geom::Height(client))};
    const HBRUSH stripBrush = ::CreateSolidBrush(colors.surfaceAlt);
    if (stripBrush != nullptr) {
        ::FillRect(memory, &strip, stripBrush);
        ::DeleteObject(stripBrush);
    }
    const RECT rule{0, stripTop, strip.right, stripTop + 1};
    const HBRUSH border = ::CreateSolidBrush(colors.border);
    if (border != nullptr) {
        ::FillRect(memory, &rule, border);
        ::DeleteObject(border);
    }

    ::SetBkMode(memory, TRANSPARENT);

    const HFONT iconFont = CreateUiFont(dpi, 22, FW_NORMAL);
    if (iconFont != nullptr) {
        const HGDIOBJ old = ::SelectObject(memory, iconFont);
        LOGFONTW logical{};
        ::GetObjectW(iconFont, sizeof(logical), &logical);
        ::wcscpy_s(logical.lfFaceName, L"Segoe MDL2 Assets");
        const HFONT glyphFont = ::CreateFontIndirectW(&logical);
        if (glyphFont != nullptr) {
            ::SelectObject(memory, glyphFont);
            ::SetTextColor(memory, IconColor(state.icon));
            RECT box{pad, pad, pad + iconSide, pad + iconSide};
            ::DrawTextW(memory, IconGlyph(state.icon), -1, &box,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            ::SelectObject(memory, iconFont);
            ::DeleteObject(glyphFont);
        }
        ::SelectObject(memory, old);
        ::DeleteObject(iconFont);
    }

    const HGDIOBJ oldFont = ::SelectObject(memory, state.font);
    ::SetTextColor(memory, colors.text);
    const int textLeft = pad + iconSide + Scale(kIconGap, dpi);
    RECT textArea{textLeft, pad, static_cast<LONG>(geom::Width(client)) - pad,
                  pad + state.textHeight};
    ::DrawTextW(memory, state.text.c_str(), -1, &textArea,
                DT_WORDBREAK | DT_NOPREFIX);
    ::SelectObject(memory, oldFont);

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}

void BuildButtons(HWND window, MessageState& state) {
    const unsigned dpi = state.dpi;
    RECT client{};
    ::GetClientRect(window, &client);

    const int pad = Scale(kPad, dpi);
    const int width = Scale(kButtonWidth, dpi);
    const int height = Scale(kButtonHeight, dpi);
    const int top = static_cast<int>(geom::Height(client)) - pad - height;
    int right = static_cast<int>(geom::Width(client)) - pad;

    const bool twoButtons = state.buttons == MessageButtons::YesNo;
    const UINT primaryText = twoButtons ? IDS_MSG_NO : IDS_SET_OK;

    // BİRİNCİL DÜĞME SAĞDA ve varsayılan odaktadır. YesNo biçiminde sağdaki
    // "Hayır"dır; kutu yalnızca geri alınamayan işlemler için kullanılıyor.
    (void)::CreateWindowExW(
        0, L"BUTTON", Loc::Str(primaryText).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, right - width, top,
        width, height, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPrimary)), nullptr,
        nullptr);

    if (twoButtons) {
        right -= width + Scale(kButtonGap, dpi);
        (void)::CreateWindowExW(
            0, L"BUTTON", Loc::Str(IDS_MSG_YES).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, right - width,
            top, width, height, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSecondary)), nullptr,
            nullptr);
    }

    for (HWND child = ::GetWindow(window, GW_CHILD); child != nullptr;
         child = ::GetWindow(child, GW_HWNDNEXT)) {
        ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(state.font),
                       TRUE);
        if (theme::IsDark()) {
            (void)::SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
        }
    }
}

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
                BuildButtons(window, *state);
            }
            return 0;

        case WM_PAINT:
            if (state != nullptr) {
                Paint(window, *state);
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
    state.font = CreateUiFont(state.dpi, 10, FW_NORMAL);
    state.background = ::CreateSolidBrush(theme::Colors().surfaceAlt);

    const int pad = Scale(kPad, state.dpi);
    const int textWidth = Scale(kWidth, state.dpi) - pad * 2 -
                          Scale(kIconSide + kIconGap, state.dpi);
    state.textHeight = MeasureText(text, textWidth, state.font);
    if (state.textHeight < Scale(kMinTextHeight, state.dpi)) {
        state.textHeight = Scale(kMinTextHeight, state.dpi);
    }

    RECT desired{0, 0, Scale(kWidth, state.dpi),
                 pad * 2 + state.textHeight +
                     Scale(kButtonHeight + kPad * 2, state.dpi)};
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
