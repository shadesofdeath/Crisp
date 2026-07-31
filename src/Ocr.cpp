// Ocr.cpp — bkz. Ocr.h.
//
// Akış: Image → PNG → IRandomAccessStream → BitmapDecoder → SoftwareBitmap →
//       OcrEngine.RecognizeAsync → OcrResult.Text
//
// PNG'den geçilmesi bilinçlidir. Alternatif, SoftwareBitmap'i doğrudan piksel
// tamponundan kurmaktı; o yol IMemoryBufferByteAccess adlı belgelenmemiş bir
// COM arayüzü ve elle adım hesabı gerektiriyor. PNG yolu iki fazladan async
// çağrı ekler (milisaniyeler) ama tamamı belgelenmiş arayüzlerden oluşur.
#include "Ocr.h"

#include "ImageCodec.h"
#include "Util.h"

#include <roapi.h>
#include <winstring.h>

#include <windows.foundation.h>
#include <windows.globalization.h>
#include <windows.graphics.imaging.h>
#include <windows.media.ocr.h>
#include <windows.storage.streams.h>

#include <vector>

namespace crisp {
namespace {

using ABI::Windows::Foundation::AsyncStatus;
using ABI::Windows::Foundation::IAsyncInfo;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Graphics::Imaging::BitmapDecoder;
using ABI::Windows::Graphics::Imaging::IBitmapDecoder;
using ABI::Windows::Graphics::Imaging::IBitmapDecoderStatics;
using ABI::Windows::Graphics::Imaging::IBitmapFrameWithSoftwareBitmap;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmap;
using ABI::Windows::Graphics::Imaging::SoftwareBitmap;
using ABI::Windows::Media::Ocr::IOcrEngine;
using ABI::Windows::Media::Ocr::IOcrEngineStatics;
using ABI::Windows::Media::Ocr::IOcrResult;
using ABI::Windows::Media::Ocr::OcrResult;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::IDataWriterFactory;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;

// Yığın üzerinde HSTRING referansı: tahsis yok, yıkım gerekmez.
class HStringRef {
public:
    explicit HStringRef(const wchar_t* text) noexcept {
        if (FAILED(::WindowsCreateStringReference(
                text, static_cast<UINT32>(::wcslen(text)), &m_header, &m_value))) {
            m_value = nullptr;
        }
    }
    HStringRef(const HStringRef&) = delete;
    HStringRef& operator=(const HStringRef&) = delete;

    [[nodiscard]] HSTRING get() const noexcept { return m_value; }

private:
    HSTRING_HEADER m_header{};
    HSTRING m_value = nullptr;
};

// Sahiplenilen HSTRING (çıkış parametreleri için).
class HStringOwner {
public:
    HStringOwner() noexcept = default;
    HStringOwner(const HStringOwner&) = delete;
    HStringOwner& operator=(const HStringOwner&) = delete;
    ~HStringOwner() {
        if (m_value != nullptr) {
            ::WindowsDeleteString(m_value);
        }
    }

    [[nodiscard]] HSTRING* put() noexcept { return &m_value; }

