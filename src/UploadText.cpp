// UploadText.cpp — Yükleme hatasını kullanıcının diline çevirir.
//
// AYRI DOSYA VE UYGULAMA KATMANINDA, ÇÜNKÜ ÇEKİRDEK ÇEVİREMEZ. `UploadPng`
// crisp_core içinde ve crisp_core `Localization`a bağımlı olamaz — çekirdeğin
// arayüz katmanına bakması yön kuralını bozar. Bu yüzden çekirdek bir
// `UploadError` kodu döndürüyor ve çeviri burada, tek yerde yapılıyor.
//
// Bir süre öyle değildi: hata cümleleri UploadHttp.cpp'de hazır Türkçe
// yazılıydı, yani on altı dilin on beşinde kullanıcı arayüzün geri kalanı kendi
// dilindeyken Türkçe bir cümle okuyordu.
#include "UploadText.h"

#include "Localization.h"
#include "resource.h"

#include <cstdio>

namespace crisp {
namespace {

[[nodiscard]] UINT StringIdFor(UploadError error) noexcept {
    switch (error) {
        case UploadError::NoService:   return IDS_UPERR_NO_SERVICE;
        case UploadError::MissingKey:  return IDS_UPERR_MISSING_KEY;
        case UploadError::NoImage:     return IDS_UPERR_NO_IMAGE;
        case UploadError::Network:     return IDS_UPERR_NETWORK;
        case UploadError::Tls:         return IDS_UPERR_TLS;
        case UploadError::Rejected:    return IDS_UPERR_REJECTED;
        case UploadError::TooLarge:    return IDS_UPERR_TOO_LARGE;
        case UploadError::TooMany:     return IDS_UPERR_TOO_MANY;
        case UploadError::Unavailable: return IDS_UPERR_UNAVAILABLE;
        case UploadError::Unexpected:  return IDS_UPERR_UNEXPECTED;
        case UploadError::Unreadable:  return IDS_UPERR_UNREADABLE;
        default:                       return IDS_UPERR_UNEXPECTED;
    }
}

}  // namespace

std::wstring UploadErrorText(const UploadResult& result) {
    std::wstring text = Loc::Str(StringIdFor(result.error));

    // %u VARSA DOLDURULUR, YOKSA DOKUNULMAZ. Bazı cümleler durum kodu taşıyor,
    // bazıları taşımıyor; çevirmen hangisinin hangisi olduğunu bilmek zorunda
    // kalmasın diye karar burada, biçim dizesine bakarak veriliyor.
    if (text.find(L"%u") != std::wstring::npos) {
        wchar_t filled[320] = {};
        ::swprintf_s(filled, text.c_str(), result.status);
        text = filled;
    }

    // Alan adı cümlenin İÇİNE değil, SONUNA eklenir. Her dilde aynı yere
    // koyabilmek için cümlenin içinde bir yer ayırmak, on altı çevirinin
    // hepsinde doğru yerde durmasını beklemek olurdu.
    if (!result.detail.empty()) {
        text += L" — ";
        text += result.detail;
    }
    return text;
}

}  // namespace crisp
