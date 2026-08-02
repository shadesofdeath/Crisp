// TestUpload.cpp — Yüklemenin ağ gerektirmeyen yarısı.
//
// HİÇBİR TEST AĞA ÇIKMAZ ve çıkmamalı: bir test paketinin geçmesi, üçüncü taraf
// bir servisin o an ayakta olmasına bağlı olamaz. Sınanan şey, gönderilecek
// isteğin doğru kurulduğu ve dönen yanıttan bağlantının doğru çıkarıldığı —
// yükleme özelliğinde yanlış gidebilecek şeylerin neredeyse tamamı bu ikisi.
#include "TestFramework.h"

#include "Upload.h"
#include "UploadInternal.h"

using namespace crisp;

namespace {

[[nodiscard]] std::vector<unsigned char> FakePng() {
    // Gerçek bir PNG olması gerekmiyor; gövdeye olduğu gibi konuyor. İçinde 0x00
    // ve 0x0D/0x0A baytları var: gövdenin metin gibi kesilip kesilmediğini
    // gösterir.
    return {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x00, 0x1A, 0x0A, 0xFF};
}

[[nodiscard]] bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

CRISP_TEST(Upload, Servis_kimlikleri_gidis_donusu) {
    // Kimlikler ayar dosyasına yazılıyor: biri değişirse kullanıcının seçimi
    // sessizce "servis yok"a döner.
    size_t count = 0;
    const UploadServiceInfo* services = UploadServices(count);
    CHECK(count == static_cast<size_t>(UploadService::Count));

    for (size_t i = 0; i < count; ++i) {
        const UploadServiceInfo& info = services[i];
        CHECK(UploadServiceFromId(info.id) == info.service);
        CHECK(UploadServiceId(info.service) == std::wstring(info.id));
    }
}

CRISP_TEST(Upload, Bilinmeyen_kimlik_None_doner) {
    // İleri bir sürümün yazdığı servis adı, açılışı engellemez.
    CHECK(UploadServiceFromId(L"birseyler") == UploadService::None);
    CHECK(UploadServiceFromId(L"") == UploadService::None);
}

CRISP_TEST(Upload, Anahtar_isteyen_servis_anahtarsiz_istek_kurmaz) {
    // İstek hiç kurulmuyor: sunucuya anlamsız bir 403 için gitmenin âlemi yok.
    const UploadRequest r = BuildUploadRequest(UploadService::Imgur, L"", FakePng(), L"a.png");
    CHECK(!r.valid);
    CHECK(r.error == std::wstring(L"missing key"));

    const UploadRequest ok =
        BuildUploadRequest(UploadService::Imgur, L"abc123", FakePng(), L"a.png");
    CHECK(ok.valid);
}

CRISP_TEST(Upload, Anahtarsiz_servis_anahtar_istemez) {
    const UploadRequest r = BuildUploadRequest(UploadService::Catbox, L"", FakePng(), L"a.png");
    CHECK(r.valid);
    CHECK(r.host == std::wstring(L"catbox.moe"));
    CHECK(Contains(r.body, "name=\"reqtype\""));
    CHECK(Contains(r.body, "fileupload"));
    CHECK(Contains(r.body, "name=\"fileToUpload\""));
}

CRISP_TEST(Upload, Bos_goruntu_reddedilir) {
    const UploadRequest r =
        BuildUploadRequest(UploadService::Catbox, L"", std::vector<unsigned char>(), L"a.png");
    CHECK(!r.valid);
}

CRISP_TEST(Upload, Servis_secilmemisse_istek_kurulmaz) {
    const UploadRequest r = BuildUploadRequest(UploadService::None, L"", FakePng(), L"a.png");
    CHECK(!r.valid);
}

CRISP_TEST(Upload, Imgur_anahtari_baslikta_govdede_degil) {
    // Anahtar gövdeye sızarsa, çok parçalı gövdeyi günlüğe basan herhangi bir
    // ara katman onu görür.
    const UploadRequest r =
        BuildUploadRequest(UploadService::Imgur, L"GIZLIANAHTAR", FakePng(), L"a.png");
    CHECK(r.valid);
    CHECK(r.headers.find(L"Authorization: Client-ID GIZLIANAHTAR") != std::wstring::npos);
    CHECK(!Contains(r.body, "GIZLIANAHTAR"));
}

CRISP_TEST(Upload, ImgBB_anahtari_govdede_yolda_degil) {
    // Tersi: ImgBB'de anahtar gövdede, çünkü sorgu dizesindeki bir anahtar
    // yönlendirmelerde ve sunucu günlüklerinde görünür.
    const UploadRequest r =
        BuildUploadRequest(UploadService::ImgBB, L"GIZLIANAHTAR", FakePng(), L"a.png");
    CHECK(r.valid);
    CHECK(r.path.find(L"GIZLIANAHTAR") == std::wstring::npos);
    CHECK(Contains(r.body, "GIZLIANAHTAR"));
}

CRISP_TEST(Upload, Govde_dosyayi_bozmadan_tasir) {
    // PNG baytları ikili; gövde metin gibi kırpılırsa görüntü bozulur.
    const std::vector<unsigned char> png = FakePng();
    const UploadRequest r = BuildUploadRequest(UploadService::Catbox, L"", png, L"a.png");
    CHECK(r.valid);

    const std::string needle(reinterpret_cast<const char*>(png.data()), png.size());
    CHECK(Contains(r.body, needle));
}

CRISP_TEST(Upload, Sinirlayici_her_seferinde_farkli) {
    // Sabit bir sınırlayıcı, içinde o diziyi barındıran bir dosyada gövdeyi
    // bozardı; PNG baytları rastgele.
    CHECK(MakeBoundary() != MakeBoundary());
}

CRISP_TEST(Upload, Sinirlayici_govdede_kapanisiyla_bulunur) {
    const UploadRequest r = BuildUploadRequest(UploadService::Uguu, L"", FakePng(), L"a.png");
    CHECK(r.valid);
    CHECK(Contains(r.body, "--\r\n"));           // kapanış
    CHECK(Contains(r.body, "name=\"files[]\""));  // Uguu'nun alan adı
}

CRISP_TEST(Upload, Litterbox_sureyi_gonderir) {
    const UploadRequest r =
        BuildUploadRequest(UploadService::Litterbox, L"", FakePng(), L"a.png");
    CHECK(r.valid);
    CHECK(Contains(r.body, "name=\"time\""));
    CHECK(Contains(r.body, "72h"));
}

// --- Yanıt okuma -----------------------------------------------------------

CRISP_TEST(Upload, Duz_metin_yanit_kirpilir) {
    CHECK(ExtractUploadLink(UploadService::Catbox, "https://files.catbox.moe/ab12.png\n") ==
          std::wstring(L"https://files.catbox.moe/ab12.png"));
    CHECK(ExtractUploadLink(UploadService::ZeroXZero, "  https://0x0.st/aBcD.png  \r\n") ==
          std::wstring(L"https://0x0.st/aBcD.png"));
}

CRISP_TEST(Upload, Duz_metin_hata_iletisi_baglanti_sayilmaz) {
    // Bu servisler hatayı da düz metin döndürüyor. "http" ile başlamayan bir
    // gövde adres değildir ve panoya kopyalanmamalı.
    CHECK(ExtractUploadLink(UploadService::Catbox, "File too large").empty());
    CHECK(ExtractUploadLink(UploadService::Uguu, "").empty());
}

CRISP_TEST(Upload, Imgur_json_baglantisi) {
    const std::string body =
        R"({"status":200,"success":true,"data":{"id":"aB1","link":"https:\/\/i.imgur.com\/aB1.png"}})";
    CHECK(ExtractUploadLink(UploadService::Imgur, body) ==
          std::wstring(L"https://i.imgur.com/aB1.png"));
}

CRISP_TEST(Upload, ImgBB_ic_ice_ayni_adli_alani_karistirmaz) {
    // BU TESTİN ASIL SEBEBİ: ImgBB yanıtında `url` üç yerde geçiyor — data.url,
    // data.display_url ve data.thumb.url. Gövdede ilk "url" arayan saf bir
    // çözüm, küçük resmin adresini döndürebilir.
    const std::string body =
        R"({"data":{"id":"x","thumb":{"url":"https://i.ibb.co/THUMB.png"},)"
        R"("url":"https://i.ibb.co/REAL.png","display_url":"https://i.ibb.co/DISPLAY.png"},)"
        R"("success":true})";
    CHECK(ExtractUploadLink(UploadService::ImgBB, body) ==
          std::wstring(L"https://i.ibb.co/REAL.png"));
}

CRISP_TEST(Upload, FreeImage_json_baglantisi) {
    const std::string body =
        R"({"status_code":200,"image":{"name":"a","url":"https://iili.io/a.png"}})";
    CHECK(ExtractUploadLink(UploadService::FreeImage, body) ==
          std::wstring(L"https://iili.io/a.png"));
}

CRISP_TEST(Upload, Bozuk_json_bos_doner) {
    CHECK(ExtractUploadLink(UploadService::Imgur, "").empty());
    CHECK(ExtractUploadLink(UploadService::Imgur, "{").empty());
    CHECK(ExtractUploadLink(UploadService::Imgur, R"({"data":{}})").empty());
    // Yol bir dizede bitmiyor: sayı, bağlantı değil.
    CHECK(ExtractUploadLink(UploadService::Imgur, R"({"data":{"link":42}})").empty());
}

CRISP_TEST(Upload, JsonFindString_yol_yurur) {
    const std::string body = R"({"a":{"b":{"c":"derin"}},"c":"sig"})";
    CHECK(JsonFindString(body, "a.b.c") == std::wstring(L"derin"));
    CHECK(JsonFindString(body, "c") == std::wstring(L"sig"));
    CHECK(JsonFindString(body, "a.c").empty());
    CHECK(JsonFindString(body, "yok").empty());
}

CRISP_TEST(Upload, JsonFindString_kacislari_cozer) {
    CHECK(JsonFindString(R"({"k":"a\/b"})", "k") == std::wstring(L"a/b"));
    CHECK(JsonFindString(R"({"k":"a\"b"})", "k") == std::wstring(L"a\"b"));
    // Anahtar adının içinde ayraç geçmesi yolu şaşırtmamalı.
    CHECK(JsonFindString(R"({"a":"{\"x\":1}","b":"deger"})", "b") ==
          std::wstring(L"deger"));
}

CRISP_TEST(Upload, Utf8_gidis_donusu) {
    const std::wstring text = L"ekran görüntüsü — ığüşöç";
    CHECK(Utf8ToWide(WideToUtf8(text)) == text);
    CHECK(WideToUtf8(L"").empty());
    CHECK(Utf8ToWide("").empty());
}
