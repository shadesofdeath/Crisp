// Upload.cpp — Yüklemenin hiçbir servise özel olmayan yarısı.
//
// multipart zarfı, JSON okuma ve UTF-8 dönüşümü. Hangi servisin nereye ne
// gönderdiği UploadServices.cpp'de, göndermenin kendisi UploadHttp.cpp'de.
//
// AĞA HİÇ ÇIKMAZ ve tam olarak bu yüzden ayrı duruyor: buradaki her şey bir
// sunucuya bağlanmadan sınanabiliyor, ve bir yükleme özelliğinin güvenilirliği
// büyük ölçüde "isteği doğru kuruyor muyuz, yanıtı doğru okuyor muyuz"
// sorularının cevabı.
#include "Upload.h"

#include "UploadInternal.h"

#include <windows.h>

namespace crisp {

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

}  // namespace crisp
