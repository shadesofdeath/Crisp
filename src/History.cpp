// History.cpp — bkz. History.h.
#include "History.h"

#include "ImageCodec.h"
#include "Util.h"

#include <algorithm>

#include <shlobj.h>

namespace crisp {
namespace {

constexpr const wchar_t* kExtension = L".png";

[[nodiscard]] uint64_t AsInteger(const FILETIME& time) noexcept {
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

// En yeni önce. Aynı saniyede iki yakalama olabildiği için eşitlikte dosya
// adına düşülür: adlar zaman damgası taşır ve MakeUniquePath bir sayaç ekler,
// bu yüzden ada göre sıralamak da doğru sırayı verir. Eşitliği çözmezsek
// std::sort'un kararsızlığı listeyi her açılışta farklı sıralayabilirdi.
[[nodiscard]] bool Newer(const HistoryEntry& a, const HistoryEntry& b) noexcept {
    const uint64_t left = AsInteger(a.written);
    const uint64_t right = AsInteger(b.written);
    if (left != right) {
        return left > right;
    }
    return a.name > b.name;
}

[[nodiscard]] bool EndsWithPng(const wchar_t* name) noexcept {
    const size_t length = ::wcslen(name);
    const size_t suffix = ::wcslen(kExtension);
    if (length <= suffix) {
        return false;
    }
    return ::_wcsicmp(name + length - suffix, kExtension) == 0;
}

}  // namespace

std::wstring HistoryStore::DefaultFolder() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw))) {
        return std::wstring();
    }
    std::wstring folder(raw);
    ::CoTaskMemFree(raw);
    folder += L"\\Crisp\\History";
    return folder;
}

bool HistoryStore::Record(const Image& image, std::wstring& writtenPath) {
    if (!image.Valid() || m_folder.empty()) {
        return false;
    }
    if (!EnsureDirectory(m_folder)) {
        LogV(L"Geçmiş klasörü oluşturulamadı: %s", m_folder.c_str());
        return false;
    }

    std::wstring path = m_folder;
    path += L'\\';
    path += TimestampForFileName();
    path += kExtension;
    path = MakeUniquePath(path);

    // GEÇMİŞ HER ZAMAN PNG: kullanıcının seçtiği kayıt biçimi JPEG olsa bile
    // geçmiş kaydı kayıpsız kalmalı. Kullanıcı geçmişten bir görüntüyü tekrar
    // düzenlerse, sıkıştırma hatası üstüne sıkıştırma hatası binmesin.
    if (!SavePng(image, path)) {
        LogV(L"Geçmişe yazılamadı: %s", path.c_str());
        return false;
    }

    writtenPath = path;
    Prune(m_limit);
    return true;
}

std::vector<HistoryEntry> HistoryStore::List() const {
    std::vector<HistoryEntry> entries;
    if (m_folder.empty()) {
        return entries;
    }

    std::wstring pattern = m_folder;
    pattern += L"\\*";
    pattern += kExtension;

    WIN32_FIND_DATAW found{};
    const HANDLE search = ::FindFirstFileW(pattern.c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) {
        return entries;   // klasör henüz yok ya da boş
    }

    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (!EndsWithPng(found.cFileName)) {
            continue;   // "*.png" kalıbı 8.3 adlar yüzünden fazlasını getirebilir
        }

        HistoryEntry entry;
        entry.path = m_folder;
        entry.path += L'\\';
        entry.path += found.cFileName;
        entry.name.assign(found.cFileName);
        const size_t dot = entry.name.rfind(L'.');
        if (dot != std::wstring::npos) {
            entry.name.erase(dot);
        }
        entry.written = found.ftLastWriteTime;
        ULARGE_INTEGER size;
        size.LowPart = found.nFileSizeLow;
        size.HighPart = found.nFileSizeHigh;
        entry.bytes = size.QuadPart;
        entries.push_back(std::move(entry));
    } while (::FindNextFileW(search, &found) != FALSE);

    ::FindClose(search);
    std::sort(entries.begin(), entries.end(), Newer);
    return entries;
}

size_t HistoryStore::Prune(size_t keep) {
    const std::vector<HistoryEntry> entries = List();
    if (entries.size() <= keep) {
        return 0;
    }

    size_t deleted = 0;
    for (size_t i = keep; i < entries.size(); ++i) {
        if (::DeleteFileW(entries[i].path.c_str()) != FALSE) {
            ++deleted;
        } else {
            LogV(L"Geçmiş kaydı silinemedi: %s", entries[i].path.c_str());
        }
    }
    return deleted;
}

bool HistoryStore::Remove(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    return ::DeleteFileW(path.c_str()) != FALSE;
}

size_t HistoryStore::Clear() {
    return Prune(0);
}

}  // namespace crisp
