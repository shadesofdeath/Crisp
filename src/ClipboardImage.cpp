// ClipboardImage.cpp — bkz. ClipboardImage.h.
#include "ClipboardImage.h"

#include "ImageCodec.h"
#include "Util.h"

#include <utility>
#include <vector>

namespace crisp {
namespace {

// Panoyu başka bir uygulama tutuyor olabilir; OpenClipboard o an başarısız
// olur. Kısa aralıklarla yeniden denenir — kullanıcıya "pano meşgul" hatası
// göstermek yerine 250 ms beklemek her zaman daha iyi.
class clipboard_scope {
public:
    explicit clipboard_scope(HWND owner) noexcept {
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (::OpenClipboard(owner)) {
                m_open = true;
                return;
            }
            ::Sleep(25);
        }
        LogV(L"OpenClipboard 10 denemede başarısız (hata %lu)", ::GetLastError());
    }

    clipboard_scope(const clipboard_scope&) = delete;
    clipboard_scope& operator=(const clipboard_scope&) = delete;

    ~clipboard_scope() {
        if (m_open) {
            ::CloseClipboard();
        }
    }

    [[nodiscard]] bool ok() const noexcept { return m_open; }

private:
    bool m_open = false;
};

// Panoya devredilene kadar HGLOBAL'i tutan sarmalayıcı. SetClipboardData
// başarılı olursa sahiplik PANOYA geçer ve bizim serbest bırakmamız YASAKTIR;
// release() tam olarak bunu ifade eder.
class global_block {
public:
    // Varsayılan kurucu: çağıran önce boş bir blok bildirip sonra taşıyabilsin.
    global_block() noexcept = default;

    explicit global_block(size_t bytes) noexcept
        : m_handle(::GlobalAlloc(GMEM_MOVEABLE, bytes)) {}

    global_block(const global_block&) = delete;
    global_block& operator=(const global_block&) = delete;

    global_block(global_block&& other) noexcept
        : m_handle(std::exchange(other.m_handle, nullptr)) {}

