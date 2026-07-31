// OverlayPaint.cpp — bkz. OverlayPaint.h.
#include "OverlayPaint.h"

#include "Geometry.h"
#include "Util.h"

#include <cstdio>

namespace crisp {
namespace {

constexpr COLORREF kAccent      = RGB(10, 132, 255);
constexpr COLORREF kPanelBack   = RGB(24, 24, 27);
constexpr COLORREF kPanelBorder = RGB(63, 63, 70);
constexpr COLORREF kTextPrimary = RGB(244, 244, 245);
constexpr COLORREF kTextDim     = RGB(161, 161, 170);
constexpr COLORREF kWhite       = RGB(255, 255, 255);

// Büyüteç kaynağı TEK SAYI olmalı: çift sayıda gerçek bir orta piksel yoktur
// ve nişangâh iki piksel arasına düşer, dolayısıyla "imlecin altındaki piksel"
// gösterilemez.
constexpr LONG kMagnifierSourceSide = 21;
constexpr LONG kMagnifierZoom = 7;
constexpr LONG kMagnifierImageSide = kMagnifierSourceSide * kMagnifierZoom;  // 147

[[nodiscard]] LONG Scale(LONG value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

// Ekran koordinatını istemci koordinatına çevirir.
[[nodiscard]] RECT ToClient(const RECT& r, const RECT& screen) noexcept {
    return RECT{r.left - screen.left, r.top - screen.top, r.right - screen.left,
                r.bottom - screen.top};
}

[[nodiscard]] POINT ToClient(POINT p, const RECT& screen) noexcept {
    return POINT{p.x - screen.left, p.y - screen.top};
}

void FillRectColor(HDC dc, const RECT& r, COLORREF color) {
    const HBRUSH brush = ::CreateSolidBrush(color);
    if (brush == nullptr) {
        return;
    }
    ::FillRect(dc, &r, brush);
    ::DeleteObject(brush);
}

// İçi boş dikdörtgen çerçeve. FrameRect fırça boyutunu kullanmadığı için
// kalınlık dört ayrı FillRect ile verilir.
void DrawFrame(HDC dc, const RECT& r, LONG thickness, COLORREF color) {
    if (thickness <= 0) {
        return;
    }
    FillRectColor(dc, RECT{r.left, r.top, r.right, r.top + thickness}, color);
    FillRectColor(dc, RECT{r.left, r.bottom - thickness, r.right, r.bottom}, color);
    FillRectColor(dc, RECT{r.left, r.top + thickness, r.left + thickness,
                           r.bottom - thickness}, color);
    FillRectColor(dc, RECT{r.right - thickness, r.top + thickness, r.right,
                           r.bottom - thickness}, color);
}

// Seçimin köşe ve kenar ortalarındaki tutamaklar. Yalnızca görsel ipucu;
// bu sürümde yeniden boyutlandırma yok, ama seçimin sınırlarını okunur kılar.
void DrawHandles(HDC dc, const RECT& r, unsigned dpi) {
    const LONG size = Scale(7, dpi);
    const LONG half = size / 2;
    const LONG midX = r.left + geom::Width(r) / 2;
    const LONG midY = r.top + geom::Height(r) / 2;

    const POINT points[] = {
        {r.left, r.top},   {midX, r.top},    {r.right, r.top},
        {r.left, midY},                      {r.right, midY},
        {r.left, r.bottom}, {midX, r.bottom}, {r.right, r.bottom},
    };

    for (const POINT& p : points) {
        const RECT handle{p.x - half, p.y - half, p.x - half + size,
                          p.y - half + size};
        FillRectColor(dc, handle, kWhite);
        DrawFrame(dc, handle, 1, kAccent);
    }
}

[[nodiscard]] HFONT CreateUiFont(unsigned dpi, int pointSize, bool bold) {
    LOGFONTW font{};
    font.lfHeight = -::MulDiv(pointSize, static_cast<int>(dpi), 72);
    font.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    // Segoe UI Windows 10/11'de daima vardır; bulunamazsa GDI en yakınını seçer.
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

// Koyu zeminli bir bilgi kutusu çizer ve kapladığı alanı döndürür.
RECT DrawPill(HDC dc, POINT topLeft, const wchar_t* text, HFONT font,
              unsigned dpi, COLORREF textColor) {
    const HGDIOBJ oldFont = ::SelectObject(dc, font);

    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, text, -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    const LONG padX = Scale(8, dpi);
    const LONG padY = Scale(4, dpi);
    const RECT box{topLeft.x, topLeft.y,
                   topLeft.x + geom::Width(measure) + padX * 2,
                   topLeft.y + geom::Height(measure) + padY * 2};

    FillRectColor(dc, box, kPanelBack);
    DrawFrame(dc, box, 1, kPanelBorder);

    RECT textArea{box.left + padX, box.top + padY, box.right - padX,
                  box.bottom - padY};
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, textColor);
    ::DrawTextW(dc, text, -1, &textArea, DT_SINGLELINE | DT_NOPREFIX);

    ::SelectObject(dc, oldFont);
    return box;
}

void DrawMagnifier(HDC dc, const OverlayVisual& visual, HDC frozenDc,
                   const Image& frozen, HFONT font, HFONT fontBold) {
    const RECT source =
        geom::MagnifierSource(visual.cursor, kMagnifierSourceSide, visual.screen);
    if (geom::IsEmpty(source)) {
        return;
    }

    const LONG imageSide = Scale(kMagnifierImageSide, visual.dpi);
    const LONG textHeight = Scale(38, visual.dpi);
    const SIZE panelSize{imageSide, imageSide + textHeight};
    const POINT origin = geom::MagnifierPlacement(
        visual.cursor, panelSize, Scale(22, visual.dpi), visual.screen);

    const POINT clientOrigin = ToClient(origin, visual.screen);
    const RECT panel{clientOrigin.x, clientOrigin.y, clientOrigin.x + panelSize.cx,
                     clientOrigin.y + panelSize.cy};

    FillRectColor(dc, panel, kPanelBack);

    // COLORONCOLOR: piksel karıştırma YOK. HALFTONE olsaydı büyüteç
    // bulanıklaşırdı; büyütecin bütün amacı tek tek pikselleri göstermek.
    ::SetStretchBltMode(dc, COLORONCOLOR);
    const RECT sourceClient = ToClient(source, visual.screen);
    ::StretchBlt(dc, panel.left, panel.top, imageSide, imageSide, frozenDc,
                 sourceClient.left, sourceClient.top, kMagnifierSourceSide,
                 kMagnifierSourceSide, SRCCOPY);

    // Nişangâh: merkez pikselin çevresine kare. Kaynak tek sayı kenarlı olduğu
    // için merkez piksel tam ortadadır.
    const LONG cellSide = imageSide / kMagnifierSourceSide;
    const LONG centerCell = kMagnifierSourceSide / 2;
    const RECT crosshair{panel.left + centerCell * cellSide,
                         panel.top + centerCell * cellSide,
                         panel.left + (centerCell + 1) * cellSide,
                         panel.top + (centerCell + 1) * cellSide};
    DrawFrame(dc, crosshair, Scale(1, visual.dpi) + 1, kAccent);

    DrawFrame(dc, panel, 1, kPanelBorder);

    // Koordinat ve renk okuması.
    const int localX = visual.cursor.x - visual.screen.left;
    const int localY = visual.cursor.y - visual.screen.top;
    const uint32_t pixel = frozen.Pixel(localX, localY);

    wchar_t line1[64];
    ::swprintf_s(line1, L"%ld, %ld", visual.cursor.x, visual.cursor.y);

    wchar_t line2[64];
    ::swprintf_s(line2, L"#%02X%02X%02X", (pixel >> 16) & 0xFFu,
                 (pixel >> 8) & 0xFFu, pixel & 0xFFu);

    ::SetBkMode(dc, TRANSPARENT);

    const HGDIOBJ oldFont = ::SelectObject(dc, fontBold);
    ::SetTextColor(dc, kTextPrimary);
    RECT textArea{panel.left + Scale(8, visual.dpi), panel.top + imageSide + Scale(4, visual.dpi),
                  panel.right - Scale(8, visual.dpi), panel.bottom};
    ::DrawTextW(dc, line1, -1, &textArea, DT_SINGLELINE | DT_NOPREFIX);

    ::SelectObject(dc, font);
    ::SetTextColor(dc, kTextDim);
    textArea.top += Scale(17, visual.dpi);
    ::DrawTextW(dc, line2, -1, &textArea, DT_SINGLELINE | DT_NOPREFIX);
    ::SelectObject(dc, oldFont);

    // Renk kutusu, onaltılık değerin sağında.
    const RECT swatch{panel.right - Scale(24, visual.dpi),
                      panel.top + imageSide + Scale(20, visual.dpi),
                      panel.right - Scale(8, visual.dpi),
                      panel.top + imageSide + Scale(33, visual.dpi)};
    FillRectColor(dc, swatch, RGB((pixel >> 16) & 0xFFu, (pixel >> 8) & 0xFFu,
                                  pixel & 0xFFu));
    DrawFrame(dc, swatch, 1, kPanelBorder);
}

void DrawHint(HDC dc, const OverlayVisual& visual, HFONT font) {
    const RECT monitor = visual.screen;
    const wchar_t* text =
        visual.colorPick
            ? L"Tıkla: rengi kopyala      Esc / sağ tık: iptal"
            : L"Sürükle: alan seç      Tıkla: pencere yakala      "
              L"Shift: kare      Esc / sağ tık: iptal";

    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, text, -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    ::SelectObject(dc, oldFont);

    const LONG padX = Scale(16, visual.dpi);
    const LONG padY = Scale(10, visual.dpi);
    const LONG width = geom::Width(measure) + padX * 2;
    const LONG height = geom::Height(measure) + padY * 2;

    // Ekranın üst ortasında; seçim genelde aşağıda başlar ve ipucu yolda olmaz.
    const POINT origin{monitor.left + (geom::Width(monitor) - width) / 2,
                       monitor.top + Scale(48, visual.dpi)};
    const POINT client = ToClient(origin, visual.screen);

    const RECT box{client.x, client.y, client.x + width, client.y + height};
    FillRectColor(dc, box, kPanelBack);
    DrawFrame(dc, box, 1, kPanelBorder);

    const HGDIOBJ previous = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, kTextDim);
    RECT textArea{box.left + padX, box.top + padY, box.right - padX, box.bottom};
    ::DrawTextW(dc, text, -1, &textArea, DT_SINGLELINE | DT_NOPREFIX);
    ::SelectObject(dc, previous);
}

}  // namespace

bool BuildDimmedCopy(const Image& source, Image& out) {
    if (!source.Valid()) {
        return false;
    }
    if (!out.Create(source.Width(), source.Height())) {
        return false;
    }

    // Sabit çarpanla karartma: her kanal 40%'a düşer. AlphaBlend ile siyah
    // bindirmekle aynı sonucu verir ama fazladan bir DC ve fırça gerektirmez.
    const auto* from = static_cast<const uint32_t*>(source.Bits());
    auto* to = static_cast<uint32_t*>(out.Bits());
    const size_t count =
        static_cast<size_t>(source.Width()) * static_cast<size_t>(source.Height());

    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = from[i];
        const uint32_t r = ((p >> 16) & 0xFFu) * 40u / 100u;
        const uint32_t g = ((p >> 8) & 0xFFu) * 40u / 100u;
        const uint32_t b = (p & 0xFFu) * 40u / 100u;
        to[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    return true;
}

void PaintOverlay(HDC target, const OverlayVisual& visual, HDC frozenDc,
                  HDC dimmedDc, const Image& frozen) {
    const LONG width = geom::Width(visual.screen);
    const LONG height = geom::Height(visual.screen);

    // 1. Karartılmış masaüstü, her yere.
    ::BitBlt(target, 0, 0, width, height, dimmedDc, 0, 0, SRCCOPY);

    const HFONT font = CreateUiFont(visual.dpi, 9, false);
    const HFONT fontBold = CreateUiFont(visual.dpi, 10, true);

    // 2. Sürükleme başlamadıysa imlecin altındaki pencereyi vurgula.
    if (!visual.dragging && !geom::IsEmpty(visual.hover)) {
        const RECT hover = ToClient(visual.hover, visual.screen);
        ::BitBlt(target, hover.left, hover.top, geom::Width(hover),
                 geom::Height(hover), frozenDc, hover.left, hover.top, SRCCOPY);
        DrawFrame(target, hover, Scale(2, visual.dpi), kAccent);
    }

    // 3. Seçim: parlak pikseller + çerçeve + tutamaklar + ölçü.
    if (!geom::IsEmpty(visual.selection)) {
        const RECT selection = ToClient(visual.selection, visual.screen);
        ::BitBlt(target, selection.left, selection.top, geom::Width(selection),
                 geom::Height(selection), frozenDc, selection.left, selection.top,
                 SRCCOPY);
        DrawFrame(target, selection, Scale(2, visual.dpi), kAccent);
        DrawHandles(target, selection, visual.dpi);

        wchar_t size[64];
        ::swprintf_s(size, L"%ld × %ld", geom::Width(visual.selection),
                     geom::Height(visual.selection));

        const HGDIOBJ oldFont = ::SelectObject(target, fontBold);
        RECT measure{0, 0, 0, 0};
        ::DrawTextW(target, size, -1, &measure,
                    DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        ::SelectObject(target, oldFont);

        const SIZE labelSize{geom::Width(measure) + Scale(16, visual.dpi),
                             geom::Height(measure) + Scale(8, visual.dpi)};
        const POINT labelOrigin = geom::SizeLabelPlacement(
            visual.selection, labelSize, Scale(8, visual.dpi), visual.screen);
        (void)DrawPill(target, ToClient(labelOrigin, visual.screen), size, fontBold,
                       visual.dpi, kTextPrimary);
    }

    // 4. Büyüteç her zaman en üstte: seçimin altında kalırsa işe yaramaz.
    if (visual.showMagnifier) {
        DrawMagnifier(target, visual, frozenDc, frozen, font, fontBold);
    }

    // 5. İpucu yalnızca kullanıcı henüz bir şey yapmadıysa. Renk seçmede
    // sürükleme olmadığı için ipucu hep durur.
    if (visual.showHint &&
        (visual.colorPick ||
         (!visual.dragging && geom::IsEmpty(visual.selection)))) {
        DrawHint(target, visual, font);
    }

    ::DeleteObject(font);
    ::DeleteObject(fontBold);
}

}  // namespace crisp
