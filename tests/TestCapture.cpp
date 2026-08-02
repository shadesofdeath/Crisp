// TestCapture.cpp — Image ve ekran yakalama.
//
// Bu testler GERÇEK bir masaüstü oturumu gerektirir; başsız bir CI ajanında
// yakalama boş dönerdi. Ekran içeriğine BAĞIMLI hiçbir doğrulama yoktur —
// yalnızca boyut, alfa ve sınır davranışı sınanır, çünkü ekranda ne olduğunu
// test bilemez.
#include "TestFramework.h"

#include "Capture.h"
#include "Geometry.h"

using namespace crisp;

CRISP_TEST(Image, Create_gecerli_boyut) {
    Image image;
    CHECK(image.Create(64, 32));
    CHECK(image.Valid());
    CHECK_EQ(image.Width(), 64);
    CHECK_EQ(image.Height(), 32);
    CHECK_EQ(image.Stride(), 64 * 4);
    CHECK(image.Bits() != nullptr);
    CHECK(image.Handle() != nullptr);
}

CRISP_TEST(Image, Create_gecersiz_boyut_reddedilir) {
    Image image;
    CHECK(!image.Create(0, 10));
    CHECK(!image.Valid());
    CHECK(!image.Create(10, 0));
    CHECK(!image.Create(-5, 10));
    // Kenar sınırı: 32767 üstü reddedilmeli (4 bayt/piksel çarpımı taşmasın)
    CHECK(!image.Create(40000, 10));
}

CRISP_TEST(Image, Create_onceki_icerigi_birakir) {
    Image image;
    CHECK(image.Create(10, 10));
    const HBITMAP first = image.Handle();
    CHECK(image.Create(20, 20));
    CHECK_EQ(image.Width(), 20);
    // Yeni tahsis, eskisinden farklı bir nesne olmalı.
    CHECK(image.Handle() != first);
}

CRISP_TEST(Image, Fill_ve_Pixel_gidis_donus) {
    Image image;
    CHECK(image.Create(8, 4));
    image.Fill(0xFF112233u);

    CHECK_EQ(image.Pixel(0, 0), 0xFF112233u);
    CHECK_EQ(image.Pixel(7, 3), 0xFF112233u);

    image.SetPixel(3, 2, 0xFFAABBCCu);
    CHECK_EQ(image.Pixel(3, 2), 0xFFAABBCCu);
    // Komşular etkilenmemeli
    CHECK_EQ(image.Pixel(2, 2), 0xFF112233u);
    CHECK_EQ(image.Pixel(4, 2), 0xFF112233u);
    CHECK_EQ(image.Pixel(3, 1), 0xFF112233u);
}

CRISP_TEST(Image, Pixel_sinir_disi_sifir_doner_yazma_yok_sayilir) {
    Image image;
    CHECK(image.Create(4, 4));
    image.Fill(0xFF000000u);

    CHECK_EQ(image.Pixel(-1, 0), 0u);
    CHECK_EQ(image.Pixel(0, -1), 0u);
    CHECK_EQ(image.Pixel(4, 0), 0u);
    CHECK_EQ(image.Pixel(0, 4), 0u);

    // Sınır dışına yazmak bellek bozmamalı; sonrasında görüntü hâlâ tutarlı.
    image.SetPixel(100, 100, 0xFFFFFFFFu);
    image.SetPixel(-5, -5, 0xFFFFFFFFu);
    CHECK_EQ(image.Pixel(0, 0), 0xFF000000u);
    CHECK_EQ(image.Pixel(3, 3), 0xFF000000u);
}

CRISP_TEST(Image, Top_down_yerlesim_satir_sirasi) {
    // Satır 0 ÜST satır olmalı. Bottom-up bir DIB'de bu test, görüntünün baş
    // aşağı olduğunu gösterirdi.
    Image image;
    CHECK(image.Create(2, 2));
    image.Fill(0u);
    image.SetPixel(0, 0, 0xFF010101u);

    const auto* pixels = static_cast<const uint32_t*>(image.Bits());
    // Bellekteki İLK piksel, (0,0) yani sol-üst olmalı.
    CHECK_EQ(pixels[0], 0xFF010101u);
}

