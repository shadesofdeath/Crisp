// ImageCodec.cpp — bkz. ImageCodec.h.
#include "ImageCodec.h"

#include "ImageCodecInternal.h"
#include "Util.h"

#include <shlwapi.h>

#include <cwctype>
#include <wincodec.h>

namespace crisp {

bool CreateFactory(ComPtr<IWICImagingFactory>& factory) {
    const HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        LogV(L"WIC fabrikası oluşturulamadı: 0x%08X", hr);
        return false;
    }
    return true;
}

namespace {

// WebP kapsayıcı kimliği eski SDK başlıklarında yoktur; elle tanımlanır.
// {1F122A9-...} değil — WIC'in yayımladığı kimlik budur.
// Kodlayıcının VAR OLMASI ayrı bir konudur, bkz. IsFormatAvailable.
const GUID kContainerFormatWebp = {
    0xe094b0e2,
    0x67f2,
    0x45b3,
    {0xb0, 0xea, 0x11, 0x53, 0x37, 0xca, 0x7c, 0xf3}};

[[nodiscard]] const GUID& ContainerFor(ImageFormat format) noexcept {
    switch (format) {
        case ImageFormat::Jpeg:
            return GUID_ContainerFormatJpeg;
        case ImageFormat::WebP:
            return kContainerFormatWebp;
        case ImageFormat::Png:
        default:
            return GUID_ContainerFormatPng;
    }
}

// Görüntüyü açık bir IStream'e kodlar. Hem bellek hem dosya yolu bu tek
// fonksiyondan geçer; iki ayrı kopya olsaydı biri düzeltilip diğeri unutulurdu.
[[nodiscard]] bool EncodeToStream(const Image& image, IStream* stream,
                                  ImageFormat imageFormat, unsigned quality) {
    if (!image.Valid() || stream == nullptr) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory)) {
        return false;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    HRESULT hr = factory->CreateEncoder(ContainerFor(imageFormat), nullptr,
                                        encoder.GetAddressOf());
    if (FAILED(hr)) {
        LogV(L"Kodlayıcı oluşturulamadı (biçim %d): 0x%08X",
             static_cast<int>(imageFormat), hr);
        return false;
    }

    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // Kalite, frame->Initialize'DAN ÖNCE yazılmalı: Initialize özellik
    // torbasını okur ve sonra yapılan değişiklikler dikkate alınmaz.
    if (imageFormat != ImageFormat::Png && properties) {
        PROPBAG2 option{};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value{};
        ::VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = static_cast<float>(quality) / 100.0f;
        // Başarısızlık ölümcül değil: kodlayıcı varsayılan kalitesini kullanır.
        CRISP_LOG_IF_FAILED(properties->Write(1, &option, &value));
    }

    hr = frame->Initialize(properties.Get());
    if (FAILED(hr)) {
        return false;
    }

    hr = frame->SetSize(static_cast<UINT>(image.Width()),
                        static_cast<UINT>(image.Height()));
    if (FAILED(hr)) {
        return false;
    }

    // JPEG'de alfa kanalı YOKTUR; 32bpp istemek kodlayıcıyı kendi dönüşümünü
    // yapmaya zorlar ve bazı sürümlerde kanal sırası bozulur. Biçime uygun
    // olanı istemek daha güvenli.
    WICPixelFormatGUID format = (imageFormat == ImageFormat::Jpeg)
                                    ? GUID_WICPixelFormat24bppBGR
                                    : GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        return false;
    }

    // Kodlayıcı istediğimiz biçimi kabul etmediyse (geri yazdıysa), pikselleri
    // dönüştürmek gerekir. WICConvertBitmapSource bunu yapar; doğrudan
    // WritePixels çağırmak yanlış yorumlanmış baytlar demek olurdu.
    if (!::IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
        ComPtr<IWICBitmap> source;
        hr = factory->CreateBitmapFromMemory(
            static_cast<UINT>(image.Width()), static_cast<UINT>(image.Height()),
            GUID_WICPixelFormat32bppBGRA, static_cast<UINT>(image.Stride()),
            static_cast<UINT>(image.Stride()) * static_cast<UINT>(image.Height()),
            static_cast<BYTE*>(image.Bits()), source.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }

        ComPtr<IWICBitmapSource> converted;
        hr = ::WICConvertBitmapSource(format, source.Get(),
                                      converted.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }

        hr = frame->WriteSource(converted.Get(), nullptr);
        if (FAILED(hr)) {
            return false;
        }
        hr = frame->Commit();
        if (FAILED(hr)) {
            return false;
        }
        return SUCCEEDED(encoder->Commit());
    }

    const UINT stride = static_cast<UINT>(image.Stride());
    const UINT total = stride * static_cast<UINT>(image.Height());
    hr = frame->WritePixels(static_cast<UINT>(image.Height()), stride, total,
                            static_cast<BYTE*>(image.Bits()));
    if (FAILED(hr)) {
        return false;
    }

    hr = frame->Commit();
    if (FAILED(hr)) {
        return false;
    }

    hr = encoder->Commit();
    return SUCCEEDED(hr);
}

