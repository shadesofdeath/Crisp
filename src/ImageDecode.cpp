// ImageDecode.cpp — PNG çözme.
//
// AYRI DOSYA: kodlamayla birlikte ImageCodec.cpp ev kuralının 400 satır
// sınırını aşıyordu (docs §9).
#include "ImageCodec.h"

#include "ImageCodecInternal.h"
#include "Util.h"

#include <shlwapi.h>
#include <wincodec.h>

namespace crisp {
namespace {


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

bool LoadImageFile(const std::wstring& path, Image& out) {
    // Çözme yolu aynı: WIC biçimi imzadan tanır ve uzantıya bakmaz. Ayrı
    // duran tek şey ANLAMI — burada bilinmeyen bir dosya açılıyor, orada
    // kendi yazdığımız PNG doğrulanıyor.
    return LoadPng(path, out);
}

}  // namespace crisp