CRISP_TEST(Image, Tasima_sahipligi_devreder) {
    Image source;
    CHECK(source.Create(16, 16));
    source.Fill(0xFF445566u);
    const HBITMAP handle = source.Handle();

    Image target = std::move(source);
    CHECK(target.Valid());
    CHECK_EQ(target.Handle() == handle, 1);
    CHECK_EQ(target.Pixel(1, 1), 0xFF445566u);
    // Kaynak artık sahip değil
    CHECK(!source.Valid());
}

CRISP_TEST(Crop, Dogru_bolgeyi_kopyalar) {
    Image source;
    CHECK(source.Create(10, 10));
    // Her piksele koordinatını kodla: yanlış bölge kopyalanırsa hemen belli olur.
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            source.SetPixel(x, y, 0xFF000000u | (static_cast<uint32_t>(x) << 8) |
                                      static_cast<uint32_t>(y));
        }
    }

    Image cropped;
    CHECK(CropImage(source, 3, 4, 5, 2, cropped));
    CHECK_EQ(cropped.Width(), 5);
    CHECK_EQ(cropped.Height(), 2);

    // Kırpılanın (0,0)'ı kaynağın (3,4)'ü olmalı.
    CHECK_EQ(cropped.Pixel(0, 0), 0xFF000000u | (3u << 8) | 4u);
    CHECK_EQ(cropped.Pixel(4, 1), 0xFF000000u | (7u << 8) | 5u);
}

CRISP_TEST(Crop, Tam_kaynak_kopyalanabilir) {
    Image source;
    CHECK(source.Create(6, 6));
    source.Fill(0xFF334455u);

    Image cropped;
    CHECK(CropImage(source, 0, 0, 6, 6, cropped));
    CHECK_EQ(cropped.Width(), 6);
    CHECK_EQ(cropped.Pixel(5, 5), 0xFF334455u);
}

CRISP_TEST(Crop, Sinir_disi_bolge_reddedilir) {
    // Sessizce kırpmak, kullanıcının seçtiğinden FARKLI bir görüntü döndürmek
    // olurdu; hata döndürmek tek doğru davranış.
    Image source;
    CHECK(source.Create(10, 10));

    Image cropped;
    CHECK(!CropImage(source, -1, 0, 5, 5, cropped));
    CHECK(!CropImage(source, 0, -1, 5, 5, cropped));
    CHECK(!CropImage(source, 6, 0, 5, 5, cropped));    // 6+5 > 10
    CHECK(!CropImage(source, 0, 6, 5, 5, cropped));
    CHECK(!CropImage(source, 0, 0, 0, 5, cropped));    // sıfır genişlik
    CHECK(!CropImage(source, 0, 0, 5, 0, cropped));
}

CRISP_TEST(Crop, Gecersiz_kaynak_ve_kendine_kopyalama_reddedilir) {
    const Image empty;
    Image out;
    CHECK(!CropImage(empty, 0, 0, 1, 1, out));

    Image source;
    CHECK(source.Create(4, 4));
    CHECK(!CropImage(source, 0, 0, 2, 2, source));   // aynı nesne
}

CRISP_TEST(Capture, VirtualScreenRect_makul) {
    const RECT screen = VirtualScreenRect();
    CHECK(!geom::IsEmpty(screen));
    CHECK(geom::Width(screen) >= 640);
    CHECK(geom::Height(screen) >= 480);
}

CRISP_TEST(Capture, MonitorRectAtCursor_sanal_ekranin_icinde) {
    const RECT screen = VirtualScreenRect();
    const RECT monitor = MonitorRectAtCursor();
    CHECK(!geom::IsEmpty(monitor));
    CHECK(monitor.left >= screen.left);
    CHECK(monitor.top >= screen.top);
    CHECK(monitor.right <= screen.right);
    CHECK(monitor.bottom <= screen.bottom);
}

