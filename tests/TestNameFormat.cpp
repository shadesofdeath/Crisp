// TestNameFormat.cpp — Dosya adı şablonu.
#include "TestFramework.h"

#include "NameFormat.h"

using namespace crisp;

namespace {

[[nodiscard]] NameContext Sample() {
    NameContext context;
    context.time.wYear = 2026;
    context.time.wMonth = 8;
    context.time.wDay = 1;
    context.time.wHour = 14;
    context.time.wMinute = 5;
    context.time.wSecond = 9;
    context.windowTitle = L"Belge - Not Defteri";
    context.width = 1920;
    context.height = 1080;
    context.counter = 42;
    context.random = 7;
    return context;
}

}  // namespace

CRISP_TEST(NameFormat, Zaman_belirtecleri_sifir_dolgulu) {
    const NameContext context = Sample();
    CHECK(ExpandNameFormat(L"%y-%mo-%d", context) == L"2026-08-01");
    CHECK(ExpandNameFormat(L"%h.%mi.%s", context) == L"14.05.09");
    CHECK(ExpandNameFormat(L"%yy", context) == L"26");
}

CRISP_TEST(NameFormat, Uzun_belirtec_kisadan_once_eslesir) {
    // "%mo" ile "%m..." aynı harfle başlıyor; kısa olan önce denenirse ay
    // hiçbir zaman çözülmez ve adlarda "%o" kalıntısı görülürdü.
    const NameContext context = Sample();
    CHECK(ExpandNameFormat(L"%mo", context) == L"08");
    CHECK(ExpandNameFormat(L"%mi", context) == L"05");
}

CRISP_TEST(NameFormat, Olcu_sayac_ve_baslik) {
    const NameContext context = Sample();
    CHECK(ExpandNameFormat(L"%px x %py", context) == L"1920 x 1080");
    CHECK(ExpandNameFormat(L"%i", context) == L"0042");
    CHECK(ExpandNameFormat(L"%pn", context) == L"Belge - Not Defteri");
}

CRISP_TEST(NameFormat, Rastgele_ayni_tohumda_ayni_sonuc) {
    // Testlerin ve önizlemenin yeniden üretilebilir olması için tohum
    // dışarıdan verilir; rand() kullanılsaydı ikisi de mümkün olmazdı.
    NameContext a = Sample();
    NameContext b = Sample();
    const std::wstring first = ExpandNameFormat(L"%ra", a);
    CHECK(first.size() == 6);
    CHECK(ExpandNameFormat(L"%ra", b) == first);

    b.random = 99;
    CHECK(ExpandNameFormat(L"%ra", b) != first);
}

CRISP_TEST(NameFormat, Bilinmeyen_belirtec_oldugu_gibi_kalir) {
    const NameContext context = Sample();
    CHECK(ExpandNameFormat(L"%q", context) == L"%q");
    CHECK(ExpandNameFormat(L"%%y", context) == L"%y");
    CHECK(ExpandNameFormat(L"sade metin", context) == L"sade metin");
    CHECK(ExpandNameFormat(L"", context) == L"");
    // Sonda yarım kalan belirteç çökmemeli.
    CHECK(ExpandNameFormat(L"dosya %", context) == L"dosya %");
}

CRISP_TEST(NameFormat, Sanitize_yasak_karakterleri_degistirir) {
    CHECK(SanitizeFileName(L"a/b\\c:d*e?f\"g<h>i|j") ==
          L"a-b-c-d-e-f-g-h-i-j");
    CHECK(SanitizeFileName(L"normal ad") == L"normal ad");
    // Sondaki nokta ve boşluk Windows'ta sessizce düşürülür; biz de düşürürüz
    // ki dosyanın adı istenen ad olsun.
    CHECK(SanitizeFileName(L"ad...  ") == L"ad");
    CHECK(SanitizeFileName(L"") == L"");
}

CRISP_TEST(NameFormat, Sanitize_yol_ust_dizine_cikamaz) {
    CHECK(SanitizeRelativePath(L"2026\\08") == L"2026\\08");
    CHECK(SanitizeRelativePath(L"2026/08/01") == L"2026\\08\\01");
    // ".." ve "." atılır: şablon kayıt klasörünün dışına yazamamalı.
    CHECK(SanitizeRelativePath(L"..\\..\\Windows") == L"Windows");
    CHECK(SanitizeRelativePath(L".\\alt") == L"alt");
    // Boş parçalar birleşir, baştaki ayraç kök yapmaz.
    CHECK(SanitizeRelativePath(L"\\\\a\\\\b\\\\") == L"a\\b");
    CHECK(SanitizeRelativePath(L"") == L"");
}
