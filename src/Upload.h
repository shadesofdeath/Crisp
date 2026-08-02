// Upload.h — Bir yakalamayı bir görsel barındırma servisine gönderir.
//
// BU DOSYA CRISP'İN İLK AĞ KODU ve bu, kendiliğinden bir tasarım kararıdır.
// README ile CHANGELOG "no upload, no account" diyor ve bu söz bozulmuyor:
// yükleme VARSAYILAN OLARAK KAPALI, kullanıcı bir servis seçene kadar hiçbir
// bağlantı kurulmaz, ve seçmediği sürece uygulama ağı hiç açmaz. Değişen şey,
// isteyen kullanıcının artık yapabilmesi.
//
// Üç kural bu dosyayı şekillendiriyor.
//
// **Yükleme İKİ AÇIK ADIMLA açılır.** Önce bir servis seçilir, sonra ya
// editördeki Yükle düğmesine basılır ya da "Yakalamadan sonra" listesindeki
// kutu işaretlenir. İkisi de kapalıyken hiçbir şey gitmez.
//
// Bu dosya bir süre "yükleme hiçbir zaman kendiliğinden olmaz" diyordu ve
// gerekçesi hâlâ geçerli: ekran görüntüsü, sahibinin bilmediği bir yere
// gitmemesi gereken içeriktir — parola yöneticisi açık bir masaüstü, bir
// sözleşme taslağı, bir hasta kaydı. Ama bunu bir kural yapmak, isteyen
// kullanıcının yerine karar vermekti. Artık her yakalamayı gönderen bir seçenek
// VAR; kapalı geliyor, adı ne yaptığını söylüyor ("Yükle ve bağlantıyı
// kopyala"), ve yakalama sonrası listesinin en sonunda duruyor — üstündeki
// sekiz seçenek görüntüyü bu makinede tutuyor, yalnızca bu göndermiyor.
//
// **Servis listesi kapalıdır.** Kullanıcı adres yazmaz; aşağıdaki yedi
// servisten birini seçer. Serbest URL alanı, yanlış yazılmış bir adrese görüntü
// göndermeyi mümkün kılardı ve hangi alan adının kimin olduğunu Crisp bilemez.
//
// **Anahtar isteyen servis anahtarsız denenmez.** Imgur ve ImgBB'nin anonim
// yolları vardı ve ikisi de kısıtlandı; anahtarsız bir istek bugün 429 ya da
// 403 dönüyor. Anahtarı olmayan kullanıcıya bunu istekten ÖNCE söylemek,
// sunucunun anlaşılmaz hatasını göstermekten iyidir.
#pragma once

#include <string>
#include <vector>

