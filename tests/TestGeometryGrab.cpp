// TestGeometryGrab.cpp — Seçim tutamaklarının aritmetiği.
//
// AYRI DOSYA: tests/TestGeometry.cpp 411 satır, ev kuralının sınırının zaten
// üstünde (docs §9); büyütmek yerine yeni konu yeni dosyaya gidiyor.
//
// Kaplamanın MESAJ akışının testi yok ve olamaz — pencere, fare yakalama ve
// gerçek bir masaüstü gerektirir. Bu yüzden ilginç aritmetiğin tamamı
// bilinçli olarak buraya, saf fonksiyonlara taşındı: sınanamayan kısım artık
// yalnızca "hangi fonksiyon ne zaman çağrılıyor".
#include "TestFramework.h"

#include "Geometry.h"

using namespace crisp;
using namespace crisp::geom;

namespace {

constexpr RECT kScreen{0, 0, 1920, 1080};
// Sol üstteki monitör birincil değilse koordinatlar negatif olur; tutamak
// aritmetiğinin de bunu kaldırması gerekiyor.
constexpr RECT kMultiScreen{-1920, 0, 1920, 1080};

constexpr LONG kHandle = 13;   // 96 dpi'de HandleGrabSize
constexpr LONG kMinSide = 6;

}  // namespace

// --- HandleRects -----------------------------------------------------------

CRISP_TEST(GeometryGrab, Tutamak_sayisi_boyuta_gore_azalir) {
    RECT boxes[8]{};
    Grab grabs[8]{};

    // 3*13 = 39'un altı: hiç tutamak yok, dikdörtgenin tamamı taşımaya kalır.
    CHECK_EQ(HandleRects(RECT{0, 0, 38, 200}, kHandle, boxes, grabs), 0);
    CHECK_EQ(HandleRects(RECT{0, 0, 200, 38}, kHandle, boxes, grabs), 0);

    // 39 ile 65 arası: yalnız köşeler.
    CHECK_EQ(HandleRects(RECT{0, 0, 39, 39}, kHandle, boxes, grabs), 4);
    CHECK_EQ(HandleRects(RECT{0, 0, 64, 200}, kHandle, boxes, grabs), 4);

    // 5*13 = 65 ve üstü: sekizi de.
    CHECK_EQ(HandleRects(RECT{0, 0, 65, 65}, kHandle, boxes, grabs), 8);
    CHECK_EQ(HandleRects(RECT{0, 0, 400, 300}, kHandle, boxes, grabs), 8);
}

CRISP_TEST(GeometryGrab, Sekiz_tutamak_birbirini_ortmez) {
    // Eşiklerin varlık sebebi bu: üst üste binen iki tutamakta hangisini
    // tuttuğunuz sıraya kalır ve kullanıcı bunu rastgele davranış olarak görür.
    RECT boxes[8]{};
    Grab grabs[8]{};
    const int count = HandleRects(RECT{100, 100, 165, 165}, kHandle, boxes, grabs);
    CHECK_EQ(count, 8);

    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            const bool disjoint = boxes[i].right <= boxes[j].left ||
                                  boxes[j].right <= boxes[i].left ||
                                  boxes[i].bottom <= boxes[j].top ||
                                  boxes[j].bottom <= boxes[i].top;
            CHECK(disjoint);
        }
    }
}

CRISP_TEST(GeometryGrab, Tutamaklar_kose_merkezli) {
    RECT boxes[8]{};
    Grab grabs[8]{};
    CHECK_EQ(HandleRects(RECT{100, 100, 400, 300}, kHandle, boxes, grabs), 8);

    // İlk kutu NW ve seçimin sol üst köşesinde ORTALANMIŞ olmalı: kenarın
    // içine kaydırılmış bir tutamak, köşeyi tutmak isteyen kullanıcının
    // ıskaladığı bir tutamaktır.
    CHECK(grabs[0] == Grab::NW);
    CHECK_RECT(boxes[0], 100 - kHandle / 2, 100 - kHandle / 2,
               100 - kHandle / 2 + kHandle, 100 - kHandle / 2 + kHandle);
    CHECK(grabs[3] == Grab::SE);
}

// --- HitTestSelection ------------------------------------------------------

