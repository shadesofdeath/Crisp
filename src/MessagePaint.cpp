// MessagePaint.cpp — İleti kutusunun çizimi ve düğmeleri.
#include "MessageInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "resource.h"

#include <commctrl.h>
#include <uxtheme.h>

namespace crisp {
namespace {


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
}  // namespace

int MsgMeasureText(const std::wstring& text, int width, HFONT font) {
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

void MsgPaint(HWND window, const MessageState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;
    const int pad = MsgScale(kMsgPad, dpi);
    const int iconSide = MsgScale(kMsgIconSide, dpi);

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
                         MsgScale(kMsgButtonHeight + kMsgPad * 2, dpi);
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

    const HFONT iconFont = MsgCreateUiFont(dpi, 22, FW_NORMAL);
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
    const int textLeft = pad + iconSide + MsgScale(kMsgIconGap, dpi);
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

void MsgBuildButtons(HWND window, MessageState& state) {
    const unsigned dpi = state.dpi;
    RECT client{};
    ::GetClientRect(window, &client);

    const int pad = MsgScale(kMsgPad, dpi);
    const int width = MsgScale(kMsgButtonWidth, dpi);
    const int height = MsgScale(kMsgButtonHeight, dpi);
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
        right -= width + MsgScale(kMsgButtonGap, dpi);
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
}  // namespace crisp