namespace crisp {

// Görüntünün gönderileceği servis.
//
// SAYILAR AYAR DOSYASINA YAZILMAZ — kimlik olarak `UploadServiceId` döndürdüğü
// metin kullanılır. Enum'a ortadan bir üye eklemek, ayar dosyasında başka bir
// servisi seçili hâle getirirdi.
enum class UploadService : unsigned {
    None = 0,
    // Anahtar istemeyenler. Kalıcı olan başta: süreli bir servise koyup
    // unutulan bağlantı, bir süre sonra sessizce ölür ve bunu ancak karşı
    // taraf fark eder.
    Catbox,      // catbox.moe — kalıcı
    Litterbox,   // litterbox.catbox.moe — 72 saat
    Uguu,        // uguu.se — 3 saat
    ZeroXZero,   // 0x0.st — dosya boyutuna göre 30–365 gün
    // Anahtar isteyenler.
    Imgur,       // api.imgur.com — Client ID
    ImgBB,       // api.imgbb.com — API anahtarı
    FreeImage,   // freeimage.host — API anahtarı (Chevereto)
    Count,
};

// Bir servis hakkında, arayüzün ve gönderim kodunun ihtiyaç duyduğu her şey.
struct UploadServiceInfo {
    UploadService service = UploadService::None;
    const wchar_t* id = L"";        // ayar dosyasındaki kararlı ad
    const wchar_t* displayName = L"";
    const wchar_t* host = L"";      // ayarlar penceresinde gösterilir
    bool needsKey = false;
    // Anahtar alanının etiketi servise göre değişiyor: Imgur "Client ID" der,
    // ImgBB "API key". Kullanıcının panosundaki şeyin adını yazmak, doğru
    // yere yapıştırmasını sağlıyor.
    const wchar_t* keyLabel = L"";
    // Bağlantının ne kadar yaşadığı, saat cinsinden; 0 = kalıcı. Ayarlar
    // penceresi bunu servisin yanında gösterir.
    //
    // SÜRELİ SERVİSLER GİZLENMEZ, SÖYLENİR. Bir ekran görüntüsünü paylaşıp üç
    // saat sonra ölen bir bağlantı bırakmak, kullanıcının kendi seçtiği bir şey
    // olmalı — sonradan keşfettiği bir şey değil.
    unsigned lifetimeHours = 0;
};

// Bilinen servisler, arayüzün göstereceği sırayla: önce anahtar istemeyenler.
[[nodiscard]] const UploadServiceInfo* UploadServices(size_t& count) noexcept;

// Tek bir servisin bilgisi. Bilinmeyen değer için `None` girdisi döner.
[[nodiscard]] const UploadServiceInfo& UploadServiceOf(UploadService service) noexcept;

// Ayar dosyasındaki ad ile enum arasındaki çeviri.
[[nodiscard]] const wchar_t* UploadServiceId(UploadService service) noexcept;
[[nodiscard]] UploadService UploadServiceFromId(const std::wstring& id) noexcept;

// Bir gönderimin neden olmadığı.
//
// KOD, CÜMLE DEĞİL — VE BU BİR KATMAN KURALI. Buradaki alan bir süre hazır
// Türkçe cümle taşıyordu ve on altı dilin on beşinde kullanıcı, arayüzün geri
// kalanı kendi dilindeyken Türkçe bir hata okuyordu. Düzeltmenin bariz yolu
// `Loc::Str` çağırmaktı ve o yol kapalı: bu dosya `crisp_core` içinde, Loc ise
// uygulama katmanında, ve çekirdeğin arayüze bağımlı olması yön kuralını bozar
// (aynı gerekçe Settings.cpp'de dil doğrulaması için de yazılı).
//
// Yani çekirdek NE OLDUĞUNU söyler, arayüz onu kullanıcının diline çevirir.
enum class UploadError : unsigned {
    None = 0,
    NoService,      // ayarlarda servis seçilmemiş
    MissingKey,     // servis anahtar istiyor, anahtar yok
    NoImage,        // gönderilecek bayt yok
    Network,        // bağlanılamadı, ad çözülemedi, zaman aşımı
    Tls,            // güvenli bağlantı doğrulanamadı
    Rejected,       // 401 / 403 — anahtar yanlış olabilir
    TooLarge,       // 413
    TooMany,        // 429
    Unavailable,    // 5xx
    Unexpected,     // beklenmeyen durum kodu
    Unreadable,     // 2xx geldi ama gövdeden bağlantı çıkmadı
};

// Bir gönderimin sonucu.
struct UploadResult {
    bool ok = false;
    std::wstring link;    // başarıda: doğrudan görüntü adresi
    UploadError error = UploadError::None;
    // Cümleye eklenecek, ÇEVRİLMEYEN parça: sunucunun durum kodu ya da alan
    // adı. Sayı ve alan adı her dilde aynı yazılır; çeviri dosyasına girmesi
    // gereken tek şey onları saran cümle.
    unsigned status = 0;         // HTTP durum kodu, varsa
    std::wstring detail;         // alan adı, varsa
};

// PNG baytlarını gönderir ve dönen bağlantıyı verir.
//
// ÇAĞIRAN İŞ PARÇACIĞINI BLOKLAR. Ağ, kullanıcı arayüzü iş parçacığında
// beklenecek bir şey değil; çağıran bunu ayrı bir iş parçacığında çalıştırır ve
// sonucu pencereye geri gönderir.
[[nodiscard]] UploadResult UploadPng(UploadService service,
                                     const std::wstring& apiKey,
                                     const std::vector<unsigned char>& png,
                                     const std::wstring& fileName);

// --- Ağ gerektirmeyen, tek başına sınanabilen parçalar ---------------------
//
// Gönderimin iki yarısı da ağdan bağımsız: isteği kurmak ve yanıtı okumak.
// İkisi de burada açıkta, çünkü hata ihtimalinin tamamı bu ikisinde ve ağa
// çıkmadan sınanabiliyorlar.

// multipart/form-data gövdesi. `fields` ad/değer çiftleri; dosya en sona eklenir.
[[nodiscard]] std::string BuildMultipartBody(
    const std::vector<std::pair<std::string, std::string>>& fields,
    const std::string& fileField, const std::string& fileName,
    const std::string& contentType, const std::vector<unsigned char>& fileBytes,
    const std::string& boundary);

// Servisin yanıt gövdesinden bağlantıyı çıkarır. Boş dize: çıkarılamadı.
//
// Anahtarsız iki servis düz metin döndürüyor (gövdenin tamamı adres), ikisi ise
// JSON. Ayrım burada, tek yerde.
[[nodiscard]] std::wstring ExtractUploadLink(UploadService service,
                                             const std::string& body);

// JSON gövdesinde noktalı bir yol arar: `JsonFindString(body, "data.link")`.
//
// TAM BİR JSON AYRIŞTIRICISI DEĞİL ve olmaya çalışmıyor. Dizeleri, kaçış
// dizilerini ve iç içe geçmeyi doğru sayar; aradığı tek şey verilen yoldaki dize
// değeridir. Bir bağımlılık eklemek ya da üç yüz satır ayrıştırıcı yazmak, iki
// servisin iki alanını okumak için ödenecek bedel değil.
[[nodiscard]] std::wstring JsonFindString(const std::string& json,
                                          const std::string& dottedPath);

}  // namespace crisp
