// UploadInternal.h — Upload.cpp ile UploadHttp.cpp arasında kalan parçalar.
//
// Yükleme iki dosyaya bölünmüş durumda: isteği KURAN taraf ile GÖNDEREN taraf.
// Bu başlık ikisinin ortak diliyken, dışarıya (Upload.h'a) çıkmıyor — çağıran
// için `UploadPng` yeterli ve bir isteğin hangi başlıkları taşıdığı onun bilmesi
// gereken bir şey değil.
#pragma once

#include "Upload.h"

#include <string>
#include <vector>

namespace crisp {

// UTF-8 ile UTF-16 arasında.
//
// PROJENİN GERİ KALANI TAMAMEN UTF-16 ve bu dönüşümler yalnızca burada
// gerekiyor: HTTP gövdesi ile başlıkları bayt akışıdır, kullanıcının anahtarı
// ile dosya adı ise wchar_t. Util.h'a konmadılar çünkü orada tek bir kullanıcısı
// olmayacak bir yardımcı, ilk yanlış kullanımını bekleyen bir yardımcıdır.
[[nodiscard]] std::string WideToUtf8(const std::wstring& text);
[[nodiscard]] std::wstring Utf8ToWide(const std::string& text);

// Gönderilmeye hazır tek bir HTTP isteği.
struct UploadRequest {
    std::wstring host;      // "api.imgur.com"
    std::wstring path;      // "/3/image"
    std::wstring headers;   // CRLF ile ayrılmış ek başlıklar; boş olabilir
    std::string body;       // gövde baytları: multipart zarfı ya da dosyanın kendisi
    // İSTİSNA VAR, O YÜZDEN ALAN VAR. On iki servis multipart/form-data ile
    // POST kabul ediyor; bashupload.app etmiyor — gövdeyi olduğu gibi dosya
    // sayıyor. Ona multipart göndermek hata vermiyor, çok daha kötüsünü
    // yapıyor: sınırlayıcı satırlarını ve başlıkları da içeren zarfın TAMAMINI
    // dosya olarak saklıyor ve geriye çalışan bir bağlantı döndürüyor.
    // Bağlantıya tıklanana kadar hiçbir şey yanlış görünmüyor.
    std::wstring verb = L"POST";
    bool rawBody = false;   // true: gövde dosyanın kendisi, zarf yok
    bool valid = false;
    std::wstring error;     // valid=false ise sebebi, kullanıcıya gösterilecek hâlde
};

// Servis ve PNG'den isteği kurar.
//
// AĞA HİÇ ÇIKMAZ ve tam olarak bu yüzden ayrı: bir isteğin doğru kurulup
// kurulmadığı, sunucuya sormadan sınanabilecek bir soru.
[[nodiscard]] UploadRequest BuildUploadRequest(UploadService service,
                                               const std::wstring& apiKey,
                                               const std::vector<unsigned char>& png,
                                               const std::wstring& fileName);

// Gövde sınırlayıcısı. Sabit değil: dosyanın içinde geçme ihtimali olan bir
// sınırlayıcı gövdeyi bozar ve PNG baytları rastgeledir.
[[nodiscard]] std::string MakeBoundary();

}  // namespace crisp
