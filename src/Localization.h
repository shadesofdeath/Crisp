// Localization.h — Çoklu dil.
//
// LoadStringW YERİNE RT_STRING blokları dil kimliğiyle DOĞRUDAN aranır. Sebep:
// LoadStringW, sürecin MUI dil sıralamasına bakar ve hangi tablonun seçileceği
// kullanıcının Windows dil listesine bağlı olur. Bizim istediğimiz, seçimin
// AYARDAN gelmesi ve bulunamayan bir dilde garantili biçimde İngilizceye
// düşmesi — ikisi de ancak aramayı kendimiz yaparsak mümkün.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {
namespace Loc {

// Desteklenen bir arayüz dili.
//
// TEK DOĞRULUK KAYNAĞI: ayarlarda saklanan kod, ayarlar penceresindeki liste ve
// RT_STRING araması için kullanılan LANGID — üçü de bu tablodan gelir.
//
// endonym ÇEVRİLMEZ: bir dilin kendi adı ("Deutsch", "日本語") arayüz hangi
// dilde olursa olsun aynı yazılır. Bu yüzden adlar kaynak dizelerinde DEĞİL
// burada literal olarak durur; aksi hâlde 16 ad × 16 tablo = 256 gereksiz
// çeviri satırı olurdu.
struct Language {
    const wchar_t* code;      // ayar değeri: "auto", "tr", "pt-BR" ...
    const wchar_t* endonym;   // listedeki ad; nullptr ise IDS_LANG_AUTO
    LANGID langId;            // RT_STRING araması; "auto" için 0
};

// İlk öğe DAİMA "auto"dur.
[[nodiscard]] const Language* Languages() noexcept;
[[nodiscard]] int LanguageCount() noexcept;

[[nodiscard]] int LanguageIndex(const std::wstring& code) noexcept;
[[nodiscard]] const wchar_t* LanguageCodeAt(int index) noexcept;
[[nodiscard]] bool IsKnownLanguage(const std::wstring& code) noexcept;

// languageSetting: L"auto" ya da tablodaki bir kod.
// auto → GetUserDefaultUILanguage() ile en yakın satır, yoksa EN.
void Initialize(HINSTANCE instance, const std::wstring& languageSetting);

// Ayar anında değiştiğinde yeniden çağrılır.
void SetLanguage(const std::wstring& languageSetting);

// Bulunamayan kimlik için BOŞ dize döner, asla nullptr.
[[nodiscard]] std::wstring Str(UINT id);

// "Kopyala\tCtrl+C" gibi menü metni üretir. Kısayol etiketi boşsa yalnızca
// metin döner — sekme karakteri boş bir sütun bırakır ve menü eğri görünür.
[[nodiscard]] std::wstring MenuText(UINT textId, UINT acceleratorId);

// Etkin dilin LANGID'i (tanı amaçlı).
[[nodiscard]] LANGID CurrentLangId() noexcept;

}  // namespace Loc
}  // namespace crisp
