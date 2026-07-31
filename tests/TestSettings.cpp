// TestSettings.cpp — Ayar deposu ve doğrulama.
//
// Tüm testler .ini arka ucunu kullanır: kayıt defteri arka ucu aynı kod
// yollarını paylaşır ama testin kullanıcının gerçek HKCU ayarlarını
// değiştirmesi kabul edilemez.
#include "TestFramework.h"

#include "Settings.h"

#include <string>

using namespace crisp;

namespace {

[[nodiscard]] std::wstring TempIni(const wchar_t* name) {
    std::wstring path{test::TempDirectory()};
    path += L'\\';
    path += name;
    ::DeleteFileW(path.c_str());   // önceki koşudan kalmasın
    return path;
}

}  // namespace

CRISP_TEST(SettingsStore, Ini_gidis_donusu_temel_tipler) {
    const SettingsStore store = SettingsStore::ForFile(TempIni(L"basic.ini"));
    CHECK(store.Prepare());

    CHECK(store.WriteUnsigned(L"Sayi", 42u));
    CHECK(store.WriteBool(L"Dogru", true));
    CHECK(store.WriteBool(L"Yanlis", false));
    CHECK(store.WriteString(L"Metin", L"C:\\Klasor\\Alt Klasor"));
    store.Flush();

    unsigned number = 0;
    CHECK(store.ReadUnsigned(L"Sayi", number));
    CHECK_EQ(number, 42u);

    bool flag = false;
    CHECK(store.ReadBool(L"Dogru", flag));
    CHECK(flag);

    flag = true;
    CHECK(store.ReadBool(L"Yanlis", flag));
    CHECK(!flag);

    std::wstring text;
    CHECK(store.ReadString(L"Metin", text));
    CHECK_STR(text, L"C:\\Klasor\\Alt Klasor");
}

CRISP_TEST(SettingsStore, Olmayan_deger_ciktiyi_degistirmez) {
    // Sözleşme: değer yoksa out'a DOKUNULMAZ. Çağıranların hepsi varsayılanı
    // out'a koyup okumayı çağırıyor; bu kural bozulursa tüm varsayılanlar
    // sessizce sıfırlanır.
    const SettingsStore store = SettingsStore::ForFile(TempIni(L"missing.ini"));
    CHECK(store.Prepare());

    unsigned number = 999u;
    CHECK(!store.ReadUnsigned(L"YokBoyleBirSey", number));
    CHECK_EQ(number, 999u);

    bool flag = true;
    CHECK(!store.ReadBool(L"BuDaYok", flag));
    CHECK(flag);

    std::wstring text{L"dokunma"};
    CHECK(!store.ReadString(L"BuDaHicYok", text));
    CHECK_STR(text, L"dokunma");
}

CRISP_TEST(SettingsStore, Bos_dize_yazilabilir_ve_yok_ile_karismaz) {
    const SettingsStore store = SettingsStore::ForFile(TempIni(L"empty.ini"));
    CHECK(store.Prepare());

    CHECK(store.WriteString(L"BosDeger", L""));
    store.Flush();

    // Boş bir dize YAZILDIYSA okuma başarılı sayılmalı ve boş dönmeli;
    // "hiç yazılmamış" ile karışmamalı.
    std::wstring value{L"onceki"};
    const bool found = store.ReadString(L"BosDeger", value);
    // GetPrivateProfileString boş değeri de "bulundu" sayar ama bazı Windows
    // sürümlerinde varsayılana düşer; hangisi olursa olsun ÇÖPPE dönmemeli.
    if (found) {
        CHECK_STR(value, L"");
    } else {
        CHECK_STR(value, L"onceki");
    }
}

CRISP_TEST(SettingsStore, Sifir_gecerli_bir_sayidir) {
    // Sentinel olarak 0 kullanılsaydı "0 yazılmış" ile "hiç yok" karışırdı.
    const SettingsStore store = SettingsStore::ForFile(TempIni(L"zero.ini"));
    CHECK(store.Prepare());

    CHECK(store.WriteUnsigned(L"Sifir", 0u));
    store.Flush();

    unsigned value = 123u;
    CHECK(store.ReadUnsigned(L"Sifir", value));
    CHECK_EQ(value, 0u);
}

CRISP_TEST(SettingsStore, ForFile_ini_arka_ucunu_secer) {
    const std::wstring path = TempIni(L"kind.ini");
    const SettingsStore store = SettingsStore::ForFile(path);
    CHECK(store.Kind() == SettingsStore::Backend::IniFile);
    CHECK_STR(store.Path(), path);
}

CRISP_TEST(Settings, Varsayilanlar_makul) {
    const Settings defaults;
    CHECK(defaults.after.copyToClipboard);
    CHECK(defaults.showMagnifier);
    CHECK_EQ(defaults.delaySeconds, 3u);
    CHECK(defaults.hotkeyRegion.assigned());
}

CRISP_TEST(Settings, Clamp_gecikmeyi_araliga_ceker) {
    Settings s;
    s.delaySeconds = 0u;
    s.Clamp();
    CHECK_EQ(s.delaySeconds, 1u);

    s.delaySeconds = 9999u;
    s.Clamp();
    CHECK_EQ(s.delaySeconds, 30u);

    s.delaySeconds = 5u;
    s.Clamp();
    CHECK_EQ(s.delaySeconds, 5u);
}

