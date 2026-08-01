// NameFormat.cpp — bkz. NameFormat.h.
#include "NameFormat.h"

#include <lmcons.h>

#include <cstdio>

namespace crisp {
namespace {

constexpr const wchar_t kAlphabet[] = L"abcdefghijklmnopqrstuvwxyz0123456789";

void AppendNumber(std::wstring& out, int value, int digits) {
    wchar_t text[16];
    ::swprintf_s(text, L"%0*d", digits, value);
    out += text;
}

[[nodiscard]] std::wstring RandomText(unsigned seed, int length) {
    // KENDİ ÜRETECİ: rand() sürece göre değişir ve aynı tohumla aynı sonucu
    // vermesi garanti değil; bu fonksiyonun sınanabilir olması gerekiyor.
    std::wstring text;
    unsigned state = seed * 1664525u + 1013904223u;
    for (int i = 0; i < length; ++i) {
        state = state * 1664525u + 1013904223u;
        text.push_back(kAlphabet[(state >> 16) % (sizeof(kAlphabet) / sizeof(wchar_t) - 1)]);
    }
    return text;
}

[[nodiscard]] std::wstring UserName() {
    wchar_t buffer[UNLEN + 1] = L"";
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (!::GetUserNameW(buffer, &size)) {
        return std::wstring();
    }
    return std::wstring(buffer);
}

[[nodiscard]] std::wstring ComputerName() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1] = L"";
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (!::GetComputerNameW(buffer, &size)) {
        return std::wstring();
    }
    return std::wstring(buffer);
}

// Belirteç eşleşmesi. UZUN OLAN ÖNCE denenmeli: "%mo" ile "%m" aynı harfle
// başlıyor ve kısa olan önce denenirse "%mo" hiçbir zaman eşleşmez.
struct Token {
    const wchar_t* name;
    int length;
};

constexpr Token kTokens[] = {
    {L"%yy", 3}, {L"%y", 2},  {L"%mo", 3}, {L"%mi", 3}, {L"%d", 2},
    {L"%h", 2},  {L"%s", 2},  {L"%pn", 3}, {L"%ra", 3}, {L"%un", 3},
    {L"%cn", 3}, {L"%px", 3}, {L"%py", 3}, {L"%i", 2},  {L"%%", 2},
};

}  // namespace

std::wstring ExpandNameFormat(const std::wstring& format,
                              const NameContext& context) {
    std::wstring out;
    out.reserve(format.size() + 32);

    size_t i = 0;
    while (i < format.size()) {
        if (format[i] != L'%') {
            out.push_back(format[i]);
            ++i;
            continue;
        }

        const Token* matched = nullptr;
        for (const Token& token : kTokens) {
            if (format.compare(i, static_cast<size_t>(token.length), token.name) == 0) {
                matched = &token;
                break;
            }
        }
        if (matched == nullptr) {
            // Bilinmeyen belirteç: olduğu gibi geçer ve kullanıcı yazım
            // hatasını sonuçta görür.
            out.push_back(format[i]);
            ++i;
            continue;
        }

        const std::wstring name(matched->name);
        if (name == L"%y") {
            AppendNumber(out, context.time.wYear, 4);
        } else if (name == L"%yy") {
            AppendNumber(out, context.time.wYear % 100, 2);
        } else if (name == L"%mo") {
            AppendNumber(out, context.time.wMonth, 2);
        } else if (name == L"%d") {
            AppendNumber(out, context.time.wDay, 2);
        } else if (name == L"%h") {
            AppendNumber(out, context.time.wHour, 2);
        } else if (name == L"%mi") {
            AppendNumber(out, context.time.wMinute, 2);
        } else if (name == L"%s") {
            AppendNumber(out, context.time.wSecond, 2);
        } else if (name == L"%px") {
            AppendNumber(out, context.width, 1);
        } else if (name == L"%py") {
            AppendNumber(out, context.height, 1);
        } else if (name == L"%i") {
            AppendNumber(out, static_cast<int>(context.counter), 4);
        } else if (name == L"%ra") {
            out += RandomText(context.random, 6);
        } else if (name == L"%pn") {
            out += context.windowTitle;
        } else if (name == L"%un") {
            out += UserName();
        } else if (name == L"%cn") {
            out += ComputerName();
        } else if (name == L"%%") {
            out.push_back(L'%');
        }
        i += static_cast<size_t>(matched->length);
    }
    return out;
}

std::wstring SanitizeFileName(const std::wstring& name) {
    std::wstring out;
    out.reserve(name.size());
    for (const wchar_t ch : name) {
        // Denetim karakterleri de gider: bir pencere başlığı sekme içerebilir
        // ve dosya adında görünmez bir karakter, sonradan bulunamayan bir
        // dosya demektir.
        if (ch < 32 || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
            ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*') {
            out.push_back(L'-');
            continue;
        }
        out.push_back(ch);
    }
    while (!out.empty() && (out.back() == L'.' || out.back() == L' ')) {
        out.pop_back();
    }
    return out;
}

std::wstring SanitizeRelativePath(const std::wstring& path) {
    std::wstring out;
    std::wstring part;

    auto flush = [&]() {
        const std::wstring clean = SanitizeFileName(part);
        part.clear();
        // ".." ATILIR: kayıt klasörünün dışına çıkan bir şablon, kullanıcının
        // yakalamalarını beklemediği bir yere yazardı.
        if (clean.empty() || clean == L"." || clean == L"..") {
            return;
        }
        if (!out.empty()) {
            out.push_back(L'\\');
        }
        out += clean;
    };

    for (const wchar_t ch : path) {
        if (ch == L'\\' || ch == L'/') {
            flush();
            continue;
        }
        part.push_back(ch);
    }
    flush();
    return out;
}

}  // namespace crisp
