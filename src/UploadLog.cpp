// UploadLog.cpp — Yüklenen bağlantıların düz metin defteri.
//
// Dosya UTF-8 ve BOM'suz: Not Defteri, VS Code ve `type` hepsi okuyor, ve BOM
// eklemek elle düzenlenen bir dosyanın ilk satırına görünmez bir karakter
// koymak olurdu.
#include "UploadLog.h"

#include "UploadInternal.h"
#include "Util.h"

#include <shlobj.h>
#include <windows.h>

#include <cstdio>
#include <cstdlib>

namespace crisp {
namespace {

constexpr wchar_t kSeparator = L'\t';

[[nodiscard]] long long NowUnixSeconds() noexcept {
    FILETIME now{};
    ::GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER value{};
    value.LowPart = now.dwLowDateTime;
    value.HighPart = now.dwHighDateTime;
    // 1601 → 1970: 11644473600 saniye, ve FILETIME 100 ns birimlerinde.
    return static_cast<long long>(value.QuadPart / 10000000ULL) - 11644473600LL;
}

}  // namespace

std::wstring UploadLogPath() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
        return std::wstring();
    }
    std::wstring path(raw);
    ::CoTaskMemFree(raw);
    path += L"\\Crisp\\uploads.txt";
    return path;
}

std::wstring FormatUploadLine(const UploadRecord& record) {
    wchar_t stamp[32] = {};
    ::swprintf_s(stamp, L"%lld", record.when);

    std::wstring line = stamp;
    line += kSeparator;
    line += record.service;
    line += kSeparator;
    line += record.link;
    return line;
}

bool ParseUploadLine(const std::wstring& line, UploadRecord& out) {
    const size_t first = line.find(kSeparator);
    if (first == std::wstring::npos) {
        return false;
    }
    const size_t second = line.find(kSeparator, first + 1);
    if (second == std::wstring::npos) {
        return false;
    }

    const std::wstring stamp = line.substr(0, first);
    const std::wstring service = line.substr(first + 1, second - first - 1);
    std::wstring link = line.substr(second + 1);

    // Satır sonu ve olası CR kırpılır; dosya bir kez CRLF ile düzenlenmiş
    // olabilir ve bağlantının ucunda görünmez bir karakter kalmamalı.
    while (!link.empty() && (link.back() == L'\r' || link.back() == L'\n' ||
                             link.back() == L' ' || link.back() == L'\t')) {
        link.pop_back();
    }

    // BAĞLANTI OLMAYAN SATIR KAYIT DEĞİLDİR. Dosya elle düzenlenebilir; oraya
    // yazılmış bir not, menüde tıklanabilir bir bağlantı gibi görünmemeli.
    if (link.rfind(L"http", 0) != 0) {
        return false;
    }

    out.when = ::_wtoi64(stamp.c_str());
    out.service = service;
    out.link = std::move(link);
    return true;
}

std::vector<UploadRecord> ReadUploadLog(size_t limit) {
    std::vector<UploadRecord> records;
    const std::wstring path = UploadLogPath();
    if (path.empty()) {
        return records;
    }

    FILE* file = nullptr;
    if (::_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) {
        return records;   // dosya yok: ilk çalıştırma
    }

    std::string bytes;
    char buffer[4096];
    size_t read = 0;
    while ((read = ::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bytes.append(buffer, read);
    }
    ::fclose(file);

    const std::wstring text = Utf8ToWide(bytes);
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find(L'\n', start);
        const std::wstring line =
            text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        UploadRecord record;
        if (ParseUploadLine(line, record)) {
            records.push_back(std::move(record));
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }

    // EN YENİ ÖNCE. Dosya sona ekleniyor, menü ise en son yükleneni en üstte
    // istiyor.
    for (size_t i = 0, j = records.size(); i + 1 < j; ++i, --j) {
        std::swap(records[i], records[j - 1]);
    }
    if (records.size() > limit) {
        records.resize(limit);
    }
    return records;
}

bool AppendUploadRecord(const UploadRecord& record, size_t limit) {
    if (record.link.empty()) {
        return false;
    }

    UploadRecord stamped = record;
    if (stamped.when == 0) {
        stamped.when = NowUnixSeconds();
    }

    // DOSYA HER SEFERİNDE BAŞTAN YAZILIR, EKLENMEZ. Sona eklemek daha ucuz
    // olurdu ama budama yine de tam bir okuma-yazma gerektiriyor, ve iki ayrı
    // yol tutmak "bazen budanmış bazen budanmamış" bir dosya demekti.
    std::vector<UploadRecord> records = ReadUploadLog(limit * 2);
    records.insert(records.begin(), std::move(stamped));
    if (records.size() > limit) {
        records.resize(limit);
    }

    const std::wstring path = UploadLogPath();
    if (path.empty()) {
        return false;
    }
    const size_t slash = path.find_last_of(L'\\');
    if (slash != std::wstring::npos && !EnsureDirectory(path.substr(0, slash))) {
        return false;
    }

    // Dosyaya en ESKİ önce yazılır; okuma tarafı ters çeviriyor ve bu sıra,
    // dosyayı elle açan birinin beklediği kronolojik sıra.
    std::wstring text;
    for (size_t i = records.size(); i > 0; --i) {
        text += FormatUploadLine(records[i - 1]);
        text += L"\r\n";
    }

    const std::string bytes = WideToUtf8(text);
    FILE* file = nullptr;
    if (::_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr) {
        LogV(L"Yükleme defteri yazılamadı: %s", path.c_str());
        return false;
    }
    const size_t written = ::fwrite(bytes.data(), 1, bytes.size(), file);
    ::fclose(file);
    return written == bytes.size();
}

bool ClearUploadLog() {
    const std::wstring path = UploadLogPath();
    if (path.empty()) {
        return false;
    }
    if (::DeleteFileW(path.c_str()) != FALSE) {
        return true;
    }
    return ::GetLastError() == ERROR_FILE_NOT_FOUND;
}

}  // namespace crisp
