// UploadServices.cpp — Hangi servis nedir, ne ister, ne döndürür.
//
// Upload.cpp'den AYRILDI çünkü iki ayrı bilgi türü barındırıyorlardı: burası
// on üç servisin her birine özel olan her şeyi (yol, alan adı, gövde biçimi,
// yanıttaki bağlantının nerede durduğu) tutuyor, Upload.cpp ise hiçbirine özel
// olmayanı (multipart zarfı, JSON okuma, UTF-8). Yeni bir servis eklemek artık
// yalnızca bu dosyaya dokunuyor.
//
// AĞA ÇIKMAZ, tıpkı Upload.cpp gibi. Bir isteğin doğru kurulup kurulmadığı ve
// bir yanıttan doğru bağlantının çıkıp çıkmadığı, sunucuya sormadan
// sınanabilecek sorular — testlerin yarısı tam olarak bu iki soruyu soruyor.
#include "Upload.h"

#include "UploadInternal.h"

#include <windows.h>

namespace crisp {

namespace {

// Servisler, arayüzün göstereceği sırayla.
//
// `id` AYAR DOSYASINA YAZILIR VE ASLA DEĞİŞMEZ. Bir servisin görünen adı
// değişebilir, alan adı el değiştirebilir; kimliği kullanıcının dosyasındaki
// seçimi bozmadan bunların hiçbirini takip edemez.
// TABLONUN SIRASI LİSTENİN SIRASIDIR. Anahtar istemeyenler önce, kendi
// içlerinde uzun ömürlüden kısaya. Anahtar isteyenlerin ilki `startsKeyGroup`
// taşır ve listede oraya bir ayraç çizilir.
constexpr UploadServiceInfo kServices[] = {
    {UploadService::None, L"none", L"—", L"", false, L"", 0, false},

    // --- Anahtar istemeyenler ------------------------------------------------
    {UploadService::Catbox, L"catbox", L"Catbox", L"catbox.moe", false, L"", 0, false},
    {UploadService::KappaLol, L"kappa", L"kappa.lol", L"kappa.lol", false, L"", 0,
     false},
    {UploadService::PoneRs, L"pone", L"pone.rs", L"pone.rs", false, L"", 0, false},
    {UploadService::ZeroXZero, L"0x0", L"0x0.st", L"0x0.st", false, L"", 720, false},
    {UploadService::QuAx, L"quax", L"qu.ax", L"qu.ax", false, L"", 720, false},
    {UploadService::Litterbox, L"litterbox", L"Litterbox", L"litterbox.catbox.moe",
     false, L"", 72, false},
    {UploadService::X0At, L"x0at", L"x0.at", L"x0.at", false, L"", 72, false},
    {UploadService::TempSh, L"tempsh", L"temp.sh", L"temp.sh", false, L"", 72, false},
    {UploadService::BashUpload, L"bashupload", L"bashupload.app", L"bashupload.app",
     false, L"", 24, false},
    {UploadService::Uguu, L"uguu", L"Uguu", L"uguu.se", false, L"", 3, false},

    // --- Anahtar isteyenler --------------------------------------------------
    {UploadService::Imgur, L"imgur", L"Imgur", L"api.imgur.com", true, L"Client ID", 0,
     true},
    {UploadService::ImgBB, L"imgbb", L"ImgBB", L"api.imgbb.com", true, L"API key", 0,
     false},
    {UploadService::FreeImage, L"freeimage", L"Freeimage.host", L"freeimage.host",
     true, L"API key", 0, false},
};

static_assert(sizeof(kServices) / sizeof(kServices[0]) ==
                  static_cast<size_t>(UploadService::Count),
              "her UploadService üyesinin tabloda tam bir satırı olmalı");

}  // namespace

const UploadServiceInfo* UploadServices(size_t& count) noexcept {
    count = sizeof(kServices) / sizeof(kServices[0]);
    return kServices;
}

const UploadServiceInfo& UploadServiceOf(UploadService service) noexcept {
    for (const UploadServiceInfo& info : kServices) {
        if (info.service == service) {
            return info;
        }
    }
    return kServices[0];
}

const wchar_t* UploadServiceId(UploadService service) noexcept {
    return UploadServiceOf(service).id;
}

UploadService UploadServiceFromId(const std::wstring& id) noexcept {
    for (const UploadServiceInfo& info : kServices) {
        if (id == info.id) {
            return info.service;
        }
    }
    // BİLİNMEYEN AD `None`'a DÖNER, hataya değil. Ayar dosyası ileri bir
    // sürümden gelmiş olabilir; tanımadığımız bir servis adı yüzünden açılmayı
    // reddetmek, kullanıcının dosyasını bizim anlamamamızı onun sorunu yapardı.
    return UploadService::None;
}

// ---------------------------------------------------------------------------
// Yanıttan bağlantı
// ---------------------------------------------------------------------------

std::wstring FirstHttpLine(const std::string& body) {
    size_t at = 0;
    while (at < body.size()) {
        size_t end = body.find('\n', at);
        if (end == std::string::npos) {
            end = body.size();
        }
        std::string line = body.substr(at, end - at);
        at = end + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
                                 line.back() == '\t')) {
            line.pop_back();
        }
        size_t begin = 0;
        while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t')) {
            ++begin;
        }
        line.erase(0, begin);

        if (line.rfind("https://", 0) == 0) {
            return Utf8ToWide(line);
        }
        if (line.rfind("http://", 0) == 0) {
            return Utf8ToWide("https://" + line.substr(7));
        }
    }
    return std::wstring();
}

