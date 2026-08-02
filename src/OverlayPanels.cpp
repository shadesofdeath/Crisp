// OverlayPanels.cpp — bkz. OverlayPanels.h.
#include "OverlayPanels.h"

#include "Geometry.h"
#include "Localization.h"
#include "OverlayDraw.h"
#include "resource.h"

#include <cstdio>
#include <string>

namespace crisp {
namespace {

// Ortak ilkeller OverlayDraw.h'de; buradaki isimler değişmesin diye
// tek tek alınıyorlar.
using draw::DrawFrame;
using draw::FillRectColor;
using draw::kAccent;
using draw::kPanelBack;
using draw::kPanelBorder;
using draw::kTextDim;
using draw::kTextPrimary;
using draw::kWhite;
using draw::Scale;
using draw::ToClient;

// Büyüteç kaynağı TEK SAYI olmalı: çift sayıda gerçek bir orta piksel yoktur
// ve nişangâh iki piksel arasına düşer, dolayısıyla "imlecin altındaki piksel"
// gösterilemez.
constexpr LONG kMagnifierSourceSide = 21;
constexpr LONG kMagnifierZoom = 7;
constexpr LONG kMagnifierImageSide = kMagnifierSourceSide * kMagnifierZoom;  // 147

}  // namespace

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
    // Yerleşmiş seçimin ipucu ayrı: o noktada "sürükle: alan seç" artık
    // yapılacak şeyi anlatmıyor, ve boyutlandırma/taşıma/Enter'ı hiçbir yer
    // söylemiyordu.
    const UINT regionHint = visual.settled ? IDS_HINT_REGION_SETTLED : IDS_HINT_REGION;
    const std::wstring text = Loc::Str(
        visual.textSelect ? IDS_HINT_TEXT
                          : (visual.colorPick ? IDS_HINT_COLOR : regionHint));

    // TEK SATIR DEĞİL. Bölge ipucu iki satır: üstte fare, altta klavye.
    // Boşluk, 1-9, 0 ve Ctrl+C bu kaplamada çalışıyordu ama hiçbir yerde
    // yazmıyordu — yalnızca README'de. Hepsini tek satıra dizmek kutuyu
    // ekrandan taşırdı, o yüzden dizgede "\n" var ve DT_SINGLELINE kalktı.
    // Diğer ipuçlarında satır sonu yok; onlar için hiçbir şey değişmiyor.
    const LONG padX = Scale(16, visual.dpi);
    const LONG padY = Scale(10, visual.dpi);

    // Kutu monitöre sığmalı. Dar bir ekranda ya da uzun bir çeviride iki
    // satır da yetmeyebilir; o durumda satırları sarıyoruz. Kırpmak seçenek
    // değil: yarısı görünen bir ipucu, tam da düzeltmeye çalıştığımız şey.
    const LONG margin = Scale(24, visual.dpi);
    const LONG roomForText = geom::Width(monitor) - (margin + padX) * 2;

    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, text.c_str(), -1, &measure, DT_CALCRECT | DT_NOPREFIX);
    const bool wrap = roomForText > 0 && geom::Width(measure) > roomForText;
    if (wrap) {
        measure = RECT{0, 0, roomForText, 0};
        ::DrawTextW(dc, text.c_str(), -1, &measure,
                    DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    }
    ::SelectObject(dc, oldFont);

    const LONG textWidth =
        wrap && geom::Width(measure) > roomForText ? roomForText : geom::Width(measure);
    const LONG width = textWidth + padX * 2;
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
    ::DrawTextW(dc, text.c_str(), -1, &textArea,
                DT_CENTER | DT_NOPREFIX | (wrap ? DT_WORDBREAK : 0u));
    ::SelectObject(dc, previous);
}

}  // namespace crisp