CRISP_TEST(Capture, MonitorRectAtPoint_tek_bir_monitor_dondurur) {
    // BU TESTİN VAR OLMA SEBEBİ BİR HATA. Seçim kaplamasının ipucu kutusu,
    // değişken adı `monitor` olan ama aslında SANAL EKRANI tutan bir
    // dikdörtgene ortalanıyordu. Tek monitörde ikisi aynı şeydir ve hata
    // görünmez; iki monitörde sanal ekranın yatay ortası tam olarak iki ekranın
    // birleştiği yerdir, kutu da ikiye bölünür.
    //
    // O yüzden asıl sınanan "boş değil" değil, ŞU: dönen dikdörtgen gerçek
    // monitörlerden BİRİ olmalı, hiçbir zaman onların birleşimi. Tek monitörlü
    // bir makinede bu sessizce geçer; iki monitörlü bir makinede eski
    // davranışı yakalar.
    const std::vector<RECT> monitors = MonitorRects();
    CHECK(!monitors.empty());

    for (const RECT& monitor : monitors) {
        const POINT center{monitor.left + geom::Width(monitor) / 2,
                           monitor.top + geom::Height(monitor) / 2};
        const RECT found = MonitorRectAtPoint(center);
        CHECK(found.left == monitor.left);
        CHECK(found.top == monitor.top);
        CHECK(found.right == monitor.right);
        CHECK(found.bottom == monitor.bottom);
    }

    // Sanal ekranın çok dışındaki bir nokta da bir monitör almalı: en yakını.
    // Boş dikdörtgen dönseydi, ona ortalanan her kutu sol üst köşeye yığılırdı.
    //
    // Değişken adı `far` DEĞİL: `far` 16-bit Windows'tan kalma bir makro ve
    // <windows.h> onu hâlâ tanımlıyor. `POINT far{...}` sessizce `POINT {...}`
    // olur, çağrı da bağımsız değişkensiz kalır.
    const RECT screen = VirtualScreenRect();
    const POINT outside{screen.right + 10000, screen.bottom + 10000};
    const RECT nearest = MonitorRectAtPoint(outside);
    CHECK(!geom::IsEmpty(nearest));

    bool isARealMonitor = false;
    for (const RECT& monitor : monitors) {
        if (nearest.left == monitor.left && nearest.top == monitor.top &&
            nearest.right == monitor.right && nearest.bottom == monitor.bottom) {
            isARealMonitor = true;
        }
    }
    CHECK(isARealMonitor);
}

CRISP_TEST(Capture, CaptureRect_istenen_boyutu_verir) {
    const RECT area{0, 0, 200, 120};
    Image image;
    CHECK(CaptureRect(area, image));
    CHECK(image.Valid());
    CHECK_EQ(image.Width(), 200);
    CHECK_EQ(image.Height(), 120);
}

CRISP_TEST(Capture, CaptureRect_alfa_opak_yazilir) {
    // BitBlt 32 bpp hedefte alfayı TANIMSIZ bırakır. Düzeltilmezse PNG tamamen
    // saydam çıkar ve kullanıcı boş bir görüntü yapıştırır — sessiz ve
    // tespit edilmesi zor bir hata.
    Image image;
    CHECK(CaptureRect(RECT{0, 0, 32, 32}, image));
    CHECK(image.Valid());

    bool allOpaque = true;
    for (int y = 0; y < image.Height(); ++y) {
        for (int x = 0; x < image.Width(); ++x) {
            if ((image.Pixel(x, y) >> 24) != 0xFFu) {
                allOpaque = false;
            }
        }
    }
    CHECK(allOpaque);
}

CRISP_TEST(Capture, CaptureRect_bos_dikdortgen_reddedilir) {
    Image image;
    CHECK(!CaptureRect(RECT{100, 100, 100, 200}, image));   // sıfır genişlik
    CHECK(!CaptureRect(RECT{100, 100, 200, 100}, image));   // sıfır yükseklik
    CHECK(!CaptureRect(RECT{200, 200, 100, 100}, image));   // ters
}

CRISP_TEST(Capture, CaptureRect_tek_piksel) {
    Image image;
    CHECK(CaptureRect(RECT{0, 0, 1, 1}, image));
    CHECK_EQ(image.Width(), 1);
    CHECK_EQ(image.Height(), 1);
}

CRISP_TEST(Capture, CaptureWindow_gecersiz_pencere_reddedilir) {
    Image image;
    CHECK(!CaptureWindow(nullptr, image));
    CHECK(!CaptureWindow(reinterpret_cast<HWND>(static_cast<INT_PTR>(0xDEAD)), image));
}

CRISP_TEST(Capture, CaptureWindow_masaustu_penceresi) {
    // Masaüstü daima vardır ve ekran kadar büyüktür; yakalama boyutu sanal
    // ekranı aşmamalı (CaptureWindow taşan kısmı hapsediyor).
    const HWND desktop = ::GetDesktopWindow();
    Image image;
    CHECK(CaptureWindow(desktop, image));
    CHECK(image.Valid());

    const RECT screen = VirtualScreenRect();
    CHECK(image.Width() <= geom::Width(screen));
    CHECK(image.Height() <= geom::Height(screen));
}
