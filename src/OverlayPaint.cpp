// OverlayPaint.cpp — bkz. OverlayPaint.h.
#include "OverlayPaint.h"

#include "Geometry.h"
#include "Localization.h"
#include "Util.h"
#include "resource.h"

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

    // Yerleşim imlecin monitörüne sığdırılır, sanal ekrana değil — büyütecin
    // kenara gelince imlecin öbür yanına GEÇMESİ bu sınırla karar veriliyor.
    // Sanal ekran verilince, imleç birinci monitörün sağ kenarındayken panel
    // sığacak yer bulmuş sayılıyor ve ikinci monitöre taşıyordu: kullanıcı
    // baktığı ekranda büyüteci kaybediyor, yan ekranda buluyordu.
    //
    // Büyütecin BÜYÜTTÜĞÜ pikseller (yukarıdaki `MagnifierSource`) sanal ekrana
    // göre kalır ve kalmalı: kaynak, masaüstünün tamamının dondurulmuş
    // görüntüsüdür ve kenardaki bir imleç için örnek alanı komşu monitöre
    // taşabilir. Kırpılan şey görüntünün nereden alındığı değil, panelin nereye
    // konduğu.
    const POINT origin = geom::MagnifierPlacement(
        visual.cursor, panelSize, Scale(22, visual.dpi),
        MonitorRectAtPoint(visual.cursor));

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

// Metin katmanı: her SATIR yuvarlatılmış bir kutuya alınır, imlecin altındaki
// satır belirginleşir, seçili kelimeler metin seçimi gibi vurgulanır.
//
// SATIR KUTUSU, KELİME KUTUSU DEĞİL: kullanıcı bir metin satırını tek bir blok
// olarak görür. Her kelimeyi ayrı kutulamak ekranı tel kafese çevirir ve
// "burada metin var" bilgisini vermek yerine görüntüyü boğar.
void DrawTextLayer(AlphaLayer& layer, const OverlayVisual& visual) {
    if (visual.layout == nullptr || visual.layout->empty()) {
        return;
    }

    const int pad = static_cast<int>(Scale(4, visual.dpi));
    const int radius = static_cast<int>(Scale(6, visual.dpi));
    const int lineCount = ocrsel::LineCount(*visual.layout);

    // 1. Satır kutuları — tanınan her metin bloğunun sınırı.
    for (int line = 0; line < lineCount; ++line) {
        RECT box = ocrsel::LineBounds(*visual.layout, line);
        if (geom::IsEmpty(box)) {
            continue;
        }
        ::InflateRect(&box, pad, pad);

        const bool hovered = (line == visual.hoverLine);
        if (hovered) {
            // İmlecin altındaki satır: hafif dolgu + belirgin çerçeve.
            layer.FillRoundRect(box, kAccent, 34, radius);
            layer.StrokeRoundRect(box, kAccent, 200, radius,
                                  static_cast<int>(Scale(1, visual.dpi)) + 1);
        } else {
            // Diğerleri: yalnızca ince çerçeve. Metnin okunmasını engellemeyecek
            // kadar soluk, "burada metin var" demeye yetecek kadar görünür.
            layer.StrokeRoundRect(box, kAccent, 90, radius,
                                  static_cast<int>(Scale(1, visual.dpi)));
        }
    }

    // 2. Seçim — kelime kelime, çünkü seçim satırın ortasında başlayıp
    // bitebilir.
    if (visual.selectionFirst < 0) {
        return;
    }

    const int selectPad = static_cast<int>(Scale(2, visual.dpi));
    const int selectRadius = static_cast<int>(Scale(3, visual.dpi));
    for (int i = visual.selectionFirst;
         i <= visual.selectionLast && i < visual.layout->count(); ++i) {
        RECT box = visual.layout->words[static_cast<size_t>(i)].bounds;
        ::InflateRect(&box, selectPad, selectPad);
        layer.FillRoundRect(box, kAccent, 120, selectRadius);
    }
}

