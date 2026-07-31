// Capture.cpp — bkz. Capture.h.
#include "Capture.h"

#include "Geometry.h"

#include <dwmapi.h>

namespace crisp {
namespace {

// Ekranın tamamı için tek bir bellek DC'si açıp BitBlt yapar. PrintWindow
// yerine BitBlt kullanılır: PrintWindow pencerenin KENDİ çizimini ister ve
// donanım hızlandırmalı içerikte (video, oyun, bazı tarayıcı sekmeleri) siyah
// kare döndürür. BitBlt ekranda GÖRÜNENİ alır, yani kullanıcının gördüğüyle
// birebir aynı sonucu verir — bir ekran alıntısı aracından beklenen budur.
[[nodiscard]] bool BlitFromScreen(const RECT& screenRect, Image& out) {
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

RECT MonitorRectForWindow(HWND window) noexcept {
    if (window == nullptr) {
        return MonitorRectAtCursor();
    }
    return RectOfMonitor(::MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY));
}

// ---------------------------------------------------------------------------
// Yakalama
// ---------------------------------------------------------------------------

bool CaptureRect(const RECT& screenRect, Image& out) {
    return BlitFromScreen(screenRect, out);
}

bool CaptureWindow(HWND window, Image& out) {
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

    return BlitFromScreen(clamped, out);
}

}  // namespace crisp
