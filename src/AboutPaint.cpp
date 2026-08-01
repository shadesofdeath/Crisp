// AboutPaint.cpp — Hakkında penceresinin çizimi.
//
// AYRI DOSYA: pencere yordamı ve ömrüyle birlikte AboutWindow.cpp 400 satırı
// aşıyordu (docs §9).
#include "AboutInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Ocr.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace about {
namespace {


[[nodiscard]] HFONT CreateUiFont(unsigned dpi, int pointSize, int weight) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(pointSize, static_cast<int>(dpi), 72);
    font.lfWeight = weight;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &r, brush);
    ::DeleteObject(brush);
}

void DrawFrame(HDC dc, const RECT& r, int thickness, COLORREF color) {
    FillRectColor(dc, RECT{r.left, r.top, r.right, r.top + thickness}, color);
    FillRectColor(dc, RECT{r.left, r.bottom - thickness, r.right, r.bottom}, color);
    FillRectColor(dc, RECT{r.left, r.top, r.left + thickness, r.bottom}, color);
    FillRectColor(dc, RECT{r.right - thickness, r.top, r.right, r.bottom}, color);
}

// Metni verilen konuma çizer ve kapladığı yüksekliği döndürür; çağıran bir
// sonraki satırı buna göre yerleştirir.
int DrawLine(HDC dc, const std::wstring& text, int x, int y, int width,
             HFONT font, COLORREF color, UINT format = DT_WORDBREAK) {
    if (text.empty()) {
        return 0;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, color);

    RECT area{x, y, x + width, y + 4000};
    const int height =
        ::DrawTextW(dc, text.c_str(), -1, &area, format | DT_NOPREFIX);
    ::SelectObject(dc, oldFont);
    return height;
}

}  // namespace

void Paint(HWND window, AboutState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);

    // Çift tamponlama: metin ve simge tek tek çizilirken arka planın kısa
    // süreliğine görünmesi titreme olarak fark edilir.
    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer = ::CreateCompatibleBitmap(dc, geom::Width(client),
                                                    geom::Height(client));
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;

    FillRectColor(memory, client, colors.surface);

    const int pad = Scale(kPad, dpi);
    const int iconSide = Scale(kIconSide, dpi);

    if (state.icon != nullptr) {
        ::DrawIconEx(memory, pad, pad, state.icon, iconSide, iconSide, 0, nullptr,
                     DI_NORMAL);
    }

    const int textLeft = pad + iconSide + Scale(20, dpi);
    const int textWidth = geom::Width(client) - textLeft - pad;
    int y = pad;

    const HFONT fontTitle = CreateUiFont(dpi, 20, FW_SEMIBOLD);
    const HFONT fontBody = CreateUiFont(dpi, 10, FW_NORMAL);
    const HFONT fontSmall = CreateUiFont(dpi, 9, FW_NORMAL);

    y += DrawLine(memory, L"Crisp", textLeft, y, textWidth, fontTitle,
                  colors.text, DT_SINGLELINE);
    y += DrawLine(memory, std::wstring(L"v") + kVersionText, textLeft, y,
                  textWidth, fontSmall, colors.textDim, DT_SINGLELINE);
    y += Scale(10, dpi);
    y += DrawLine(memory, Loc::Str(IDS_ABOUT_TAGLINE), textLeft, y, textWidth,
                  fontBody, colors.text);

    // Açıklama, simgenin altından tam genişlikte devam eder: iki sütun
    // hizasını korumak için sol kenar simgeyle aynı yerden başlar.
    y = pad + iconSide + Scale(18, dpi);
    if (y < pad + iconSide) {
        y = pad + iconSide + Scale(18, dpi);
    }
    const int fullWidth = geom::Width(client) - pad * 2;
    y += DrawLine(memory, Loc::Str(IDS_ABOUT_BODY), pad, y, fullWidth, fontBody,
                  colors.textDim);
    y += Scale(12, dpi);

    // Ayraç
    FillRectColor(memory, RECT{pad, y, geom::Width(client) - pad, y + 1},
                  colors.border);
    y += Scale(12, dpi);

    y += DrawLine(memory,
                  Loc::Str(IsOcrAvailable() ? IDS_ABOUT_OCR_YES : IDS_ABOUT_OCR_NO),
                  pad, y, fullWidth, fontSmall, colors.textDim, DT_SINGLELINE);
    y += Scale(4, dpi);
    y += DrawLine(memory, L"MIT · © 2026 ShadesOfDeath", pad, y, fullWidth,
                  fontSmall, colors.textDim, DT_SINGLELINE);

    // GitHub bağlantısı
    {
        const HGDIOBJ oldFont = ::SelectObject(memory, fontSmall);
        RECT measure{0, 0, 0, 0};
        ::DrawTextW(memory, kRepositoryUrl, -1, &measure,
                    DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        ::SelectObject(memory, oldFont);

        state.linkRect = RECT{pad, y + Scale(6, dpi), pad + geom::Width(measure),
                              y + Scale(6, dpi) + geom::Height(measure)};
        DrawLine(memory, kRepositoryUrl, state.linkRect.left, state.linkRect.top,
                 fullWidth, fontSmall, colors.accent, DT_SINGLELINE);
        if (state.linkHot) {
            // Altı çizili: bağlantı olduğu ancak imleç üstündeyken belli olur.
            FillRectColor(memory,
                          RECT{state.linkRect.left, state.linkRect.bottom - 1,
                               state.linkRect.right, state.linkRect.bottom},
                          colors.accent);
        }
    }

    // Kapat düğmesi — sağ altta.
    {
        const int buttonWidth = Scale(96, dpi);
        const int buttonHeight = Scale(32, dpi);
        state.closeRect = RECT{geom::Width(client) - pad - buttonWidth,
                               geom::Height(client) - pad - buttonHeight,
                               geom::Width(client) - pad,
                               geom::Height(client) - pad};

        FillRectColor(memory, state.closeRect,
                      state.closeHot ? colors.accent : colors.surfaceAlt);
        DrawFrame(memory, state.closeRect, 1,
                  state.closeHot ? colors.accent : colors.border);

        const HGDIOBJ oldFont = ::SelectObject(memory, fontBody);
        ::SetBkMode(memory, TRANSPARENT);
        ::SetTextColor(memory, state.closeHot ? RGB(255, 255, 255) : colors.text);
        RECT textArea = state.closeRect;
        const std::wstring closeText = Loc::Str(IDS_PIN_CLOSE);
        ::DrawTextW(memory, closeText.c_str(), -1, &textArea,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ::SelectObject(memory, oldFont);
    }

    ::BitBlt(dc, 0, 0, geom::Width(client), geom::Height(client), memory, 0, 0,
             SRCCOPY);

    ::DeleteObject(fontTitle);
    ::DeleteObject(fontBody);
    ::DeleteObject(fontSmall);
    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}
}  // namespace about
}  // namespace crisp
