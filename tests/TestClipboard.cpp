// TestClipboard.cpp — Pano gidiş-dönüşü.
//
// Bu testler SİSTEM PANOSUNU DEĞİŞTİRİR. Koşu sonunda panoda test görüntüsü
// kalır; bu, testlerin gerçek pano API'sini kullanmasının kaçınılmaz bedeli.
// Sahte bir pano katmanı yazmak, tam da doğrulamak istediğimiz şeyi —
// CF_DIB'in gerçekten yazılıp okunabildiğini — sınamayı bırakırdı.
#include "TestFramework.h"

#include "ClipboardImage.h"
#include "ImageCodec.h"

using namespace crisp;

namespace {

void PaintGradient(Image& image) {
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            const uint32_t r = static_cast<uint32_t>(x * 3 + 1) & 0xFFu;
            const uint32_t g = static_cast<uint32_t>(y * 5 + 2) & 0xFFu;
            const uint32_t b = static_cast<uint32_t>(x + y) & 0xFFu;
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

}  // namespace

CRISP_TEST(Clipboard, Gidis_donusu_pikselleri_korur) {
    Image original;
    CHECK(original.Create(41, 27));
    PaintGradient(original);

    CHECK(CopyImageToClipboard(original, nullptr));
    CHECK(ClipboardHasImage());

    Image readBack;
    CHECK(ReadImageFromClipboard(readBack, nullptr));
    CHECK_EQ(readBack.Width(), 41);
    CHECK_EQ(readBack.Height(), 27);
    CHECK(ImagesIdentical(original, readBack));
}

CRISP_TEST(Clipboard, Iki_bicim_birden_yazilir) {
    // CF_DIB eski Win32 uygulamaları, "PNG" modern web uygulamaları için.
    // Yalnızca biri yazılsaydı kullanıcıların bir kısmı bozuk yapıştırırdı.
    Image image;
    CHECK(image.Create(12, 12));
    image.Fill(0xFF3366CCu);

    CHECK(CopyImageToClipboard(image, nullptr));

    const UINT pngFormat = ::RegisterClipboardFormatW(L"PNG");
    CHECK(pngFormat != 0);

    if (::OpenClipboard(nullptr)) {
        const bool hasDib = ::IsClipboardFormatAvailable(CF_DIB) != FALSE;
        const bool hasPng = ::IsClipboardFormatAvailable(pngFormat) != FALSE;
        ::CloseClipboard();
        CHECK(hasDib);
        CHECK(hasPng);
    } else {
        CHECK(false);   // pano açılamadı, test bir şey doğrulayamadı
    }
}

CRISP_TEST(Clipboard, Dib_bicimi_dogru_yonde_yazilir) {
    // CF_DIB bottom-up yazılır (biHeight pozitif). Bunu doğrulamak için üst
    // satırı işaretleyip HAM bloğa bakarız: bottom-up ise işaretli satır
    // bellekte EN SONDA olmalı. Yanlış yön, Paint'e yapıştırınca baş aşağı
    // görüntü demektir ve gidiş-dönüş testi bunu YAKALAYAMAZ (kendi hatamızı
    // kendi okuyucumuzla tersine çevirir).
    Image image;
    CHECK(image.Create(4, 3));
    image.Fill(0xFF000000u);
    for (int x = 0; x < 4; ++x) {
        image.SetPixel(x, 0, 0xFFFFFFFFu);   // üst satır beyaz
    }

    CHECK(CopyImageToClipboard(image, nullptr));

    if (!::OpenClipboard(nullptr)) {
        CHECK(false);
        return;
    }

    const HANDLE handle = ::GetClipboardData(CF_DIB);
    CHECK(handle != nullptr);
    if (handle != nullptr) {
        const auto* header = static_cast<const BITMAPINFOHEADER*>(::GlobalLock(handle));
        CHECK(header != nullptr);
        if (header != nullptr) {
            CHECK_EQ(header->biWidth, 4);
            CHECK_EQ(header->biHeight, 3);      // POZİTİF = bottom-up
            CHECK_EQ(header->biBitCount, 32);

            const auto* pixels = reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(header) + header->biSize);
            // Bottom-up: bellekteki SON satır, görüntünün ÜST satırıdır.
            const uint32_t lastRowFirstPixel = pixels[2 * 4];
            CHECK_EQ(lastRowFirstPixel & 0x00FFFFFFu, 0x00FFFFFFu);
            // Bellekteki İLK satır, görüntünün ALT satırı: siyah.
            CHECK_EQ(pixels[0] & 0x00FFFFFFu, 0x00000000u);
            ::GlobalUnlock(handle);
        }
    }
    ::CloseClipboard();
}

CRISP_TEST(Clipboard, Gecersiz_goruntu_reddedilir) {
    const Image empty;
    CHECK(!CopyImageToClipboard(empty, nullptr));
}

CRISP_TEST(Clipboard, Tek_piksel_goruntu) {
    Image image;
    CHECK(image.Create(1, 1));
    image.SetPixel(0, 0, 0xFFABCDEFu);

    CHECK(CopyImageToClipboard(image, nullptr));

    Image readBack;
    CHECK(ReadImageFromClipboard(readBack, nullptr));
    CHECK_EQ(readBack.Width(), 1);
    CHECK_EQ(readBack.Height(), 1);
    CHECK_EQ(readBack.Pixel(0, 0), 0xFFABCDEFu);
}

CRISP_TEST(Clipboard, Metin_kopyalama) {
    const wchar_t* text = L"Crisp OCR testi — ığüşöç ĞÜŞÖÇ";
    CHECK(CopyTextToClipboard(text, nullptr));

    if (!::OpenClipboard(nullptr)) {
        CHECK(false);
        return;
    }
    CHECK(::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE);

    const HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
    CHECK(handle != nullptr);
    if (handle != nullptr) {
        const auto* stored = static_cast<const wchar_t*>(::GlobalLock(handle));
        CHECK(stored != nullptr);
        if (stored != nullptr) {
            CHECK(::wcscmp(stored, text) == 0);
            ::GlobalUnlock(handle);
        }
    }
    ::CloseClipboard();
}

CRISP_TEST(Clipboard, Metin_yazmak_goruntuyu_temizler) {
    // EmptyClipboard çağrılmazsa panoda hem eski görüntü hem yeni metin kalır
    // ve yapıştıran uygulama hangisini alacağını bilemez.
    Image image;
    CHECK(image.Create(8, 8));
    image.Fill(0xFF808080u);
    CHECK(CopyImageToClipboard(image, nullptr));
    CHECK(ClipboardHasImage());

    CHECK(CopyTextToClipboard(L"artik metin", nullptr));
    CHECK(!ClipboardHasImage());
}

CRISP_TEST(Clipboard, Nullptr_metin_reddedilir) {
    CHECK(!CopyTextToClipboard(nullptr, nullptr));
}