std::wstring ExtractUploadLink(UploadService service, const std::string& body) {
    switch (service) {
        // Düz metin döndürenler: gövdenin tamamı adresin kendisi. Sondaki
        // satır sonu kırpılır, yoksa panoya kopyalanan bağlantının ucunda
        // görünmez bir karakter kalır.
        case UploadService::Catbox:
        case UploadService::Litterbox:
        case UploadService::Uguu:
        case UploadService::ZeroXZero:
        case UploadService::PoneRs:
        case UploadService::X0At:
        case UploadService::TempSh: {
            std::string trimmed = body;
            while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r' ||
                                        trimmed.back() == ' ' || trimmed.back() == '\t')) {
                trimmed.pop_back();
            }
            size_t begin = 0;
            while (begin < trimmed.size() && (trimmed[begin] == ' ' || trimmed[begin] == '\n' ||
                                              trimmed[begin] == '\r' || trimmed[begin] == '\t')) {
                ++begin;
            }
            trimmed.erase(0, begin);

            // SUNUCU HATA METNİ DE DÜZ METİNDİR. "http" ile başlamayan bir gövde
            // adres değil, hata iletisidir; bağlantı diye kopyalanmamalı.
            if (trimmed.rfind("http", 0) != 0) {
                return std::wstring();
            }
            return Utf8ToWide(trimmed);
        }

        // bashupload GÖVDEYE BAŞKA ŞEYLER DE YAZIYOR: boş satırlar, ardından
        // adres, ardından dosyanın ne zaman öleceğini anlatan iki dilli bir
        // uyarı. "Gövdenin tamamı adrestir" kuralı burada tutmuyor; adres,
        // http ile başlayan İLK SATIR.
        case UploadService::BashUpload:
            return FirstHttpLine(body);

        case UploadService::Imgur:
            return JsonFindString(body, "data.link");
        case UploadService::ImgBB:
            return JsonFindString(body, "data.url");
        case UploadService::FreeImage:
            return JsonFindString(body, "image.url");
        case UploadService::KappaLol:
            return JsonFindString(body, "link");
        // Yanıt `{"files":[{...,"url":"..."}],"success":true}`. Yol dizinin
        // İÇİNDEKİ ilk nesneye giriyor — tek dosya gönderdiğimiz için aradığımız
        // da o.
        case UploadService::QuAx:
            return JsonFindString(body, "files.url");

        default:
            return std::wstring();
    }
}

// ---------------------------------------------------------------------------
// İstek
// ---------------------------------------------------------------------------