// Herhangi bir kaynağı 32 bpp BGRA'ya çevirip Image'a kopyalar.
}  // namespace

bool CopyToImage(IWICImagingFactory* factory,
                               IWICBitmapSource* source, Image& out) {
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = converter->Initialize(source, GUID_WICPixelFormat32bppBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        return false;
    }

    if (!out.Create(static_cast<int>(width), static_cast<int>(height))) {
        return false;
    }

    const UINT stride = static_cast<UINT>(out.Stride());
    const UINT total = stride * height;
    hr = converter->CopyPixels(nullptr, stride, total,
                               static_cast<BYTE*>(out.Bits()));
    if (FAILED(hr)) {
        out.Reset();
        return false;
    }
    return true;
}

// PNG yapısal olarak TAM mı?
//
// NEDEN GEREKLİ: WIC'in çözücüsü hoşgörülüdür. Yarıda kesilmiş bir PNG'ye
// S_OK döndürür ve elde ne varsa onu çözer; kalan satırlar tanımsız kalır.
// Panodan ya da diskten gelen bozuk bir veri bu yüzden sessizce "yarım
// ekran görüntüsü" olarak görünür. Panoda ayrıca CF_DIB de bulunduğu için
// burada reddetmek, çağıranın sağlam olan diğer biçime düşmesini sağlar.
//
// Kontrol ucuzdur: imza + son yığının IEND olması. Tam bir CRC doğrulaması
// PNG'nin tamamını taramayı gerektirirdi ve kesilmiş dosyayı yakalamak için
// gereken şey yalnızca sonun yerinde olmasıdır.
bool LooksLikeCompletePng(const uint8_t* data, size_t size) noexcept {
    constexpr uint8_t kSignature[8] = {0x89, 0x50, 0x4E, 0x47,
                                       0x0D, 0x0A, 0x1A, 0x0A};
    // İmza (8) + en az bir yığın başlığı + IEND (12) olmadan geçerli olamaz.
    if (data == nullptr || size < sizeof(kSignature) + 12) {
        return false;
    }
    for (size_t i = 0; i < sizeof(kSignature); ++i) {
        if (data[i] != kSignature[i]) {
            return false;
        }
    }

    // IEND yığını: uzunluk(4) + "IEND"(4) + CRC(4) — dosyanın son 12 baytı.
    const uint8_t* tail = data + size - 12;
    return tail[4] == 'I' && tail[5] == 'E' && tail[6] == 'N' && tail[7] == 'D';
}

namespace {


}  // namespace

