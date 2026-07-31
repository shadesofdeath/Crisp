// ImageCodec.cpp — bkz. ImageCodec.h.
#include "ImageCodec.h"

#include "Util.h"

#include <shlwapi.h>
#include <wincodec.h>

namespace crisp {
namespace {

[[nodiscard]] bool CreateFactory(ComPtr<IWICImagingFactory>& factory) {
    const HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        LogV(L"WIC fabrikası oluşturulamadı: 0x%08X", hr);
        return false;
    }
    return true;
}

// Görüntüyü açık bir IStream'e kodlar. Hem bellek hem dosya yolu bu tek
// fonksiyondan geçer; iki ayrı kopya olsaydı biri düzeltilip diğeri unutulurdu.
[[nodiscard]] bool EncodeToStream(const Image& image, IStream* stream) {
    if (!image.Valid() || stream == nullptr) {
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory)) {
        return false;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    HRESULT hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr,
                                        encoder.GetAddressOf());
    if (FAILED(hr)) {
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

    hr = frame->Initialize(properties.Get());
    if (FAILED(hr)) {
        return false;
    }

    hr = frame->SetSize(static_cast<UINT>(image.Width()),
                        static_cast<UINT>(image.Height()));
    if (FAILED(hr)) {
        return false;
    }

    // İstenen biçim; WIC destekleyemezse kendi seçtiğini geri yazar, o yüzden
    // değişken const değil.
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        return false;
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
[[nodiscard]] bool CopyToImage(IWICImagingFactory* factory,
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
[[nodiscard]] bool LooksLikeCompletePng(const uint8_t* data, size_t size) noexcept {
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

[[nodiscard]] bool DecodeFromStream(IStream* stream, Image& out) {
    ComPtr<IWICImagingFactory> factory;
    if (!CreateFactory(factory)) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromStream(stream, nullptr,
                                                  WICDecodeMetadataCacheOnDemand,
                                                  decoder.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    return CopyToImage(factory.Get(), frame.Get(), out);
}

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

    if (!EncodeToStream(image, stream.Get())) {
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
    if (!image.Valid() || path.empty()) {
        return false;
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

    if (!EncodeToStream(image, stream.Get())) {
        return false;
    }

    hr = stream->Commit(STGC_DEFAULT);
    return SUCCEEDED(hr);
}

bool DecodePng(const uint8_t* data, size_t size, Image& out) {
    out.Reset();
    if (data == nullptr || size == 0 || size > MAXUINT32) {
        return false;
    }
    if (!LooksLikeCompletePng(data, size)) {
        LogV(L"PNG eksik ya da bozuk (%zu bayt); çözme denenmedi", size);
        return false;
    }

    ComPtr<IStream> stream;
    *stream.GetAddressOf() = ::SHCreateMemStream(data, static_cast<UINT>(size));
    if (!stream) {
        return false;
    }

    return DecodeFromStream(stream.Get(), out);
}

bool LoadPng(const std::wstring& path, Image& out) {
    out.Reset();
    if (path.empty()) {
        return false;
    }

    ComPtr<IStream> stream;
    const HRESULT hr = ::SHCreateStreamOnFileEx(
        path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL,
        FALSE, nullptr, stream.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    return DecodeFromStream(stream.Get(), out);
}

}  // namespace crisp