CRISP_TEST(Settings, Clamp_hicbir_eylem_yoksa_panoyu_geri_acar) {
    // Aksi hâlde araç yakalar ve sonucu sessizce atar.
    Settings s;
    s.after.copyToClipboard = false;
    s.after.saveToFile = false;
    s.after.pinToScreen = false;
    s.after.openEditor = false;
    s.Clamp();
    CHECK(s.after.copyToClipboard);
}

CRISP_TEST(Settings, Clamp_tek_eylem_yeterliyse_dokunmaz) {
    Settings s;
    s.after.copyToClipboard = false;
    s.after.saveToFile = true;
    s.Clamp();
    CHECK(!s.after.copyToClipboard);
    CHECK(s.after.saveToFile);
}

CRISP_TEST(Settings, Clamp_degistiricisiz_kisayolu_iptal_eder) {
    // Değiştiricisiz kısayol, tek tuşa basınca yakalama başlatırdı.
    Settings s;
    s.hotkeyRegion.modifiers = 0;
    s.hotkeyRegion.key = 'S';
    s.Clamp();
    CHECK(!s.hotkeyRegion.assigned());

    s.hotkeyWindow.modifiers = MOD_CONTROL | MOD_SHIFT;
    s.hotkeyWindow.key = 'W';
    s.Clamp();
    CHECK(s.hotkeyWindow.assigned());
}

CRISP_TEST(Settings, Clamp_tek_basina_MOD_WIN_kabul_edilmez) {
    // Windows, Win+harf kombinasyonlarının çoğunu kendisi ayırıyor.
    Settings s;
    s.hotkeyDelayed.modifiers = MOD_WIN;
    s.hotkeyDelayed.key = 'D';
    s.Clamp();
    CHECK(!s.hotkeyDelayed.assigned());
}

CRISP_TEST(Settings, Hotkey_paketleme_gidis_donusu) {
    const Hotkey original{MOD_CONTROL | MOD_SHIFT, 'S'};
    const Hotkey restored = Hotkey::unpack(original.packed());
    CHECK_EQ(restored.modifiers, original.modifiers);
    CHECK_EQ(restored.key, original.key);
}

CRISP_TEST(Settings, Kaydet_yukle_tam_gidis_donusu) {
    const SettingsStore store = SettingsStore::ForFile(TempIni(L"full.ini"));

    Settings original;
    original.saveFolder = L"D:\\Ekran Goruntuleri";
    original.after.copyToClipboard = false;
    original.after.saveToFile = true;
    original.after.pinToScreen = true;
    original.after.openEditor = false;
    original.delaySeconds = 7u;
    original.showMagnifier = false;
    original.showWindowHighlight = false;
    original.playShutterSound = true;
    original.hotkeyRegion = Hotkey{MOD_CONTROL | MOD_ALT, 'Q'};
    original.hotkeyWindow = Hotkey{MOD_SHIFT | MOD_ALT, 'Z'};

    CHECK(original.Save(store));

    Settings loaded;
    loaded.Load(store);

    CHECK_STR(loaded.saveFolder, L"D:\\Ekran Goruntuleri");
    CHECK(!loaded.after.copyToClipboard);
    CHECK(loaded.after.saveToFile);
    CHECK(loaded.after.pinToScreen);
    CHECK(!loaded.after.openEditor);
    CHECK_EQ(loaded.delaySeconds, 7u);
    CHECK(!loaded.showMagnifier);
    CHECK(!loaded.showWindowHighlight);
    CHECK(loaded.playShutterSound);
    CHECK_EQ(loaded.hotkeyRegion.modifiers, MOD_CONTROL | MOD_ALT);
    CHECK_EQ(loaded.hotkeyRegion.key, 'Q');
    CHECK_EQ(loaded.hotkeyWindow.key, 'Z');
}

CRISP_TEST(Settings, Yukleme_bozuk_degerleri_duzeltir) {
    const SettingsStore store = SettingsStore::ForFile(TempIni(L"corrupt.ini"));
    CHECK(store.Prepare());

    // Elle kurcalanmış bir .ini taklidi: saçma gecikme, hiçbir eylem yok.
    CHECK(store.WriteUnsigned(L"DelaySeconds", 100000u));
    CHECK(store.WriteBool(L"CopyToClipboard", false));
    CHECK(store.WriteBool(L"SaveToFile", false));
    CHECK(store.WriteBool(L"PinToScreen", false));
    CHECK(store.WriteBool(L"OpenEditor", false));
    store.Flush();

    Settings loaded;
    loaded.Load(store);

    CHECK_EQ(loaded.delaySeconds, 30u);
    CHECK(loaded.after.copyToClipboard);
}

CRISP_TEST(Settings, EffectiveSaveFolder_bos_ise_varsayilan_verir) {
    Settings s;
    s.saveFolder.clear();
    const std::wstring folder = s.EffectiveSaveFolder();
    CHECK(!folder.empty());

    s.saveFolder = L"E:\\Ozel";
    CHECK_STR(s.EffectiveSaveFolder(), L"E:\\Ozel");
}
