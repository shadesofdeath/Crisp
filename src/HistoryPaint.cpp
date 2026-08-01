// HistoryPaint.cpp — Geçmiş penceresinin çizimi.
//
// AYRI DOSYA: yerleşim ve küçük resim üretimiyle birlikte HistoryWindow.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9).
#include "HistoryInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace history {


void Paint(HWND window, State& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const int width = static_cast<int>(geom::Width(client));
    const int height = static_cast<int>(geom::Height(client));

    const HDC memory = ::CreateCompatibleDC(dc);
    const HBITMAP buffer = ::CreateCompatibleBitmap(dc, width, height);
    const HGDIOBJ oldBitmap = ::SelectObject(memory, buffer);

    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;
    const int pad = Scale(kPad, dpi);
    const int header = Scale(kHeaderHeight, dpi);

    FillRectColor(memory, client, colors.surface);

    const HFONT fontHeader = CreateUiFont(dpi, 12, FW_SEMIBOLD);
    const HFONT fontLabel = CreateUiFont(dpi, 9, FW_NORMAL);
    const HFONT fontDim = CreateUiFont(dpi, 8, FW_NORMAL);

    // Üstbilgi şeridi — kaydırılan içerikten ayrı durur ve sabittir.
    FillRectColor(memory, RECT{0, 0, width, header}, colors.surfaceAlt);
    FillRectColor(memory, RECT{0, header - 1, width, header}, colors.border);
    DrawText(memory, Loc::Str(IDS_HISTORY_TITLE),
             RECT{pad, 0, width - pad, header}, fontHeader, colors.text,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    {
        wchar_t count[64];
        ::swprintf_s(count, L"%zu / %zu", state.tiles.size(),
                     state.store != nullptr ? state.store->Limit() : 0u);
        DrawText(memory, count, RECT{pad, 0, width - pad, header}, fontDim,
                 colors.textDim, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    if (state.tiles.empty()) {
        DrawText(memory, Loc::Str(IDS_HISTORY_EMPTY),
                 RECT{pad, header, width - pad, height}, fontLabel,
                 colors.textDim, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        // HER KUTUCUK BİR KART: zemini her zaman çizilir. Yalnızca seçiliyi
        // boyamak, küçük resimlerin havada duran farklı boyutlu lekeler gibi
        // görünmesine yol açıyordu — ızgarayı görünür kılan şey kartın kendisi.
        //
        // KARTLAR DOĞRUDAN GDI İLE çizilir, alfa katmanıyla değil. Katman kendi
        // kökenine göre çalışıyor ve kutucuk koordinatlarının başlıktan
        // çıkarılması gerekiyordu; iki koordinat uzayı arasındaki bu çeviri
        // kartları küçük resimlerden ayrı bir yere düşürüyordu. Tek uzay
        // kullanmak, kartın etiketleriyle aynı yerde durmasını garantiler.
        const int radius = Scale(10, dpi) * 2;   // RoundRect eksen UZUNLUĞU ister
        for (size_t i = 0; i < state.tiles.size(); ++i) {
            const RECT& box = state.tiles[i].bounds;
            if (box.bottom < header || box.top > height) {
                continue;   // görünmeyen satırlar
            }
            const bool selected = static_cast<int>(i) == state.selected;
            const bool hovered = static_cast<int>(i) == state.hovered;

            const COLORREF fill =
                selected ? Mix(colors.surfaceAlt, colors.accent, 58)
                         : (hovered ? Mix(colors.surfaceAlt, colors.text, 22)
                                    : colors.surfaceAlt);
            const COLORREF edge = selected ? colors.accent : colors.border;
            const int edgeWidth = selected ? Scale(2, dpi) : 1;

            const HBRUSH brush = ::CreateSolidBrush(fill);
            const HPEN pen = ::CreatePen(PS_SOLID, edgeWidth, edge);
            if (brush != nullptr && pen != nullptr) {
                const HGDIOBJ oldBrush = ::SelectObject(memory, brush);
                const HGDIOBJ oldPen = ::SelectObject(memory, pen);
                ::RoundRect(memory, box.left, box.top, box.right, box.bottom,
                            radius, radius);
                ::SelectObject(memory, oldPen);
                ::SelectObject(memory, oldBrush);
            }
            if (brush != nullptr) {
                ::DeleteObject(brush);
            }
            if (pen != nullptr) {
                ::DeleteObject(pen);
            }
        }

        // Sonra küçük resimler ve etiketler doğrudan tampona.
        ::SetStretchBltMode(memory, HALFTONE);
        ::SetBrushOrgEx(memory, 0, 0, nullptr);
        const HDC imageDc = ::CreateCompatibleDC(dc);
        const int inset = Scale(kInset, dpi);
        for (const Tile& tile : state.tiles) {
            if (tile.bounds.bottom < header || tile.bounds.top > height) {
                continue;
            }

            // Küçük resmin durabileceği alan. ReloadTiles tam olarak bu ölçüye
            // göre ölçekler; yine de merkezleme buradan hesaplanır ki iki yer
            // birbirinden bağımsız kaymasın.
            const RECT thumbBox{tile.bounds.left + inset, tile.bounds.top + inset,
                                tile.bounds.right - inset,
                                tile.bounds.top + Scale(kThumbHeight, dpi)};
            const Image& thumb = tile.thumbnail;
            const int x = thumbBox.left +
                          (static_cast<int>(geom::Width(thumbBox)) -
                           thumb.Width()) / 2;
            const int y = thumbBox.top +
                          (static_cast<int>(geom::Height(thumbBox)) -
                           thumb.Height()) / 2;

            // Yakalamalar saydam olabilir; altına bir çerçeve koymak, siyah bir
            // dikdörtgen göstermekten iyidir.
            FillRectColor(memory,
                          RECT{x - 1, y - 1, x + thumb.Width() + 1,
                               y + thumb.Height() + 1},
                          colors.border);

            const HGDIOBJ oldImage = ::SelectObject(imageDc, thumb.Handle());
            ::BitBlt(memory, x, y, thumb.Width(), thumb.Height(), imageDc, 0, 0,
                     SRCCOPY);
            ::SelectObject(imageDc, oldImage);

            RECT label{tile.bounds.left + Scale(6, dpi),
                       tile.bounds.top + Scale(kThumbHeight, dpi),
                       tile.bounds.right - Scale(6, dpi),
                       tile.bounds.top + Scale(kThumbHeight + 20, dpi)};
            DrawText(memory, TimeLabel(tile.entry), label, fontLabel, colors.text,
                     DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            label.top = label.bottom;
            label.bottom = tile.bounds.bottom - Scale(6, dpi);
            DrawText(memory, SizeLabel(tile), label, fontDim, colors.textDim,
                     DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        ::DeleteDC(imageDc);
    }

    ::BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);

    ::DeleteObject(fontHeader);
    ::DeleteObject(fontLabel);
    ::DeleteObject(fontDim);
    ::SelectObject(memory, oldBitmap);
    ::DeleteObject(buffer);
    ::DeleteDC(memory);
    ::EndPaint(window, &paint);
}
}  // namespace history
}  // namespace crisp
