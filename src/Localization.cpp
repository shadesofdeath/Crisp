// Localization.cpp — bkz. Localization.h.
#include "Localization.h"

#include "Util.h"
#include "resource.h"

namespace crisp {
namespace Loc {
namespace {

// Diller kod sırasına göre; ilk satır daima "auto".
constexpr Language kLanguages[] = {
    {L"auto",  nullptr,        0},
    {L"cs",    L"Čeština",     MAKELANGID(LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC)},
    {L"de",    L"Deutsch",     MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN)},
    {L"en",    L"English",     MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)},
    {L"es",    L"Español",     MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH)},
    {L"fr",    L"Français",    MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH)},
    {L"it",    L"Italiano",    MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN)},
    {L"ja",    L"日本語",       MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN)},
    {L"ko",    L"한국어",       MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN)},
    {L"nl",    L"Nederlands",  MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH)},
    {L"pl",    L"Polski",      MAKELANGID(LANG_POLISH, SUBLANG_POLISH_POLAND)},
    {L"pt-BR", L"Português (Brasil)",
     MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN)},
    {L"ru",    L"Русский",     MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA)},
    {L"sv",    L"Svenska",     MAKELANGID(LANG_SWEDISH, SUBLANG_SWEDISH)},
    {L"tr",    L"Türkçe",      MAKELANGID(LANG_TURKISH, SUBLANG_TURKISH_TURKEY)},
    {L"uk",    L"Українська",  MAKELANGID(LANG_UKRAINIAN, SUBLANG_UKRAINIAN_UKRAINE)},
    {L"zh-CN", L"简体中文",     MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)},
};

constexpr LANGID kFallbackLangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

// Dosya kapsamlı static: ev kuralı global değişkeni yasaklar, .cpp içindeki
// static'e izin verir.
HINSTANCE g_instance = nullptr;
LANGID g_langId = kFallbackLangId;

// Kullanıcının Windows arayüz dilinden tabloya en yakın satırı bulur.
// Önce tam eşleşme (pt-BR), sonra birincil dil eşleşmesi (pt → pt-BR).
[[nodiscard]] LANGID ResolveAutomatic() noexcept {
    const LANGID user = ::GetUserDefaultUILanguage();

    for (const Language& language : kLanguages) {
        if (language.langId == user) {
            return language.langId;
        }
    }
    const WORD userPrimary = PRIMARYLANGID(user);
    for (const Language& language : kLanguages) {
        if (language.langId != 0 && PRIMARYLANGID(language.langId) == userPrimary) {
            return language.langId;
        }
    }
    return kFallbackLangId;
}

[[nodiscard]] LANGID LangIdForCode(const std::wstring& code) noexcept {
    if (code.empty() || code == L"auto") {
        return ResolveAutomatic();
    }
    for (const Language& language : kLanguages) {
        if (language.langId != 0 && code == language.code) {
            return language.langId;
        }
    }
    return ResolveAutomatic();
}

// RT_STRING blokları 16 dizelik kümeler hâlinde saklanır: kimlik id için blok
// numarası (id / 16) + 1, blok içindeki sıra id % 16'dır. Blok, dizeleri
// "uzunluk + karakterler" biçiminde ardışık tutar; istenen sıraya kadar
// atlanır.
[[nodiscard]] bool LoadFromBlock(LANGID langId, UINT id, std::wstring& out) {
    if (g_instance == nullptr) {
        return false;
    }

    const UINT blockId = (id / 16) + 1;
    const UINT indexInBlock = id % 16;

    const HRSRC found = ::FindResourceExW(g_instance, RT_STRING,
                                          MAKEINTRESOURCEW(blockId), langId);
    if (found == nullptr) {
        return false;
    }
    const HGLOBAL loaded = ::LoadResource(g_instance, found);
    if (loaded == nullptr) {
        return false;
    }
    const auto* cursor = static_cast<const wchar_t*>(::LockResource(loaded));
    if (cursor == nullptr) {
        return false;
    }

    const DWORD blockBytes = ::SizeofResource(g_instance, found);
    const wchar_t* const end =
        cursor + (blockBytes / sizeof(wchar_t));

    for (UINT i = 0; i < indexInBlock; ++i) {
        if (cursor >= end) {
            return false;
        }
        cursor += 1 + *cursor;   // uzunluk sözcüğü + karakterler
    }
    if (cursor >= end) {
        return false;
    }

    const WORD length = *cursor;
    if (length == 0) {
        return false;   // bu dilde çevrilmemiş; çağıran geri düşüşe gitsin
    }
    if (cursor + 1 + length > end) {
        return false;
    }
    out.assign(cursor + 1, length);
    return true;
}

}  // namespace

const Language* Languages() noexcept { return kLanguages; }

int LanguageCount() noexcept {
    return static_cast<int>(sizeof(kLanguages) / sizeof(kLanguages[0]));
}

int LanguageIndex(const std::wstring& code) noexcept {
    for (int i = 0; i < LanguageCount(); ++i) {
        if (code == kLanguages[i].code) {
            return i;
        }
    }
    return 0;   // bilinmeyen kod → "auto"
}

const wchar_t* LanguageCodeAt(int index) noexcept {
    if (index < 0 || index >= LanguageCount()) {
        return kLanguages[0].code;
    }
    return kLanguages[index].code;
}

bool IsKnownLanguage(const std::wstring& code) noexcept {
    for (const Language& language : kLanguages) {
        if (code == language.code) {
            return true;
        }
    }
    return false;
}

void Initialize(HINSTANCE instance, const std::wstring& languageSetting) {
    g_instance = instance;
    g_langId = LangIdForCode(languageSetting);
}

void SetLanguage(const std::wstring& languageSetting) {
    g_langId = LangIdForCode(languageSetting);
}

std::wstring Str(UINT id) {
    std::wstring text;
    if (LoadFromBlock(g_langId, id, text)) {
        return text;
    }
    // Çevrilmemiş dize İngilizceden alınır: eksik bir çeviri boş bir menü
    // öğesi değil, okunabilir bir metin olmalı.
    if (g_langId != kFallbackLangId && LoadFromBlock(kFallbackLangId, id, text)) {
        return text;
    }
    // Dile bağlı olmayan blok (LANG_NEUTRAL) son çare.
    if (LoadFromBlock(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), id, text)) {
        return text;
    }
    LogV(L"Dize bulunamadı: %u", id);
    return std::wstring();
}

std::wstring MenuText(UINT textId, UINT acceleratorId) {
    std::wstring text = Str(textId);
    if (acceleratorId == 0) {
        return text;
    }
    const std::wstring accelerator = Str(acceleratorId);
    if (accelerator.empty()) {
        return text;
    }
    text += L'\t';
    text += accelerator;
    return text;
}

LANGID CurrentLangId() noexcept { return g_langId; }

}  // namespace Loc
}  // namespace crisp
