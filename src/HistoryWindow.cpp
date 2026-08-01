// HistoryWindow.cpp — Geçmiş penceresinin yerleşimi ve çizimi.
// Mesaj işleme ve pencere ömrü HistoryInput.cpp'dedir.
#include "HistoryInternal.h"

#include "Geometry.h"
#include "ImageCodec.h"
#include "ImageTransform.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace history {

HFONT CreateUiFont(unsigned dpi, int pointSize, int weight) {
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

// İki rengi karıştırır; amount 0..255 arası ikinci rengin ağırlığıdır.
// Alfa yerine önceden hesaplanmış katı renk kullanmak, kartları tek bir GDI
// çağrısıyla çizmeyi mümkün kılar.
COLORREF Mix(COLORREF base, COLORREF over, int amount) noexcept {
    const int inverse = 255 - amount;
    const int r = (GetRValue(base) * inverse + GetRValue(over) * amount) / 255;
    const int g = (GetGValue(base) * inverse + GetGValue(over) * amount) / 255;
    const int b = (GetBValue(base) * inverse + GetBValue(over) * amount) / 255;
    return RGB(r, g, b);
}

void DrawText(HDC dc, const std::wstring& text, RECT area, HFONT font,
              COLORREF color, UINT format) {
    const HGDIOBJ old = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, color);
    ::DrawTextW(dc, text.c_str(), -1, &area, format | DT_NOPREFIX);
    ::SelectObject(dc, old);
}

// Küçük resmi kutuyu DOLDURACAK şekilde ölçekler, sonra ortadan kırpar.
//
// SIĞDIRMAK YERİNE KIRPMAK: sığdırma her kutucuğa farklı boyutta bir resim
// koyuyordu ve ızgara, ortak bir hizası olmayan dağınık lekeler gibi
// görünüyordu. Kırpma hepsini aynı dikdörtgene oturtur; gerçek ölçüler zaten
// kutucuğun altında yazılı olduğu için bilgi de kaybolmaz.
//
// BÜYÜTME YAPILMAZ: kutudan küçük bir yakalamayı şişirmek onu bulanık bir
// lekeye çevirirdi; o durumda resim olduğu boyutta ortalanır.
bool MakeThumbnail(const Image& source, int boxWidth, int boxHeight,
                                 Image& out) {
    if (!source.Valid() || boxWidth <= 0 || boxHeight <= 0) {
        return false;
    }

    const double byWidth = static_cast<double>(boxWidth) / source.Width();
    const double byHeight = static_cast<double>(boxHeight) / source.Height();
    const double factor = byWidth > byHeight ? byWidth : byHeight;

    if (factor >= 1.0) {
        return CropImage(source, 0, 0, source.Width(), source.Height(), out);
    }

    int width = static_cast<int>(source.Width() * factor + 0.5);
    int height = static_cast<int>(source.Height() * factor + 0.5);
    width = width < boxWidth ? boxWidth : width;
    height = height < boxHeight ? boxHeight : height;

    Image covered;
    if (!ScaleImage(source, width, height, covered)) {
        return false;
    }
    // ÜST ORTADAN kırpılır, tam ortadan değil: bir ekran görüntüsünde ayırt
    // edici olan şey genellikle üstteki başlık çubuğu ya da ilk satırlardır.
    const int x = (width - boxWidth) / 2;
    const int y = (height - boxHeight) / 4;
    return CropImage(covered, x, y, boxWidth, boxHeight, out);
}

std::wstring SizeLabel(const Tile& tile) {
    wchar_t text[64];
    ::swprintf_s(text, L"%d × %d", tile.fullWidth, tile.fullHeight);
    return std::wstring(text);
}

// "2026-08-01 14-22-05" → "01.08.2026 14:22". Dosya adı sıralanabilir olsun
// diye tireli yazılır; kullanıcıya gösterilirken okunur hâle çevrilir.
std::wstring TimeLabel(const HistoryEntry& entry) {
    SYSTEMTIME utc{};
    SYSTEMTIME local{};
    if (::FileTimeToSystemTime(&entry.written, &utc) == FALSE ||
        ::SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local) == FALSE) {
        return entry.name;
    }

    wchar_t date[64] = L"";
    wchar_t time[64] = L"";
    ::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr,
                      date, static_cast<int>(std::size(date)), nullptr);
    ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr,
                      time, static_cast<int>(std::size(time)));

    std::wstring label(date);
    label += L' ';
    label += time;
    return label;
}

int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

