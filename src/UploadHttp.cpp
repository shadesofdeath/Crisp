// UploadHttp.cpp — Yüklemenin ağ yarısı, WinHTTP ile.
//
// TAMAMI HTTPS. Hiçbir servise düz HTTP ile bağlanılmıyor ve bağlanılamaz:
// yüklenen şey kullanıcının ekranı ve yoldaki bir ara sunucunun onu okuyabilmesi
// kabul edilebilir değil. Sertifika doğrulaması da GEVŞETİLMİYOR — birçok örnek
// kodda görülen SECURITY_FLAG_IGNORE_* bayrakları burada yok; sertifikası
// doğrulanamayan bir sunucuya görüntü gönderilmez, hata döner.
//
// WinHTTP seçildi, WinINet değil: WinINet kullanıcı arayüzü olan bir süreç
// varsayar ve arka plan iş parçacığından kullanımı desteklenmiyor. Bu istek
// tam olarak arka plan iş parçacığından yapılıyor.
#include "UploadInternal.h"

#include "Util.h"

#include <windows.h>
#include <winhttp.h>

namespace crisp {
namespace {

// Kullanıcı aracısı. 0x0.st gibi servisler otomatik araçlardan kendilerini
// tanıtmalarını istiyor ve tanıtmayanları engelliyor; ayrıca bir sorun çıkarsa
// sunucu tarafında kimin yaptığı görülebilsin.
constexpr wchar_t kUserAgent[] = L"Crisp/1.0 (+https://github.com/shadesofdeath/Crisp)";

// Yanıt için kabul edilen en büyük gövde. Beklenen gövde birkaç yüz bayt; bu
// sınır, hatalı ya da kötü niyetli bir sunucunun belleği tüketmesini engelliyor.
constexpr DWORD kMaxResponse = 1u << 20;   // 1 MiB

// WinHTTP tutamacı — kapsamdan çıkınca kapanır. Bu dosyada dört farklı hata
// yolu var ve her birinde üç tutamacı elle kapatmak, birini unutmanın yolu.
class Handle {
public:
    Handle() = default;
    explicit Handle(HINTERNET handle) noexcept : m_handle(handle) {}
    ~Handle() {
        if (m_handle != nullptr) {
            ::WinHttpCloseHandle(m_handle);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle& operator=(HINTERNET handle) noexcept {
        if (m_handle != nullptr) {
            ::WinHttpCloseHandle(m_handle);
        }
        m_handle = handle;
        return *this;
    }

    [[nodiscard]] HINTERNET get() const noexcept { return m_handle; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_handle != nullptr; }

private:
    HINTERNET m_handle = nullptr;
};

// Sunucunun döndürdüğü durum koduna göre, kullanıcıya gösterilebilecek bir
// cümle. Kod da yazılıyor: destek isteyen kullanıcının elinde bir tutamak olsun.
[[nodiscard]] std::wstring StatusMessage(DWORD status) {
    wchar_t buffer[160] = {};
    if (status == 401 || status == 403) {
        ::swprintf_s(buffer, L"Servis isteği reddetti (%lu). API anahtarı yanlış olabilir.",
                     status);
    } else if (status == 413) {
        ::swprintf_s(buffer, L"Görüntü servisin sınırından büyük (%lu).", status);
    } else if (status == 429) {
        ::swprintf_s(buffer, L"Çok fazla istek gönderildi (%lu). Biraz bekleyin.", status);
    } else if (status >= 500) {
        ::swprintf_s(buffer, L"Servis şu an yanıt veremiyor (%lu).", status);
    } else {
        ::swprintf_s(buffer, L"Servis beklenmeyen bir yanıt verdi (%lu).", status);
    }
    return buffer;
}

}  // namespace

UploadResult UploadPng(UploadService service, const std::wstring& apiKey,
                       const std::vector<unsigned char>& png,
                       const std::wstring& fileName) {
    UploadResult result;

    const UploadRequest request = BuildUploadRequest(service, apiKey, png, fileName);
    if (!request.valid) {
        // BuildUploadRequest'in hataları çağıranın ayarıyla ilgili, ağla değil.
        if (request.error == L"missing key") {
            result.error = L"Bu servis bir API anahtarı istiyor. Ayarlar › Yükleme.";
        } else if (request.error == L"empty image") {
            result.error = L"Gönderilecek görüntü yok.";
        } else {
            result.error = L"Yükleme servisi seçilmemiş. Ayarlar › Yükleme.";
        }
        return result;
    }

    Handle session(::WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        result.error = L"Ağ başlatılamadı.";
        return result;
    }

    // Zaman aşımları AÇIKÇA veriliyor. WinHTTP'nin varsayılan alma zaman aşımı
    // 30 saniye ve yanıt vermeyen bir sunucuda kullanıcı yarım dakika boyunca
    // ne olduğunu bilmiyor. Yükleme, kullanıcının beklediği bir işlem; bekleme
    // bir yerde bitmeli.
    ::WinHttpSetTimeouts(session.get(), 10000, 10000, 20000, 60000);

    Handle connection(::WinHttpConnect(session.get(), request.host.c_str(),
                                       INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) {
        result.error = L"Servise bağlanılamadı: " + request.host;
        return result;
    }

    Handle http(::WinHttpOpenRequest(connection.get(), L"POST", request.path.c_str(),
                                     nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!http) {
        result.error = L"İstek oluşturulamadı.";
        return result;
    }

    if (::WinHttpSendRequest(http.get(), request.headers.c_str(),
                             static_cast<DWORD>(-1),
                             const_cast<char*>(request.body.data()),
                             static_cast<DWORD>(request.body.size()),
                             static_cast<DWORD>(request.body.size()), 0) == FALSE) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_WINHTTP_SECURE_FAILURE) {
            result.error = L"Güvenli bağlantı doğrulanamadı.";
        } else if (error == ERROR_WINHTTP_TIMEOUT) {
            result.error = L"Bağlantı zaman aşımına uğradı.";
        } else if (error == ERROR_WINHTTP_NAME_NOT_RESOLVED) {
            result.error = L"Adres çözümlenemedi: " + request.host;
        } else {
            wchar_t buffer[96] = {};
            ::swprintf_s(buffer, L"Gönderilemedi (hata %lu).", error);
            result.error = buffer;
        }
        return result;
    }

    if (::WinHttpReceiveResponse(http.get(), nullptr) == FALSE) {
        result.error = L"Servisten yanıt alınamadı.";
        return result;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    ::WinHttpQueryHeaders(http.get(),
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                          WINHTTP_NO_HEADER_INDEX);

    std::string body;
    for (;;) {
        DWORD available = 0;
        if (::WinHttpQueryDataAvailable(http.get(), &available) == FALSE || available == 0) {
            break;
        }
        if (body.size() + available > kMaxResponse) {
            available = static_cast<DWORD>(kMaxResponse - body.size());
            if (available == 0) {
                break;
            }
        }
        const size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (::WinHttpReadData(http.get(), body.data() + offset, available, &read) == FALSE) {
            body.resize(offset);
            break;
        }
        body.resize(offset + read);
        if (read == 0) {
            break;
        }
    }

    // DURUM KODU ÖNCE OKUNUR AMA GÖVDE DE OKUNUR. Bazı servisler 200 dönüp
    // gövdede hata yazıyor, bazıları hata kodu dönüp gövdede sebebi veriyor;
    // ikisine de bakmadan doğru cümleyi kuramayız.
    if (status < 200 || status >= 300) {
        result.error = StatusMessage(status);
        return result;
    }

    result.link = ExtractUploadLink(service, body);
    if (result.link.empty()) {
        // 200 geldi ama bağlantı çıkarılamadı. Bu, servisin yanıt biçimini
        // değiştirdiği anlamına gelebilir; gövdenin başı günlüğe yazılıyor ki
        // sorun bildiren kullanıcıdan istenebilsin.
        LogV(L"Yükleme: %s yanıtı ayrıştırılamadı: %.200hs",
             UploadServiceOf(service).displayName, body.c_str());
        result.error = L"Servis yanıt verdi ama bağlantı okunamadı.";
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace crisp