    global_block& operator=(global_block&& other) noexcept {
        if (this != &other) {
            if (m_handle != nullptr) {
                ::GlobalFree(m_handle);
            }
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    ~global_block() {
        if (m_handle != nullptr) {
            ::GlobalFree(m_handle);
        }
    }

    [[nodiscard]] HGLOBAL get() const noexcept { return m_handle; }
    [[nodiscard]] bool valid() const noexcept { return m_handle != nullptr; }
    [[nodiscard]] HGLOBAL release() noexcept {
        HGLOBAL h = m_handle;
        m_handle = nullptr;
        return h;
    }

private:
    HGLOBAL m_handle = nullptr;
};

class global_lock {
public:
    explicit global_lock(HGLOBAL handle) noexcept
        : m_handle(handle), m_data(::GlobalLock(handle)) {}

    global_lock(const global_lock&) = delete;
    global_lock& operator=(const global_lock&) = delete;

    ~global_lock() {
        if (m_data != nullptr) {
            ::GlobalUnlock(m_handle);
        }
    }

    [[nodiscard]] void* get() const noexcept { return m_data; }
    [[nodiscard]] bool valid() const noexcept { return m_data != nullptr; }

private:
    HGLOBAL m_handle;
    void* m_data;
};

[[nodiscard]] UINT PngClipboardFormat() noexcept {
    // Kayıtlı biçim kimliği oturum boyunca sabittir; her çağrıda sormak gereksiz.
    static const UINT format = ::RegisterClipboardFormatW(L"PNG");
    return format;
}

// CF_DIB için AŞAĞIDAN YUKARI (bottom-up) DIB üretir. Image top-down'dır, ama
// CF_DIB'i negatif yükseklikle yazmak eski uygulamaların bir kısmında görüntüyü
// baş aşağı gösterir; pano biçimi en muhafazakâr biçimde yazılmalı.
[[nodiscard]] bool BuildDibBlock(const Image& image, global_block& block) {
    const size_t headerSize = sizeof(BITMAPINFOHEADER);
    const size_t pixelBytes = static_cast<size_t>(image.Stride()) *
                              static_cast<size_t>(image.Height());

    global_block local{headerSize + pixelBytes};
    if (!local.valid()) {
        return false;
    }

    const global_lock lock{local.get()};
    if (!lock.valid()) {
        return false;
    }

    auto* header = static_cast<BITMAPINFOHEADER*>(lock.get());
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = image.Width();
    header->biHeight = image.Height();   // pozitif = bottom-up
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(pixelBytes);
    header->biXPelsPerMeter = 0;
    header->biYPelsPerMeter = 0;
    header->biClrUsed = 0;
    header->biClrImportant = 0;

    auto* destination = static_cast<uint8_t*>(lock.get()) + headerSize;
    const auto* source = static_cast<const uint8_t*>(image.Bits());
    const size_t stride = static_cast<size_t>(image.Stride());

    for (int y = 0; y < image.Height(); ++y) {
        // Kaynak satır y, hedefte (height - 1 - y): dikey çevirme.
        const size_t destinationRow =
            static_cast<size_t>(image.Height() - 1 - y) * stride;
        ::memcpy(destination + destinationRow,
                 source + static_cast<size_t>(y) * stride, stride);
    }

    block = std::move(local);
    return true;
}

[[nodiscard]] bool BuildPngBlock(const Image& image, global_block& block) {
    std::vector<uint8_t> png;
    if (!EncodePng(image, png) || png.empty()) {
        return false;
    }

    global_block local{png.size()};
    if (!local.valid()) {
        return false;
    }

    const global_lock lock{local.get()};
    if (!lock.valid()) {
        return false;
    }

    ::memcpy(lock.get(), png.data(), png.size());
    block = std::move(local);
    return true;
}

// CF_DIB bloğunu Image'a çevirir. Hem bottom-up hem top-down kaynak kabul
// edilir: panoya yazan başka bir uygulama hangisini seçtiyse.
[[nodiscard]] bool ImageFromDib(const void* data, size_t size, Image& out) {
    if (data == nullptr || size < sizeof(BITMAPINFOHEADER)) {
        return false;
    }

    const auto* header = static_cast<const BITMAPINFOHEADER*>(data);
    if (header->biSize < sizeof(BITMAPINFOHEADER) || header->biCompression != BI_RGB) {
        return false;
    }
    // Bu yol yalnızca kendi yazdığımız biçimi geri okumak için var; 24/32 bit
    // dışındaki derinlikler (paletli, 16 bit) desteklenmez.
    if (header->biBitCount != 32 && header->biBitCount != 24) {
        return false;
    }

    const int width = header->biWidth;
    const bool bottomUp = header->biHeight > 0;
    const int height = bottomUp ? header->biHeight : -header->biHeight;
    if (width <= 0 || height <= 0) {
        return false;
    }

    const size_t sourceStride =
        ((static_cast<size_t>(width) * header->biBitCount + 31) / 32) * 4;
    const size_t needed = header->biSize + sourceStride * static_cast<size_t>(height);
    if (size < needed) {
        return false;
    }

    if (!out.Create(width, height)) {
        return false;
    }

    const auto* pixels = static_cast<const uint8_t*>(data) + header->biSize;
    const int bytesPerPixel = header->biBitCount / 8;

    for (int y = 0; y < height; ++y) {
        const size_t sourceRow =
            static_cast<size_t>(bottomUp ? (height - 1 - y) : y) * sourceStride;
        const uint8_t* row = pixels + sourceRow;
        for (int x = 0; x < width; ++x) {
            const uint8_t* p = row + static_cast<size_t>(x) * bytesPerPixel;
            const uint32_t alpha = (bytesPerPixel == 4) ? p[3] : 0xFFu;
            // 24 bitte alfa yoktur; 32 bitte de pano yazarları sıfır bırakabilir,
            // o hâlde opak sayılır — aksi hâlde görüntü tamamen saydam olurdu.
            const uint32_t effectiveAlpha = (alpha == 0) ? 0xFFu : alpha;
            out.SetPixel(x, y,
                         (effectiveAlpha << 24) | (static_cast<uint32_t>(p[2]) << 16) |
                             (static_cast<uint32_t>(p[1]) << 8) |
                             static_cast<uint32_t>(p[0]));
        }
    }
    return true;
}

}  // namespace

bool CopyImageToClipboard(const Image& image, HWND owner) {
    if (!image.Valid()) {
        return false;
    }

    // Bloklar pano AÇILMADAN ÖNCE hazırlanır: PNG kodlaması milisaniyeler
    // sürer ve o süre boyunca panoyu kilitli tutmak, kopyalamaya çalışan
    // başka uygulamaları bekletir.
    global_block dib;
    global_block png;
    const bool haveDib = BuildDibBlock(image, dib);
    const bool havePng = BuildPngBlock(image, png);
    if (!haveDib && !havePng) {
        return false;
    }

    const clipboard_scope clipboard{owner};
    if (!clipboard.ok()) {
        return false;
    }

    if (!::EmptyClipboard()) {
        return false;
    }

    bool wroteAny = false;

    if (haveDib && ::SetClipboardData(CF_DIB, dib.get()) != nullptr) {
        (void)dib.release();   // sahiplik panoda
        wroteAny = true;
    }

    const UINT pngFormat = PngClipboardFormat();
    if (havePng && pngFormat != 0 &&
        ::SetClipboardData(pngFormat, png.get()) != nullptr) {
        (void)png.release();
        wroteAny = true;
    }

    return wroteAny;
}

bool ClipboardHasImage() noexcept {
    const UINT pngFormat = PngClipboardFormat();
    return ::IsClipboardFormatAvailable(CF_DIB) ||
           (pngFormat != 0 && ::IsClipboardFormatAvailable(pngFormat));
}

bool ReadImageFromClipboard(Image& out, HWND owner) {
    out.Reset();

    const clipboard_scope clipboard{owner};
    if (!clipboard.ok()) {
        return false;
    }

    // Önce PNG: alfayı ve renk doğruluğunu kayıpsız taşır.
    const UINT pngFormat = PngClipboardFormat();
    if (pngFormat != 0 && ::IsClipboardFormatAvailable(pngFormat)) {
        const HANDLE handle = ::GetClipboardData(pngFormat);
        if (handle != nullptr) {
            const size_t size = ::GlobalSize(handle);
            const global_lock lock{handle};
            if (lock.valid() && size > 0 &&
                DecodePng(static_cast<const uint8_t*>(lock.get()), size, out)) {
                return true;
            }
        }
    }

    if (::IsClipboardFormatAvailable(CF_DIB)) {
        const HANDLE handle = ::GetClipboardData(CF_DIB);
        if (handle != nullptr) {
            const size_t size = ::GlobalSize(handle);
            const global_lock lock{handle};
            if (lock.valid() && ImageFromDib(lock.get(), size, out)) {
                return true;
            }
        }
    }

    return false;
}

bool CopyTextToClipboard(const wchar_t* text, HWND owner) {
    if (text == nullptr) {
        return false;
    }

    const size_t characters = ::wcslen(text) + 1;
    global_block block{characters * sizeof(wchar_t)};
    if (!block.valid()) {
        return false;
    }

    {
        const global_lock lock{block.get()};
        if (!lock.valid()) {
            return false;
        }
        ::memcpy(lock.get(), text, characters * sizeof(wchar_t));
    }

    const clipboard_scope clipboard{owner};
    if (!clipboard.ok()) {
        return false;
    }
    if (!::EmptyClipboard()) {
        return false;
    }

    if (::SetClipboardData(CF_UNICODETEXT, block.get()) == nullptr) {
        return false;
    }
    (void)block.release();
    return true;
}

}  // namespace crisp
