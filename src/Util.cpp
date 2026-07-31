// Util.cpp — Util.h'deki serbest fonksiyonlar.
#include "Util.h"

#include <shlwapi.h>

#include <cstdarg>
#include <cstdio>

namespace crisp {

void LogV(const wchar_t* format, ...) {
    wchar_t buffer[512];

    va_list args;
    va_start(args, format);
    // _vsnwprintf_s taşmada kesip DAİMA sonlandırır; dönüş değeri -1 olsa bile
    // tampon geçerli bir dizedir, bu yüzden ayrıca kontrol edilmez.
    const int written = ::_vsnwprintf_s(buffer, _TRUNCATE, format, args);
    va_end(args);

    if (written < 0 && buffer[0] == L'\0') {
        return;
    }

    ::OutputDebugStringW(L"[Crisp] ");
    ::OutputDebugStringW(buffer);
    ::OutputDebugStringW(L"\n");
}

std::wstring ModulePath() {
    // MAX_PATH yetmeyebilir; uzun yol desteği açık sistemlerde tampon büyütülür.
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD written = ::GetModuleFileNameW(
            nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (written == 0) {
            return std::wstring();
        }
        if (written < path.size()) {
            path.resize(written);
            return path;
        }
        if (path.size() >= 32768) {
            return std::wstring();
        }
        path.resize(path.size() * 2);
    }
}

std::wstring ModuleDirectory() {
    std::wstring path = ModulePath();
    const size_t slash = path.find_last_of(L'\\');
    if (slash == std::wstring::npos) {
        return std::wstring();
    }
    path.resize(slash);
    return path;
}

std::wstring TimestampForFileName() {
    SYSTEMTIME st{};
    ::GetLocalTime(&st);

    wchar_t buffer[32];
    const int written = ::swprintf_s(buffer, L"%04u-%02u-%02u %02u-%02u-%02u",
                                     st.wYear, st.wMonth, st.wDay, st.wHour,
                                     st.wMinute, st.wSecond);
    if (written <= 0) {
        return std::wstring(L"capture");
    }
    return std::wstring(buffer);
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }

    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    // Üst dizin yoksa önce o oluşturulur; özyineleme derinliği yol
    // bileşeni sayısı kadardır, pratikte onlarca değil birkaç seviye.
    const size_t slash = path.find_last_of(L'\\');
    if (slash != std::wstring::npos && slash > 2) {
        if (!EnsureDirectory(path.substr(0, slash))) {
            return false;
        }
    }

    if (::CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    // Araya başka biri girip oluşturmuş olabilir; bu bir hata değil.
    return ::GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring MakeUniquePath(const std::wstring& desired) {
    if (::GetFileAttributesW(desired.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return desired;
    }

    // Uzantı, SON noktadan itibaren alınır ve yalnızca son dizin ayracından
    // sonra aranır: "C:\a.b\dosya" gibi yollarda dizin adındaki nokta uzantı
    // sanılmamalı.
    const size_t slash = desired.find_last_of(L'\\');
    const size_t searchFrom = (slash == std::wstring::npos) ? 0 : slash + 1;
    const size_t dot = desired.find_last_of(L'.');

    std::wstring stem;
    std::wstring extension;
    if (dot != std::wstring::npos && dot > searchFrom) {
        stem = desired.substr(0, dot);
        extension = desired.substr(dot);
    } else {
        stem = desired;
    }

    for (int index = 2; index < 1000; ++index) {
        std::wstring candidate = stem;
        candidate += L" (";
        candidate += std::to_wstring(index);
        candidate += L')';
        candidate += extension;

        if (::GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) {
            return candidate;
        }
    }

    // 999 çakışma: çağıran yine de yazmayı denesin, üzerine yazmak sessizce
    // vazgeçmekten iyidir.
    return desired;
}

}  // namespace crisp