CRISP_TEST(GeometryGrab, Isabet_once_koseye_bakar) {
    const RECT selection{100, 100, 400, 300};
    CHECK(HitTestSelection(selection, POINT{100, 100}, kHandle) == Grab::NW);
    CHECK(HitTestSelection(selection, POINT{400, 300}, kHandle) == Grab::SE);
    CHECK(HitTestSelection(selection, POINT{250, 100}, kHandle) == Grab::N);
    CHECK(HitTestSelection(selection, POINT{100, 200}, kHandle) == Grab::W);
}

CRISP_TEST(GeometryGrab, Ic_bolge_tasimadir_disi_hicbir_sey) {
    const RECT selection{100, 100, 400, 300};
    CHECK(HitTestSelection(selection, POINT{250, 200}, kHandle) == Grab::Move);
    CHECK(HitTestSelection(selection, POINT{50, 50}, kHandle) == Grab::None);
    CHECK(HitTestSelection(selection, POINT{500, 200}, kHandle) == Grab::None);
}

CRISP_TEST(GeometryGrab, Kucuk_secimin_tamami_tasimadir) {
    // Tutamak sunulmayan boyutlarda kullanıcı sıkışıp kalmamalı: 20x20 bir
    // seçimin her noktası taşımadır, kenarı dâhil.
    const RECT tiny{100, 100, 120, 120};
    CHECK(HitTestSelection(tiny, POINT{100, 100}, kHandle) == Grab::Move);
    CHECK(HitTestSelection(tiny, POINT{110, 110}, kHandle) == Grab::Move);
    CHECK(HitTestSelection(tiny, POINT{119, 119}, kHandle) == Grab::Move);
}

// --- OffsetClamped ---------------------------------------------------------

CRISP_TEST(GeometryGrab, Tasima_boyutu_korur) {
    const RECT r{100, 100, 300, 200};
    CHECK_RECT(OffsetClamped(r, 50, 25, kScreen), 150, 125, 350, 225);
}

CRISP_TEST(GeometryGrab, Tasima_kenarda_durur_daralmaz) {
    // ASIL MESELE BU. ClampTo her kenarı bağımsız kırpıyor, yani kenara
    // dayanan bir seçimi daraltırdı; taşımak boyutu değiştirmemeli.
    const RECT r{100, 100, 300, 200};
    const LONG w = Width(r);
    const LONG h = Height(r);

    const RECT left = OffsetClamped(r, -500, 0, kScreen);
    CHECK_RECT(left, 0, 100, w, 200);
    const RECT top = OffsetClamped(r, 0, -500, kScreen);
    CHECK_RECT(top, 100, 0, 300, h);
    const RECT right = OffsetClamped(r, 5000, 0, kScreen);
    CHECK_RECT(right, 1920 - w, 100, 1920, 200);
    const RECT bottom = OffsetClamped(r, 0, 5000, kScreen);
    CHECK_RECT(bottom, 100, 1080 - h, 300, 1080);

    // Köşeye doğru: iki eksen birden durur, boyut yine korunur.
    const RECT corner = OffsetClamped(r, -5000, -5000, kScreen);
    CHECK_RECT(corner, 0, 0, w, h);
}

CRISP_TEST(GeometryGrab, Tasima_negatif_koordinatli_masaustunde) {
    const RECT r{-1800, 100, -1600, 200};
    CHECK_RECT(OffsetClamped(r, -500, 0, kMultiScreen), -1920, 100, -1720, 200);
    CHECK_RECT(OffsetClamped(r, 100, 0, kMultiScreen), -1700, 100, -1500, 200);
}

CRISP_TEST(GeometryGrab, Sinirdan_buyuk_dikdortgen_kirpilir) {
    // Sığdırmanın yolu yok; elde kalan tek davranış kırpmak.
    const RECT huge{-100, -100, 3000, 2000};
    CHECK_RECT(OffsetClamped(huge, 10, 10, kScreen), 0, 0, 1920, 1080);
}

// --- ResizeByGrab ----------------------------------------------------------