bool EncodePng(const Image& image, std::vector<uint8_t>& out) {
    out.clear();
    if (!image.Valid()) {
        return false;
    }

    // SHCreateMemStream büyüyebilen bir bellek akışı verir; CreateStreamOnHGlobal
    // ile aynı işi görür ama HGLOBAL sahipliğini elle yönetmeyi gerektirmez.
    ComPtr<IStream> stream;
    *stream.GetAddressOf() = ::SHCreateMemStream(nullptr, 0);
    if (!stream) {
        return false;
    }

    if (!EncodeToStream(image, stream.Get(), ImageFormat::Png, 100)) {
        return false;
    }

    STATSTG stat{};
    HRESULT hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) {
        return false;
    }
    // 4 GB üstü bir PNG üretmiş olamayız; yine de daraltmadan önce doğrula.
    if (stat.cbSize.HighPart != 0 || stat.cbSize.LowPart == 0) {
        return false;
    }

    const LARGE_INTEGER origin{};
    hr = stream->Seek(origin, STREAM_SEEK_SET, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    out.resize(stat.cbSize.LowPart);
    ULONG read = 0;
    hr = stream->Read(out.data(), stat.cbSize.LowPart, &read);
    if (FAILED(hr) || read != stat.cbSize.LowPart) {
        out.clear();
        return false;
    }
    return true;
}

bool SavePng(const Image& image, const std::wstring& path) {
    return SaveImage(image, path, ImageFormat::Png, 100);
}

bool SaveImage(const Image& image, const std::wstring& path, ImageFormat format,
               unsigned quality) {
    if (!image.Valid() || path.empty()) {
        return false;
    }
    if (quality < 1u) {
        quality = 1u;
    } else if (quality > 100u) {
        quality = 100u;
    }

    const size_t slash = path.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        if (!EnsureDirectory(path.substr(0, slash))) {
            LogV(L"Hedef klasör oluşturulamadı");
            return false;
        }
    }

    ComPtr<IStream> stream;
    // STGM_CREATE: varsa üzerine yaz. Çağıran benzersiz adı zaten
    // MakeUniquePath ile seçiyor, buraya gelen yol kasıtlıdır.
    HRESULT hr = ::SHCreateStreamOnFileEx(
        path.c_str(), STGM_WRITE | STGM_CREATE | STGM_SHARE_DENY_WRITE,
        FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, stream.GetAddressOf());
    if (FAILED(hr)) {
        LogV(L"Dosya akışı açılamadı: 0x%08X", hr);
        return false;
    }

    if (!EncodeToStream(image, stream.Get(), format, quality)) {
        return false;
    }

    hr = stream->Commit(STGC_DEFAULT);
    return SUCCEEDED(hr);
}

bool IsFormatAvailable(ImageFormat format) {
    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory)) {
        return false;
    }
    ComPtr<IWICBitmapEncoder> encoder;
    return SUCCEEDED(
        factory->CreateEncoder(ContainerFor(format), nullptr, encoder.GetAddressOf()));
}

ImageFormat FormatFromPath(const std::wstring& path) noexcept {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) {
        return ImageFormat::Png;
    }
    std::wstring extension = path.substr(dot + 1);
    for (wchar_t& c : extension) {
        c = static_cast<wchar_t>(::towlower(c));
    }
    if (extension == L"jpg" || extension == L"jpeg") {
        return ImageFormat::Jpeg;
    }
    if (extension == L"webp") {
        return ImageFormat::WebP;
    }
    return ImageFormat::Png;
}

const wchar_t* ExtensionForFormat(ImageFormat format) noexcept {
    switch (format) {
        case ImageFormat::Jpeg:
            return L"jpg";
        case ImageFormat::WebP:
            return L"webp";
        case ImageFormat::Png:
        default:
            return L"png";
    }
}

ImageFormat FormatFromString(const wchar_t* value) noexcept {
    if (value == nullptr) {
        return ImageFormat::Png;
    }
    if (::wcscmp(value, L"jpg") == 0 || ::wcscmp(value, L"jpeg") == 0) {
        return ImageFormat::Jpeg;
    }
    if (::wcscmp(value, L"webp") == 0) {
        return ImageFormat::WebP;
    }
    return ImageFormat::Png;
}


}  // namespace crisp