    [[nodiscard]] std::wstring str() const {
        if (m_value == nullptr) {
            return std::wstring();
        }
        UINT32 length = 0;
        const wchar_t* raw = ::WindowsGetStringRawBuffer(m_value, &length);
        return raw == nullptr ? std::wstring() : std::wstring(raw, length);
    }

private:
    HSTRING m_value = nullptr;
};

// STA üzerinde kısa senkron bekleme. Completed handler sınıfı yazmamak için
// IAsyncInfo durumu yoklanır. OCR bir kullanıcı eylemiyle tetiklenir ve
// tipik olarak yüz milisaniyeler sürer; en fazla 15 saniye beklenir.
[[nodiscard]] HRESULT AwaitAsync(IInspectable* operation) {
    ComPtr<IAsyncInfo> info;
    HRESULT hr = operation->QueryInterface(
        __uuidof(IAsyncInfo), reinterpret_cast<void**>(info.GetAddressOf()));
    if (FAILED(hr)) {
        return hr;
    }

    for (int spin = 0; spin < 1500; ++spin) {
        AsyncStatus status = AsyncStatus::Started;
        hr = info->get_Status(&status);
        if (FAILED(hr)) {
            return hr;
        }
        if (status == AsyncStatus::Completed) {
            return S_OK;
        }
        if (status != AsyncStatus::Started) {
            HRESULT code = E_FAIL;
            (void)info->get_ErrorCode(&code);
            return FAILED(code) ? code : E_FAIL;
        }
        ::Sleep(10);
    }
    return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
}

template <typename TFactory>
[[nodiscard]] HRESULT GetStatics(const wchar_t* className, ComPtr<TFactory>& out) {
    const HStringRef name(className);
    if (name.get() == nullptr) {
        return E_FAIL;
    }
    return ::RoGetActivationFactory(name.get(), __uuidof(TFactory),
                                    reinterpret_cast<void**>(out.GetAddressOf()));
}

// PNG baytlarını bellek içi bir WinRT akışına yazar.
[[nodiscard]] HRESULT StreamFromBytes(const std::vector<uint8_t>& bytes,
                                      ComPtr<IRandomAccessStream>& out) {
    const HStringRef className(
        RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream);
    ComPtr<IInspectable> instance;
    HRESULT hr = ::RoActivateInstance(className.get(), instance.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IRandomAccessStream> stream;
    hr = instance->QueryInterface(__uuidof(IRandomAccessStream),
                                  reinterpret_cast<void**>(stream.GetAddressOf()));
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IDataWriterFactory> writerFactory;
    hr = GetStatics(RuntimeClass_Windows_Storage_Streams_DataWriter, writerFactory);
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IOutputStream> outputStream;
    hr = stream->GetOutputStreamAt(0, outputStream.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IDataWriter> writer;
    hr = writerFactory->CreateDataWriter(outputStream.Get(), writer.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    hr = writer->WriteBytes(static_cast<UINT32>(bytes.size()),
                            const_cast<BYTE*>(bytes.data()));
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IAsyncOperation<UINT32>> storeOperation;
    hr = writer->StoreAsync(storeOperation.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }
    hr = AwaitAsync(storeOperation.Get());
    if (FAILED(hr)) {
        return hr;
    }

    // Akış konumu SIFIRLANMALI: DataWriter sonuna kadar yazdı, çözücü baştan
    // okumak ister. Atlanırsa BitmapDecoder "bozuk görüntü" der.
    hr = stream->Seek(0);
    if (FAILED(hr)) {
        return hr;
    }

    out = std::move(stream);
    return S_OK;
}

[[nodiscard]] HRESULT SoftwareBitmapFromStream(IRandomAccessStream* stream,
                                               ComPtr<ISoftwareBitmap>& out) {
    ComPtr<IBitmapDecoderStatics> decoderStatics;
    HRESULT hr = GetStatics(RuntimeClass_Windows_Graphics_Imaging_BitmapDecoder,
                            decoderStatics);
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IAsyncOperation<BitmapDecoder*>> decoderOperation;
    hr = decoderStatics->CreateAsync(stream, decoderOperation.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }
    hr = AwaitAsync(decoderOperation.Get());
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IBitmapDecoder> decoder;
    hr = decoderOperation->GetResults(decoder.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    // GetSoftwareBitmapAsync IBitmapDecoder'da DEĞİL, kare arayüzündedir.
    // BitmapDecoder ilk kareyi kendisi temsil eder, bu yüzden aynı nesne
    // IBitmapFrameWithSoftwareBitmap olarak sorgulanabilir.
    ComPtr<IBitmapFrameWithSoftwareBitmap> frame;
    hr = decoder->QueryInterface(__uuidof(IBitmapFrameWithSoftwareBitmap),
                                 reinterpret_cast<void**>(frame.GetAddressOf()));
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<IAsyncOperation<SoftwareBitmap*>> bitmapOperation;
    hr = frame->GetSoftwareBitmapAsync(bitmapOperation.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }
    hr = AwaitAsync(bitmapOperation.Get());
    if (FAILED(hr)) {
        return hr;
    }

    return bitmapOperation->GetResults(out.GetAddressOf());
}

[[nodiscard]] HRESULT CreateEngine(ComPtr<IOcrEngine>& out) {
    ComPtr<IOcrEngineStatics> statics;
    HRESULT hr = GetStatics(RuntimeClass_Windows_Media_Ocr_OcrEngine, statics);
    if (FAILED(hr)) {
        return hr;
    }

    // Kullanıcının dil profilinden motor kurulur. Desteklenen dil yoksa
    // S_OK döner ama işaretçi NULL olur — bu bir hata değil, "bu makinede OCR
    // dili kurulu değil" demektir.
    hr = statics->TryCreateFromUserProfileLanguages(out.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }
    return out ? S_OK : E_NOTIMPL;
}

}  // namespace

bool IsOcrAvailable() {
    ComPtr<IOcrEngine> engine;
    return SUCCEEDED(CreateEngine(engine));
}

bool RecognizeText(const Image& image, std::wstring& text) {
    text.clear();
    if (!image.Valid()) {
        return false;
    }

    ComPtr<IOcrEngine> engine;
    HRESULT hr = CreateEngine(engine);
    if (FAILED(hr)) {
        LogV(L"OCR motoru oluşturulamadı: 0x%08X", hr);
        return false;
    }

    std::vector<uint8_t> png;
    if (!EncodePng(image, png)) {
        return false;
    }

    ComPtr<IRandomAccessStream> stream;
    hr = StreamFromBytes(png, stream);
    if (FAILED(hr)) {
        LogV(L"OCR akışı hazırlanamadı: 0x%08X", hr);
        return false;
    }

    ComPtr<ISoftwareBitmap> bitmap;
    hr = SoftwareBitmapFromStream(stream.Get(), bitmap);
    if (FAILED(hr)) {
        LogV(L"OCR için bitmap çözülemedi: 0x%08X", hr);
        return false;
    }

    ComPtr<IAsyncOperation<OcrResult*>> operation;
    hr = engine->RecognizeAsync(bitmap.Get(), operation.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }
    hr = AwaitAsync(operation.Get());
    if (FAILED(hr)) {
        LogV(L"OCR tanıma başarısız: 0x%08X", hr);
        return false;
    }

    ComPtr<IOcrResult> result;
    hr = operation->GetResults(result.GetAddressOf());
    if (FAILED(hr) || !result) {
        return false;
    }

    HStringOwner recognized;
    hr = result->get_Text(recognized.put());
    if (FAILED(hr)) {
        return false;
    }

    // Metin bulunamaması BAŞARIDIR: boş bir bölge seçmek hata değil.
    text = recognized.str();
    return true;
}

}  // namespace crisp