void DrawHint(HDC dc, const OverlayVisual& visual, HFONT font) {
    // İMLECİN MONİTÖRÜ, SANAL EKRAN DEĞİL.
    //
    // Burada `visual.screen` yazıyordu. Değişkenin adı `monitor` olduğu için
    // doğru görünüyordu, ama o dikdörtgen bütün masaüstünü kapsar — ve iki
    // monitörlü bir masaüstünde sanal ekranın yatay ortası tam olarak iki
    // ekranın birleştiği yerdir. İpucu kutusu bu yüzden ikiye bölünüyor,
    // yarısı bir ekranda yarısı diğerinde kalıyordu.
    //
    // Kaplama sanal ekranın tamamını kaplamayı sürdürüyor; seçimi bir
    // monitörden diğerine sürükleyebilmek gerekiyor. Değişen yalnızca üstüne
    // çizilen kutunun nereye ortalandığı: kullanıcının baktığı ekrana.
    const RECT monitor = MonitorRectAtPoint(visual.cursor);
    const std::wstring text = Loc::Str(
        visual.textSelect ? IDS_HINT_TEXT
                          : (visual.colorPick ? IDS_HINT_COLOR : IDS_HINT_REGION));

    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, text.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
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
    ::DrawTextW(dc, text.c_str(), -1, &textArea, DT_SINGLELINE | DT_NOPREFIX);
    ::SelectObject(dc, previous);
}

}  // namespace

bool BuildDimmedCopy(const Image& source, unsigned dimPercent, Image& out) {
    if (!source.Valid()) {
        return false;
    }
    if (!out.Create(source.Width(), source.Height())) {
        return false;
    }

    // Çarpanla karartma: her kanal (100 - dim)%'a düşer. AlphaBlend ile siyah
    // bindirmekle aynı sonucu verir ama fazladan bir DC ve fırça gerektirmez.
    const uint32_t keep = 100u - (dimPercent > 80u ? 80u : dimPercent);
    const auto* from = static_cast<const uint32_t*>(source.Bits());
    auto* to = static_cast<uint32_t*>(out.Bits());
    const size_t count =
        static_cast<size_t>(source.Width()) * static_cast<size_t>(source.Height());

    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = from[i];
        const uint32_t r = ((p >> 16) & 0xFFu) * keep / 100u;
        const uint32_t g = ((p >> 8) & 0xFFu) * keep / 100u;
        const uint32_t b = (p & 0xFFu) * keep / 100u;
        to[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
    return true;
}

void PaintOverlay(HDC target, const OverlayVisual& visual, HDC frozenDc,
                  HDC dimmedDc, const Image& frozen) {
    const LONG width = geom::Width(visual.screen);
    const LONG height = geom::Height(visual.screen);

    const HFONT font = CreateUiFont(visual.dpi, 9, false);
    const HFONT fontBold = CreateUiFont(visual.dpi, 10, true);

    // Metin seçmede masaüstü hafifçe karartılır: kutular ve vurgular üstünde
    // ayrışsın diye. Tam karartma olmaz — kullanıcı okuyacağı metni seçiyor ve
    // %40'a düşürülmüş bir ekranda hangi kelimeyi aldığını göremez.
    if (visual.textSelect) {
        ::BitBlt(target, 0, 0, width, height, frozenDc, 0, 0, SRCCOPY);

        if (visual.layer != nullptr && visual.layer->valid()) {
            visual.layer->Clear();
            DrawTextLayer(*visual.layer, visual);
            visual.layer->BlendTo(target);
        }

        if (visual.showHint) {
            DrawHint(target, visual, font);
        }
        ::DeleteObject(font);
        ::DeleteObject(fontBold);
        return;
    }

    // 1. Karartılmış masaüstü, her yere.
    ::BitBlt(target, 0, 0, width, height, dimmedDc, 0, 0, SRCCOPY);

    // 1b. ARTI İMLEÇ, her şeyin ALTINDA: seçimin ve vurgulanan pencerenin
    // üstünden geçseydi, tam da hizalanmaya çalışılan kenarı örterdi.
    if (visual.crosshair && !visual.colorPick) {
        const LONG cx = visual.cursor.x - visual.screen.left;
        const LONG cy = visual.cursor.y - visual.screen.top;
        const LONG thickness = Scale(1, visual.dpi) < 1 ? 1 : Scale(1, visual.dpi);
        const HBRUSH brush = ::CreateSolidBrush(kAccent);
        if (brush != nullptr) {
            RECT horizontal{0, cy, width, cy + thickness};
            RECT vertical{cx, 0, cx + thickness, height};
            ::FillRect(target, &horizontal, brush);
            ::FillRect(target, &vertical, brush);
            ::DeleteObject(brush);
        }
    }

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
