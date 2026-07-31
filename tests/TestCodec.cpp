// TestCodec.cpp — PNG kodlama/çözme. Asıl soru: pikseller gidip geri geldiğinde
// AYNI mı? Boyut ve HRESULT kontrolü, ters çevrilmiş ya da kanalları karışmış
// bir görüntüyü yakalayamaz.
#include "TestFramework.h"

#include "ImageCodec.h"
#include "Util.h"

#include <string>
#include <vector>

using namespace crisp;

namespace {

// Her pikseli farklı olan bir desen: kanal karışması (BGR↔RGB) ve satır
// çevrilmesi bu desende hemen ortaya çıkar, düz renkte çıkmaz.
void PaintTestPattern(Image& image) {
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            const uint32_t r = static_cast<uint32_t>(x * 7 + 3) & 0xFFu;
            const uint32_t g = static_cast<uint32_t>(y * 11 + 5) & 0xFFu;
            const uint32_t b = static_cast<uint32_t>(x * y + 17) & 0xFFu;
            image.SetPixel(x, y, 0xFF000000u | (r << 16) | (g << 8) | b);
        }
    }
}

[[nodiscard]] bool ImagesIdentical(const Image& a, const Image& b) {
    if (a.Width() != b.Width() || a.Height() != b.Height()) {
        return false;
    }
    for (int y = 0; y < a.Height(); ++y) {
        for (int x = 0; x < a.Width(); ++x) {
            if (a.Pixel(x, y) != b.Pixel(x, y)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::wstring TempFile(const wchar_t* name) {
    std::wstring path{test::TempDirectory()};
    path += L'\\';
    path += name;
    return path;
}

}  // namespace

CRISP_TEST(Codec, EncodePng_png_imzasi_uretir) {
    Image image;
    CHECK(image.Create(16, 16));
    image.Fill(0xFF7F7F7Fu);

    std::vector<uint8_t> png;
    CHECK(EncodePng(image, png));
    CHECK(png.size() > 8);

    // PNG imzası: 89 50 4E 47 0D 0A 1A 0A
    if (png.size() >= 8) {
        CHECK_EQ(png[0], 0x89);
        CHECK_EQ(png[1], 0x50);
        CHECK_EQ(png[2], 0x4E);
        CHECK_EQ(png[3], 0x47);
        CHECK_EQ(png[4], 0x0D);
        CHECK_EQ(png[5], 0x0A);
        CHECK_EQ(png[6], 0x1A);
        CHECK_EQ(png[7], 0x0A);
    }
}

CRISP_TEST(Codec, EncodePng_gecersiz_goruntu_reddedilir) {
    const Image empty;
    std::vector<uint8_t> png;
    CHECK(!EncodePng(empty, png));
    CHECK(png.empty());
}

CRISP_TEST(Codec, Bellek_gidis_donusu_pikselleri_korur) {
    Image original;
    CHECK(original.Create(37, 23));   // asal olmayan, tek sayı: hizalama tuzağı
    PaintTestPattern(original);

    std::vector<uint8_t> png;
    CHECK(EncodePng(original, png));

    Image decoded;
    CHECK(DecodePng(png.data(), png.size(), decoded));
    CHECK_EQ(decoded.Width(), 37);
    CHECK_EQ(decoded.Height(), 23);
    CHECK(ImagesIdentical(original, decoded));
}

CRISP_TEST(Codec, Gidis_donusu_satir_sirasini_korur) {
    // Yalnızca üst satırı beyaz olan bir görüntü. Kodlayıcı ya da çözücü
    // satırları ters çevirseydi beyaz satır ALTA düşerdi ve bu test yakalardı.
    Image original;
    CHECK(original.Create(8, 8));
    original.Fill(0xFF000000u);
    for (int x = 0; x < 8; ++x) {
        original.SetPixel(x, 0, 0xFFFFFFFFu);
    }

    std::vector<uint8_t> png;
    CHECK(EncodePng(original, png));

    Image decoded;
    CHECK(DecodePng(png.data(), png.size(), decoded));
    CHECK_EQ(decoded.Pixel(0, 0), 0xFFFFFFFFu);
    CHECK_EQ(decoded.Pixel(7, 0), 0xFFFFFFFFu);
    CHECK_EQ(decoded.Pixel(0, 7), 0xFF000000u);
}

CRISP_TEST(Codec, Gidis_donusu_kanal_sirasini_korur) {
    // Saf kırmızı. Kanallar karışsaydı mavi ya da yeşil olarak dönerdi.
    Image original;
    CHECK(original.Create(4, 4));
    original.Fill(0xFFFF0000u);   // opak kırmızı

    std::vector<uint8_t> png;
    CHECK(EncodePng(original, png));

    Image decoded;
    CHECK(DecodePng(png.data(), png.size(), decoded));
    CHECK_EQ(decoded.Pixel(2, 2), 0xFFFF0000u);
}

CRISP_TEST(Codec, DecodePng_bozuk_veri_reddedilir) {
    const uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    Image decoded;
    CHECK(!DecodePng(garbage, sizeof(garbage), decoded));
    CHECK(!decoded.Valid());

    CHECK(!DecodePng(nullptr, 0, decoded));
    CHECK(!DecodePng(garbage, 0, decoded));
}

CRISP_TEST(Codec, DecodePng_kirpilmis_png_reddedilir) {
    Image original;
    CHECK(original.Create(16, 16));
    PaintTestPattern(original);

    std::vector<uint8_t> png;
    CHECK(EncodePng(original, png));
    CHECK(png.size() > 20);

    // Yarısını kes: imza doğru ama veri eksik.
    png.resize(png.size() / 2);

    Image decoded;
    CHECK(!DecodePng(png.data(), png.size(), decoded));
}

CRISP_TEST(Codec, SavePng_ve_LoadPng_dosya_gidis_donusu) {
    Image original;
    CHECK(original.Create(29, 17));
    PaintTestPattern(original);

    const std::wstring path = TempFile(L"roundtrip.png");
    CHECK(SavePng(original, path));
    CHECK(::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);

    Image loaded;
    CHECK(LoadPng(path, loaded));
    CHECK(ImagesIdentical(original, loaded));
}

CRISP_TEST(Codec, SavePng_olmayan_alt_klasoru_olusturur) {
    std::wstring path{test::TempDirectory()};
    path += L"\\yeni\\alt\\klasor\\resim.png";

    Image image;
    CHECK(image.Create(4, 4));
    image.Fill(0xFF00FF00u);

    CHECK(SavePng(image, path));
    CHECK(::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);

    // Testin bıraktığı ağaç temizlenir; TestMain yalnızca düz dosyaları siler.
    ::DeleteFileW(path.c_str());
    std::wstring dir{test::TempDirectory()};
    ::RemoveDirectoryW((dir + L"\\yeni\\alt\\klasor").c_str());
    ::RemoveDirectoryW((dir + L"\\yeni\\alt").c_str());
    ::RemoveDirectoryW((dir + L"\\yeni").c_str());
}

CRISP_TEST(Codec, LoadPng_olmayan_dosya_reddedilir) {
    Image image;
    CHECK(!LoadPng(TempFile(L"yok_boyle_bir_dosya.png"), image));
    CHECK(!LoadPng(L"", image));
}

CRISP_TEST(Codec, Tek_piksel_goruntu) {
    Image original;
    CHECK(original.Create(1, 1));
    original.SetPixel(0, 0, 0xFF123456u);

    std::vector<uint8_t> png;
    CHECK(EncodePng(original, png));

    Image decoded;
    CHECK(DecodePng(png.data(), png.size(), decoded));
    CHECK_EQ(decoded.Width(), 1);
    CHECK_EQ(decoded.Height(), 1);
    CHECK_EQ(decoded.Pixel(0, 0), 0xFF123456u);
}

CRISP_TEST(Codec, FormatFromPath_uzantiyi_tanir) {
    CHECK(FormatFromPath(L"C:\\a\\b.png") == ImageFormat::Png);
    CHECK(FormatFromPath(L"C:\\a\\b.jpg") == ImageFormat::Jpeg);
    CHECK(FormatFromPath(L"C:\\a\\b.jpeg") == ImageFormat::Jpeg);
    CHECK(FormatFromPath(L"C:\\a\\b.webp") == ImageFormat::WebP);
    // Büyük harf de tanınmalı: kullanıcı ".PNG" yazabilir.
    CHECK(FormatFromPath(L"C:\\a\\b.JPG") == ImageFormat::Jpeg);
    // Bilinmeyen ve uzantısız PNG'ye düşer — sessizce kodlanamayan bir biçim
    // seçmektense bilinen bir biçime düşmek doğru.
    CHECK(FormatFromPath(L"C:\\a\\b.tiff") == ImageFormat::Png);
    CHECK(FormatFromPath(L"C:\\a\\b") == ImageFormat::Png);
}

CRISP_TEST(Codec, ExtensionForFormat_gidis_donus) {
    CHECK(FormatFromString(ExtensionForFormat(ImageFormat::Png)) == ImageFormat::Png);
    CHECK(FormatFromString(ExtensionForFormat(ImageFormat::Jpeg)) == ImageFormat::Jpeg);
    CHECK(FormatFromString(ExtensionForFormat(ImageFormat::WebP)) == ImageFormat::WebP);
    CHECK(FormatFromString(nullptr) == ImageFormat::Png);
    CHECK(FormatFromString(L"saçmalık") == ImageFormat::Png);
}

CRISP_TEST(Codec, PNG_ve_JPEG_daima_kullanilabilir) {
    // Windows bu iki kodlayıcıyı her zaman taşır; taşımadığı bir kurulumda
    // aracın kaydetme özelliği zaten çalışmazdı ve bunu bilmek gerekir.
    CHECK(IsFormatAvailable(ImageFormat::Png));
    CHECK(IsFormatAvailable(ImageFormat::Jpeg));
}

CRISP_TEST(Codec, JPEG_kaydedilebilir_ve_geri_okunabilir) {
    Image original;
    CHECK(original.Create(48, 32));
    PaintTestPattern(original);

    const std::wstring path = TempFile(L"kalite.jpg");
    CHECK(SaveImage(original, path, ImageFormat::Jpeg, 92));
    CHECK(::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);

    // JPEG KAYIPLIDIR: pikseller birebir aynı OLMAZ. Doğrulanabilecek şey
    // boyutun korunması ve dosyanın gerçekten çözülebilmesi.
    Image loaded;
    CHECK(LoadPng(path, loaded));   // WIC kapsayıcıyı kendisi tanır
    CHECK_EQ(loaded.Width(), 48);
    CHECK_EQ(loaded.Height(), 32);
}

CRISP_TEST(Codec, JPEG_kalitesi_dosya_boyutunu_degistirir) {
    // Kalite ayarının gerçekten kodlayıcıya ULAŞTIĞINI kanıtlar. Özellik
    // torbası Initialize'dan sonra yazılsaydı sessizce yok sayılır ve iki
    // dosya aynı boyutta çıkardı.
    Image original;
    CHECK(original.Create(120, 90));
    PaintTestPattern(original);

    const std::wstring low = TempFile(L"dusuk.jpg");
    const std::wstring high = TempFile(L"yuksek.jpg");
    CHECK(SaveImage(original, low, ImageFormat::Jpeg, 10));
    CHECK(SaveImage(original, high, ImageFormat::Jpeg, 100));

    WIN32_FILE_ATTRIBUTE_DATA lowInfo{};
    WIN32_FILE_ATTRIBUTE_DATA highInfo{};
    CHECK(::GetFileAttributesExW(low.c_str(), GetFileExInfoStandard, &lowInfo));
    CHECK(::GetFileAttributesExW(high.c_str(), GetFileExInfoStandard, &highInfo));
    CHECK(lowInfo.nFileSizeLow < highInfo.nFileSizeLow);
}

CRISP_TEST(Codec, Genis_ve_ince_goruntular) {
    // 1 piksel yüksekliğinde çok geniş ve tersi: stride hesabındaki bir hata
    // burada patlar.
    for (const auto& size : {SIZE{500, 1}, SIZE{1, 500}}) {
        Image original;
        CHECK(original.Create(size.cx, size.cy));
        PaintTestPattern(original);

        std::vector<uint8_t> png;
        CHECK(EncodePng(original, png));

        Image decoded;
        CHECK(DecodePng(png.data(), png.size(), decoded));
        CHECK(ImagesIdentical(original, decoded));
    }
}
