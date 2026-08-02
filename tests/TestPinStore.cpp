// TestPinStore.cpp — İğnelerin diskteki kaydının satır biçimi.
//
// DOSYAYA DOKUNMADAN SINANIR. Biçimlendirme ve çözümleme açıkta duruyor tam da
// bunun için: bir iğnenin konumunun yanlış geri yüklenmesi ekranda kolayca
// gözden kaçar, ama gidiş-dönüş sınaması onu anında yakalar.
#include "TestFramework.h"

#include "PinStore.h"

using namespace crisp;

CRISP_TEST(PinStore, Gidis_donus_degeri_korur) {
    PinRecord record;
    record.imageFile = L"pin-03.png";
    record.x = -1920;   // soldaki ikinci monitör
    record.y = 240;
    record.zoom = 150;
    record.opacity = 140;

    PinRecord back;
    CHECK(ParsePinLine(FormatPinLine(record), back));
    CHECK(back.imageFile == record.imageFile);
    CHECK_EQ(back.x, record.x);
    CHECK_EQ(back.y, record.y);
    CHECK_EQ(back.zoom, record.zoom);
    CHECK_EQ(back.opacity, record.opacity);
}

CRISP_TEST(PinStore, Bozuk_satir_reddedilir) {
    PinRecord out;
    CHECK(!ParsePinLine(L"", out));
    CHECK(!ParsePinLine(L"yalnizca metin", out));
    // Alan eksik: dosya adı yok.
    CHECK(!ParsePinLine(L"10\t20\t100\t255", out));
    // Dosya adı boş.
    CHECK(!ParsePinLine(L"10\t20\t100\t255\t", out));
}

CRISP_TEST(PinStore, Klasor_disina_cikan_ad_reddedilir) {
    // İNDEKS ELLE DÜZENLENEBİLİR BİR METİN DOSYASI. İçine yol yazılmış bir
    // satır, uygulamayı iğne klasörünün dışındaki bir dosyayı okumaya ikna
    // edebilirdi.
    PinRecord out;
    CHECK(!ParsePinLine(L"0\t0\t100\t255\t..\\..\\gizli.png", out));
    CHECK(!ParsePinLine(L"0\t0\t100\t255\tC:\\Windows\\x.png", out));
    CHECK(!ParsePinLine(L"0\t0\t100\t255\talt/klasor.png", out));
}

CRISP_TEST(PinStore, Aralik_disi_degerler_duzeltilir) {
    // Bozuk bir sayı yüzünden iğneyi HİÇ göstermemektense makul bir değerle
    // göstermek yeğdir: kullanıcı görüntüsünü geri istiyor.
    PinRecord out;
    CHECK(ParsePinLine(L"0\t0\t99999\t99999\tpin-00.png", out));
    CHECK(out.zoom <= 800);
    CHECK(out.opacity <= 255u);

    CHECK(ParsePinLine(L"0\t0\t1\t0\tpin-00.png", out));
    CHECK(out.zoom >= 10);
    CHECK(out.opacity >= 20u);
}

CRISP_TEST(PinStore, Yol_uygulama_verisinin_altinda) {
    const std::wstring folder = PinFolder();
    const std::wstring index = PinIndexPath();
    CHECK(!folder.empty());
    CHECK(folder.find(L"\\Crisp\\Pins") != std::wstring::npos);
    // İndeks klasörün İÇİNDE olmalı: dışarıda bir dosya, klasörü silen
    // kullanıcının yarım bir durumla kalması demekti.
    CHECK(index.rfind(folder, 0) == 0);
}