UploadRequest BuildUploadRequest(UploadService service, const std::wstring& apiKey,
                                 const std::vector<unsigned char>& png,
                                 const std::wstring& fileName) {
    UploadRequest request;

    const UploadServiceInfo& info = UploadServiceOf(service);
    if (service == UploadService::None || service >= UploadService::Count) {
        request.error = L"no service";
        return request;
    }
    if (png.empty()) {
        request.error = L"empty image";
        return request;
    }
    // ANAHTAR YOKSA İSTEK HİÇ KURULMAZ. Imgur ve ImgBB'nin anonim yolları
    // kapandı; anahtarsız bir istek sunucudan anlamsız bir 403 ile dönerdi ve
    // kullanıcı sorunun kendi ayarında olduğunu anlamazdı.
    if (info.needsKey && apiKey.empty()) {
        request.error = L"missing key";
        return request;
    }

    const std::string boundary = MakeBoundary();
    const std::string name = WideToUtf8(fileName.empty() ? L"crisp.png" : fileName);
    const std::string key = WideToUtf8(apiKey);

    std::vector<std::pair<std::string, std::string>> fields;
    std::string fileField = "file";

    request.host = info.host;

    switch (service) {
        case UploadService::Catbox:
            request.path = L"/user/api.php";
            fields.emplace_back("reqtype", "fileupload");
            fileField = "fileToUpload";
            break;

        case UploadService::Litterbox:
            request.path = L"/resources/internals/api.php";
            fields.emplace_back("reqtype", "fileupload");
            // Servisin izin verdiği en uzun süre. Litterbox'ı seçen kullanıcı
            // zaten süreli olduğunu biliyor; içinden en kısasını seçmek, o
            // bilgiyi bir sürprize çevirirdi.
            fields.emplace_back("time", "72h");
            fileField = "fileToUpload";
            break;

        case UploadService::Uguu:
            // output=text: JSON yerine düz adres. Uguu'nun JSON şeması sürümler
            // arasında değişti, düz metin çıktısı değişmedi.
            request.path = L"/upload?output=text";
            fileField = "files[]";
            break;

        case UploadService::ZeroXZero:
            request.path = L"/";
            fileField = "file";
            break;

        case UploadService::TempSh:
            request.path = L"/upload";
            fileField = "file";
            break;

        // x0.at, 0x0.st'nin aynı arayüzü. `keep_name` GÖNDERİLMİYOR: gönderdiğimiz
        // her dosyanın adı zaten "crisp.png" ve servis uzantıyı kendiliğinden
        // koruyor, dolayısıyla adres yine .png ile bitiyor. Alanı eklemek, tek
        // kazancı her bağlantıda aynı kelimeyi görmek olurdu.
        case UploadService::X0At:
            request.path = L"/";
            fileField = "file";
            break;

        case UploadService::QuAx:
            request.path = L"/upload";
            fileField = "files[]";
            break;

        case UploadService::PoneRs:
            // Uguu ile aynı gerekçe: JSON yerine düz adres iste.
            request.path = L"/upload?output=text";
            fileField = "files[]";
            break;

        case UploadService::KappaLol:
            request.path = L"/api/upload";
            fileField = "file";
            break;

        // BAŞLIK OLMADAN DOSYA TEK KEZ İNDİRİLİR. bashupload'ın varsayılanı
        // "indir ve sil": bağlantıyı gönderdiğiniz kişi açtığında dosya ölür ve
        // ikinci kişi hiçbir şey bulamaz — bir ekran görüntüsü bağlantısı için
        // en kötü davranış. `X-Expiration-Seconds` verildiğinde dosya süre
        // boyunca defalarca indirilebiliyor. Sunucu 24 saatten fazlasını kendi
        // sınırına çekiyor, o yüzden doğrudan 24 saat isteniyor.
        case UploadService::BashUpload:
            // `curl -T dosya` ile aynı istek: PUT, gövde dosyanın kendisi,
            // dosya adı yolda. Multipart burada işe yaramıyor; gerekçesi
            // `UploadRequest::rawBody`ın yanında.
            request.path = L"/" + (fileName.empty() ? std::wstring(L"crisp.png")
                                                    : fileName);
            request.verb = L"PUT";
            request.rawBody = true;
            break;

        case UploadService::Imgur:
            request.path = L"/3/image";
            request.headers = L"Authorization: Client-ID " + apiKey;
            fileField = "image";
            break;

        case UploadService::ImgBB:
            // Anahtar SORGU DİZESİNDE DEĞİL, gövdede. İkisi de kabul ediliyor;
            // gövde, anahtarın bir yönlendirmede ya da bir günlükte görünme
            // ihtimalini ortadan kaldırıyor.
            request.path = L"/1/upload";
            fields.emplace_back("key", key);
            fileField = "image";
            break;

        case UploadService::FreeImage:
            request.path = L"/api/1/upload";
            fields.emplace_back("key", key);
            fields.emplace_back("format", "json");
            fileField = "source";
            break;

        default:
            request.error = L"no service";
            return request;
    }

    std::wstring contentType;
    if (request.rawBody) {
        request.body.assign(reinterpret_cast<const char*>(png.data()), png.size());
        contentType = L"Content-Type: image/png";
        // Bu başlık OLMADAN dosya TEK KEZ indirilir. bashupload'ın varsayılanı
        // "indir ve sil": bağlantıyı gönderdiğiniz kişi açtığında dosya ölür ve
        // ikinci kişi hiçbir şey bulamaz — bir ekran görüntüsü bağlantısı için
        // en kötü davranış. Süre verildiğinde dosya o süre boyunca defalarca
        // indirilebiliyor; sunucu 24 saatten fazlasını kendi sınırına çektiği
        // için doğrudan 24 saat isteniyor.
        contentType += L"\r\nX-Expiration-Seconds: 86400";
    } else {
        request.body =
            BuildMultipartBody(fields, fileField, name, "image/png", png, boundary);
        contentType = L"Content-Type: multipart/form-data; boundary=";
        contentType += Utf8ToWide(boundary);
    }

    if (request.headers.empty()) {
        request.headers = contentType;
    } else {
        request.headers += L"\r\n";
        request.headers += contentType;
    }

    request.valid = true;
    return request;
}

}  // namespace crisp
