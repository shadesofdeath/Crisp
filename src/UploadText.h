// UploadText.h — Yükleme hatasının kullanıcıya gösterilecek hâli.
//
// Çekirdek (`crisp_core`) bir `UploadError` kodu döndürür ve orada kalır;
// çeviri uygulama katmanının işidir. Gerekçe UploadText.cpp'de.
#pragma once

#include "Upload.h"

#include <string>

namespace crisp {

// Sonuçtaki hatayı, kullanıcının diline çevrilmiş tek bir cümleye dönüştürür.
// Durum kodu ve alan adı — çevrilmeyen parçalar — cümleye eklenir.
[[nodiscard]] std::wstring UploadErrorText(const UploadResult& result);

}  // namespace crisp