void ReloadTiles(HWND window, State& state) {
    if (state.store == nullptr) {
        return;
    }

    // Seçili öğenin YOLU hatırlanır, dizini değil: araya yeni bir yakalama
    // girdiğinde ya da bir kayıt silindiğinde dizinler kayar ve kullanıcı
    // bambaşka bir görüntüyü seçili bulurdu.
    std::wstring previous;
    if (state.selected >= 0 &&
        state.selected < static_cast<int>(state.tiles.size())) {
        previous = state.tiles[static_cast<size_t>(state.selected)].entry.path;
    }

    const int thumbWidth = Scale(kTileWidth - kInset * 2, state.dpi);
    const int thumbHeight = Scale(kThumbHeight - kInset, state.dpi);

    state.tiles.clear();
    for (HistoryEntry& entry : state.store->List()) {
        Image full;
        if (!LoadPng(entry.path, full)) {
            LogV(L"Geçmiş kaydı okunamadı: %s", entry.path.c_str());
            continue;   // bozuk dosya listeyi boşaltmasın
        }

        Tile tile;
        tile.fullWidth = full.Width();
        tile.fullHeight = full.Height();
        if (!MakeThumbnail(full, thumbWidth, thumbHeight, tile.thumbnail)) {
            continue;
        }
        tile.entry = std::move(entry);
        state.tiles.push_back(std::move(tile));
    }

    state.selected = state.tiles.empty() ? -1 : 0;
    if (!previous.empty()) {
        for (size_t i = 0; i < state.tiles.size(); ++i) {
            if (state.tiles[i].entry.path == previous) {
                state.selected = static_cast<int>(i);
                break;
            }
        }
    }
    state.hovered = -1;

    Layout(window, state);
    UpdateScrollBar(window, state);
}

void Layout(HWND window, State& state) {
    RECT client{};
    ::GetClientRect(window, &client);

    const int pad = Scale(kPad, state.dpi);
    const int gap = Scale(kGap, state.dpi);
    const int tileWidth = Scale(kTileWidth, state.dpi);
    const int tileHeight = Scale(kTileHeight, state.dpi);
    const int header = Scale(kHeaderHeight, state.dpi);

    const int usable = static_cast<int>(geom::Width(client)) - pad * 2;
    state.columns = (usable + gap) / (tileWidth + gap);
    if (state.columns < 1) {
        state.columns = 1;   // dar pencerede tek sütun; sıfır sütun bölme hatası
    }

    // Izgara ORTALANIR. Sola yaslamak, sütun sayısı tam bölmediğinde sağda bir
    // sütunluk boşluk bırakıyordu ve pencere ortasız görünüyordu.
    const int gridWidth =
        state.columns * tileWidth + (state.columns - 1) * gap;
    int origin = (static_cast<int>(geom::Width(client)) - gridWidth) / 2;
    if (origin < pad) {
        origin = pad;
    }

    for (size_t i = 0; i < state.tiles.size(); ++i) {
        const int index = static_cast<int>(i);
        const int column = index % state.columns;
        const int row = index / state.columns;
        const int left = origin + column * (tileWidth + gap);
        const int top = header + pad + row * (tileHeight + gap) - state.scroll;
        state.tiles[i].bounds =
            RECT{left, top, left + tileWidth, top + tileHeight};
    }

    const int rows = state.tiles.empty()
                         ? 0
                         : (static_cast<int>(state.tiles.size()) +
                            state.columns - 1) / state.columns;
    state.contentHeight =
        rows == 0 ? 0 : header + pad * 2 + rows * tileHeight + (rows - 1) * gap;
}

void UpdateScrollBar(HWND window, State& state) {
    RECT client{};
    ::GetClientRect(window, &client);
    const int visible = static_cast<int>(geom::Height(client));

    const int maximum = state.contentHeight > visible
                            ? state.contentHeight - visible
                            : 0;
    if (state.scroll > maximum) {
        state.scroll = maximum;
    }
    if (state.scroll < 0) {
        state.scroll = 0;
    }

    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMin = 0;
    info.nMax = state.contentHeight > 0 ? state.contentHeight - 1 : 0;
    info.nPage = static_cast<UINT>(visible);
    info.nPos = state.scroll;
    ::SetScrollInfo(window, SB_VERT, &info, TRUE);
}

int TileAt(const State& state, POINT client) noexcept {
    for (size_t i = 0; i < state.tiles.size(); ++i) {
        if (::PtInRect(&state.tiles[i].bounds, client)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}


}  // namespace history
}  // namespace crisp
