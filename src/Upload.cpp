// Upload.cpp — Servis tablosu ve ağ gerektirmeyen yarısı.
//
// İsteği KURMAK ve yanıtı OKUMAK burada; göndermek UploadHttp.cpp'de. Ayrım
// keyfi değil: hata ihtimalinin neredeyse tamamı bu dosyada ve bu dosyanın
// tamamı ağa çıkmadan sınanabiliyor. Bir sunucuya bağlanmadan "Imgur yanıtından
// bağlantıyı doğru çıkarıyor muyuz" sorusunu cevaplayabilmek, bu özelliğin
// güvenilirliğinin büyük kısmı.
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
constexpr UploadServiceInfo kServices[] = {
    {UploadService::None, L"none", L"—", L"", false, L"", 0},

    {UploadService::Catbox, L"catbox", L"Catbox", L"catbox.moe", false, L"", 0},
    {UploadService::Litterbox, L"litterbox", L"Litterbox", L"litterbox.catbox.moe",
     false, L"", 72},
    {UploadService::Uguu, L"uguu", L"Uguu", L"uguu.se", false, L"", 3},
    {UploadService::ZeroXZero, L"0x0", L"0x0.st", L"0x0.st", false, L"", 720},

    {UploadService::Imgur, L"imgur", L"Imgur", L"api.imgur.com", true, L"Client ID", 0},
    {UploadService::ImgBB, L"imgbb", L"ImgBB", L"api.imgbb.com", true, L"API key", 0},
    {UploadService::FreeImage, L"freeimage", L"Freeimage.host", L"freeimage.host",
     true, L"API key", 0},
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
// multipart/form-data
// ---------------------------------------------------------------------------

std::string BuildMultipartBody(
    const std::vector<std::pair<std::string, std::string>>& fields,
    const std::string& fileField, const std::string& fileName,
    const std::string& contentType, const std::vector<unsigned char>& fileBytes,
    const std::string& boundary) {
    std::string body;
    // Kabaca doğru bir ilk ayırma: gövdenin neredeyse tamamı dosya.
    body.reserve(fileBytes.size() + 512 + fields.size() * 128);

    for (const auto& field : fields) {
        body += "--";
        body += boundary;
        body += "\r\nContent-Disposition: form-data; name=\"";
        body += field.first;
        body += "\"\r\n\r\n";
        body += field.second;
        body += "\r\n";
    }

    // DOSYA EN SONDA. Bazı sunucular alanları akış hâlinde okuyor ve dosyayı
    // gördüğünde önceki alanların hepsinin gelmiş olmasını bekliyor — Chevereto
    // tabanlı freeimage.host'ta `key` dosyadan sonra gelirse istek reddediliyor.
    body += "--";
    body += boundary;
    body += "\r\nContent-Disposition: form-data; name=\"";
    body += fileField;
    body += "\"; filename=\"";
    body += fileName;
    body += "\"\r\nContent-Type: ";
    body += contentType;
    body += "\r\n\r\n";
    body.append(reinterpret_cast<const char*>(fileBytes.data()), fileBytes.size());
    body += "\r\n--";
    body += boundary;
    body += "--\r\n";

    return body;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

namespace {

// `json[at]` bir dizenin açılış tırnağındayken, dizeyi okur ve `at`'i kapanış
// tırnağının bir sonrasına taşır. Kaçış dizileri çözülür.
[[nodiscard]] std::string ReadJsonString(const std::string& json, size_t& at) {
    std::string out;
    ++at;   // açılış tırnağı
    while (at < json.size()) {
        const char c = json[at];
        if (c == '"') {
            ++at;
            return out;
        }
        if (c == '\\' && at + 1 < json.size()) {
            const char next = json[at + 1];
            switch (next) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                // \uXXXX ATLANMIYOR AMA ÇÖZÜLMÜYOR DA: aradığımız değerler URL
                // ve URL'de \u kaçışı olmaz. Onları olduğu gibi bırakmak, yanlış
                // çözmekten iyi.
                default:  out += next; break;
            }
            at += 2;
            continue;
        }
        out += c;
        ++at;
    }
    return out;   // kapanmamış dize: elimizdekini veririz
}

// `at`'ten başlayan değeri atlar. Dizeleri ve iç içe geçmeyi sayar.
void SkipJsonValue(const std::string& json, size_t& at) {
    while (at < json.size() && (json[at] == ' ' || json[at] == '\n' ||
                                json[at] == '\r' || json[at] == '\t')) {
        ++at;
    }
    if (at >= json.size()) {
        return;
    }
    if (json[at] == '"') {
        (void)ReadJsonString(json, at);
        return;
    }
    if (json[at] == '{' || json[at] == '[') {
        int depth = 0;
        while (at < json.size()) {
            const char c = json[at];
            if (c == '"') {
                (void)ReadJsonString(json, at);
                continue;
            }
            if (c == '{' || c == '[') {
                ++depth;
            } else if (c == '}' || c == ']') {
                --depth;
                if (depth == 0) {
                    ++at;
                    return;
                }
            }
            ++at;
        }
        return;
    }
    // Sayı, true/false/null: bir sonraki ayraca kadar.
    while (at < json.size() && json[at] != ',' && json[at] != '}' && json[at] != ']') {
        ++at;
    }
}

// Bir nesnenin İÇİNDE, verilen adlı anahtarı bulur ve `at`'i değerinin başına
// taşır. Yalnızca O SEVİYEDEKİ anahtarlara bakar — iç içe bir nesnedeki aynı
// adlı anahtar bulunmaz, ki `data.url` ile `data.thumb.url` karışmasın.
[[nodiscard]] bool EnterObjectKey(const std::string& json, size_t& at,
                                  const std::string& key) {
    while (at < json.size() && json[at] != '{') {
        ++at;
    }
    if (at >= json.size()) {
        return false;
    }
    ++at;   // '{'

    while (at < json.size()) {
        while (at < json.size() && json[at] != '"' && json[at] != '}') {
            ++at;
        }
        if (at >= json.size() || json[at] == '}') {
            return false;
        }
        const std::string name = ReadJsonString(json, at);
        while (at < json.size() && json[at] != ':') {
            ++at;
        }
        if (at >= json.size()) {
            return false;
        }
        ++at;   // ':'
        while (at < json.size() && (json[at] == ' ' || json[at] == '\n' ||
                                    json[at] == '\r' || json[at] == '\t')) {
            ++at;
        }
        if (name == key) {
            return true;
        }
        SkipJsonValue(json, at);
    }
    return false;
}

}  // namespace

std::wstring JsonFindString(const std::string& json, const std::string& dottedPath) {
    size_t at = 0;
    size_t start = 0;

    while (start <= dottedPath.size()) {
        const size_t dot = dottedPath.find('.', start);
        const std::string part = dottedPath.substr(
            start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!EnterObjectKey(json, at, part)) {
            return std::wstring();
        }
        if (dot == std::string::npos) {
            if (at >= json.size() || json[at] != '"') {
                return std::wstring();   // yol bir dizede bitmiyor
            }
            return Utf8ToWide(ReadJsonString(json, at));
        }
        start = dot + 1;
    }
    return std::wstring();
}

// ---------------------------------------------------------------------------
// Yanıttan bağlantı
// ---------------------------------------------------------------------------

std::wstring ExtractUploadLink(UploadService service, const std::string& body) {
    switch (service) {
        // Anahtarsız dördü düz metin döndürüyor: gövdenin tamamı adresin
        // kendisi. Sondaki satır sonu kırpılır, yoksa panoya kopyalanan
        // bağlantının ucunda görünmez bir karakter kalır.
        case UploadService::Catbox:
        case UploadService::Litterbox:
        case UploadService::Uguu:
        case UploadService::ZeroXZero: {
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

        case UploadService::Imgur:
            return JsonFindString(body, "data.link");
        case UploadService::ImgBB:
            return JsonFindString(body, "data.url");
        case UploadService::FreeImage:
            return JsonFindString(body, "image.url");

        default:
            return std::wstring();
    }
}

// ---------------------------------------------------------------------------
// Kodlama
// ---------------------------------------------------------------------------

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return std::string();
    }
    std::string out(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                          out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                             static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                          out.data(), needed);
    return out;
}

std::string MakeBoundary() {
    // Zaman + sayaçtan türetilir. Kriptografik olması gerekmiyor; gereken tek
    // şey, gövdedeki PNG baytlarının içinde geçmemesi — ve "----CrispBoundary"
    // öneki zaten bir PNG'de bulunmayacak bir dizi.
    static unsigned counter = 0;
    LARGE_INTEGER now{};
    ::QueryPerformanceCounter(&now);

    char tail[40] = {};
    ::sprintf_s(tail, "%016llx%08x",
                static_cast<unsigned long long>(now.QuadPart), ++counter);

    std::string boundary = "----CrispBoundary";
    boundary += tail;
    return boundary;
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

    request.body = BuildMultipartBody(fields, fileField, name, "image/png", png, boundary);

    std::wstring contentType = L"Content-Type: multipart/form-data; boundary=";
    contentType += Utf8ToWide(boundary);
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
