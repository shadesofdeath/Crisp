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
    CHECK(defaults.hotkeys[0].key.assigned());
    CHECK(defaults.hotkeys[0].action == HotkeyAction::Region);
    // Son iki yuva boş gelir: kullanıcı isterse doldurur.
    CHECK(!defaults.hotkeys[kHotkeySlots - 1].key.assigned());
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

CRISP_TEST(Settings, Clamp_degistiricisiz_kisayolu_artik_silmez) {
    // BU TEST ESKİDEN TERSİNİ SÖYLÜYORDU. Clamp, değiştiricisi olmayan bir harfi
    // siliyordu. Gerekçe gerçekti — tek başına bağlanan bir harf, o harfi
    // sistemdeki her metin kutusundan alır — ama sonucu, kullanıcının ayarlar
    // penceresinde seçip kaydettiği tuşun bir sonraki açılışta yok olmasıydı.
    // Tek tuşla ekran görüntüsü almak yaygın bir alışkanlık ve bedeli olan bir
    // seçim, bedeli anlatılıp kullanıcıya bırakılır.
    Settings s;
    s.hotkeys[0].key = Hotkey{0, 'S'};
    s.Clamp();
    CHECK(s.hotkeys[0].key.assigned());

    s.hotkeys[1].key = Hotkey{MOD_CONTROL | MOD_SHIFT, 'W'};
    s.Clamp();
    CHECK(s.hotkeys[1].key.assigned());
}

CRISP_TEST(Settings, HotkeyTypesCharacters_bedeli_olan_tuslari_ayirir) {
    // Bu yordam artık hiçbir şeyi yasaklamıyor. Yalnızca ipucu satırının
    // anlattığı ayrımı tutuyor: bu tuşu tek başına bağlarsam yazarken bir şey
    // kaybeder miyim?
    CHECK(HotkeyTypesCharacters('S'));
    CHECK(HotkeyTypesCharacters('7'));
    CHECK(HotkeyTypesCharacters(VK_SPACE));

    // İşlev tuşları ve zaten bu iş için var olan tuşlar: bedelsiz.
    CHECK(!HotkeyTypesCharacters(VK_F1));
    CHECK(!HotkeyTypesCharacters(VK_F24));
    CHECK(!HotkeyTypesCharacters(VK_SNAPSHOT));
    CHECK(!HotkeyTypesCharacters(VK_PAUSE));
    CHECK(!HotkeyTypesCharacters(VK_MEDIA_PLAY_PAUSE));

    // TKL (sayısal takımsız) bir klavyede tek başına bağlamak için en uygun
    // tuşlar bunlar ve hiçbiri karakter üretmiyor. Eskiden listede yoktular ve
    // "değiştirici ister" sayılıyorlardı.
    CHECK(!HotkeyTypesCharacters(VK_INSERT));
    CHECK(!HotkeyTypesCharacters(VK_HOME));
    CHECK(!HotkeyTypesCharacters(VK_END));
    CHECK(!HotkeyTypesCharacters(VK_PRIOR));
    CHECK(!HotkeyTypesCharacters(VK_NEXT));

    // Ve döndürdüğü değer kısayolun kaderini belirlemiyor: ikisi de Clamp'ten
    // sağ çıkar.
    Settings s;
    s.hotkeys[0].key = Hotkey{0, VK_INSERT};
    s.hotkeys[1].key = Hotkey{0, 'K'};
    s.Clamp();
    CHECK(s.hotkeys[0].key.assigned());
    CHECK(s.hotkeys[1].key.assigned());
}

CRISP_TEST(Settings, Clamp_eylemsiz_yuvanin_tusunu_silmez) {
    // Eylemi henüz seçilmemiş bir yuva RegisterHotKey'e hiç verilmez, o yüzden
    // kimseden tuş çalmaz. Tuşu silmek yalnızca kullanıcının yazdığını yok
    // ediyor ve alanı boşaltıyordu.
    Settings s;
    s.hotkeys[4].key = Hotkey{MOD_CONTROL | MOD_SHIFT, 'L'};
    s.hotkeys[4].action = HotkeyAction::None;
    s.Clamp();
    CHECK(s.hotkeys[4].key.assigned());
    CHECK(s.hotkeys[4].action == HotkeyAction::None);
}

CRISP_TEST(Settings, Clamp_MOD_WIN_kararini_RegisterHotKey_e_birakir) {
    // BU TEST DE TERSİNİ SÖYLÜYORDU. Win+harf bileşimlerinin çoğunu Windows
    // kendisi ayırdığı için Clamp onları siliyordu — ama Win+harf'in TAMAMI
    // ayrılmış değil ve hangilerinin boş olduğunu bilen tek merci
    // RegisterHotKey. Ayrılmış olanlar zaten kayıtta reddediliyor ve kullanıcı
    // "şu kısayollar kaydedilemedi" iletisini görüyor; sessizce silmek, aynı
    // bilgiyi vermeden tuşu yok ediyordu.
    Settings s;
    s.hotkeys[3].key = Hotkey{MOD_WIN, 'D'};
    s.Clamp();
    CHECK(s.hotkeys[3].key.assigned());
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
    original.hotkeys[0].key = Hotkey{MOD_CONTROL | MOD_ALT, 'Q'};
    original.hotkeys[1].key = Hotkey{MOD_SHIFT | MOD_ALT, 'Z'};
    original.hotkeys[4].key = Hotkey{MOD_CONTROL | MOD_SHIFT, 'L'};
    original.hotkeys[4].action = HotkeyAction::LastRegion;
    original.includeCursor = true;
    original.dimStrength = 65u;
    original.fileNameFormat = L"%y%mo%d-%px";
    original.subFolderFormat = L"%y\\%mo";
    original.lastRegion = RECT{-100, 20, 340, 260};

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
    CHECK_EQ(loaded.hotkeys[0].key.modifiers, MOD_CONTROL | MOD_ALT);
    CHECK_EQ(loaded.hotkeys[0].key.key, 'Q');
    CHECK_EQ(loaded.hotkeys[1].key.key, 'Z');
    CHECK(loaded.hotkeys[4].action == HotkeyAction::LastRegion);
    CHECK(loaded.includeCursor);
    CHECK_EQ(loaded.dimStrength, 65u);
    CHECK_STR(loaded.fileNameFormat, L"%y%mo%d-%px");
    CHECK_STR(loaded.subFolderFormat, L"%y\\%mo");
    // KOORDİNATLAR İŞARETLİ: sol üstteki monitör birincil değilse negatif olur
    // ve unsigned olarak saklanıp geri çevrilmezse ekranın dışına düşerdi.
    CHECK_EQ(loaded.lastRegion.left, -100);
    CHECK_EQ(loaded.lastRegion.right, 340);
    CHECK(loaded.HasLastRegion());
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
