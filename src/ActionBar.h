// ActionBar.h — Yerleşmiş seçimin yanındaki eylem çubuğunun DÜZENİ.
//
// NEDEN VAR: seçim artık bırakılınca yerleşiyor ve orada duruyor, ama o anda
// yapılabilecek tek şey Enter'a basıp AYARLARDA ne yazıyorsa onu almaktı. "Bunu
// bir kereliğine kaydetmek istiyorum" demek, ayarlar penceresini açıp kutu
// işaretlemeyi, yakalamayı, sonra geri dönüp kutuyu kaldırmayı gerektiriyordu.
//
// ÇEKİRDEKTE, ÇÜNKÜ ÇİZİM DEĞİL. Buradaki her şey — kaç düğme var, nerede
// duruyorlar, imlecin altındaki hangisi — pencere açmadan hesaplanabilir ve
// hesaplanabildiği için sınanabilir. Simgelerin nasıl çizildiği uygulama
// katmanında (OverlayActions.cpp).
//
// YERLEŞİM VE İSABET AYNI FONKSİYONDAN GELİR. Çizilen düğme kümesi ile
// tıklanabilen kümenin ayrı hesaplandığı bir sürüm bu projede zaten yaşandı:
// seçimin sekiz tutamağı aylarca çizildi ve hiçbiri fareye yanıt vermedi.
#pragma once

#include <windows.h>

namespace crisp {

// Yerleşmiş seçimin yanındaki çubuktan seçilen eylem.
//
// AYARLARI BU YAKALAMA İÇİN GEÇERSİZ KILAR. Yakalama sonrası listesi "her
// yakalamada ne olsun" sorusunun cevabı; çubuk ise "BU yakalamada ne olsun"
// sorusunun.
//
// `None`: çubuğa dokunulmadı, ayarlardaki eylemler çalışır. Enter ve çift tık
// bunu verir — varsayılan davranış değişmiyor.
enum class OverlayAction {
    None = 0,
    Copy,
    Save,
    Edit,
    Pin,
    Upload,
    Ocr,
    Count,
};

// Çubuktaki bir düğme.
struct ActionButton {
    OverlayAction action = OverlayAction::None;
    RECT bounds{};     // ekran koordinatı
    UINT nameId = 0;   // ipucu balonundaki ad (IDS_BAR_*)
};

// Çubuğun 96 dpi mantıksal ölçüleri.
inline constexpr int kActionButtonSide = 34;
inline constexpr int kActionBarPadding = 4;
inline constexpr int kActionBarGap = 8;

// Çubuğun düğmelerini ve kapladığı dikdörtgeni hesaplar. `buttons` en az
// `OverlayAction::Count` öğe almalı; dönen sayı gerçek düğme sayısıdır.
//
// `uploadEnabled` false ise Yükle düğmesi HİÇ ÇİZİLMEZ. Kapalı bir düğme
// göstermek, kullanıcıya tıklayıp neden hiçbir şey olmadığını sorduran bir şey
// olurdu; ayarlarda servis seçilmemişken yükleme diye bir seçenek de yok.
//
// Seçim çubuğu barındıramayacak kadar darsa 0 döner ve çubuk çizilmez.
[[nodiscard]] int ActionButtons(const RECT& selection, const RECT& monitor,
                                unsigned dpi, bool uploadEnabled,
                                ActionButton* buttons, RECT& barBounds) noexcept;

// Noktanın altındaki düğmenin dizini; yoksa -1.
[[nodiscard]] int ActionButtonAt(const ActionButton* buttons, int count,
                                 POINT screenPoint) noexcept;

}  // namespace crisp
