// TestShellIntegration.cpp — Explorer sağ tık fiilinin kaydı.
//
// GERÇEK KAYIT DEFTERİNE YAZAR ama kendi fiil adıyla: testler "Crisp.Test"
// altında çalışır ve kullanıcının gerçek "Crisp.Edit" kaydına dokunmaz.
// Ayrı bir sahte kayıt katmanı yazmak, sınanan şeyi — RegSetKeyValueW'nin
// gerçekten çalışıp çalışmadığını — sınamamak olurdu.
#include "TestFramework.h"

#include "ShellIntegration.h"
#include "Util.h"

using namespace crisp;

namespace {

constexpr const wchar_t* kTestVerb = L"Crisp.Test";

// Test bittiğinde kaydı her hâlükârda temizler; başarısız bir doğrulama
// makinede artık bırakmamalı.
struct VerbCleanup {
    ~VerbCleanup() { (void)UnregisterShellMenu(kTestVerb); }
};

}  // namespace

CRISP_TEST(ShellIntegration, Kayit_gidis_donusu) {
    const VerbCleanup cleanup;
    (void)UnregisterShellMenu(kTestVerb);
    CHECK(!IsShellMenuRegistered(kTestVerb));

    CHECK(RegisterShellMenu(L"Test ile düzenle", kTestVerb));
    CHECK(IsShellMenuRegistered(kTestVerb));
    // Komut BU exe'yi göstermeli; taşınmış bir kurulumda menü öğesi sessizce
    // bozuk kalırdı ve bu denetim tam olarak onu yakalar.
    CHECK(ShellMenuPathIsCurrent(kTestVerb));

    CHECK(UnregisterShellMenu(kTestVerb));
    CHECK(!IsShellMenuRegistered(kTestVerb));
    CHECK(!ShellMenuPathIsCurrent(kTestVerb));
}

CRISP_TEST(ShellIntegration, Kaldirma_yoksa_da_basarilidir) {
    // "Kayıtlı olmasın" isteği, zaten kayıtlı değilken de karşılanmıştır;
    // hata döndürmek çağıranı olmayan bir sorunu bildirmeye zorlardı.
    (void)UnregisterShellMenu(kTestVerb);
    CHECK(UnregisterShellMenu(kTestVerb));
    CHECK(UnregisterShellMenu(kTestVerb));
}

CRISP_TEST(ShellIntegration, Ikinci_kayit_ustune_yazar) {
    const VerbCleanup cleanup;
    CHECK(RegisterShellMenu(L"Birinci", kTestVerb));
    CHECK(RegisterShellMenu(L"İkinci", kTestVerb));
    CHECK(IsShellMenuRegistered(kTestVerb));
    CHECK(ShellMenuPathIsCurrent(kTestVerb));
}

CRISP_TEST(ShellIntegration, Gercek_fiil_testten_etkilenmez) {
    // Testin kendi adıyla çalıştığının kanıtı: gerçek fiilin durumu ne ise
    // öyle kalmalı.
    const bool before = IsShellMenuRegistered();
    {
        const VerbCleanup cleanup;
        CHECK(RegisterShellMenu(L"Test", kTestVerb));
    }
    CHECK_EQ(IsShellMenuRegistered(), before);
}
