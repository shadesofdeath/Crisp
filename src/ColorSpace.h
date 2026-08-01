// ColorSpace.h — Renk dönüşümleri ve metin biçimleri.
//
// PENCERE YOK: renk seçicinin matematiği buradadır ve çekirdekte durur, çünkü
// "ton şeridinin ucu tam kırmızıya dönüyor mu" ya da "#0f0 üç haneli hâliyle
// okunuyor mu" gibi sorular pencere açmadan sınanabilmeli. Seçicinin kendisi
// (ColorPicker.cpp) yalnızca bu fonksiyonları çizime bağlar.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

// Ton 0..360 (derece), doygunluk ve parlaklık 0..1.
//
// TON DERECE OLARAK: renk çarkı bir açıdır ve 0..1'e sıkıştırmak, kullanıcıya
// gösterilecek "210°" gibi bir değeri her seferinde geri çarpmak demekti.
struct Hsv {
    double hue = 0.0;
    double saturation = 0.0;
    double value = 0.0;
};

[[nodiscard]] Hsv RgbToHsv(COLORREF color) noexcept;
[[nodiscard]] COLORREF HsvToRgb(const Hsv& hsv) noexcept;

// "#1E90FF", "1e90ff", "#0f0" ve "0f0" kabul edilir. Üç hane, her hanenin
// ikilenmesidir (CSS kuralı): #0f0 → #00ff00.
//
// BAŞARISIZLIKTA out'a DOKUNULMAZ: kullanıcı hex alanına yazarken her tuş
// sonrası dize geçersiz olur ve her yarım girdi rengi sıfırlasaydı alan
// kullanılamazdı.
[[nodiscard]] bool ParseHexColor(const wchar_t* text, COLORREF& out) noexcept;

// Daima "#RRGGBB", büyük harf.
[[nodiscard]] std::wstring FormatHexColor(COLORREF color);

// Algısal parlaklık 0..255 (ITU-R BT.601 ağırlıkları).
[[nodiscard]] int RelativeLuma(COLORREF color) noexcept;

// Bu rengin ÜSTÜNE yazılacak metin koyu mu olmalı? Sarı bir örneğin üstüne
// beyaz yazmak onu okunmaz yapar; kararı gözle vermek yerine parlaklık
// hesaplanır.
[[nodiscard]] bool PrefersDarkInk(COLORREF background) noexcept;

// İki renk birbirinden AYIRT EDİLEBİLİR mi?
//
// "İkisi de koyu mu" diye sormak yetmez: koyu bir arayüz zemininde kırmızı
// (parlaklık 116) gayet okunur ama ikisi de "koyu" sayıldığı için reddedilirdi
// — kalınlık listesindeki önizleme çizgisi tam olarak bu yüzden kullanıcının
// seçtiği rengi değil beyazı gösteriyordu.
[[nodiscard]] bool HasContrast(COLORREF a, COLORREF b) noexcept;

}  // namespace crisp
