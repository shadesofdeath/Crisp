// Capture.h — Ekran pikselleri → bellek içi görüntü.
//
// TEK GÖRÜNTÜ TİPİ: her yerde 32 bit, YUKARIDAN AŞAĞI (top-down) BGRA DIB
// kullanılır. Windows'un varsayılanı aşağıdan yukarı DIB'dir; onu seçseydik
// yakalama, büyüteç, PNG kodlama ve pano kodunun her birinde satırları ters
// çevirmeyi hatırlamak gerekirdi ve biri unutulduğunda görüntü sessizce baş
// aşağı çıkardı. Negatif yükseklikle bir kez top-down istemek, o hatanın
// tamamını ortadan kaldırır.
#pragma once

#include "Util.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace crisp {

class Image {
public:
    Image() noexcept = default;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    // width/height pozitif olmalı. Başarısızlıkta nesne geçersiz kalır.
    [[nodiscard]] bool Create(int width, int height);

    [[nodiscard]] bool Valid() const noexcept {
        return m_bitmap.valid() && m_bits != nullptr;
    }

    [[nodiscard]] int Width() const noexcept { return m_width; }
    [[nodiscard]] int Height() const noexcept { return m_height; }

    // Satır uzunluğu bayt cinsinden. 32 bpp'de daima width * 4'tür; DIB
    // hizalaması 4 baytın katıdır ve piksel zaten 4 bayttır.
    [[nodiscard]] int Stride() const noexcept { return m_width * 4; }

    [[nodiscard]] HBITMAP Handle() const noexcept { return m_bitmap.get(); }
    [[nodiscard]] void* Bits() const noexcept { return m_bits; }

    // 0xAARRGGBB düzeninde tek piksel. Sınır dışı okuma 0 döner — testlerin ve
    // büyütecin kenarlarda ayrı bir kontrol yazmasına gerek kalmasın.
    [[nodiscard]] uint32_t Pixel(int x, int y) const noexcept;
    void SetPixel(int x, int y, uint32_t argb) noexcept;

    // Tüm yüzeyi tek renge boyar (testler ve arka plan için).
    void Fill(uint32_t argb) noexcept;

    void Reset() noexcept;

private:
    unique_hbitmap m_bitmap;
    void* m_bits = nullptr;   // DIB bölümüne ait; m_bitmap ile birlikte ölür
    int m_width = 0;
    int m_height = 0;
};

// Tüm monitörleri kapsayan dikdörtgen. Sol-üst monitör birincil değilse
// koordinatlar NEGATİF olabilir; bu yüzden hiçbir yerde "sol üst (0,0)"
// varsayılmaz.
[[nodiscard]] RECT VirtualScreenRect() noexcept;

// İmlecin bulunduğu monitörün sınırları.
[[nodiscard]] RECT MonitorRectAtCursor() noexcept;

// Verilen ekran noktasını barındıran monitörün sınırları.
//
// MonitorRectAtCursor imleci ÇAĞRI ANINDA sorar. Kaplama boyarken imlecin
// nerede olduğunu zaten biliyor — girdi olayıyla gelen konumu taşıyor — ve onu
// yeniden sormak, fare olay ile boyama arasında kıpırdadığında bir kareyi
// yanlış monitöre çizer. Nokta bu yüzden dışarıdan verilir.
//
// Nokta hiçbir monitörün içinde değilse (monitörler arasındaki boşluk, ya da
// sanal ekranın dışı) en yakın monitör döner: hiçbir zaman boş dikdörtgen
// dönmez, çünkü çağıranların hepsi bir şey ortalamaya çalışıyor.
[[nodiscard]] RECT MonitorRectAtPoint(POINT point) noexcept;

// Tüm monitörler, SOLDAN SAĞA sıralı. Sıra kararlı olmalı: kullanıcı
// "2. monitör" dediğinde her seferinde aynı ekranı almalı ve EnumDisplayMonitors
// sürücü sırasını verir, ekrandaki diziliş sırasını değil.
[[nodiscard]] std::vector<RECT> MonitorRects();

// Verilen pencereyi barındıran monitörün sınırları.
[[nodiscard]] RECT MonitorRectForWindow(HWND window) noexcept;

// Ekran koordinatlarındaki dikdörtgeni yakalar. Boş dikdörtgen başarısızlıktır.
//
// includeCursor: fare imleci de görüntüye çizilir.
//
// BitBlt İMLECİ ALMAZ — CAPTUREBLT bayrağı yalnızca katmanlı pencereleri
// katar, imleci değil. İmleç ayrıca sorulup elle çizilmelidir ve bu, bir
// ekran alıntısı aracında en çok istenen eksiklerden biriydi: "şuraya tıkla"
// anlatan bir görüntüde imlecin olmaması onu anlaşılmaz kılıyor.
[[nodiscard]] bool CaptureRect(const RECT& screenRect, Image& out,
                               bool includeCursor = false);

// Kaynağın bir bölgesini yeni bir görüntüye kopyalar. x/y KAYNAĞA GÖRE
// koordinattır (ekran koordinatı değil); çağıran dönüşümü kendisi yapar.
// İstenen bölge kaynağın dışına taşıyorsa başarısız olur — sessizce kırpmak,
// kullanıcının seçtiğinden farklı bir görüntü döndürmek olurdu.
[[nodiscard]] bool CropImage(const Image& source, int x, int y, int width,
                             int height, Image& out);

// Pencereyi yakalar. DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) ile
// GERÇEK çerçeve sınırı alınır: GetWindowRect, Windows 10/11'de görünmez
// yeniden boyutlandırma kenarlığını da içerir ve düz GetWindowRect ile yakalanan
// her pencere görüntüsünün kenarında birkaç piksel arka plan kalır.
[[nodiscard]] bool CaptureWindow(HWND window, Image& out,
                                 bool includeCursor = false);

}  // namespace crisp