CRISP_TEST(GeometryGrab, Her_tutamak_yalniz_kendi_kenarini_tasir) {
    const RECT origin{100, 100, 400, 300};

    CHECK_RECT(ResizeByGrab(origin, Grab::W, POINT{50, 999}, kMinSide, kScreen),
               50, 100, 400, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::E, POINT{500, 999}, kMinSide, kScreen),
               100, 100, 500, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::N, POINT{999, 50}, kMinSide, kScreen),
               100, 50, 400, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::S, POINT{999, 400}, kMinSide, kScreen),
               100, 100, 400, 400);

    // Köşeler iki kenarı birden taşır, diğer ikisi yerinde kalır.
    CHECK_RECT(ResizeByGrab(origin, Grab::NW, POINT{50, 50}, kMinSide, kScreen),
               50, 50, 400, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::SE, POINT{500, 400}, kMinSide, kScreen),
               100, 100, 500, 400);
    CHECK_RECT(ResizeByGrab(origin, Grab::NE, POINT{500, 50}, kMinSide, kScreen),
               100, 50, 500, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::SW, POINT{50, 400}, kMinSide, kScreen),
               50, 100, 400, 400);
}

CRISP_TEST(GeometryGrab, Kenar_karsisinin_otesine_gecemez) {
    // Sürükleme ortasında tutamağın adı değişmez: kenar `minSide`da durur ve
    // dikdörtgen dönmez.
    const RECT origin{100, 100, 400, 300};

    // Sol kenarı sağın ötesine: sağ 400'de kalır, sol 394'te durur.
    CHECK_RECT(ResizeByGrab(origin, Grab::W, POINT{900, 200}, kMinSide, kScreen),
               400 - kMinSide, 100, 400, 300);
    // Sağ kenarı solun ötesine.
    CHECK_RECT(ResizeByGrab(origin, Grab::E, POINT{20, 200}, kMinSide, kScreen),
               100, 100, 100 + kMinSide, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::N, POINT{200, 900}, kMinSide, kScreen),
               100, 300 - kMinSide, 400, 300);
    CHECK_RECT(ResizeByGrab(origin, Grab::S, POINT{200, 20}, kMinSide, kScreen),
               100, 100, 400, 100 + kMinSide);
}

CRISP_TEST(GeometryGrab, Boyutlandirma_ekranin_disina_tasmaz) {
    // Bu doğrulama olmasa, ekran dışına çekilmiş bir seçimde Enter hiçbir şey
    // yapmaz: CropImage aralık dışı dikdörtgeni reddediyor ve kullanıcı sessiz
    // bir başarısızlık görüyor.
    const RECT origin{100, 100, 400, 300};

    const RECT up = ResizeByGrab(origin, Grab::N, POINT{200, -500}, kMinSide, kScreen);
    CHECK(up.top >= kScreen.top);
    const RECT down = ResizeByGrab(origin, Grab::S, POINT{200, 5000}, kMinSide, kScreen);
    CHECK(down.bottom <= kScreen.bottom);
    const RECT left = ResizeByGrab(origin, Grab::W, POINT{-500, 200}, kMinSide, kScreen);
    CHECK(left.left >= kScreen.left);
    const RECT right = ResizeByGrab(origin, Grab::E, POINT{5000, 200}, kMinSide, kScreen);
    CHECK(right.right <= kScreen.right);
}

CRISP_TEST(GeometryGrab, Hicbir_tutamak_bos_dikdortgen_uretmez) {
    const RECT origin{100, 100, 400, 300};
    const Grab all[] = {Grab::N,  Grab::S,  Grab::E,  Grab::W,
                        Grab::NE, Grab::NW, Grab::SE, Grab::SW};
    const POINT extremes[] = {{-9999, -9999}, {9999, 9999}, {0, 0},
                              {1920, 1080},   {250, 200}};

    for (const Grab grab : all) {
        for (const POINT p : extremes) {
            const RECT r = ResizeByGrab(origin, grab, p, kMinSide, kScreen);
            CHECK(!IsEmpty(r));
            CHECK(Width(r) >= kMinSide);
            CHECK(Height(r) >= kMinSide);
        }
    }
}

CRISP_TEST(GeometryGrab, En_kucuk_kenar_ekran_kenarinda_da_tutulur) {
    // Sınırın dibindeki bir kenarı `minSide` kadar geri itmek onu sınırın
    // ötesine çıkarabilir; ikinci bir clamp bunu toparlıyor.
    const RECT origin{0, 0, 300, 200};
    const RECT r = ResizeByGrab(origin, Grab::W, POINT{900, 100}, kMinSide, kScreen);
    CHECK(r.left >= kScreen.left);
    CHECK(Width(r) >= kMinSide);
}
