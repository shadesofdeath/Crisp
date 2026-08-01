// Capture.cpp — bkz. Capture.h.
#include "Capture.h"

#include "Geometry.h"

#include <dwmapi.h>

namespace crisp {
namespace {

// İmleci verilen DC'ye, ekran dikdörtgenine göre konumlandırarak çizer.
//
// SICAK NOKTA ÇIKARILIR: GetCursorInfo imlecin EKRAN konumunu verir ama simge
// verisi, tıklanan noktanın simgenin neresine düştüğünü ayrıca söyler. Onu
// hesaba katmadan çizmek, oku birkaç piksel sağ-aşağı kaydırır — bir "buraya
// tıkla" görüntüsünde yanlış yeri işaret etmek demektir.
void DrawCursorInto(HDC dc, const RECT& screenRect) {
    CURSORINFO cursor{};
    cursor.cbSize = sizeof(cursor);
    if (!::GetCursorInfo(&cursor) || (cursor.flags & CURSOR_SHOWING) == 0 ||
        cursor.hCursor == nullptr) {
        return;
    }

    ICONINFO info{};
    if (!::GetIconInfo(cursor.hCursor, &info)) {
        return;
    }
    // GetIconInfo maskeleri KOPYALAYARAK verir; bırakılmazsa her yakalamada
    // iki GDI nesnesi sızar.
    if (info.hbmColor != nullptr) {
        ::DeleteObject(info.hbmColor);
    }
    if (info.hbmMask != nullptr) {
        ::DeleteObject(info.hbmMask);
    }

    const int x = cursor.ptScreenPos.x - screenRect.left -
                  static_cast<int>(info.xHotspot);
    const int y = cursor.ptScreenPos.y - screenRect.top -
                  static_cast<int>(info.yHotspot);
    (void)::DrawIconEx(dc, x, y, cursor.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
}

// Ekranın tamamı için tek bir bellek DC'si açıp BitBlt yapar. PrintWindow
// yerine BitBlt kullanılır: PrintWindow pencerenin KENDİ çizimini ister ve
// donanım hızlandırmalı içerikte (video, oyun, bazı tarayıcı sekmeleri) siyah
// kare döndürür. BitBlt ekranda GÖRÜNENİ alır, yani kullanıcının gördüğüyle
// birebir aynı sonucu verir — bir ekran alıntısı aracından beklenen budur.
[[nodiscard]] bool BlitFromScreen(const RECT& screenRect, Image& out,
                                  bool includeCursor) {
    const int width  = static_cast<int>(geom::Width(screenRect));
    const int height = static_cast<int>(geom::Height(screenRect));
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (!out.Create(width, height)) {
        return false;
    }

    // nullptr = tüm sanal ekran; DC_DESKTOP yerine bu kullanılır çünkü
    // GetDC(nullptr) çok monitörlü kurulumda negatif koordinatları da kapsar.
    const window_dc screenDc{nullptr};
    if (!screenDc.valid()) {
        return false;
    }

    const unique_hdc memoryDc{::CreateCompatibleDC(screenDc.get())};
    if (!memoryDc) {
        return false;
    }

    const dc_selection selection{memoryDc.get(), out.Handle()};

    // CAPTUREBLT imleç DEĞİL, katmanlı (saydam) pencereleri dahil eder. Onsuz
    // saydam menüler ve OSD'ler görüntüde eksik kalır.
    if (!::BitBlt(memoryDc.get(), 0, 0, width, height, screenDc.get(),
                  screenRect.left, screenRect.top, SRCCOPY | CAPTUREBLT)) {
        return false;
    }

    // İMLEÇ ALFA SABİTLENMEDEN ÖNCE çizilir: DrawIconEx de 32 bpp hedefe
    // yazarken alfayı tanımsız bırakır ve sonradan sabitlenmezse imlecin
    // olduğu yer PNG'de saydam bir delik olurdu.
    if (includeCursor) {
        DrawCursorInto(memoryDc.get(), screenRect);
    }

    // BitBlt 32 bpp hedefe yazarken alfa baytını TANIMSIZ bırakır; çoğu
    // sürücüde 0 gelir. Bu görüntü panoya PNG olarak da konacağı için tamamen
    // saydam bir resim üretmemek adına alfa 255'e sabitlenir.
    auto* pixels = static_cast<uint32_t*>(out.Bits());
    if (pixels != nullptr) {
        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        for (size_t i = 0; i < count; ++i) {
            pixels[i] |= 0xFF000000u;
        }
    }

    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Image
// ---------------------------------------------------------------------------

bool Image::Create(int width, int height) {
    Reset();

    if (width <= 0 || height <= 0) {
        return false;
    }
    // 4 bayt/piksel çarpımının taşmaması gerekir; 32767 kenar, 4 GB'lık bir
    // görüntüden çok daha küçük ve gerçek ekranların çok üstünde.
    if (width > 32767 || height > 32767) {
        return false;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth       = width;
    info.bmiHeader.biHeight      = -height;   // negatif = top-down
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    unique_hbitmap bitmap{::CreateDIBSection(nullptr, &info, DIB_RGB_COLORS,
                                             &bits, nullptr, 0)};
    if (!bitmap || bits == nullptr) {
        return false;
    }

    m_bitmap = std::move(bitmap);
    m_bits = bits;
    m_width = width;
    m_height = height;
    return true;
}

void Image::Reset() noexcept {
    m_bitmap.reset();
    m_bits = nullptr;
    m_width = 0;
    m_height = 0;
}

uint32_t Image::Pixel(int x, int y) const noexcept {
    if (!Valid() || x < 0 || y < 0 || x >= m_width || y >= m_height) {
        return 0;
    }
    const auto* row = reinterpret_cast<const uint32_t*>(
        static_cast<const uint8_t*>(m_bits) + static_cast<size_t>(y) * Stride());
    return row[x];
}

void Image::SetPixel(int x, int y, uint32_t argb) noexcept {
    if (!Valid() || x < 0 || y < 0 || x >= m_width || y >= m_height) {
        return;
    }
    auto* row = reinterpret_cast<uint32_t*>(
        static_cast<uint8_t*>(m_bits) + static_cast<size_t>(y) * Stride());
    row[x] = argb;
}

void Image::Fill(uint32_t argb) noexcept {
    if (!Valid()) {
        return;
    }
    auto* pixels = static_cast<uint32_t*>(m_bits);
    const size_t count = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    for (size_t i = 0; i < count; ++i) {
        pixels[i] = argb;
    }
}

// ---------------------------------------------------------------------------
// Ekran sorguları
// ---------------------------------------------------------------------------

RECT VirtualScreenRect() noexcept {
    const int left   = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top    = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width  = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Ölçümler oturum kilitliyken 0 dönebilir; o hâlde birincil ekrana düş.
    if (width <= 0 || height <= 0) {
        return RECT{0, 0, ::GetSystemMetrics(SM_CXSCREEN),
                    ::GetSystemMetrics(SM_CYSCREEN)};
    }
    return RECT{left, top, left + width, top + height};
}

namespace {

[[nodiscard]] RECT RectOfMonitor(HMONITOR monitor) noexcept {
    if (monitor == nullptr) {
        return VirtualScreenRect();
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!::GetMonitorInfoW(monitor, &info)) {
        return VirtualScreenRect();
    }
    return info.rcMonitor;
}

}  // namespace

RECT MonitorRectAtCursor() noexcept {
    POINT cursor{};
    if (!::GetCursorPos(&cursor)) {
        return VirtualScreenRect();
    }
    return RectOfMonitor(::MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY));
}

std::vector<RECT> MonitorRects() {
    std::vector<RECT> rects;
    ::EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (::GetMonitorInfoW(monitor, &info)) {
                reinterpret_cast<std::vector<RECT>*>(data)->push_back(info.rcMonitor);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&rects));

    // Basit ekleme sıralaması: monitör sayısı tek haneli ve std::sort'u dâhil
    // etmek bu dosyaya <algorithm> getirirdi.
    for (size_t i = 1; i < rects.size(); ++i) {
        const RECT current = rects[i];
        size_t j = i;
        while (j > 0 && (rects[j - 1].left > current.left ||
                         (rects[j - 1].left == current.left &&
                          rects[j - 1].top > current.top))) {
            rects[j] = rects[j - 1];
            --j;
        }
        rects[j] = current;
    }
    return rects;
}

RECT MonitorRectForWindow(HWND window) noexcept {
    if (window == nullptr) {
        return MonitorRectAtCursor();
    }
    return RectOfMonitor(::MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY));
}

// ---------------------------------------------------------------------------
// Yakalama
// ---------------------------------------------------------------------------

bool CaptureRect(const RECT& screenRect, Image& out, bool includeCursor) {
    return BlitFromScreen(screenRect, out, includeCursor);
}

bool CropImage(const Image& source, int x, int y, int width, int height,
               Image& out) {
    if (!source.Valid() || width <= 0 || height <= 0) {
        return false;
    }
    if (x < 0 || y < 0 || x + width > source.Width() ||
        y + height > source.Height()) {
        return false;
    }
    // Kaynak ve hedef aynı nesne olamaz: aşağıdaki kopyalama sırasında hedefi
    // yeniden tahsis etmek kaynağın piksellerini serbest bırakırdı.
    if (&source == &out) {
        return false;
    }

    if (!out.Create(width, height)) {
        return false;
    }

    const auto* sourceBytes = static_cast<const uint8_t*>(source.Bits());
    auto* destinationBytes = static_cast<uint8_t*>(out.Bits());
    const size_t rowBytes = static_cast<size_t>(width) * 4;

    for (int row = 0; row < height; ++row) {
        const uint8_t* from = sourceBytes +
                              static_cast<size_t>(y + row) * source.Stride() +
                              static_cast<size_t>(x) * 4;
        uint8_t* to = destinationBytes + static_cast<size_t>(row) * out.Stride();
        ::memcpy(to, from, rowBytes);
    }
    return true;
}

bool CaptureWindow(HWND window, Image& out, bool includeCursor) {
    if (window == nullptr || !::IsWindow(window)) {
        return false;
    }

    RECT bounds{};
    // DWM sınırı görünür çerçevedir. Başarısız olursa (DWM kapalı, klasik tema)
    // GetWindowRect'e düşülür: birkaç piksel fazla kenarlık, hiç yakalamamaktan
    // iyidir.
    const HRESULT hr = ::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                               &bounds, sizeof(bounds));
    if (FAILED(hr) || geom::IsEmpty(bounds)) {
        if (!::GetWindowRect(window, &bounds)) {
            return false;
        }
    }

    // Pencere ekran dışına taşabilir; taşan kısımda ekranda piksel yoktur ve
    // BitBlt oraya çöp kopyalar. Görünen kısımla sınırla.
    const RECT clamped = geom::ClampTo(bounds, VirtualScreenRect());
    if (geom::IsEmpty(clamped)) {
        return false;
    }

    return BlitFromScreen(clamped, out, includeCursor);
}

}  // namespace crisp
