// TestColorSpace.cpp — Renk dönüşümleri ve hex ayrıştırma.
#include "TestFramework.h"

#include "ColorSpace.h"

using namespace crisp;

namespace {

// Yuvarlama yüzünden gidiş-dönüş tam eşit çıkmayabilir; bir birimlik sapma
// gözle görülmez ve testin bunu hata sayması, gerçek bir hatayı gölgelerdi.
[[nodiscard]] bool NearColor(COLORREF a, COLORREF b) noexcept {
    auto close = [](int x, int y) { return x - y <= 1 && y - x <= 1; };
    return close(GetRValue(a), GetRValue(b)) && close(GetGValue(a), GetGValue(b)) &&
           close(GetBValue(a), GetBValue(b));
}

}  // namespace

CRISP_TEST(ColorSpace, Hsv_gidis_donusu_rengi_korur) {
    const COLORREF samples[] = {
        RGB(255, 0, 0),    RGB(0, 255, 0),   RGB(0, 0, 255),
        RGB(255, 255, 0),  RGB(0, 255, 255), RGB(255, 0, 255),
        RGB(30, 144, 255), RGB(18, 52, 86),  RGB(200, 120, 40)};
    for (const COLORREF sample : samples) {
        CHECK(NearColor(HsvToRgb(RgbToHsv(sample)), sample));
    }
}

CRISP_TEST(ColorSpace, Gri_tonlarin_doygunlugu_sifir) {
    for (const COLORREF grey : {RGB(0, 0, 0), RGB(128, 128, 128),
                                RGB(255, 255, 255)}) {
        const Hsv hsv = RgbToHsv(grey);
        CHECK(hsv.saturation == 0.0);
    }
    // Siyahın parlaklığı 0, beyazınki 1.
    CHECK(RgbToHsv(RGB(0, 0, 0)).value == 0.0);
    CHECK(RgbToHsv(RGB(255, 255, 255)).value == 1.0);
}

CRISP_TEST(ColorSpace, Ton_dereceleri_beklenen_yerde) {
    auto hue = [](COLORREF c) { return static_cast<int>(RgbToHsv(c).hue + 0.5); };
    CHECK_EQ(hue(RGB(255, 0, 0)), 0);
    CHECK_EQ(hue(RGB(255, 255, 0)), 60);
    CHECK_EQ(hue(RGB(0, 255, 0)), 120);
    CHECK_EQ(hue(RGB(0, 255, 255)), 180);
    CHECK_EQ(hue(RGB(0, 0, 255)), 240);
    CHECK_EQ(hue(RGB(255, 0, 255)), 300);
}

CRISP_TEST(ColorSpace, HsvToRgb_araligin_disini_kirpar) {
    // Kaydırıcı uçlara dayandığında ton 360'ı, doygunluk 1'i geçebilir;
    // kırpılmasaydı renk karta dışına taşıp siyaha dönerdi.
    CHECK(NearColor(HsvToRgb(Hsv{360.0, 1.0, 1.0}), RGB(255, 0, 0)));
    CHECK(NearColor(HsvToRgb(Hsv{-30.0, 1.0, 1.0}), RGB(255, 0, 128)));
    CHECK(NearColor(HsvToRgb(Hsv{200.0, 5.0, 5.0}), HsvToRgb(Hsv{200.0, 1.0, 1.0})));
    CHECK(NearColor(HsvToRgb(Hsv{200.0, -1.0, 0.5}), RGB(128, 128, 128)));
}

CRISP_TEST(ColorSpace, Hex_alti_ve_uc_hane) {
    COLORREF color = 0;
    CHECK(ParseHexColor(L"#1E90FF", color));
    CHECK_EQ(color, RGB(0x1E, 0x90, 0xFF));

    CHECK(ParseHexColor(L"1e90ff", color));
    CHECK_EQ(color, RGB(0x1E, 0x90, 0xFF));

    // Üç hane CSS kuralıyla ikilenir: #0f0 → #00ff00.
    CHECK(ParseHexColor(L"#0f0", color));
    CHECK_EQ(color, RGB(0, 255, 0));

    CHECK(ParseHexColor(L"  #abc  ", color));
    CHECK_EQ(color, RGB(0xAA, 0xBB, 0xCC));
}

CRISP_TEST(ColorSpace, Hex_gecersiz_girdi_ciktiya_dokunmaz) {
    COLORREF color = RGB(1, 2, 3);
    CHECK(!ParseHexColor(nullptr, color));
    CHECK(!ParseHexColor(L"", color));
    CHECK(!ParseHexColor(L"#12", color));
    CHECK(!ParseHexColor(L"#12345", color));
    CHECK(!ParseHexColor(L"#12345g", color));
    CHECK(!ParseHexColor(L"#1234567", color));
    // Kısmi girdi rengi sıfırlamamalı: kullanıcı her tuşta bir kez geçersiz
    // bir dize üretir ve alan kullanılamaz hâle gelirdi.
    CHECK_EQ(color, RGB(1, 2, 3));
}

CRISP_TEST(ColorSpace, Hex_bicimi_gidis_donusu) {
    const COLORREF color = RGB(0x0A, 0xB1, 0xC2);
    CHECK(FormatHexColor(color) == L"#0AB1C2");
    COLORREF parsed = 0;
    CHECK(ParseHexColor(FormatHexColor(color).c_str(), parsed));
    CHECK_EQ(parsed, color);
}

CRISP_TEST(ColorSpace, Okunakli_murekkep_secimi) {
    CHECK(PrefersDarkInk(RGB(255, 255, 255)));
    CHECK(PrefersDarkInk(RGB(255, 214, 10)));    // sarı: üstüne siyah yazılır
    CHECK(!PrefersDarkInk(RGB(0, 0, 0)));
    CHECK(!PrefersDarkInk(RGB(10, 132, 255)));   // mavi: üstüne beyaz yazılır
}

CRISP_TEST(ColorSpace, Kontrast_ayirt_edilebilirligi_olcer) {
    const COLORREF darkSurface = RGB(32, 32, 35);
    const COLORREF accent = RGB(10, 132, 255);
    const COLORREF red = RGB(255, 59, 48);

    // Kırmızı koyu zeminde okunur — "ikisi de koyu" diyen eski kural bunu
    // reddediyordu ve kalınlık listesi kullanıcının rengini göstermiyordu.
    CHECK(HasContrast(red, darkSurface));
    // Vurgu mavisiyle aynı kırmızı ayırt edilemez.
    CHECK(!HasContrast(red, accent));
    // Uç durumlar.
    CHECK(HasContrast(RGB(255, 255, 255), RGB(0, 0, 0)));
    CHECK(!HasContrast(RGB(70, 70, 70), RGB(75, 75, 75)));
    // Simetrik olmalı: sıranın önemi yok.
    CHECK(HasContrast(darkSurface, red) == HasContrast(red, darkSurface));
}
