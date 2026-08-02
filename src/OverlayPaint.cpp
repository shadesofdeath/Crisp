// OverlayPaint.cpp — bkz. OverlayPaint.h.
#include "OverlayPaint.h"

#include "Geometry.h"
#include "OverlayDraw.h"
#include "OverlayActions.h"
#include "OverlayPanels.h"
#include "Util.h"

#include <cstdio>

namespace crisp {
namespace {

// Ortak ilkeller OverlayDraw.h'de; buradaki isimler değişmesin diye
// tek tek alınıyorlar.
using draw::DrawFrame;
using draw::DrawPill;
using draw::FillRectColor;
using draw::kAccent;
using draw::kPanelBack;
using draw::kPanelBorder;
using draw::kTextDim;
using draw::kTextPrimary;
using draw::kWhite;
using draw::Scale;
using draw::ToClient;

// Seçimin tutamakları.
//
// ÇİZİLEN KÜME, TUTULAN KÜMEDİR. Buradaki geometri bir zamanlar yerel bir
// hesaptı ve hiçbir şeyi tutmuyordu — sekiz kare çiziliyordu, hiçbiri fareye
// yanıt vermiyordu. Şimdi hem sayıyı hem yerleri `geom::HandleRects` veriyor,
// isabet testiyle aynı fonksiyon; küçük bir seçimde tutamaklar dörde iner ya da
// tamamen kalkar ve çizim bunu kendiliğinden izler.
//
// TUTMA ALANI ÇİZİLENDEN BÜYÜK. Kutular tutma boyutuna göre yerleştirilir —
// ikisi aynı merkezlerde — ama `kHandleDrawSide` kadar çizilir: 13 piksel
// fareyle yakalanabilir bir hedef, 7 piksel ise göze hoş gelen bir işaret.
void DrawHandles(HDC dc, const RECT& screenSelection, const RECT& screen,
                 unsigned dpi) {
    RECT boxes[8]{};
    geom::Grab grabs[8]{};
    const int count =
        geom::HandleRects(screenSelection, Scale(kHandleGrabSide, dpi), boxes, grabs);

    const LONG drawSide = Scale(kHandleDrawSide, dpi);
    for (int i = 0; i < count; ++i) {
        const LONG centreX = boxes[i].left + geom::Width(boxes[i]) / 2;
        const LONG centreY = boxes[i].top + geom::Height(boxes[i]) / 2;
        const POINT origin = ToClient(POINT{centreX - drawSide / 2,
                                           centreY - drawSide / 2},
                                      screen);
        const RECT handle{origin.x, origin.y, origin.x + drawSide,
                          origin.y + drawSide};
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
    // ARTI İMLEÇ NİŞAN ALMAK İÇİNDİR. Seçim yerleştikten sonra nişan
    // alınacak bir şey kalmıyor ve ekranı kesen iki çizgi yalnızca gürültü
    // oluyor; bir tutamak sürüklenirken ise yeniden nişan alınıyor, o yüzden
    // geri geliyor.
    if (visual.crosshair && !visual.colorPick &&
        !(visual.settled && !visual.dragging)) {
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
    // Yerleşmiş bir seçim varken pencere vurgusu çizilmez: pencere seçme o
    // noktada bitmiştir ve vurgulanan çerçeve, tıklandığında yakalanacak şeyi
    // gösteriyor gibi durur — ki göstermiyor.
    if (!visual.dragging && !visual.settled && !geom::IsEmpty(visual.hover)) {
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
        DrawHandles(target, visual.selection, visual.screen, visual.dpi);

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

    // 3b. Eylem çubuğu: yalnızca seçim YERLEŞTİĞİNDE ve sürüklenmiyorken. Bir
    // tutamak sürüklenirken çizmek, imlecin altından kaçan bir hedef bırakırdı.
    if (visual.showActionBar && visual.settled && !visual.dragging) {
        ActionButton buttons[static_cast<size_t>(OverlayAction::Count)]{};
        RECT bar{};
        const int count =
            ActionButtons(visual.selection, MonitorRectAtPoint(visual.cursor),
                          visual.dpi, visual.uploadEnabled, buttons, bar);
        DrawActionBar(target, visual.screen, visual.dpi, buttons, count,
                      visual.hoverAction, font);
    }

    // 4. Büyüteç her zaman en üstte: seçimin altında kalırsa işe yaramaz.
    if (visual.showMagnifier && !(visual.settled && !visual.dragging)) {
        DrawMagnifier(target, visual, frozenDc, frozen, font, fontBold);
    }

    // 5. İpucu yalnızca kullanıcı henüz bir şey yapmadıysa. Renk seçmede
    // sürükleme olmadığı için ipucu hep durur.
    if (visual.showHint &&
        (visual.colorPick || visual.settled ||
         (!visual.dragging && geom::IsEmpty(visual.selection)))) {
        DrawHint(target, visual, font);
    }

    ::DeleteObject(font);
    ::DeleteObject(fontBold);
}

}  // namespace crisp
